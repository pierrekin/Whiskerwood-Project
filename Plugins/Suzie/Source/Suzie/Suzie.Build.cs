using System.IO;
using UnrealBuildTool;
public class Suzie : ModuleRules
{
	public Suzie(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		OptimizeCode = CodeOptimization.Never;

		PublicIncludePaths.AddRange(new string[] {}); 
		
		PrivateIncludePaths.AddRange(new string[] {});


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Blutility",
				"Json",
				"UnrealEd",
				"Projects",
				"BlueprintGraph",
				"zlib",
				"GameplayTags",
			}
			);

		// The modkit engine adds hooks for the schema override and gameplay tags; a stock engine has neither
		bool bModkitEngine = File.Exists(Path.Combine(EngineDirectory, "Source", "Runtime", "CoreUObject", "Public", "Serialization", "UnversionedSchemaOverride.h"));
		PrivateDefinitions.Add("SUZIE_MODKIT_ENGINE=" + (bModkitEngine ? "1" : "0"));

		DynamicallyLoadedModuleNames.AddRange(new string[] {});
	}
}
