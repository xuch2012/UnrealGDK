#!/usr/bin/env bash
set -e -u -x -o pipefail

cd "$(dirname "$0")/../"

echo "Validating FASTBuild builds and populating engine cache"

u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Game Win64 Development -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Editor Win64 Development -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Win32 Development -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Win64 Development -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Linux Development -WaitMutex -FromMsBuild

u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Game Win64 Shipping -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Win32 Shipping -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Win64 Shipping -WaitMutex -FromMsBuild
u:/Engine/Binaries/DotNET/UnrealBuildTool.exe UE4Server Linux Shipping -WaitMutex -FromMsBuild
