using UnrealBuildTool;
using System.Collections.Generic;

public class PortalEditorTarget : TargetRules
{
	public PortalEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "Portal" } );
	}
}
