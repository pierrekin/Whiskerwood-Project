#pragma once

#include "CoreMinimal.h"

// How the game is started and where it loads mods from, as decided by the launcher chosen in the settings
struct FGameLaunch
{
	// A URL, or the path of a program
	FString Command;

	// The game's Saved/mods folder
	FString ModsFolder;
};

namespace GameLaunchers
{
	// Works out the launch for the chosen launcher. On failure OutError says what is missing and what to set
	bool Resolve(FGameLaunch& OutLaunch, FString& OutError);

	// Starts the game the way Resolve says
	bool Launch(FString& OutError);
}
