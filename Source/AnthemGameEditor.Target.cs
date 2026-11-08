using UnrealBuildTool;
using System.Collections.Generic;

public class AnthemGameEditorTarget : TargetRules
{
	public AnthemGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AnthemGame");
	}
}
