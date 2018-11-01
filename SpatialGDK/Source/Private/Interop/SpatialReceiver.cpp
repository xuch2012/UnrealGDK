// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
#pragma optimize("", off)

#include "Interop/SpatialReceiver.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "EngineClasses/SpatialActorChannel.h"
#include "EngineClasses/SpatialNetConnection.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Engine/World.h"
#include "Interop/Connection/SpatialWorkerConnection.h"
#include "Interop/GlobalStateManager.h"
#include "Interop/SpatialPlayerSpawner.h"
#include "Interop/SpatialSender.h"
#include "Schema/DynamicComponent.h"
#include "Schema/Rotation.h"
#include "Schema/UnrealMetadata.h"
#include "SpatialConstants.h"
#include "Utils/ComponentReader.h"
#include "Utils/EntityRegistry.h"
#include "Utils/RepLayoutUtils.h"

DEFINE_LOG_CATEGORY(LogSpatialReceiver);

using namespace improbable;

template <typename T>
T* GetComponentData(USpatialReceiver& Receiver, Worker_EntityId EntityId)
{
	for (PendingAddComponentWrapper& PendingAddComponent : Receiver.PendingAddComponents)
	{
		if (PendingAddComponent.EntityId == EntityId && PendingAddComponent.ComponentId == T::ComponentId)
		{
			return static_cast<T*>(PendingAddComponent.Data.Get());
		}
	}

	return nullptr;
}

void USpatialReceiver::Init(USpatialNetDriver* InNetDriver, FTimerManager* InTimerManager)
{
	NetDriver = InNetDriver;
	StaticComponentView = InNetDriver->StaticComponentView;
	Sender = InNetDriver->Sender;
	PackageMap = InNetDriver->PackageMap;
	World = InNetDriver->GetWorld();
	TypebindingManager = InNetDriver->TypebindingManager;
	GlobalStateManager = InNetDriver->GlobalStateManager;
	TimerManager = InTimerManager;
}

void USpatialReceiver::OnCriticalSection(bool InCriticalSection)
{
	if (InCriticalSection)
	{
		EnterCriticalSection();
	}
	else
	{
		LeaveCriticalSection();
	}
}

void USpatialReceiver::EnterCriticalSection()
{
	UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entering critical section."));
	check(!bInCriticalSection);
	bInCriticalSection = true;
}

void USpatialReceiver::LeaveCriticalSection()
{
	UE_LOG(LogSpatialReceiver, Verbose, TEXT("Leaving critical section."));
	check(bInCriticalSection);

	for (Worker_EntityId& PendingAddEntity : PendingAddEntities)
	{
		ReceiveActor(PendingAddEntity);
	}

	for (Worker_AuthorityChangeOp& PendingAuthorityChange : PendingAuthorityChanges)
	{
		HandleActorAuthority(PendingAuthorityChange);
	}

	for (Worker_EntityId& PendingRemoveEntity : PendingRemoveEntities)
	{
		RemoveActor(PendingRemoveEntity);
	}

	// Mark that we've left the critical section.
	bInCriticalSection = false;
	PendingAddEntities.Empty();
	PendingAddComponents.Empty();
	PendingAuthorityChanges.Empty();
	PendingRemoveEntities.Empty();
}

void USpatialReceiver::OnAddEntity(Worker_AddEntityOp& Op)
{
	UE_LOG(LogSpatialReceiver, Verbose, TEXT("AddEntity: %lld"), Op.entity_id);

	check(bInCriticalSection);

	PendingAddEntities.Emplace(Op.entity_id);
}

void USpatialReceiver::OnAddComponent(Worker_AddComponentOp& Op)
{
	UE_LOG(LogSpatialReceiver, Verbose, TEXT("AddComponent component ID: %u entity ID: %lld"),
		Op.data.component_id, Op.entity_id);

	if (!bInCriticalSection)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Received a dynamically added component, these are currently unsupported - component ID: %u entity ID: %lld"),
			Op.data.component_id, Op.entity_id);
		return;
	}

	TSharedPtr<improbable::Component> Data;

	switch (Op.data.component_id)
	{
	case SpatialConstants::ENTITY_ACL_COMPONENT_ID:
	case SpatialConstants::METADATA_COMPONENT_ID:
	case SpatialConstants::POSITION_COMPONENT_ID:
	case SpatialConstants::PERSISTENCE_COMPONENT_ID:
	case SpatialConstants::ROTATION_COMPONENT_ID:
	case SpatialConstants::PLAYER_SPAWNER_COMPONENT_ID:
	case SpatialConstants::SINGLETON_COMPONENT_ID:
	case SpatialConstants::UNREAL_METADATA_COMPONENT_ID:
		// Ignore static spatial components as they are managed by the SpatialStaticComponentView.
		return;
	case SpatialConstants::GLOBAL_STATE_MANAGER_COMPONENT_ID:
		GlobalStateManager->ApplyData(Op.data);
		GlobalStateManager->LinkExistingSingletonActors();
		return;
	default:
		Data = MakeShared<improbable::DynamicComponent>(Op.data);
		break;
	}

	PendingAddComponents.Emplace(Op.entity_id, Op.data.component_id, Data);
}

void USpatialReceiver::OnRemoveEntity(Worker_RemoveEntityOp& Op)
{
	RemoveActor(Op.entity_id);
}

void USpatialReceiver::OnAuthorityChange(Worker_AuthorityChangeOp& Op)
{
	if (bInCriticalSection)
	{
		PendingAuthorityChanges.Add(Op);
		return;
	}

	HandleActorAuthority(Op);
}

// TODO UNR-640 - This function needs a pass once we introduce soft handover (AUTHORITY_LOSS_IMMINENT)
void USpatialReceiver::HandleActorAuthority(Worker_AuthorityChangeOp& Op)
{
	if (NetDriver->IsServer())
	{
		if (Op.component_id == SpatialConstants::GLOBAL_STATE_MANAGER_COMPONENT_ID
			&& Op.authority == WORKER_AUTHORITY_AUTHORITATIVE)
		{
			GlobalStateManager->ExecuteInitialSingletonActorReplication();
			return;
		}

		// If we became authoritative over the position component. set our role to be ROLE_Authority
		// and set our RemoteRole to be ROLE_AutonomousProxy iff the actor has an owning connection.
		if (Op.component_id == SpatialConstants::POSITION_COMPONENT_ID)
		{
			if (AActor* Actor = NetDriver->GetEntityRegistry()->GetActorFromEntityId(Op.entity_id))
			{
				if (Op.authority == WORKER_AUTHORITY_AUTHORITATIVE)
				{
					Actor->Role = ROLE_Authority;

					if (Actor->GetNetConnection() != nullptr)
					{
						Actor->RemoteRole = ROLE_AutonomousProxy;
					}
					else if (Actor->IsA<APawn>())
					{
						Actor->RemoteRole = ROLE_AutonomousProxy;
					}
					else
					{
						Actor->RemoteRole = ROLE_SimulatedProxy;
					}
				}
				else if (Op.authority == WORKER_AUTHORITY_NOT_AUTHORITATIVE)
				{
					Actor->Role = ROLE_SimulatedProxy;
					Actor->RemoteRole = ROLE_Authority;
				}
			}
		}
	}
	else
	{
		// Check to see if we became authoritative over the ClientRPC component over this entity
		// If we did, our local role should be ROLE_AutonomousProxy. Otherwise ROLE_SimulatedProxy
		if (AActor* Actor = NetDriver->GetEntityRegistry()->GetActorFromEntityId(Op.entity_id))
		{
			FClassInfo* Info = TypebindingManager->FindClassInfoByClass(Actor->GetClass());
			check(Info);

			if (Op.component_id == Info->SchemaComponents[SCHEMA_ClientRPC])
			{
				Actor->Role = Op.authority == WORKER_AUTHORITY_AUTHORITATIVE ? ROLE_AutonomousProxy : ROLE_SimulatedProxy;
			}
		}
	}
}

