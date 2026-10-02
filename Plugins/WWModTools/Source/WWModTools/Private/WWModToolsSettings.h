#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WWModToolsSettings.generated.h"

UENUM()
enum class EWWGameLauncher : uint8
{
	Steam,
	Heroic UMETA(ToolTip="GOG's copy installed through Heroic, on Linux"),
	Lutris UMETA(ToolTip="A copy installed through Lutris, on Linux"),
	Custom UMETA(ToolTip="Any other way of running the game: set its launch command and mods folder"),
};

UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="WW Mod Tools"))
class UWWModToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("WWModTools"); }

	UPROPERTY(Config, EditAnywhere, Category="Build",
		meta=(DisplayName="RunUAT Path (override)",
			ToolTip="Leave blank to use the engine this project was opened with"))
	FFilePath RunUATOverride;

	UPROPERTY(Config, EditAnywhere, Category="Game",
		meta=(ToolTip="What runs your copy of the game. It decides how the game is started and where mods are installed"))
	EWWGameLauncher Launcher = EWWGameLauncher::Steam;

	UPROPERTY(Config, EditAnywhere, Category="Game",
		meta=(EditCondition="Launcher == EWWGameLauncher::Custom", EditConditionHides,
			ToolTip="A URL, such as a GOG Galaxy link, or the path of a program that starts the game"))
	FString CustomLaunchCommand;

	UPROPERTY(Config, EditAnywhere, Category="Game",
		meta=(EditCondition="Launcher == EWWGameLauncher::Custom", EditConditionHides,
			ToolTip="The game's Saved/mods folder"))
	FDirectoryPath CustomModsFolder;
};
