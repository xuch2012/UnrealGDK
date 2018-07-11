// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
#pragma optimize("", off)

#include "TypeStructure.h"
#include "Net/RepLayout.h"
#include "SpatialGDKEditorInteropCodeGenerator.h"

FString GetFullCPPName(UClass* Class)
{
	if (Class->IsChildOf(AActor::StaticClass()))
	{
		return FString::Printf(TEXT("A%s"), *Class->GetName());
	}
	else
	{
		return FString::Printf(TEXT("U%s"), *Class->GetName());
	}
}

FString GetLifetimeConditionAsString(ELifetimeCondition Condition)
{
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("ELifetimeCondition"), true);
	if (!EnumPtr)
	{
		return FString("Invalid");
	}
	return EnumPtr->GetNameByValue((int64)Condition).ToString();
}

FString GetRepNotifyLifetimeConditionAsString(ELifetimeRepNotifyCondition Condition)
{
	switch (Condition)
	{
	case REPNOTIFY_OnChanged: return FString(TEXT("REPNOTIFY_OnChanged"));
	case REPNOTIFY_Always: return FString(TEXT("REPNOTIFY_Always"));
	default:
		checkNoEntry();
	}
	return FString();
}

TArray<EReplicatedPropertyGroup> GetAllReplicatedPropertyGroups()
{
	static TArray<EReplicatedPropertyGroup> Groups = {REP_SingleClient, REP_MultiClient};
	return Groups;
}

FString GetReplicatedPropertyGroupName(EReplicatedPropertyGroup Group)
{
	return Group == REP_SingleClient ? TEXT("SingleClient") : TEXT("MultiClient");
}

TArray<ERPCType> GetRPCTypes()
{
	static TArray<ERPCType> Groups = {RPC_Client, RPC_Server, RPC_NetMulticast};
	return Groups;
}

ERPCType GetRPCTypeFromFunction(UFunction* Function)
{
	if (Function->FunctionFlags & EFunctionFlags::FUNC_NetClient)
	{
		return ERPCType::RPC_Client;
	}
	if (Function->FunctionFlags & EFunctionFlags::FUNC_NetServer)
	{
		return ERPCType::RPC_Server;
	}
	if (Function->FunctionFlags & EFunctionFlags::FUNC_NetMulticast)
	{
		return ERPCType::RPC_NetMulticast;
	}
	else
	{
		checkNoEntry();
		return ERPCType::RPC_Unknown;
	}
}

FString GetRPCTypeName(ERPCType RPCType)
{
	switch (RPCType)
	{
	case ERPCType::RPC_Client:
		return "Client";
	case ERPCType::RPC_Server:
		return "Server";
	case ERPCType::RPC_NetMulticast:
		return "NetMulticast";
	default:
		checkf(false, TEXT("RPCType is invalid!"));
		return "";
	}
}

void VisitAllObjects(TSharedPtr<FUnrealType> TypeNode, TFunction<bool(TSharedPtr<FUnrealType>)> Visitor, bool bRecurseIntoSubobjects)
{
	bool bShouldRecurseFurther = Visitor(TypeNode);
	for (auto& PropertyPair : TypeNode->Properties)
	{
		if (bShouldRecurseFurther && PropertyPair.Value->Type.IsValid())
		{
			// Either recurse into subobjects if they're structs or bRecurseIntoSubobjects is true.
			if (bRecurseIntoSubobjects || PropertyPair.Value->Property->IsA<UStructProperty>())
			{
				VisitAllObjects(PropertyPair.Value->Type, Visitor, bRecurseIntoSubobjects);
			}
		}
	}
}

void VisitAllProperties(TSharedPtr<FUnrealType> TypeNode, TFunction<bool(TSharedPtr<FUnrealProperty>)> Visitor, bool bRecurseIntoSubobjects)
{
	for (auto& PropertyPair : TypeNode->Properties)
	{
		bool bShouldRecurseFurther = Visitor(PropertyPair.Value);
		if (bShouldRecurseFurther && PropertyPair.Value->Type.IsValid())
		{
			// Either recurse into subobjects if they're structs or bRecurseIntoSubobjects is true.
			if (bRecurseIntoSubobjects || PropertyPair.Value->Property->IsA<UStructProperty>())
			{
				VisitAllProperties(PropertyPair.Value->Type, Visitor, bRecurseIntoSubobjects);
			}
		}
	}
}

