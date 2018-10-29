# Map Travel

This doc is intended for advanced users only.

## Client Travel 
Client travel is the process of changing which map a client currently has loaded and can also be used to change which server the client is connected to. 

In the GDK we can use client travel to move a client from an offline state, to a connected state, whether that be connected to a local deployment or a cloud deployment. Additionally you can go the other way round to move a client connected to a deployment, to an offline state.

### User guide
It is necessary to always have the client(s) have the same map loaded which the server(s) are running in your deployment. 

#### Using Receptionist

#### Using Locator

> Experimental: You can use `client travel` to change which cloud deployment a client is connected to. This functionality is highly experimental and requires you to write your own authorization code.

> Warning: The GDK does not yet support [World Compositions](UnrealLINK) and so you cannot have multiple maps loaded into the same server. This means you cannot client travel to different maps and stay connected to the same server or deployment.

### Client Travel - Technical Details
To use client travel with SpatialOS we have made changes to the Unreal Engine which detects if you have Spatial Networking enabled. If so, when the client travel URL specified contains a host to connect to, we create a `SpatialPendingNetGame` instead of a default Unreal `PendingNetGame`. This will internally create a `SpatialNetConnection` which connects you to the host specified. 

## Server Travel
> Warning: Server travel is in an experimental state and is only supported in single server-worker configurations for now.   
> Server travel is not supported in PIE.

Server travel in Unreal is the concept of changing the map/world/level for the server and all connected clients. A common use case is starting a server which is in a Lobby level, clients will connect to this lobby level and choose load-out or character etc. When ready, a client sends an RPC to the server which triggers the server travel.

When server travel is triggered, the server will tell all clients to begin to `client travel` to the map specified, if the server travel is seamless then the client maintains its connection to the server. If not seamless then all the clients disconnect from the server and reconnect once they have loaded the map. Internally the server does a similar process, it loads in the new level, usually a game world for all the clients to play on, and begins accepting player spawn requests once ready.

### User guide
To use server travel with the GDK there will be a couple of extra steps to ensure the Spatial deployment is in the correct state when transitioning maps. 

