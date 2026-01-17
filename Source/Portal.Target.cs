using UnrealBuildTool;
using System.Collections.Generic;

public class PortalTarget : TargetRules
{
	public PortalTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "Portal" } );
	}
}