namespace {
class FSpatialActorCreator {
private:
	// This stuff needs to be initialized at creation.
	Worker_EntityId EntityId;
	USpatialNetDriver* NetDriver;
	UWorld* World;
	USpatialStaticComponentView* StaticComponentView;
	UEntityRegistry* EntityRegistry;
	USpatialTypebindingManager* TypebindingManager;
	USpatialSender* Sender;

public:
	AActor* StaticActor = nullptr;
	AActor* EntityActor = nullptr;
	USpatialActorChannel* Channel = nullptr;

public:
	FSpatialActorCreator(
		Worker_EntityId EntityId,
		USpatialNetDriver* NetDriver)
		: EntityId(EntityId)
		, NetDriver(NetDriver)
	{
		World = NetDriver->GetWorld();
		StaticComponentView = NetDriver->StaticComponentView;
		EntityRegistry = NetDriver->GetEntityRegistry();
		TypebindingManager = NetDriver->TypebindingManager;
		Sender = NetDriver->Sender;
	}

	void CopyAllObjectProperties(UObject* DestObject, UObject* SourceObject, const FString& ActorNameForLogging, USceneComponent*& OutNewAttachParent)
	{
		check(DestObject->IsA(SourceObject->GetClass()));

		for (TFieldIterator<UProperty> PropertyIter(DestObject->GetClass()); PropertyIter; ++PropertyIter)
		{
			UProperty* Property = *PropertyIter;

			// TODO: do only instance editable properties?
			if (Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}

			void* SourcePtr = Property->ContainerPtrToValuePtr<void>(SourceObject);
			void* DestPtr = Property->ContainerPtrToValuePtr<void>(DestObject);

			if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
			{
				UObject* SourceValue = ObjectProperty->GetPropertyValue(SourcePtr);

				if (SourceValue && !SourceValue->IsFullNameStableForNetworking())
				{
					FString RefPath = SourceValue->GetPathName();

					// Fix up the path so its references map to the current world (important for PIE).
					if (GEngine)
					{
						GEngine->NetworkRemapPath(NetDriver, RefPath);
					}

					UE_LOG(LogSpatialReceiver, Log, TEXT("Stably-named actor %s attempting to resolve object reference %s"),
						*ActorNameForLogging, *RefPath);

					// TODO: add to unresolved references if not found
					UObject* RefTarget = FindObject<UObject>(ANY_PACKAGE, *RefPath);
					if (RefTarget)
					{
						// TODO: handle non-ChildActorComponent AttachParent
						if (ObjectProperty->GetName().Contains(TEXT("AttachParent")) && RefTarget->IsA(UChildActorComponent::StaticClass()))
						{
							// Special case. Add ourselves to the parent's AttachChildren array, as well as set the ChildActor property.
							// TODO: handle replicated ChildActorComponent as well
							UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(RefTarget);
							OutNewAttachParent = ChildActorComponent;
							UE_LOG(LogSpatialReceiver, Log, TEXT("Stably-named actor %s assigning myself as a child to object %s because of property %s"),

								*ActorNameForLogging,
								RefTarget ? *RefTarget->GetFullName() : TEXT("nullptr"),
								*ObjectProperty->GetName());
						}
						else
						{
							ObjectProperty->SetObjectPropertyValue(DestPtr, RefTarget);
							UE_LOG(LogSpatialReceiver, Log, TEXT("Stably-named actor %s mapping object reference %s to object %s"),
								*ActorNameForLogging,
								*ObjectProperty->GetName(),
								RefTarget ? *RefTarget->GetFullName() : TEXT("nullptr"));
						}
					}
					else
					{
						UE_LOG(LogSpatialReceiver, Log, TEXT("Stably-named actor %s failed to resolve object reference %s for property %s"),
							*ActorNameForLogging,
							*RefPath,
							*ObjectProperty->GetName());
					}
				}
				else
				{
					UE_LOG(LogSpatialReceiver, Log, TEXT("Stably-named actor %s ignoring object reference %s for property %s"),
						*ActorNameForLogging,
						SourceValue ? *SourceValue->GetFullName() : TEXT("nullptr"),
						*ObjectProperty->GetName());
				}
			}
			else
			{
				// Copy the actual property value over to the new actor's component.
				Property->CopyCompleteValue(DestPtr, SourcePtr);
			}
		}
	}

	void CopyAllObjectProperties(UObject* DestObject, UObject* SourceObject, const FString& ActorNameForLogging)
	{
		USceneComponent* DummyNewAttachParent = nullptr;
		CopyAllObjectProperties(DestObject, SourceObject, ActorNameForLogging, DummyNewAttachParent);
	}

	void CopyAllComponentProperties(AActor* DestActor, AActor* SourceActor, const FString& ActorNameForLogging, USceneComponent*& OutNewAttachParent)
	{
		// Gather the components from the template actor by name.
		TMap<FString, UActorComponent*> SourceComponentMap;
		for (UActorComponent* SourceComponent : SourceActor->GetComponents())
		{
			SourceComponentMap.Emplace(SourceComponent->GetName(), SourceComponent);
		}

		// Walk through the new actor's components and copy over values from the template actor.
		USceneComponent* DestRootComponent = DestActor->GetRootComponent();
		for (UActorComponent* DestComponent : DestActor->GetComponents())
		{
			UActorComponent** SourceComponentPtr = SourceComponentMap.Find(DestComponent->GetName());
			if (SourceComponentPtr == nullptr || *SourceComponentPtr == nullptr)
			{
				UE_LOG(LogSpatialReceiver, Error, TEXT("Couldn't find component data %s to copy to stably-named actor %s from %s"),
					*DestComponent->GetFullName(), *ActorNameForLogging, *SourceActor->GetFullName());
				continue;
			}
			UActorComponent* SourceComponent = *SourceComponentPtr;

			if (DestComponent == DestRootComponent)
			{
				CopyAllObjectProperties(DestComponent, SourceComponent, ActorNameForLogging, OutNewAttachParent);
			}
			else
			{
				CopyAllObjectProperties(DestComponent, SourceComponent, ActorNameForLogging);
			}
		}
	}

	AActor* CreateActor(improbable::Position* Position, improbable::Rotation* Rotation, UClass* ActorClass, bool bDeferred)
	{
		return CreateActor(Position, Rotation, ActorClass, bDeferred, NAME_None);
	}

