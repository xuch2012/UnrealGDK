# Get started: Set up

To start using the GDK for unreal, you need to ensure you have the correct software installed and that your machine is capable of running Unreal Engine. 

## Sign up for a SpatialOS account, or make sure you are logged in

If you have already signed up, make sure you are logged into [Improbable.io](https://improbable.io). If you are logged in, you should see your picture in the top right of this page; if you are not logged in, select __Sign in__ at the top of this page and follow the instructions.

If you have not signed up before, you can sign up [here](<https://improbable.io/get-spatialos>).

## Set up your machine

### Step 1: Hardware

- Ensure your machine meets the minimum hardware requirements for Unreal Engine:
  Refer to the <a href="https://docs.unrealengine.com/en-US/GettingStarted/RecommendedSpecifications" data-track-link="Clicked UE4 Recommendations|product=Docs|platform=Win|label=Win" target="_blank">UE4 hardware recommendations</a> for further information about the minimum hardware requirements.
- Recommended storage: 60GB+ available space

### Step 2:  Network settings

Ensure your network is configured to work with SpatialOS:

- Refer to the [SpatialOS network settings (SpatialOS documentation)](https://docs.improbable.io/reference/latest/shared/get-started/requirements#network-settings). 

### Step 3: Software

To build the GDK for Unreal you need the following software installed on your machine:

- **Windows 10,** with Command Prompt or PowerShell as your terminal

  - The GDK for Unreal is only supported on Windows 10. 

- <a href="https://gitforwindows.org" data-track-link="Clicked GIT for Windows|product=Docs|platform=Win|label=Win" target="_blank">**Git for Windows**</a>

  - The GDK for Unity and Unreal Engine source code is hosted on GitHub. You need to download and install Git for windows in order to clone the GDK and Unreal Engine repositories. 

- <a href="https://console.improbable.io/installer/download/stable/latest/win" data-track-link="Clicked Download SpatialOS|product=Docs|platform=Win|label=Win" target="_blank">**SpatialOS version 13.2**</a>

  - Run the <a href="https://console.improbable.io/installer/download/stable/latest/win" data-track-link="SpatialOS Installer Downloaded|product=Docs|platform=Win|label=Win" target="_blank">SpatialOS Installer</a>

    This installs the [SpatialOS CLI]({{urlRoot}}/content/glossary#spatial-command-line-tool-cli) , the [SpatialOS Launcher]({{urlRoot}}/content/glossary#launcher), and 32-bit and 64-bit Visual C++ Redistributables

- The <a href="https://developer.microsoft.com/en-us/windows/downloads/sdk-archive" data-track-link="Clicked Windows SDK 8.1|product=Docs|platform=Win|label=Win" target="_blank">**Windows SDK 8.1**</a>

  - The Windows SDK 8.1 provides libraries, headers metadata and tools required for building Windows 10 applications. 

- **Visual Studio** <a href="https://visualstudio.microsoft.com/vs/older-downloads/" data-track-link="Clicked VS 2015|product=Docs|platform=Win|label=Win" target="_blank">2015</a> or <a href="https://visualstudio.microsoft.com/downloads/2017" data-track-link="Clicked VS 2017|product=Docs|platform=Win|label=Win">2017</a> (we recommend 2017). During the installation, make sure you select the following items in the Workloads tab:<br>
      - Universal Windows Platform development<br>
      - Desktop development with C++<br>
      - Game development with C++

## Get and build the GDK’s Unreal Engine Fork

The GDK for Unreal extends Unreal Engine's networking capabilities at its core. To use it you first need to build our SpatialOS fork of Unreal Engine from source.

> This process has several steps and building the Unreal Engine from source can take up to a few hours. 

### Step 1: Unreal Engine EULA

In order to get access to our fork, you need to link your GitHub account to a verified Epic Games account, and agree to the Unreal Engine End User License Agreement ([EULA](https://www.unrealengine.com/en-US/eula)). You can not use the GDK without doing this first. To do this, see the [Unreal documentation](https://www.unrealengine.com/en-US/ue4-on-github).

### Step 2: Getting the Unreal Engine fork source code and Unreal Linux cross-platform support

1. In a terminal window, clone the [Unreal Engine fork](https://github.com/improbableio/UnrealEngine/tree/4.20-SpatialOSUnrealGDK) repository. (You may get a 404 from this link. See  the instructions above, under _Unreal Engine EULA_, on how to get access.) <br><br>

   Clone the Unreal Engine fork by opening a terminal and running either of these commands: <br><br>

   - (HTTPS) `git clone https://github.com/improbableio/UnrealEngine.git -b 4.20-SpatialOSUnrealGDK`
   - (SSH) `git clone git@github.com:improbableio/UnrealEngine.git -b 4.20-SpatialOSUnrealGDK` <br><br>

2. To build Unreal server-workers for SpatialOS deployments, you need to build the Unreal Engine fork targeting Linux. This requires cross-compilation of your SpatialOS project and the Unreal Engine fork.

In Unreal's [Compiling for Linux](https://wiki.unrealengine.com/Compiling_For_Linux) documentation, in the **getting the toolchain** section, click v11 **clang 5.0.0-based** to download the archive **v11_clang-5.0.0-centos7.zip** containing the Linux cross-compilation toolchain, then unzip this file into a suitable directory.

### Step 3: Adding environment variables

You need to add two [environment variables](https://docs.microsoft.com/en-us/windows/desktop/shell/user-environment-variables): one to set the path to the Unreal Engine fork directory, and another one to set the path to the Linux cross-platform support.

1. Go to **Control Panel > System and Security > System > Advanced system settings > Advanced > Environment variables**.
2. Create a system variable named **UNREAL_HOME**.
3. Set the variable value to be the path to the directory you cloned the Unreal Engine fork into.
4. Make sure that the new environment variable is registered by restarting your terminal and running `echo %UNREAL_HOME%` (Command Prompt) or`echo$Env:UNREAL_HOME` (PowerShell). If the environment variable is registered correctly, this returns the path to the directory you cloned the Unreal Engine fork into. If it doesn’t, check that you’ve set the environment variable correctly.
5. Create a system variable named **LINUX_MULTIARCH_ROOT**.
6. Set the variable value to be the path to the directory of your unzipped Linux cross compilation toolchain.
7. Make sure that the new environment variable is registered by restarting your terminal and running `echo %LINUX_MULTIARCH_ROOT%` (Command Prompt) or `echo $Env:LINUX_MULTIARCH_ROOT` (PowerShell).

If the environment variable is registered correctly, this returns the path you unzipped `v11_clang-5.0.0-centos7.zip` into. If it doesn’t, check that you’ve set the environment variable correctly.

### Step 4: Building Unreal Engine

> Building the Unreal Engine fork from source could take up to a few hours.

1. Open **File Explorer** and navigate to the directory you cloned the Unreal Engine fork into.

2. Double-click **`Setup.bat`**.

   This installs prerequisites for building Unreal Engine 4. This process can take a long time to complete.

   > While running the Setup file, you should see `Checking dependencies (excluding Mac, Android)...`. If it also says `excluding Linux`, make sure that you set the environment variable `LINUX_MULTIARCH_ROOT` correctly, and run the Setup file again.

1. In the same directory, double-click **`GenerateProjectFiles.bat`**.

   This file automatically sets up the project files required to build Unreal Engine 4.

   > Note: If you encounter an `error MSB4036: The "GetReferenceNearestTargetFrameworkTask" task was not found` when building with Visual Studio 2017, check that you have NuGet Package Manager installed via the Visual Studio installer.

2. In the same directory, open **UE4.sln** in Visual Studio.

3. In Visual Studio, on the toolbar, go to **Build** > **Configuration Manager** and set your active solution configuration to **Development Editor** and your active solution platform to **Win64**.

4. In the Solution Explorer window, right-click on the **UE4** project and select **Build** (you may be prompted to install some dependencies first). <br>

   Visual Studio then builds Unreal Engine, which can take up to a couple of hours.

   You have now built Unreal Engine 4 with cross-compilation for Linux.

> Once you've built Unreal Engine, *don't move it into another directory*: that will break the integration.

#### Next: [Follow the Multiserver Shooter tutorial]({{urlRoot}}/content/get-started/tutorial)  