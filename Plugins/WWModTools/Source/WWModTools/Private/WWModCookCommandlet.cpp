#include "WWModCookCommandlet.h"
#include "ModActions.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogWWModCook, Log, All);

UWWModCookCommandlet::UWWModCookCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UWWModCookCommandlet::Main(const FString& Params)
{
	FString ModName;
	if (!FParse::Value(*Params, TEXT("Mod="), ModName) || ModName.IsEmpty())
	{
		UE_LOG(LogWWModCook, Error, TEXT("Usage: -run=WWModCook -Mod=<ModName>"));
		return 1;
	}

	FModInfo Info;
	if (!ModActions::LoadModInfo(ModName, Info))
	{
		UE_LOG(LogWWModCook, Error, TEXT("No mod details recorded for '%s' (%s). Create the mod or set its details in the editor first"),
			*ModName, *ModActions::GetSourceUPluginPath(ModName));
		return 1;
	}
	if (ModActions::NeedsDetailsPrompt(Info))
	{
		UE_LOG(LogWWModCook, Error, TEXT("'%s' is missing some mod details. Set them with Update Mod Details in the editor first"),
			*ModName);
		return 1;
	}

	// As the editor action does, refresh the fields the mod tools own, such as the engine version
	FModInfo Normalised = Info;
	ModActions::ApplyFieldDefaults(Normalised);
	if (!Normalised.Values.OrderIndependentCompareEqual(Info.Values) && !ModActions::SaveModInfo(Normalised))
	{
		UE_LOG(LogWWModCook, Error, TEXT("Failed to update %s"), *ModActions::GetSourceUPluginPath(ModName));
		return 1;
	}

	// A commandlet starts with nothing scanned, and the mod's chunk comes from its label asset
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ FString::Printf(TEXT("/Game/Mods/%s"), *ModName) }, true);

	FModCookJob Job;
	FString Error;
	if (!ModActions::PrepareModCook(ModName, Job, Error))
	{
		UE_LOG(LogWWModCook, Error, TEXT("%s"), *Error);
		return 1;
	}

	UE_LOG(LogWWModCook, Display, TEXT("Cooking '%s' (chunk %d): %s %s"), *ModName, Job.ChunkId, *Job.RunUAT, *Job.Args);

	int32 Installed = 0;
	const EModCookResult Result = ModActions::RunModCook(Job,
		[&ModName](const FString& Status)
		{
			UE_LOG(LogWWModCook, Display, TEXT("%s: %s"), *ModName, *Status);
		},
		[]() { return IsEngineExitRequested(); },
		Installed, Error);

	if (Result != EModCookResult::Installed)
	{
		UE_LOG(LogWWModCook, Error, TEXT("'%s' was not installed: %s"), *ModName, *Error);
		return 1;
	}

	UE_LOG(LogWWModCook, Display, TEXT("'%s' installed (%d file(s)) in %s"), *ModName, Installed, *(ModActions::GetModsInstallRoot() / ModName));
	return 0;
}
