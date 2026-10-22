using UnrealBuildTool;
using System.Collections.Generic;

public class AnthemGameTarget : TargetRules
{
	public AnthemGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AnthemGame");
	}
}
