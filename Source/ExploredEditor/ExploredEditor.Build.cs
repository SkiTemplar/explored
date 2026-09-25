using UnrealBuildTool;

public class ExploredEditor : ModuleRules
{
	public ExploredEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "UnrealEd", "Explored"
		});
	}
}
