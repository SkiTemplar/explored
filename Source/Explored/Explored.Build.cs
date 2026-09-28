using UnrealBuildTool;

public class Explored : ModuleRules
{
	public Explored(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"ProceduralMeshComponent", "PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI", "Slate", "SlateCore", "UMG", "Json", "DeveloperSettings" });
	}
}
