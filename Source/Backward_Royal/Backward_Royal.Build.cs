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

        // 패키징 시 프로젝트 루트의 Data 폴더(JSON 설정 파일들)를 빌드 출력에 포함
        // NonUFS: .pak 파일 안이 아니라 별도 파일로 복사 (FFileHelper로 읽기 위해)
        // $(ProjectDir) = 프로젝트 루트 (.uproject 위치), ... = 재귀적 모든 파일
        RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/BR_DataTool.exe", StagedFileType.NonUFS);
    }
}
