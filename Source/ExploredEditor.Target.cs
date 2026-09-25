using UnrealBuildTool;

public class ExploredEditorTarget : TargetRules
{
	public ExploredEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new[] { "Explored", "ExploredEditor" });
	}
}