void VisitAllProperties(TSharedPtr<FUnrealRPC> RPCNode, TFunction<bool(TSharedPtr<FUnrealProperty>)> Visitor, bool bRecurseIntoSubobjects)
{
	for (auto& PropertyPair : RPCNode->Parameters)
	{
		bool bShouldRecurseFurther = Visitor(PropertyPair.Value);
		if (bShouldRecurseFurther && PropertyPair.Value->Type.IsValid())
		{
			// Either recurse into subobjects if they're structs or bRecurseIntoSubobjects is true.
			if (bRecurseIntoSubobjects || PropertyPair.Value->Property->IsA<UStructProperty>())
			{
				VisitAllProperties(PropertyPair.Value->Type, Visitor, bRecurseIntoSubobjects);
			}
		}
	}
}

// GenerateChecksum is a method which replicates how Unreal generates it's own CompatibleChecksum for RepLayout Cmds.
// The original code can be found in the Unreal Engine's RepLayout. We use this to ensure we have the correct property at run-time.
uint32 GenerateChecksum(UProperty* Property, uint32 ParentChecksum, int32 StaticArrayIndex)
{
	uint32 Checksum = 0;
	Checksum = FCrc::StrCrc32(*Property->GetName().ToLower(), ParentChecksum);            // Evolve checksum on name
	Checksum = FCrc::StrCrc32(*Property->GetCPPType(nullptr, 0).ToLower(), Checksum);     // Evolve by property type
	Checksum = FCrc::StrCrc32(*FString::Printf(TEXT("%i"), StaticArrayIndex), Checksum);  // Evolve by StaticArrayIndex
	return Checksum;
}

TSharedPtr<FUnrealProperty> CreateUnrealProperty(TSharedPtr<FUnrealType> TypeNode, UProperty* Property, uint32 ParentChecksum, uint32 StaticArrayIndex)
{
	TSharedPtr<FUnrealProperty> PropertyNode = MakeShared<FUnrealProperty>();
	PropertyNode->Property = Property;
	PropertyNode->ContainerType = TypeNode;
	PropertyNode->ParentChecksum = ParentChecksum;
	PropertyNode->StaticArrayIndex = StaticArrayIndex;

	// Generate a checksum for this PropertyNode to be used to match properties with the RepLayout Cmds later.
	PropertyNode->CompatibleChecksum = GenerateChecksum(Property, ParentChecksum, StaticArrayIndex);
	TypeNode->Properties.Add(Property, PropertyNode);
	return PropertyNode;
}

TSharedPtr<FUnrealReplicationDataWrapper> BuildUnrealReplicationDataWrapper(UClass* Class)
{
	TSharedPtr<FUnrealReplicationDataWrapper> RepDataWrapper = MakeShared<FUnrealReplicationDataWrapper>();

	RepDataWrapper->ReplicatedPropertyData.InitFromObjectClass(Class);
	// RepDataWrapper->MigratablePropertyData.InitMigratablePropertiesFromObjectClass(Class); // This will remain deprecated until we can handle migratable properties in non-replicated components. UNR-

	TSharedPtr<FUnrealType> UnrealPropertyWrapper = BuildUnrealPropertyWrapper(Class, 0, 0);
	RepDataWrapper->MigratableData = GetFlatMigratableData(UnrealPropertyWrapper);

	// Iterate through each RPC in the class.
	for (TFieldIterator<UFunction> RemoteFunction(Class); RemoteFunction; ++RemoteFunction)
	{
		if (RemoteFunction->FunctionFlags & FUNC_NetClient ||
			RemoteFunction->FunctionFlags & FUNC_NetServer ||
			RemoteFunction->FunctionFlags & FUNC_NetMulticast)
		{
			FRepLayout* RPCData = new FRepLayout();
			RPCData->InitFromFunction(*RemoteFunction);
			RepDataWrapper->RPCPropertyDataMap.Add(*RemoteFunction, RPCData);
		}
	}

	return RepDataWrapper;
}