	AActor* CreateActor(improbable::Position* Position, improbable::Rotation* Rotation, UClass* ActorClass, bool bDeferred, FName ActorName)
	{
		FVector InitialLocation = improbable::Coordinates::ToFVector(Position->Coords);
		FRotator InitialRotation = Rotation->ToFRotator();
		AActor* NewActor = nullptr;
		if (ActorClass)
		{
			//bRemoteOwned needs to be public in source code. This might be a controversial change.
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bRemoteOwned = !NetDriver->IsServer();
			SpawnInfo.bNoFail = true;
			// We defer the construction in the GDK pipeline to allow initialization of replicated properties first.
			SpawnInfo.bDeferConstruction = bDeferred;
			if (!ActorName.IsNone())
			{
				SpawnInfo.Name = ActorName;
			}

			FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(InitialLocation, World->OriginLocation);

			NewActor = World->SpawnActorAbsolute(ActorClass, FTransform(InitialRotation, SpawnLocation), SpawnInfo);
			check(NewActor);
		}

		return NewActor;
	}

	AActor* CreateNewStablyNamedActor(const FString& StablePath, improbable::Position* Position, improbable::Rotation* Rotation, UClass* ActorClass, Worker_EntityId EntityId)
	{
		StaticActor = Cast<AActor>(StaticLoadObject(AActor::StaticClass(), nullptr, *StablePath));
		if (StaticActor == nullptr)
		{
			UE_LOG(LogSpatialReceiver, Warning, TEXT("Failed to find stably-named actor for entity %lld with path %s"), EntityId, *StablePath);
			return nullptr;
		}

		AActor* NewActor = CreateActor(Position, Rotation, ActorClass, true, StaticActor->GetFName());
		if (NewActor == nullptr)
		{
			UE_LOG(LogSpatialReceiver, Error, TEXT("Failed to create new stably-named actor for entity %lld from template %s"), EntityId, *StaticActor->GetFullName());
			return nullptr;
		}

		// Copy over property values from the template actor into the new one.
		//FString ActorNameForLogging = FString::Printf(TEXT("%s (entity %lld)"), *NewActor->GetFullName(), EntityId);
		//CopyAllObjectProperties(NewActor, StaticActor, ActorNameForLogging, OutNewAttachParent);
		//CopyAllComponentProperties(NewActor, StaticActor, ActorNameForLogging, OutNewAttachParent);

		// Initialize components after copying over their data from the map.
		//check(!NewActor->IsActorInitialized());
		//NewActor->PreInitializeComponents();
		//NewActor->InitializeComponents();
		//NewActor->PostInitializeComponents();
		//check(NewActor->IsActorInitialized());

		UE_LOG(LogSpatialReceiver, Log, TEXT("Created actor %s from stably-named actor %s for entity %lld"),
			*NewActor->GetFName().ToString(), *StaticActor->GetFullName(), EntityId);

		return NewActor;
	}

	bool CreateActorForEntity()
	{
		improbable::Position* Position = StaticComponentView->GetComponentData<improbable::Position>(EntityId);
		improbable::Metadata* Metadata = StaticComponentView->GetComponentData<improbable::Metadata>(EntityId);
		improbable::Rotation* Rotation = StaticComponentView->GetComponentData<improbable::Rotation>(EntityId);

		check(Position && Metadata);

		UClass* ActorClass = Metadata->GetNativeEntityClass();

		if (ActorClass == nullptr)
		{
			// TODO: error
			return false;
		}

		// Initial Singleton Actor replication is handled with GlobalStateManager::LinkExistingSingletonActors
		if (NetDriver->IsServer() && ActorClass->HasAnySpatialClassFlags(SPATIALCLASS_Singleton))
		{
			return false;
		}

		improbable::UnrealMetadata* UnrealMetadataComponent = StaticComponentView->GetComponentData<improbable::UnrealMetadata>(EntityId);
		check(UnrealMetadataComponent);

		UNetConnection* Connection = nullptr;
		bool bDoingDeferredSpawn = false;

		// If we're checking out a player controller, spawn it via "USpatialNetDriver::AcceptNewPlayer"
		if (NetDriver->IsServer() && ActorClass->IsChildOf(APlayerController::StaticClass()))
		{
			checkf(!UnrealMetadataComponent->OwnerWorkerAttribute.IsEmpty(), TEXT("A player controller entity must have an owner worker attribute."));

			FString URLString = FURL().ToString();
			URLString += TEXT("?workerAttribute=") + UnrealMetadataComponent->OwnerWorkerAttribute;

			Connection = NetDriver->AcceptNewPlayer(FURL(nullptr, *URLString, TRAVEL_Absolute), true);
			check(Connection);

			EntityActor = Connection->PlayerController;
		}
		else
		{
			UE_LOG(LogSpatialReceiver, Verbose, TEXT("Spawning a %s whilst checking out an entity."), *ActorClass->GetFullName());

			if (EntityActor == nullptr && !UnrealMetadataComponent->StaticPath.IsEmpty())
			{
				// If this actor has a stable path, attempt to load the object from the map file to grab its initial data.
				// Note that this can fail and return a null actor.
				EntityActor = CreateNewStablyNamedActor(*UnrealMetadataComponent->StaticPath, Position, Rotation, ActorClass, EntityId);
			}

			if (EntityActor == nullptr)
			{
				EntityActor = CreateActor(Position, Rotation, ActorClass, true);
			}

			if (EntityActor == nullptr)
			{
				// If we've gotten this far and still don't have an actor, something has gone wrong, so early out.
				UE_LOG(LogSpatialReceiver, Error, TEXT("Failed to spawn an actor for entity %lld of class %s"), EntityId, *ActorClass->GetFullName());
				return false;
			}

			bDoingDeferredSpawn = true;

			// Don't have authority over Actor until SpatialOS delegates authority
			EntityActor->Role = ROLE_SimulatedProxy;
			EntityActor->RemoteRole = ROLE_Authority;

			// Get the net connection for this actor.
			if (NetDriver->IsServer())
			{
				// Currently, we just create an actor channel on the "catch-all" connection, then create a new actor channel once we check out the player controller
				// and create a new connection. This is fine due to lazy actor channel creation in USpatialNetDriver::ServerReplicateActors. However, the "right" thing to do
				// would be to make sure to create anything which depends on the PlayerController _after_ the PlayerController's connection is set up so we can use the right
				// one here. We should revisit this after implementing working sets - UNR:411
				Connection = NetDriver->GetSpatialOSNetConnection();
			}
			else
			{
				Connection = NetDriver->GetSpatialOSNetConnection();
			}
		}

		// Set up actor channel.
		USpatialPackageMapClient* SpatialPackageMap = Cast<USpatialPackageMapClient>(Connection->PackageMap);
		Channel = Cast<USpatialActorChannel>(Connection->CreateChannel(CHTYPE_Actor, NetDriver->IsServer()));
		if (!Channel)
		{
			UE_LOG(LogSpatialReceiver, Warning, TEXT("Failed to create an actor channel when receiving entity %lld. The actor will not be spawned."), EntityId);
			EntityActor->Destroy(true);
			return false;
		}

		// Add to entity registry.
		EntityRegistry->AddToRegistry(EntityId, EntityActor);

		FVector InitialLocation = improbable::Coordinates::ToFVector(Position->Coords);
		FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(InitialLocation, World->OriginLocation);
		FRotator SpawnRotation = Rotation->ToFRotator();
		if (bDoingDeferredSpawn)
		{
			EntityActor->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));
		}