#### Generate snapshot 
Generate a snapshot for the map you intend to server travel to using the [Snapshot Generator](Link) and save it to `<GameRoot>\Content\Spatial\Snapshots\`. You can either copy your generated snapshot manually (from `<ProjectRoot>\spatial\snapshots\default.snapshot`) or setup your project settings to generate the snapshot for your level into that folder. You can find these settings via **Edit > Project Settings > SpatialOS Unreal GDK > Snapshot path**.

The snapshot will be read from `<GameRoot>\Content\Spatial\Snapshots\` when the `ServerTravel` command is called. To ensure this works in a cloud deployment, add the `Spatial\Snapshots` folder to your  **Additional Non-Asset Directories To Copy for dedicated server only** found at **File > Package Project > Packaging settings**. As shown:

[IMAGE HERE]()

Additionally don't forget to package the map you intend to travel to. Again from the **Package Settings** add your map(s) too **List of maps to include in a packaged build**.

[IMAGE HERE]()

#### Specify snapshot in Server Travel URL
Pass the snapshot to load as part of the map URL when calling server travel. For example:

```
FString ServerTravelURL = "ExampleMap?snapshot=ExampleMap.snapshot"
UWorld* World = GetWorld();
World->ServerTravel(ServerTravelURL, true);
```

The GDK has also added the `clientsStayConnected` URL parameter.  
Adding this URL parameter to your server travel URL will prevent clients from disconnecting from SpatialOS during the server travel process. We recommend doing this to prevent extra load when clients attempt to re-connect to SpatialOS. For example: 

```
FString ServerTravelURL = "ExampleMap?snapshot=ExampleMap.snapshot?clientsStayConnected"
```

### Server Travel - Technical Details
For server travel in a Spatial environment we have a few things to consider. It’s not normal for a Spatial deployment to ‘load a new world’, generally Spatial was made for very large persistent worlds. This means we need to perform extra steps onto the deployment to support server travel. 

> It should also be noted that server travel is only supported in non-PIE configurations. Launching a Server Worker in PIE has a dependency on the `Use dedicated server` network configuration available in PIE. Unfortunately this setting has a dependency on `Use single process` which doesn’t support server travel. To test and develop server travel for your game with the SpatialOS GDK for Unreal you will need to launch `external` workers (via `LaunchServer.bat`) or managed workers (using a managed worker launch configuration such as `one_worker.json`).

The first is handling Spatial’s snapshot system, normally we load a snapshot for a deployment at startup and then forget about it. Since we will be changing worlds, and we generate a single snapshot per world, we need to change the snapshot we have loaded into the deployment at runtime. We’ve achieved this by ‘wiping’ the deployment. Essentially we delete all entities in the world and when it’s in an empty state we manually load a new snapshot, which snapshot to load is specified in the URL of the server travel.

The second is handling player connections. Normally once a map is loaded in Unreal on the server, clients can just connect, but for Spatial we need to make sure the deployment is in a good state (snapshot fully loaded) before the clients should be allowed to connect.

The world wiping is handled at the start of server travel, after clients have unloaded their current world and started loading a new map, but before servers have started unloading their current world. Only the worker which has authority over the GSM should be able to wipe the world. It’s done by a large and expensive EntityQuery for all entities in the world. Once we have an entity query response of all entities that exist, we send deletion requests for each and every one. After finishing, the servers continue standard Unreal server travel and load in the new world.

The snapshot loading is again handled by the worker which is authoritative over the GSM, in this case the worker which was previously authoritative over the GSM since we have now deleted the old GSM in the wipe process. The process of loading the snapshot starts once the server with authority has loaded the world (`SpatialNetDriver::OnMapLoaded`). Using the `SnapshotManger`, this server will read the snapshot specified in the map URL from the `Game/Content/Spatial/Snapshots` directory. Iterating through all entities in the snapshot, the worker will send a spawn request for all of them. Once the GSM has been spawned and the spawning process for the rest of the entities has completed, the server which gains authority over the GSM will set the `AcceptingPlayers` field to true. This signals clients that they can now send player spawn requests. Clients know about the AcceptingPlayers state by sending entity queries on a timer for its existence and state.


## Default connection flows
#### In PIE
Launching a PIE client from the editor it will connect to a local SpatialOS deployment by default, this is for quick editing and debugging purposes.

#### With built clients
By default, outside of PIE, clients will not connect to a SpatialOS deployment. This is so you can implement your own connection flow, whether that be through an offline login screen or a connected lobby, etc.

To connect a client to a deployment from an offline state, you must [client travel](LINK) ....

To have the client launch script `LaunchClient.bat` which we have provided automatically connect to a local deployment, add your local IP `127.0.0.1` to the script after specifying the `.uproject` of your game. For example:
```
@echo off
call "%~dp0ProjectPaths.bat"
"%UNREAL_HOME%\Engine\Binaries\Win64\UE4Editor-Win64-Debug.exe" "%~dp0%PROJECT_PATH%\%GAME_NAME%.uproject" 127.0.0.1 -game -log -workerType UnrealClient -stdout -nowrite -unattended -nologtimes -nopause -noin -messaging -NoVerifyGC -windowed -ResX=800 -ResY=600
```

#### With the launcher
When launching a client from the SpatialOS console using the [Launcher](LINK) the client will not connect to SpatialOS by default. However it will have the `Locator` information required to connect to said deployment included as command-line-arguments. This means you can simply perform a client travel to the correct map that the deployment has loaded and the locator connection flow will connect you to the deployment. 
> Quick tip: For quick testing or debugging, you can just run `open 127.0.0.1` from a game clients `console` window and this will connect you to the cloud deployment.