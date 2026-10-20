# BlueprintTypeOverrides

An Unreal Engine editor plugin that changes the types of a game's cooked Blueprints in memory each
time the editor or a commandlet starts, as the project's `Config/BlueprintTypeOverrides.json` lists
them, so the project's own Blueprints can be built as the game runs them. For now an override
widens the class of an object property, a function's parameter or a class's variable to a base of
the one it has; `Source/BlueprintTypeOverrides/Private/BlueprintTypeOverridesModule.cpp` describes
the file.

It was mise's `plugin/BlueprintOverrides`, which read `Config/BlueprintOverrides.json`.