		if (StaticActor)
		{
			// Populated when setting component values, used to inform the parent that it has a new child.
			USceneComponent* NewAttachParent = nullptr;

			// Copy component properties over from the static actor, if we're spawning from one.
			// NOTE: we do this after FinishSpawning because components that exist only in blueprints won't be added until then.
			FString ActorNameForLogging = FString::Printf(TEXT("%s (entity %lld)"), *EntityActor->GetFullName(), EntityId);
			CopyAllObjectProperties(EntityActor, StaticActor, ActorNameForLogging, NewAttachParent);
			CopyAllComponentProperties(EntityActor, StaticActor, ActorNameForLogging, NewAttachParent);

			// Update the final transform for this actor to account for any differences in the static actor's scale.
			// TODO: scale should probably be spatially-replicated in case it changes
			StaticActor->UpdateComponentTransforms();
			EntityActor->UpdateComponentTransforms();
			EntityActor->SetActorTransform(FTransform(SpawnRotation, SpawnLocation, StaticActor->GetActorScale3D()));

			// TODO: probably need to do this after applying SpatialOS component data too.
			if (NewAttachParent)
			{
				NewAttachParent->ConditionalUpdateComponentToWorld();
				EntityActor->SetActorTransform(NewAttachParent->GetComponentTransform());
				EntityActor->AttachToComponent(NewAttachParent, FAttachmentTransformRules::SnapToTargetIncludingScale);
			}

			// After all of the above, make sure the transforms within the actor are up to date.
			EntityActor->UpdateComponentTransforms();
			EntityActor->UpdateOverlaps();
		}

		FClassInfo* Info = TypebindingManager->FindClassInfoByClass(ActorClass);
		SpatialPackageMap->ResolveEntityActor(EntityActor, EntityId, improbable::CreateOffsetMapFromActor(EntityActor, Info));
		Channel->SetChannelActor(EntityActor);

		return true;
	}

	void FinalizeNewActor()
	{
		if (!NetDriver->IsServer())
		{
			// Update interest on the entity's components after receiving initial component data (so Role and RemoteRole are properly set).
			Sender->SendComponentInterest(EntityActor, EntityId);

			// This is a bit of a hack unfortunately, among the core classes only PlayerController implements this function and it requires
			// a player index. For now we don't support split screen, so the number is always 0.
			if (EntityActor->IsA(APlayerController::StaticClass()))
			{
				uint8 PlayerIndex = 0;
				// FInBunch takes size in bits not bytes
				FInBunch Bunch(NetDriver->ServerConnection, &PlayerIndex, sizeof(PlayerIndex) * 8);
				EntityActor->OnActorChannelOpen(Bunch, NetDriver->ServerConnection);
			}
			else
			{
				FInBunch Bunch(NetDriver->ServerConnection);
				EntityActor->OnActorChannelOpen(Bunch, NetDriver->ServerConnection);
			}

		}

		// Taken from PostNetInit
		if (!EntityActor->HasActorBegunPlay())
		{
			EntityActor->DispatchBeginPlay();
		}

		EntityActor->UpdateOverlaps();
	}
};
}

void USpatialReceiver::ReceiveActor(Worker_EntityId EntityId)
{
	checkf(World, TEXT("We should have a world whilst processing ops."));
	check(NetDriver);

	UEntityRegistry* EntityRegistry = NetDriver->GetEntityRegistry();
	check(EntityRegistry);

	if (AActor* EntityActor = EntityRegistry->GetActorFromEntityId(EntityId))
	{
		UE_LOG(LogSpatialReceiver, Log, TEXT("Entity for actor %s has been checked out on the worker which spawned it."), *EntityActor->GetName());

		// Assume SimulatedProxy until we've been delegated Authority
		bool bAuthority = StaticComponentView->GetAuthority(EntityId, improbable::Position::ComponentId) == WORKER_AUTHORITY_AUTHORITATIVE;
		EntityActor->Role = bAuthority ? ROLE_Authority : ROLE_SimulatedProxy;
		EntityActor->RemoteRole = bAuthority ? ROLE_SimulatedProxy : ROLE_Authority;
		if (bAuthority)
		{
			if (EntityActor->GetNetConnection() != nullptr || EntityActor->IsA<APawn>())
			{
				EntityActor->RemoteRole = ROLE_AutonomousProxy;
			}
		}

		UE_LOG(LogSpatialReceiver, Log, TEXT("Received create entity response op for %lld"), EntityId);
	}
	else
	{
		FSpatialActorCreator ActorCreator(EntityId, NetDriver);

		if (!ActorCreator.CreateActorForEntity())
		{
			// Failed or early-outed. Error will have already been printed.
			return;
		}

		// Apply initial replicated properties.
		// This was moved to after FinishingSpawning because components existing only in blueprints aren't added until spawning is complete
		// Potentially we could split out the initial actor state and the initial component state
		for (PendingAddComponentWrapper& PendingAddComponent : PendingAddComponents)
		{
			if (PendingAddComponent.EntityId == EntityId && PendingAddComponent.Data.IsValid() && PendingAddComponent.Data->bIsDynamic)
			{
				ApplyComponentData(EntityId, *static_cast<improbable::DynamicComponent*>(PendingAddComponent.Data.Get())->Data, ActorCreator.Channel);
			}
		}

		ActorCreator.FinalizeNewActor();
	}
}

void USpatialReceiver::RemoveActor(Worker_EntityId EntityId)
{
	AActor* Actor = NetDriver->GetEntityRegistry()->GetActorFromEntityId(EntityId);

	UE_LOG(LogSpatialReceiver, Log, TEXT("Worker %s Remove Actor: %s %lld"), *NetDriver->Connection->GetWorkerId(), Actor ? *Actor->GetName() : TEXT("nullptr"), EntityId);

	// Actor already deleted (this worker was most likely authoritative over it and deleted it earlier).
	if (!Actor || Actor->IsPendingKill())
	{
		if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
		{
			ActorChannel->ConditionalCleanUp();
		}
		CleanupDeletedEntity(EntityId);
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(Actor))
	{
		// Force APlayerController::DestroyNetworkActorHandled to return false
		PC->Player = nullptr;
	}

	// Workaround for camera loss on handover: prevent UnPossess() (non-authoritative destruction of pawn, while being authoritative over the controller)
	// TODO: Check how AI controllers are affected by this (UNR-430)
	// TODO: This should be solved properly by working sets (UNR-411)
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		AController* Controller = Pawn->Controller;

		if (Controller && Controller->HasAuthority())
		{
			Pawn->Controller = nullptr;
		}
	}

	if (Actor->GetClass()->HasAnySpatialClassFlags(SPATIALCLASS_Singleton))
	{
		return;
	}

	// Destruction of actors can cause the destruction of associated actors (eg. Character > Controller). Actor destroy
	// calls will eventually find their way into USpatialActorChannel::DeleteEntityIfAuthoritative() which checks if the entity
	// is currently owned by this worker before issuing an entity delete request. If the associated entity is still authoritative 
	// on this server, we need to make sure this worker doesn't issue an entity delete request, as this entity is really 
	// transitioning to the same server as the actor we're currently operating on, and is just a few frames behind. 
	// We make the assumption that if we're destroying actors here (due to a remove entity op), then this is only due to two
	// situations;
	// 1. Actor's entity has been transitioned to another server
	// 2. The Actor was deleted on another server
	// In neither situation do we want to delete associated entities, so prevent them from being issued.
	// TODO: fix this with working sets (UNR-411)
	NetDriver->StartIgnoringAuthoritativeDestruction();

	// Clean up the actor channel. For clients, this will also call destroy on the actor.
	if (USpatialActorChannel* ActorChannel = NetDriver->GetActorChannelByEntityId(EntityId))
	{
		ActorChannel->ConditionalCleanUp();
	}
	else
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Removing actor as a result of a remove entity op but cannot find the actor channel! Actor: %s %lld"), *Actor->GetName(), EntityId);
	}

	// It is safe to call AActor::Destroy even if the destruction has already started.
	if (!Actor->Destroy(true))
	{
		UE_LOG(LogSpatialReceiver, Error, TEXT("Failed to destroy actor in RemoveActor %s %lld"), *Actor->GetName(), EntityId);
	}
	NetDriver->StopIgnoringAuthoritativeDestruction();

	CleanupDeletedEntity(EntityId);
}

