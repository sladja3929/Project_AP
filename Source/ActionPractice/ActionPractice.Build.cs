// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActionPractice : ModuleRules
{
	public ActionPractice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"GameplayStateTreeModule",
			"StateTreeModule",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"NetCore",
			"Niagara",
			"PhysicsCore",
			"NavigationSystem",
			"MotionWarping"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		//게임플레이 디버거 커스텀 카테고리 (WITH_GAMEPLAY_DEBUGGER 정의 + 모듈 의존성, Shipping 제외)
		SetupGameplayDebuggerSupport(Target);

		PublicIncludePaths.AddRange(new string[] {
			"ActionPractice"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
