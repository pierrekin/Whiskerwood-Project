# CookedContent

An Unreal Engine editor plugin that makes a game's cooked content load and read correctly in a
project whose native types [Suzie](https://github.com/trumank/Suzie) builds from the game's jmap.

- Game-accurate unversioned property schemas, built from the jmap, so cooked assets whose classes
  differ between the game and the editor still load.
- A hook for hand-written binary serializers of structs the game serializes natively.
- The game's gameplay tags, read from the jmap, so cooked Blueprints keep the tags they reference.
- The /Script/ classes Suzie makes, registered with the IoStore loader, so cooked packages can
  import them.

It needs Suzie with `FSuzieDelegates`, which it binds from the PostDefault loading phase so that
its work runs inside Suzie's PostEngineInit startup.

## Where the code came from

The code is Buckminsterfullerene02's work, from the Whiskerwood cooked editor support of his
Suzie fork (branch `ww-5.8`, commit `b2a3ae0`, "whiskerwood cooked editor support"), where it was
`SuzieSchemaOverride`, `SuzieCustomSerializers`, `SuzieGameplayTags` and part of
`FSuziePluginModule::StartupModule`. It moved here with its files, types, log category, console
command and shadow property owner renamed, and nothing else changed.

Suzie is MIT licensed by Archengius and Truman Kilen; `LICENSE` is its license.
