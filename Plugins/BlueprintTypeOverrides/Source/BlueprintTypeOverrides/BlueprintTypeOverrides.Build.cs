using UnrealBuildTool;

public class BlueprintTypeOverrides : ModuleRules
{
	public BlueprintTypeOverrides(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json" });
	}
}