TSharedPtr<FUnrealType> BuildUnrealPropertyWrapper(UStruct* Type, uint32 ParentChecksum, int32 StaticArrayIndex)
{
	// Struct types will set this to nullptr.
	UClass* Class = Cast<UClass>(Type);

	// Create type node.
	TSharedPtr<FUnrealType> TypeNode = MakeShared<FUnrealType>();
	TypeNode->Type = Type;

	// Iterate through each property in the struct.
	for (TFieldIterator<UProperty> It(Type); It; ++It)
	{
		UProperty* Property = *It;
		
		// Create property node and add it to the AST.
		TSharedPtr<FUnrealProperty> PropertyNode = CreateUnrealProperty(TypeNode, Property, ParentChecksum, StaticArrayIndex);

		// If this property not a struct or object (which can contain more properties), stop here.
		if (!Property->IsA<UStructProperty>() && !Property->IsA<UObjectProperty>())
		{
			for (int i = 1; i < Property->ArrayDim; i++)
			{
				CreateUnrealProperty(TypeNode, Property, ParentChecksum, i);
			}
			continue;
		}

		// If this is a struct property, then get the struct type and recurse into it.
		if (Property->IsA<UStructProperty>())
		{
			UStructProperty* StructProperty = Cast<UStructProperty>(Property);

			// This is the property for the 0th struct array member.
			uint32 ParentPropertyNodeChecksum = PropertyNode->CompatibleChecksum;
			PropertyNode->Type = BuildUnrealPropertyWrapper(StructProperty->Struct, ParentPropertyNodeChecksum, 0);
			PropertyNode->Type->ParentProperty = PropertyNode;

			// For static arrays we need to make a new struct array member node.
			for (int i = 1; i < Property->ArrayDim; i++)
			{
				// Create a new PropertyNode.
				TSharedPtr<FUnrealProperty> StaticStructArrayPropertyNode = CreateUnrealProperty(TypeNode, Property, ParentChecksum, i);

				// Generate Type information on the inner struct.
				// Note: The parent checksum of the properties within a struct that is a member of a static struct array, is the checksum for the struct itself after index modification.
				StaticStructArrayPropertyNode->Type = BuildUnrealPropertyWrapper(StructProperty->Struct, StaticStructArrayPropertyNode->CompatibleChecksum, 0);
				StaticStructArrayPropertyNode->Type->ParentProperty = StaticStructArrayPropertyNode;
			}
			continue;
		}

		// If this is an object property, then we need to do two things:
		//	 1) Determine whether this property is a strong or weak reference to the object. Some subobjects (such as the CharacterMovementComponent)
		//		are in fact owned by the character, and can be stored in the same entity as the character itself. Some subobjects (such as the Controller
		//		field in AActor) is a weak reference, and should just store a reference to the real object. We inspect the CDO to determine whether
		//		the owner of the property value is equal to itself. As structs don't have CDOs, we assume that all object properties in structs are
		//		weak references.
		//
		//   2) Obtain the concrete object type stored in this property. For example, the property containing the CharacterMovementComponent
		//      might be a property which stores a MovementComponent pointer, so we'd need to somehow figure out the real type being stored there
		//		during runtime. This is determined by getting the CDO of this class to determine what is stored in that property.
		UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
		check(ObjectProperty);

		bool bIsComponent = ObjectProperty->IsA<UActorComponent>();

		// If this is a property of a struct, assume it's a weak reference.
		if (!Class)
		{
			continue;
		}
		
		UObject* ContainerCDO = Class->GetDefaultObject();
		check(ContainerCDO);

		// This is to ensure we handle static array properties only once.
		bool bHandleStaticArrayProperties = true;

		// Obtain the properties actual value from the CDO, so we can figure out its true type.
		UObject* Value = ObjectProperty->GetPropertyValue_InContainer(ContainerCDO);
		if (Value)
		{
			// If this is an editor-only property, skip it. As we've already added to the property list at this stage, just remove it.
			if (Value->IsEditorOnly())
			{
				UE_LOG(LogSpatialGDKInteropCodeGenerator, Warning, TEXT("%s - editor only, skipping"), *Property->GetName());
				TypeNode->Properties.Remove(Property);
				continue;
			}

			// Check whether the owner of this value is the CDO itself.
			if (Value->GetOuter() == ContainerCDO)
			{
				UE_LOG(LogSpatialGDKInteropCodeGenerator, Warning, TEXT("Property Class: %s Instance Class: %s"), *ObjectProperty->PropertyClass->GetName(), *Value->GetClass()->GetName());

				// This property is definitely a strong reference, recurse into it.
				PropertyNode->Type = BuildUnrealPropertyWrapper(ObjectProperty->PropertyClass, ParentChecksum, 0);
				PropertyNode->Type->ParentProperty = PropertyNode;

				// For static arrays we need to make a new object array member node.
				for (int i = 1; i < Property->ArrayDim; i++)
				{
					TSharedPtr<FUnrealProperty> StaticObjectArrayPropertyNode = CreateUnrealProperty(TypeNode, Property, ParentChecksum, i);

					// Note: The parent checksum of static arrays of strong object references will be the parent checksum of this class.
					StaticObjectArrayPropertyNode->Type = BuildUnrealPropertyWrapper(ObjectProperty->PropertyClass, ParentChecksum, 0);
					StaticObjectArrayPropertyNode->Type->ParentProperty = StaticObjectArrayPropertyNode;
				}
				
				bHandleStaticArrayProperties = false;
			}
			else
			{
				// The values outer is not us, store as weak reference.
				UE_LOG(LogSpatialGDKInteropCodeGenerator, Warning, TEXT("%s - %s weak reference (outer not this)"), *Property->GetName(), *ObjectProperty->PropertyClass->GetName());
			}
		}
		else
		{
			// If value is just nullptr, then we clearly don't own it.
			UE_LOG(LogSpatialGDKInteropCodeGenerator, Warning, TEXT("%s - %s weak reference (null init)"), *Property->GetName(), *ObjectProperty->PropertyClass->GetName());
		}

		// Weak reference static arrays are handled as a single UObjectRef per static array member.
		if (bHandleStaticArrayProperties)
		{
			for (int i = 1; i < Property->ArrayDim; i++)
			{
				CreateUnrealProperty(TypeNode, Property, ParentChecksum, i);
			}
		}
	} // END TFieldIterator<UProperty>

	// If this is not a class, exit now, as structs cannot have RPCs or replicated properties.
	if (!Class)
	{
		return TypeNode;
	}

	// Find the handover properties.
	uint16 MigratableDataHandle = 1;
	VisitAllProperties(TypeNode, [&MigratableDataHandle](TSharedPtr<FUnrealProperty> PropertyInfo)
	{
		if (PropertyInfo->Property->PropertyFlags & CPF_Handover)
		{
			PropertyInfo->MigratableData = MakeShared<FUnrealMigratableData>();
			PropertyInfo->MigratableData->Handle = MigratableDataHandle++;
		}
		return true;
	}, true);

	return TypeNode;
}