void USpatialReceiver::CleanupDeletedEntity(Worker_EntityId EntityId)
{
	Cast<USpatialPackageMapClient>(NetDriver->GetSpatialOSNetConnection()->PackageMap)->RemoveEntityActor(EntityId);
	NetDriver->GetEntityRegistry()->RemoveFromRegistry(EntityId);
	NetDriver->RemoveActorChannel(EntityId);
}

void USpatialReceiver::ApplyComponentData(Worker_EntityId EntityId, Worker_ComponentData& Data, USpatialActorChannel* Channel)
{
	uint32 Offset = 0;
	bool bFoundOffset = TypebindingManager->FindOffsetByComponentId(Data.component_id, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("EntityId %lld, ComponentId %d - Could not find offset for component id when applying component data to Actor %s!"), EntityId, Data.component_id, *Channel->GetActor()->GetName());
		return;
	}

	UObject* TargetObject = PackageMap->GetObjectFromUnrealObjectRef(FUnrealObjectRef(EntityId, Offset));
	if (TargetObject == nullptr)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("EntityId %lld, ComponentId %d, Offset %d - Could not find target object with given offset for Actor %s!"), EntityId, Data.component_id, Offset, *Channel->GetActor()->GetName());
		return;
	}

	UClass* Class = TypebindingManager->FindClassByComponentId(Data.component_id);
	checkf(Class, TEXT("Component %d isn't hand-written and not present in ComponentToClassMap."), Data.component_id);

	FChannelObjectPair ChannelObjectPair(Channel, TargetObject);

	ESchemaComponentType ComponentType = TypebindingManager->FindCategoryByComponentId(Data.component_id);

	if (ComponentType == SCHEMA_Data || ComponentType == SCHEMA_OwnerOnly)
	{
		FObjectReferencesMap& ObjectReferencesMap = UnresolvedRefsMap.FindOrAdd(ChannelObjectPair);
		TSet<FUnrealObjectRef> UnresolvedRefs;

		ComponentReader Reader(NetDriver, ObjectReferencesMap, UnresolvedRefs, /* bCallRepNotifies */ false);
		Reader.ApplyComponentData(Data, TargetObject, Channel, /* bIsHandover */ false);

		QueueIncomingRepUpdates(ChannelObjectPair, ObjectReferencesMap, UnresolvedRefs);
	}
	else if (ComponentType == SCHEMA_Handover)
	{
		FObjectReferencesMap& ObjectReferencesMap = UnresolvedRefsMap.FindOrAdd(ChannelObjectPair);
		TSet<FUnrealObjectRef> UnresolvedRefs;

		ComponentReader Reader(NetDriver, ObjectReferencesMap, UnresolvedRefs, /* bCallRepNotifies */ false);
		Reader.ApplyComponentData(Data, TargetObject, Channel, /* bIsHandover */ true);

		QueueIncomingRepUpdates(ChannelObjectPair, ObjectReferencesMap, UnresolvedRefs);
	}
	else
	{
		UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entity: %d Component: %d - Skipping because RPC components don't have actual data."), EntityId, Data.component_id);
	}
}

void USpatialReceiver::OnComponentUpdate(Worker_ComponentUpdateOp& Op)
{
	if (StaticComponentView->GetAuthority(Op.entity_id, Op.update.component_id) == WORKER_AUTHORITY_AUTHORITATIVE)
	{
		UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entity: %d Component: %d - Skipping update because this was short circuited"), Op.entity_id, Op.update.component_id);
		return;
	}

	switch (Op.update.component_id)
	{
	case SpatialConstants::ENTITY_ACL_COMPONENT_ID:
	case SpatialConstants::METADATA_COMPONENT_ID:
	case SpatialConstants::POSITION_COMPONENT_ID:
	case SpatialConstants::PERSISTENCE_COMPONENT_ID:
	case SpatialConstants::ROTATION_COMPONENT_ID:
	case SpatialConstants::PLAYER_SPAWNER_COMPONENT_ID:
	case SpatialConstants::SINGLETON_COMPONENT_ID:
	case SpatialConstants::UNREAL_METADATA_COMPONENT_ID:
		UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entity: %d Component: %d - Skipping because this is hand-written Spatial component"), Op.entity_id, Op.update.component_id);
		return;
	case SpatialConstants::GLOBAL_STATE_MANAGER_COMPONENT_ID:
		GlobalStateManager->ApplyUpdate(Op.update);
		GlobalStateManager->LinkExistingSingletonActors();
		return;
	}

	USpatialActorChannel* Channel = NetDriver->GetActorChannelByEntityId(Op.entity_id);
	if (Channel == nullptr)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Worker: %s Entity: %d Component: %d - No actor channel for update"), *NetDriver->Connection->GetWorkerId(), Op.entity_id, Op.update.component_id);
		return;
	}

	FClassInfo* Info = TypebindingManager->FindClassInfoByComponentId(Op.update.component_id);
	if (Info == nullptr)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Entity: %d Component: %d - Couldn't find ClassInfo for component id"), Op.entity_id, Op.update.component_id);
		return;
	}

	uint32 Offset;
	bool bFoundOffset = TypebindingManager->FindOffsetByComponentId(Op.update.component_id, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Entity: %d Component: %d - Couldn't find Offset for component id"), Op.entity_id, Op.update.component_id);
		return;
	}

	UObject* TargetObject = nullptr;

	if (Offset == 0)
	{
		TargetObject = Channel->GetActor();
	}
	else
	{
		TargetObject = PackageMap->GetObjectFromUnrealObjectRef(FUnrealObjectRef(Channel->GetEntityId(), Offset));
	}

	if (TargetObject == nullptr)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("Entity: %d Component: %d - Couldn't find target object for update"), Op.entity_id, Op.update.component_id);
		return;
	}

	ESchemaComponentType Category = TypebindingManager->FindCategoryByComponentId(Op.update.component_id);

	if (Category == ESchemaComponentType::SCHEMA_Data || Category == ESchemaComponentType::SCHEMA_OwnerOnly)
	{
		ApplyComponentUpdate(Op.update, TargetObject, Channel, /* bIsHandover */ false);
	}
	else if (Category == ESchemaComponentType::SCHEMA_Handover)
	{
		if (!NetDriver->IsServer())
		{
			UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entity: %d Component: %d - Skipping Handover component because we're a client."), Op.entity_id, Op.update.component_id);
			return;
		}

		ApplyComponentUpdate(Op.update, TargetObject, Channel, /* bIsHandover */ true);
	}
	else if (Category == ESchemaComponentType::SCHEMA_NetMulticastRPC)
	{
		if (TArray<UFunction*>* RPCArray = Info->RPCs.Find(SCHEMA_NetMulticastRPC))
		{
			ReceiveMulticastUpdate(Op.update, TargetObject, *RPCArray);
		}
	}
	else
	{
		UE_LOG(LogSpatialReceiver, Verbose, TEXT("Entity: %d Component: %d - Skipping because it's an empty component update from an RPC component. (most likely as a result of gaining authority)"), Op.entity_id, Op.update.component_id);
	}
}

