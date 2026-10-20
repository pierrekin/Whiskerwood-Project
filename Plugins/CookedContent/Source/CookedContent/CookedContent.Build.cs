using System.IO;
using UnrealBuildTool;

public class CookedContent : ModuleRules
{
	public CookedContent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// As Suzie builds, which the schema override and gameplay tag code was written in
		OptimizeCode = CodeOptimization.Never;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "GameplayTags", "Suzie" });

		// Loading cooked content needs the cooked editor's engine, which adds the unversioned schema override hook. On any other engine the module builds empty
		bool bCookedEditorEngine = File.Exists(Path.Combine(EngineDirectory, "Source", "Runtime", "CoreUObject", "Public", "Serialization", "UnversionedSchemaOverride.h"));
		PrivateDefinitions.Add("WITH_COOKED_EDITOR_ENGINE=" + (bCookedEditorEngine ? "1" : "0"));
	}
}