FUnrealFlatRepData GetFlatRepData(TSharedPtr<FUnrealType> TypeInfo)
{
	FUnrealFlatRepData RepData;
	RepData.Add(REP_MultiClient);
	RepData.Add(REP_SingleClient);

	VisitAllProperties(TypeInfo, [&RepData](TSharedPtr<FUnrealProperty> PropertyInfo)
	{
		if (PropertyInfo->ReplicationData.IsValid())
		{
			EReplicatedPropertyGroup Group = REP_MultiClient;
			switch (PropertyInfo->ReplicationData->Condition)
			{
			case COND_AutonomousOnly:
			case COND_OwnerOnly:
				Group = REP_SingleClient;
				break;
			}
			RepData[Group].Add(PropertyInfo->ReplicationData->Handle, PropertyInfo);
		}
		return true;
	}, false);

	// Sort by replication handle.
	RepData[REP_MultiClient].KeySort([](uint16 A, uint16 B)
	{
		return A < B;
	});
	RepData[REP_SingleClient].KeySort([](uint16 A, uint16 B)
	{
		return A < B;
	});
	return RepData;
}

FGroupedRepCmds GetGroupedRepCmds(FRepLayout* RepLayout)
{
	FGroupedRepCmds FlatRepData;
	FlatRepData.Add(REP_MultiClient);
	FlatRepData.Add(REP_SingleClient);

	for(int i = 0; i < RepLayout->Cmds.Num(); i++)
	{
		FRepLayoutCmd Cmd = RepLayout->Cmds[i];
		FRepParentCmd ParentCmd = RepLayout->Parents[Cmd.ParentIndex];

		if(Cmd.Property)
		{
			EReplicatedPropertyGroup Group = REP_MultiClient;
			switch (ParentCmd.Condition)
			{
			case COND_AutonomousOnly:
			case COND_OwnerOnly:
				Group = REP_SingleClient;
				break;
			}
			FlatRepData[Group].Add(Cmd.RelativeHandle, Cmd);
		}
	}

	// Sort by replication handle.
	FlatRepData[REP_MultiClient].KeySort([](uint16 A, uint16 B)
	{
		return A < B;
	});
	FlatRepData[REP_SingleClient].KeySort([](uint16 A, uint16 B)
	{
		return A < B;
	});

	return FlatRepData;
}

FCmdHandlePropertyMap GetFlatMigratableData(TSharedPtr<FUnrealType> TypeInfo)
{
	FCmdHandlePropertyMap MigratableData;
	VisitAllProperties(TypeInfo, [&MigratableData](TSharedPtr<FUnrealProperty> PropertyInfo)
	{
		if (PropertyInfo->MigratableData.IsValid())
		{
			MigratableData.Add(PropertyInfo->MigratableData->Handle, PropertyInfo);
		}
		return true;
	}, true);

	// Sort by property handle.
	MigratableData.KeySort([](uint16 A, uint16 B)
	{
		return A < B;
	});
	return MigratableData;
}