void USpatialReceiver::OnCommandRequest(Worker_CommandRequestOp& Op)
{
	Schema_FieldId CommandIndex = Schema_GetCommandRequestCommandIndex(Op.request.schema_type);
	UE_LOG(LogSpatialReceiver, Verbose, TEXT("Received command request (entity: %lld, component: %d, command: %d)"), Op.entity_id, Op.request.component_id, CommandIndex);

	if (Op.request.component_id == SpatialConstants::PLAYER_SPAWNER_COMPONENT_ID && CommandIndex == 1)
	{
		Schema_Object* Payload = Schema_GetCommandRequestObject(Op.request.schema_type);

		// Op.caller_attribute_set has two attributes.
		// 1. The attribute of the worker type
		// 2. The attribute of the specific worker that sent the request
		// We want to give authority to the specific worker, so we grab the second element from the attribute set.
		NetDriver->PlayerSpawner->ReceivePlayerSpawnRequest(GetStringFromSchema(Payload, 1), Op.caller_attribute_set.attributes[1], Op.request_id);
		return;
	}

	Worker_CommandResponse Response = {};
	Response.component_id = Op.request.component_id;
	Response.schema_type = Schema_CreateCommandResponse(Op.request.component_id, CommandIndex);

	uint32 Offset = 0;
	bool bFoundOffset = TypebindingManager->FindOffsetByComponentId(Op.request.component_id, Offset);
	if (!bFoundOffset)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("No offset found for ComponentId %d"), Op.request.component_id);
		Sender->SendCommandResponse(Op.request_id, Response);
		return;
	}

	UObject* TargetObject = PackageMap->GetObjectFromUnrealObjectRef(FUnrealObjectRef(Op.entity_id, Offset));
	if (TargetObject == nullptr)
	{
		UE_LOG(LogSpatialReceiver, Warning, TEXT("No target object found for EntityId %d"), Op.entity_id);
		Sender->SendCommandResponse(Op.request_id, Response);
		return;
	}

	FClassInfo* Info = TypebindingManager->FindClassInfoByObject(TargetObject);
	check(Info);

	ESchemaComponentType RPCType = TypebindingManager->FindCategoryByComponentId(Op.request.component_id);
	check(RPCType >= SCHEMA_FirstRPC && RPCType <= SCHEMA_LastRPC);

	const TArray<UFunction*>* RPCArray = Info->RPCs.Find(RPCType);
	check(RPCArray);
	check((int)CommandIndex - 1 < RPCArray->Num());

	UFunction* Function = (*RPCArray)[CommandIndex - 1];

	ReceiveRPCCommandRequest(Op.request, TargetObject, Function);

	Sender->SendCommandResponse(Op.request_id, Response);
}

void USpatialReceiver::OnCommandResponse(Worker_CommandResponseOp& Op)
{
	if (Op.entity_id == SpatialConstants::SPAWNER_ENTITY_ID && Op.response.component_id == SpatialConstants::PLAYER_SPAWNER_COMPONENT_ID)
	{
		NetDriver->PlayerSpawner->ReceivePlayerSpawnResponse(Op);
	}

	ReceiveCommandResponse(Op);
}

void USpatialReceiver::ReceiveCommandResponse(Worker_CommandResponseOp& Op)
{
	TSharedRef<FPendingRPCParams>* ReliableRPCPtr = PendingReliableRPCs.Find(Op.request_id);
	if (ReliableRPCPtr == nullptr)
	{
		// We received a response for an unreliable RPC, ignore.
		return;
	}

	TSharedRef<FPendingRPCParams> ReliableRPC = *ReliableRPCPtr;
	PendingReliableRPCs.Remove(Op.request_id);
	if (Op.status_code != WORKER_STATUS_CODE_SUCCESS)
	{
		if (ReliableRPC->Attempts < SpatialConstants::MAX_NUMBER_COMMAND_ATTEMPTS)
		{
			float WaitTime = SpatialConstants::GetCommandRetryWaitTimeSeconds(ReliableRPC->Attempts);
			UE_LOG(LogSpatialReceiver, Log, TEXT("%s: retrying in %f seconds. Error code: %d Message: %s"),
				*ReliableRPC->Function->GetName(), WaitTime, (int)Op.status_code, UTF8_TO_TCHAR(Op.message));

			if (!ReliableRPC->TargetObject.IsValid())
			{
				UE_LOG(LogSpatialReceiver, Warning, TEXT("%s: target object was destroyed before we could deliver the RPC."),
					*ReliableRPC->Function->GetName());
				return;
			}

			// Queue retry
			FTimerHandle RetryTimer;
			TimerManager->SetTimer(RetryTimer, [this, ReliableRPC]()
			{
				Sender->SendRPC(ReliableRPC);
			}, WaitTime, false);
		}
		else
		{
			UE_LOG(LogSpatialReceiver, Error, TEXT("%s: failed too many times, giving up (%u attempts). Error code: %d Message: %s"),
				*ReliableRPC->Function->GetName(), SpatialConstants::MAX_NUMBER_COMMAND_ATTEMPTS, (int)Op.status_code, UTF8_TO_TCHAR(Op.message));
		}
	}
}

void USpatialReceiver::ApplyComponentUpdate(const Worker_ComponentUpdate& ComponentUpdate, UObject* TargetObject, USpatialActorChannel* Channel, bool bIsHandover)
{
	FChannelObjectPair ChannelObjectPair(Channel, TargetObject);

	FObjectReferencesMap& ObjectReferencesMap = UnresolvedRefsMap.FindOrAdd(ChannelObjectPair);
	TSet<FUnrealObjectRef> UnresolvedRefs;
	ComponentReader Reader(NetDriver, ObjectReferencesMap, UnresolvedRefs, /* blCallRepNotifies */ true);
	Reader.ApplyComponentUpdate(ComponentUpdate, TargetObject, Channel, bIsHandover);

	QueueIncomingRepUpdates(ChannelObjectPair, ObjectReferencesMap, UnresolvedRefs);
}

