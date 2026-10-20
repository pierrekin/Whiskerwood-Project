// Makes a game's cooked content load and read correctly in an editor whose native types Suzie
// builds from the game's jmap.
//
// Suzie starts in the PostEngineInit phase and makes the types; this module starts in the earlier
// PostDefault phase only to bind Suzie's delegates, so its work runs inside Suzie's startup at the
// points it needs: for each jmap file once its types are finalized, and once every file is
// processed, while native gameplay tags can still be registered and before the later
// PostEngineInit modules load.

#include "CookedContentModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogCookedContent);

#if WITH_COOKED_EDITOR_ENGINE

#include "CookedCustomSerializers.h"
#include "CookedGameplayTags.h"
#include "CookedSchemaOverride.h"
#include "Serialization/AsyncLoadingEvents.h"
#include "SuziePlugin.h"

class FCookedContentModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        JmapProcessedHandle = FSuzieDelegates::OnJmapProcessed.AddLambda(
            [](const TSharedPtr<FJsonObject>& Objects, const FSuzieDelegates::FPropertyBuilder& PropertyBuilder)
            {
                // Harvest the game's per-class property order for unversioned schema overrides
                FCookedSchemaOverrides::Get().BuildFromJmap(Objects, PropertyBuilder);

                // Collect the game's gameplay tags from dumped GameplayTagsList instances (full dumps only);
                // registration happens once after all files are processed
                FCookedGameplayTags::Get().CollectFromJmap(Objects);
            });

        AllJmapsProcessedHandle = FSuzieDelegates::OnAllJmapsProcessed.AddLambda([]
            {
                // Register Suzie's freshly generated /Script/ classes with the IoStore loader's global import store.
                // The loader registered all script objects early in startup (before Suzie's PostEngineInit module
                // ran), so cooked game packages reference those classes by a script-import hash the loader cannot
                // yet resolve - opening such an asset fails with "Could not find class object". Re-running the
                // registration is idempotent and additive (it only adds the new /Script/ objects), and makes
                // those hashes resolve so cooked content whose class is Suzie-generated can load.
                NotifyRegistrationComplete();

                // Must run while native tag registration is still open: DoneAddingNativeTags is bound to the
                // OnPostEngineInit broadcast, which fires after PostEngineInit plugin modules load
                FCookedGameplayTags::Get().RegisterCollectedTags();

                // Activate game-accurate unversioned schemas for cooked content (no-op if nothing diverges)
                FCookedSchemaOverrides::Get().RegisterProvider();
            });

        FSuzieDelegates::FindStructSerializer.BindStatic(&CookedCustomSerializers::FindStructSerializer);
    }

    virtual void ShutdownModule() override
    {
        FCookedSchemaOverrides::Get().UnregisterProvider();

        FSuzieDelegates::OnJmapProcessed.Remove(JmapProcessedHandle);
        FSuzieDelegates::OnAllJmapsProcessed.Remove(AllJmapsProcessedHandle);
        FSuzieDelegates::FindStructSerializer.Unbind();
    }

private:
    FDelegateHandle JmapProcessedHandle;
    FDelegateHandle AllJmapsProcessedHandle;
};

IMPLEMENT_MODULE(FCookedContentModule, CookedContent)

#else

// Without the cooked editor's engine there is no cooked content to load, so the module does nothing
IMPLEMENT_MODULE(FDefaultModuleImpl, CookedContent)

#endif