// Goes through all RPCs in the TypeInfo and returns a list of all the unique RPC source classes.
TArray<FString> GetRPCTypeOwners(TSharedPtr<FUnrealType> TypeInfo)
{
	TArray<FString> RPCTypeOwners;
	VisitAllObjects(TypeInfo, [&RPCTypeOwners](TSharedPtr<FUnrealType> Type)
	{
		for (auto& RPC : Type->RPCs)
		{
			FString RPCOwnerName = *RPC.Value->Function->GetOuter()->GetName();
			RPCTypeOwners.AddUnique(RPCOwnerName);
			UE_LOG(LogSpatialGDKInteropCodeGenerator, Log, TEXT("RPC Type Owner Found - %s ::  %s"), *RPCOwnerName, *RPC.Value->Function->GetName());
		}
		return true;
	}, true);
	return RPCTypeOwners;
}

FUnrealRPCsByType GetAllRPCsByType(TSharedPtr<FUnrealType> TypeInfo)
{
	FUnrealRPCsByType RPCsByType;
	RPCsByType.Add(RPC_Client);
	RPCsByType.Add(RPC_Server);
	RPCsByType.Add(RPC_NetMulticast);
	VisitAllObjects(TypeInfo, [&RPCsByType](TSharedPtr<FUnrealType> Type)
	{
		for (auto& RPC : Type->RPCs)
		{
			RPCsByType.FindOrAdd(RPC.Value->Type).Add(RPC.Value);
		}
		return true;
	}, true);
	return RPCsByType;
}

FUnrealRPCsByTypeNew GetAllRPCsByTypeNew(TMap<UFunction*, FRepLayout*> RPCs)
{
	FUnrealRPCsByTypeNew RPCsByType;
	RPCsByType.Add(RPC_Client);
	RPCsByType.Add(RPC_Server);
	RPCsByType.Add(RPC_NetMulticast);

	for (auto& RPC : RPCs)
	{
		auto RPCType = GetRPCTypeFromFunction(RPC.Key);
		TPair<UFunction*, FRepLayout*> NewRPC{ RPC.Key, RPC.Value };
		RPCsByType.FindOrAdd(RPCType).Add(NewRPC);
	}
	return RPCsByType;
}

TArray<TSharedPtr<FUnrealProperty>> GetFlatRPCParameters(TSharedPtr<FUnrealRPC> RPCNode)
{
	TArray<TSharedPtr<FUnrealProperty>> ParamList;
	VisitAllProperties(RPCNode, [&ParamList](TSharedPtr<FUnrealProperty> Property)
	{
		// If the property is a generic struct without NetSerialize, recurse further.
		if (Property->Property->IsA<UStructProperty>())
		{
			if (Cast<UStructProperty>(Property->Property)->Struct->StructFlags & STRUCT_NetSerializeNative)
			{
				// We want to skip recursing into structs which have NetSerialize implemented.
				// This is to prevent flattening their internal structure, they will be represented as 'bytes'.
				ParamList.Add(Property);
				return false;
			}

			// For static arrays we want to stop recursion and serialize the property.
			// Note: This will use NetSerialize or SerializeBin which is currently known to not recursively call NetSerialize on inner structs. UNR-333
			if (Property->Property->ArrayDim > 1)
			{
				ParamList.Add(Property);
				return false;
			}

			// Generic struct. Recurse further.
			return true;
		}

		// If the RepType is not a generic struct, such as Vector3f or Plane, add to ParamList and stop recursion.
		ParamList.Add(Property);
		return false;
	}, false);
	return ParamList;
}

TArray<TSharedPtr<FUnrealProperty>> GetPropertyChain(TSharedPtr<FUnrealProperty> LeafProperty)
{
	TArray<TSharedPtr<FUnrealProperty>> OutputChain;
	TSharedPtr<FUnrealProperty> CurrentProperty = LeafProperty;
	while (CurrentProperty.IsValid())
	{
		OutputChain.Add(CurrentProperty);
		if (CurrentProperty->ContainerType.IsValid())
		{
			TSharedPtr<FUnrealType> EnclosingType = CurrentProperty->ContainerType.Pin();
			CurrentProperty = EnclosingType->ParentProperty.Pin();
		}
		else
		{
			CurrentProperty.Reset();
		}
	}

	// As we started at the leaf property and worked our way up, we need to reverse the list at the end.
	Algo::Reverse(OutputChain);
	return OutputChain;
}