void USpatialReceiver::ReceiveMulticastUpdate(const Worker_ComponentUpdate& ComponentUpdate, UObject* TargetObject, const TArray<UFunction*>& RPCArray)
{
	Schema_Object* EventsObject = Schema_GetComponentUpdateEvents(ComponentUpdate.schema_type);

	for (Schema_FieldId EventIndex = 1; (int)EventIndex <= RPCArray.Num(); EventIndex++)
	{
		UFunction* Function = RPCArray[EventIndex - 1];
		for (uint32 i = 0; i < Schema_GetObjectCount(EventsObject, EventIndex); i++)
		{
			Schema_Object* EventData = Schema_IndexObject(EventsObject, EventIndex, i);

			TArray<uint8> PayloadData = GetPayloadFromSchema(EventData, 1);
			// A bit hacky, we should probably include the number of bits with the data instead.
			int64 CountBits = PayloadData.Num() * 8;

			ApplyRPC(TargetObject, Function, PayloadData, CountBits);
		}
	}
}

void USpatialReceiver::ApplyRPC(UObject* TargetObject, UFunction* Function, TArray<uint8>& PayloadData, int64 CountBits)
{
	uint8* Parms = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero(Parms, Function->ParmsSize);

	TSet<FUnrealObjectRef> UnresolvedRefs;

	FSpatialNetBitReader PayloadReader(PackageMap, PayloadData.GetData(), CountBits, UnresolvedRefs);

	TSharedPtr<FRepLayout> RepLayout = NetDriver->GetFunctionRepLayout(Function);
	RepLayout_ReceivePropertiesForRPC(*RepLayout, PayloadReader, Parms);

	if (UnresolvedRefs.Num() == 0)
	{
		TargetObject->ProcessEvent(Function, Parms);
	}
	else
	{
		QueueIncomingRPC(UnresolvedRefs, TargetObject, Function, PayloadData, CountBits);
	}

	// Destroy the parameters.
	// warning: highly dependent on UObject::ProcessEvent freeing of parms!
	for (TFieldIterator<UProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parms);
	}
}

void USpatialReceiver::OnReserveEntityIdResponse(Worker_ReserveEntityIdResponseOp& Op)
{
	UE_LOG(LogSpatialReceiver, Log, TEXT("Received reserve entity Id: request id: %d, entity id: %lld"), Op.request_id, Op.entity_id);
	if (USpatialActorChannel* Channel = PopPendingActorRequest(Op.request_id))
	{
		Channel->OnReserveEntityIdResponse(Op);
	}
}

void USpatialReceiver::OnCreateEntityIdResponse(Worker_CreateEntityResponseOp& Op)
{
	if (USpatialActorChannel* Channel = PopPendingActorRequest(Op.request_id))
	{
		Channel->OnCreateEntityResponse(Op);
	}
}

void USpatialReceiver::AddPendingActorRequest(Worker_RequestId RequestId, USpatialActorChannel* Channel)
{
	PendingActorRequests.Add(RequestId, Channel);
}

void USpatialReceiver::AddPendingReliableRPC(Worker_RequestId RequestId, TSharedRef<FPendingRPCParams> Params)
{
	PendingReliableRPCs.Add(RequestId, Params);
}

USpatialActorChannel* USpatialReceiver::PopPendingActorRequest(Worker_RequestId RequestId)
{
	USpatialActorChannel** ChannelPtr = PendingActorRequests.Find(RequestId);
	if (ChannelPtr == nullptr)
	{
		return nullptr;
	}
	USpatialActorChannel* Channel = *ChannelPtr;
	PendingActorRequests.Remove(RequestId);
	return Channel;
}

void USpatialReceiver::ProcessQueuedResolvedObjects()
{
	for (TPair<UObject*, FUnrealObjectRef>& It : ResolvedObjectQueue)
	{
		ResolvePendingOperations_Internal(It.Key, It.Value);
	}
	ResolvedObjectQueue.Empty();
}

void USpatialReceiver::ResolvePendingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	if (bInCriticalSection)
	{
		ResolvedObjectQueue.Add(TPair<UObject*, FUnrealObjectRef>{ Object, ObjectRef });
	}
	else
	{
		ResolvePendingOperations_Internal(Object, ObjectRef);
	}
}

void USpatialReceiver::QueueIncomingRepUpdates(FChannelObjectPair ChannelObjectPair, const FObjectReferencesMap& ObjectReferencesMap, const TSet<FUnrealObjectRef>& UnresolvedRefs)
{
	for (const FUnrealObjectRef& UnresolvedRef : UnresolvedRefs)
	{
		UE_LOG(LogSpatialReceiver, Log, TEXT("Added pending incoming property for object ref: %s, target object: %s"), *UnresolvedRef.ToString(), *ChannelObjectPair.Value->GetName());
		IncomingRefsMap.FindOrAdd(UnresolvedRef).Add(ChannelObjectPair);
	}

	if (ObjectReferencesMap.Num() == 0)
	{
		UnresolvedRefsMap.Remove(ChannelObjectPair);
	}
}

void USpatialReceiver::QueueIncomingRPC(const TSet<FUnrealObjectRef>& UnresolvedRefs, UObject* TargetObject, UFunction* Function, const TArray<uint8>& PayloadData, int64 CountBits)
{
	TSharedPtr<FPendingIncomingRPC> IncomingRPC = MakeShared<FPendingIncomingRPC>(UnresolvedRefs, TargetObject, Function, PayloadData, CountBits);

	for (const FUnrealObjectRef& UnresolvedRef : UnresolvedRefs)
	{
		FIncomingRPCArray& IncomingRPCArray = IncomingRPCMap.FindOrAdd(UnresolvedRef);
		IncomingRPCArray.Add(IncomingRPC);
	}
}

void USpatialReceiver::ResolvePendingOperations_Internal(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	UE_LOG(LogSpatialReceiver, Log, TEXT("Resolving pending object refs and RPCs which depend on object: %s %s."), *Object->GetName(), *ObjectRef.ToString());

	Sender->ResolveOutgoingOperations(Object, /* bIsHandover */ false);
	Sender->ResolveOutgoingOperations(Object, /* bIsHandover */ true);
	ResolveIncomingOperations(Object, ObjectRef);
	Sender->ResolveOutgoingRPCs(Object);
	ResolveIncomingRPCs(Object, ObjectRef);
}

void USpatialReceiver::ResolveIncomingOperations(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	// TODO: queue up resolved objects since they were resolved during process ops
	// and then resolve all of them at the end of process ops - UNR:582

	TSet<FChannelObjectPair>* TargetObjectSet = IncomingRefsMap.Find(ObjectRef);
	if (!TargetObjectSet)
	{
		return;
	}

	UE_LOG(LogSpatialReceiver, Log, TEXT("Resolving incoming operations depending on object ref %s, resolved object: %s"), *ObjectRef.ToString(), *Object->GetName());

	for (FChannelObjectPair& ChannelObjectPair : *TargetObjectSet)
	{
		FObjectReferencesMap* UnresolvedRefs = UnresolvedRefsMap.Find(ChannelObjectPair);
		if (!UnresolvedRefs)
		{
			continue;
		}

		if (!ChannelObjectPair.Key.IsValid() || !ChannelObjectPair.Value.IsValid())
		{
			UnresolvedRefsMap.Remove(ChannelObjectPair);
			continue;
		}

		USpatialActorChannel* DependentChannel = ChannelObjectPair.Key.Get();
		UObject* ReplicatingObject = ChannelObjectPair.Value.Get();

		bool bStillHasUnresolved = false;
		bool bSomeObjectsWereMapped = false;
		TArray<UProperty*> RepNotifies;

		FRepLayout& RepLayout = DependentChannel->GetObjectRepLayout(ReplicatingObject);
		FRepStateStaticBuffer& ShadowData = DependentChannel->GetObjectStaticBuffer(ReplicatingObject);

		ResolveObjectReferences(RepLayout, ReplicatingObject, *UnresolvedRefs, ShadowData.GetData(), (uint8*)ReplicatingObject, ShadowData.Num(), RepNotifies, bSomeObjectsWereMapped, bStillHasUnresolved);

		if (bSomeObjectsWereMapped)
		{
			UE_LOG(LogSpatialReceiver, Log, TEXT("Resolved for target object %s"), *ReplicatingObject->GetName());
			DependentChannel->PostReceiveSpatialUpdate(ReplicatingObject, RepNotifies);
		}

		if (!bStillHasUnresolved)
		{
			UnresolvedRefsMap.Remove(ChannelObjectPair);
		}
	}

	IncomingRefsMap.Remove(ObjectRef);
}

