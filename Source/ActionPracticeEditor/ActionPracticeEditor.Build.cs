using UnrealBuildTool;

public class ActionPracticeEditor : ModuleRules
{
	public ActionPracticeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ActionPractice",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"PropertyEditor",
			"GameplayTags",
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"ActionPracticeEditor"
		});
	}
}
