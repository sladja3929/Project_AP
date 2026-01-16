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
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ActionPractice",
			"ActionPractice/Variant_Platforming",
			"ActionPractice/Variant_Combat",
			"ActionPractice/Variant_Combat/AI",
			"ActionPractice/Variant_SideScrolling",
			"ActionPractice/Variant_SideScrolling/Gameplay",
			"ActionPractice/Variant_SideScrolling/AI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