void USpatialReceiver::ResolveIncomingRPCs(UObject* Object, const FUnrealObjectRef& ObjectRef)
{
	FIncomingRPCArray* IncomingRPCArray = IncomingRPCMap.Find(ObjectRef);
	if (!IncomingRPCArray)
	{
		return;
	}

	UE_LOG(LogSpatialReceiver, Log, TEXT("Resolving incoming RPCs depending on object ref %s, resolved object: %s"), *ObjectRef.ToString(), *Object->GetName());

	for (const TSharedPtr<FPendingIncomingRPC>& IncomingRPC : *IncomingRPCArray)
	{
		if (!IncomingRPC->TargetObject.IsValid())
		{
			// The target object has been destroyed before this RPC was resolved
			continue;
		}

		IncomingRPC->UnresolvedRefs.Remove(ObjectRef);
		if (IncomingRPC->UnresolvedRefs.Num() == 0)
		{
			ApplyRPC(IncomingRPC->TargetObject.Get(), IncomingRPC->Function, IncomingRPC->PayloadData, IncomingRPC->CountBits);
		}
	}

	IncomingRPCMap.Remove(ObjectRef);
}

void USpatialReceiver::ResolveObjectReferences(FRepLayout& RepLayout, UObject* ReplicatedObject, FObjectReferencesMap& ObjectReferencesMap, uint8* RESTRICT StoredData, uint8* RESTRICT Data, int32 MaxAbsOffset, TArray<UProperty*>& RepNotifies, bool& bOutSomeObjectsWereMapped, bool& bOutStillHasUnresolved)
{
	for (auto It = ObjectReferencesMap.CreateIterator(); It; ++It)
	{
		int32 AbsOffset = It.Key();

		if (AbsOffset >= MaxAbsOffset)
		{
			UE_LOG(LogSpatialReceiver, Log, TEXT("ResolveObjectReferences: Removed unresolved reference: AbsOffset >= MaxAbsOffset: %d"), AbsOffset);
			It.RemoveCurrent();
			continue;
		}

		FObjectReferences& ObjectReferences = It.Value();
		UProperty* Property = ObjectReferences.Property;
		// ParentIndex is -1 for handover properties
		FRepParentCmd* Parent = ObjectReferences.ParentIndex >= 0 ? &RepLayout.Parents[ObjectReferences.ParentIndex] : nullptr;

		if (ObjectReferences.Array)
		{
			check(Property->IsA<UArrayProperty>());

			Property->CopySingleValue(StoredData + AbsOffset, Data + AbsOffset);

			FScriptArray* StoredArray = (FScriptArray*)(StoredData + AbsOffset);
			FScriptArray* Array = (FScriptArray*)(Data + AbsOffset);

			int32 NewMaxOffset = Array->Num() * Property->ElementSize;

			bool bArrayHasUnresolved = false;
			ResolveObjectReferences(RepLayout, ReplicatedObject, *ObjectReferences.Array, (uint8*)StoredArray->GetData(), (uint8*)Array->GetData(), NewMaxOffset, RepNotifies, bOutSomeObjectsWereMapped, bArrayHasUnresolved);
			if (!bArrayHasUnresolved)
			{
				It.RemoveCurrent();
			}
			else
			{
				bOutStillHasUnresolved = true;
			}
			continue;
		}

		bool bResolvedSomeRefs = false;
		UObject* SinglePropObject = nullptr;

		for (auto UnresolvedIt = ObjectReferences.UnresolvedRefs.CreateIterator(); UnresolvedIt; ++UnresolvedIt)
		{
			FUnrealObjectRef& ObjectRef = *UnresolvedIt;

			FNetworkGUID NetGUID = PackageMap->GetNetGUIDFromUnrealObjectRef(ObjectRef);
			if (NetGUID.IsValid())
			{
				UObject* Object = PackageMap->GetObjectFromNetGUID(NetGUID, true);
				check(Object);

				UE_LOG(LogSpatialReceiver, Log, TEXT("ResolveObjectReferences: Resolved object ref: Offset: %d, Object ref: %s, PropName: %s, ObjName: %s"), AbsOffset, *ObjectRef.ToString(), *Property->GetNameCPP(), *Object->GetName());

				UnresolvedIt.RemoveCurrent();
				bResolvedSomeRefs = true;

				if (ObjectReferences.bSingleProp)
				{
					SinglePropObject = Object;
				}
			}
		}

		if (bResolvedSomeRefs)
		{
			if (!bOutSomeObjectsWereMapped)
			{
				ReplicatedObject->PreNetReceive();
				bOutSomeObjectsWereMapped = true;
			}

			if (Parent && Parent->Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				Property->CopySingleValue(StoredData + AbsOffset, Data + AbsOffset);
			}

			if (ObjectReferences.bSingleProp)
			{
				UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property);
				check(ObjectProperty);

				ObjectProperty->SetObjectPropertyValue(Data + AbsOffset, SinglePropObject);
			}
			else
			{
				TSet<FUnrealObjectRef> NewUnresolvedRefs;
				FSpatialNetBitReader BitReader(PackageMap, ObjectReferences.Buffer.GetData(), ObjectReferences.NumBufferBits, NewUnresolvedRefs);
				check(Property->IsA<UStructProperty>());
				ReadStructProperty(BitReader, Cast<UStructProperty>(Property), NetDriver, Data + AbsOffset, bOutStillHasUnresolved);
			}

			if (Parent && Parent->Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				if (Parent->RepNotifyCondition == REPNOTIFY_Always || !Property->Identical(StoredData + AbsOffset, Data + AbsOffset))
				{
					RepNotifies.AddUnique(Parent->Property);
				}
			}
		}

		if (ObjectReferences.UnresolvedRefs.Num() > 0)
		{
			bOutStillHasUnresolved = true;
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void USpatialReceiver::ReceiveRPCCommandRequest(const Worker_CommandRequest& CommandRequest, UObject* TargetObject, UFunction* Function)
{
	Schema_Object* RequestObject = Schema_GetCommandRequestObject(CommandRequest.schema_type);

	TArray<uint8> PayloadData = GetPayloadFromSchema(RequestObject, 1);
	// A bit hacky, we should probably include the number of bits with the data instead.
	int64 CountBits = PayloadData.Num() * 8;

	ApplyRPC(TargetObject, Function, PayloadData, CountBits);
}

#pragma optimize("", on)
