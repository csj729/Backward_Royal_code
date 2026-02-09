// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class Backward_Royal : ModuleRules
{
	public Backward_Royal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"Slate",
			"SlateCore",
            "Json",
            "JsonUtilities",
            "Niagara",
            "GeometryCollectionEngine",
            "ChaosSolverEngine",
            "NavigationSystem",
			"AssetRegistry"
        });
		
		// Standalone 모드에서 Null Online Subsystem을 사용하기 위해 동적 로드
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");

		// PIE 종료 시 월드 참조 assertion 방지: PrePIEEnded에서 먼저 정리
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
