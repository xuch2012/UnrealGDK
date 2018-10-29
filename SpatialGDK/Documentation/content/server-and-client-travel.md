# Client and Server Travel

## Client Travel 
> Warning: The GDK does not yet support [World Compositions](UnrealLINK) and so you cannot have multiple maps loaded into the same server. This means you cannot client travel to different maps and stay connected to the same server or deployment.

Client travel is the process of changing which map a client currently has loaded and can also be used to change which server the client is connected to.

In base Unreal you can client travel by calling 


#### Using client travel to deployment travel


## Server Travel
> Warning: Server travel is in an experimental state and is only supported in single server-worker configurations for now.   
> Server travel is not supported in PIE.

### How to use with the GDK
To use server travel with the GDK there will be a couple of extra steps to ensure the Spatial deployment is in the correct state when transitioning maps. 

#### Generate snapshot 
Generate a snapshot for the map you intend to server travel to using the [Snapshot Generator](Link) and save it to `<GameRoot>\Content\Spatial\Snapshots\`. You could either copy your generated snapshot manually (from `<ProjectRoot>\spatial\snapshots\default.snapshot`) or setup your project settings to generate the snapshot for your level into that folder. You can find these settings via **Edit > Project Settings > SpatialOS Unreal GDK > Snapshot path**.

The snapshot will be read from `<GameRoot>\Content\Spatial\Snapshots\` when the `ServerTravel` command is called. To ensure this works in a cloud deployment, add the `Spatial\Snapshots` folder to your  **Additional Non-Asset Directories To Copy for dedicated server only** found at **File > Package Project > Packaging settings**. As shown:

[IMAGE HERE]()

Additionally for cooked / built out workers don't forget to package the map you intend to travel to. Again from the **Package Settings** add your map(s) too **List of maps to include in a packaged build**.

[IMAGE HERE]()

#### Specify snapshot in Server Travel URL
Pass the snapshot to load as part of the map URL when calling server travel. For example:

```
FString MapName = "ExampleMap?snapshot=ExampleMap.snapshot"
UWorld* World = GetWorld();
World->ServerTravel(MapName, true);
```

### 

## Default connection flow
> Warning: Connections are hard.
#### In PIE
When launching a PIE client from the editor it will connect to a local SpatialOS deployment by default, this is for quick editing and debugging. ....

#### With built clients
By default, outside of PIE, clients will not connect to a SpatialOS deployment, this is so you can implement your own connection flow, whether that be through an offline login screen or a connected lobby, etc.

To connect a client to a deployment from an offline state, you must [client travel](LINK) ....

To have the client launch script `LaunchClient.bat` automatically connect to a local deployment, add your local IP `127.0.0.1` to the script after specifying the `.uproject` of your game. For example:
```
@echo off
call "%~dp0ProjectPaths.bat"
"%UNREAL_HOME%\Engine\Binaries\Win64\UE4Editor-Win64-Debug.exe" "%~dp0%PROJECT_PATH%\%GAME_NAME%.uproject" 127.0.0.1 -game -log -workerType UnrealClient -stdout -nowrite -unattended -nologtimes -nopause -noin -messaging -NoVerifyGC -windowed -ResX=800 -ResY=600
```

#### With the launcher
When launching a client from the SpatialOS console using the [Launcher](LINK) ...

## Locator and Receptionist

#### Using Receptionist

#### Using Locator