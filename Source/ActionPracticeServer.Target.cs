using UnrealBuildTool;
using System.Collections.Generic;

public class ActionPracticeServerTarget : TargetRules
{
	public ActionPracticeServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ActionPractice");
		bUseLoggingInShipping = true;
	}
}
