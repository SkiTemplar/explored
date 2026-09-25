using UnrealBuildTool;

public class ExploredEditor : ModuleRules
{
	public ExploredEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "UnrealEd", "Explored",
			"ImageWrapper", "MeshDescription", "StaticMeshDescription",
			"AssetRegistry", "AssetTools", "RenderCore", "PhysicsCore", "Json"
		});
	}
}
