#include "GameLaunchers.h"
#include "WWModToolsSettings.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const TCHAR* SteamAppId = TEXT("2489330");
static const TCHAR* GameExe = TEXT("Whiskerwood.exe");

#if PLATFORM_LINUX
static FString HomeDir()
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
}

// The game's mods folder inside a Wine prefix. Proton names the Windows user steamuser and plain Wine uses the
// Linux user's name, so whichever already has the game's folder wins
static FString ModsFolderInPrefix(const FString& Prefix)
{
	TArray<FString> Users;
	IFileManager::Get().FindFiles(Users, *(Prefix / TEXT("drive_c/users/*")), false, true);
	for (const FString& User : Users)
	{
		const FString Saved = Prefix / TEXT("drive_c/users") / User / TEXT("AppData/Local/Whiskerwood/Saved");
		if (IFileManager::Get().DirectoryExists(*Saved))
		{
			return Saved / TEXT("mods");
		}
	}

	const FString User = Users.Contains(TEXT("steamuser"))
		? FString(TEXT("steamuser"))
		: FPlatformMisc::GetEnvironmentVariable(TEXT("USER"));
	return Prefix / TEXT("drive_c/users") / User / TEXT("AppData/Local/Whiskerwood/Saved/mods");
}

// Every Steam library, from the default installs' and the Flatpak's library lists
static TArray<FString> SteamLibraries()
{
	const FString Home = HomeDir();
	TArray<FString> Roots = {
		Home / TEXT(".steam/steam"),
		Home / TEXT(".local/share/Steam"),
		Home / TEXT(".var/app/com.valvesoftware.Steam/.local/share/Steam"),
		Home / TEXT(".var/app/com.valvesoftware.Steam/.steam/steam"),
	};

	TArray<FString> Libraries;
	for (const FString& Root : Roots)
	{
		if (!IFileManager::Get().DirectoryExists(*Root)) continue;
		Libraries.AddUnique(FPaths::ConvertRelativePathToFull(Root));

		FString LibraryFolders;
		if (!FFileHelper::LoadFileToString(LibraryFolders, *(Root / TEXT("steamapps/libraryfolders.vdf")))) continue;

		TArray<FString> Lines;
		LibraryFolders.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT("\""), true);
			Fields.RemoveAll([](const FString& Field) { return Field.TrimStartAndEnd().IsEmpty(); });
			if (Fields.Num() == 2 && Fields[0] == TEXT("path"))
			{
				Libraries.AddUnique(Fields[1]);
			}
		}
	}
	return Libraries;
}

static bool ResolveSteamMods(FString& OutFolder, FString& OutError)
{
	for (const FString& Library : SteamLibraries())
	{
		const FString Prefix = Library / TEXT("steamapps/compatdata") / SteamAppId / TEXT("pfx");
		if (IFileManager::Get().DirectoryExists(*Prefix))
		{
			OutFolder = ModsFolderInPrefix(Prefix);
			return true;
		}
	}
	OutError = TEXT("Could not find the Steam copy's Proton prefix. Run the game once from Steam, or choose the Custom launcher and set the mods folder");
	return false;
}

static TSharedPtr<FJsonObject> ReadJson(const FString& Path)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return nullptr;
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	return FJsonSerializer::Deserialize(Reader, Json) ? Json : nullptr;
}

// Heroic keeps the GOG games it installed in gog_store/installed.json, and each game's Wine prefix in
// GamesConfig/<appName>.json
static bool ResolveHeroic(FGameLaunch& OutLaunch, FString& OutError)
{
	const FString Home = HomeDir();
	for (const FString& Config : { Home / TEXT(".config/heroic"), Home / TEXT(".var/app/com.heroicgameslauncher.hgl/config/heroic") })
	{
		const TSharedPtr<FJsonObject> Installed = ReadJson(Config / TEXT("gog_store/installed.json"));
		const TArray<TSharedPtr<FJsonValue>>* Games = nullptr;
		if (!Installed || !Installed->TryGetArrayField(TEXT("installed"), Games)) continue;

		for (const TSharedPtr<FJsonValue>& Game : *Games)
		{
			const TSharedPtr<FJsonObject>* Fields = nullptr;
			FString AppName, InstallPath;
			if (!Game->TryGetObject(Fields)
				|| !(*Fields)->TryGetStringField(TEXT("appName"), AppName)
				|| !(*Fields)->TryGetStringField(TEXT("install_path"), InstallPath)
				|| !FPaths::FileExists(InstallPath / GameExe))
			{
				continue;
			}

			const TSharedPtr<FJsonObject> GameConfig = ReadJson(Config / TEXT("GamesConfig") / (AppName + TEXT(".json")));
			const TSharedPtr<FJsonObject>* Settings = nullptr;
			FString Prefix;
			if (!GameConfig || !GameConfig->TryGetObjectField(AppName, Settings) || !(*Settings)->TryGetStringField(TEXT("winePrefix"), Prefix))
			{
				OutError = FString::Printf(TEXT("Heroic has Whiskerwood installed but no Wine prefix recorded in %s"),
					*(Config / TEXT("GamesConfig") / (AppName + TEXT(".json"))));
				return false;
			}

			OutLaunch.Command = FString::Printf(TEXT("heroic://launch/gog/%s"), *AppName);
			OutLaunch.ModsFolder = ModsFolderInPrefix(Prefix);
			return true;
		}
	}
	OutError = TEXT("Could not find Whiskerwood installed in Heroic. Choose the Custom launcher to set the launch command and mods folder");
	return false;
}

// Lutris keeps each game in a games/<slug>-<id>.yml file whose game section names the exe and the Wine prefix
static bool ResolveLutris(FGameLaunch& OutLaunch, FString& OutError)
{
	const FString Home = HomeDir();
	for (const FString& Dir : { Home / TEXT(".local/share/lutris/games"), Home / TEXT(".config/lutris/games"), Home / TEXT(".var/app/net.lutris.Lutris/data/lutris/games") })
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.yml")), true, false);
		for (const FString& File : Files)
		{
			TArray<FString> Lines;
			if (!FFileHelper::LoadFileToStringArray(Lines, *(Dir / File))) continue;

			bool bInGame = false;
			FString Exe, Prefix;
			for (const FString& Line : Lines)
			{
				if (!Line.IsEmpty() && !FChar::IsWhitespace(Line[0]))
				{
					bInGame = Line.TrimEnd() == TEXT("game:");
					continue;
				}
				if (!bInGame) continue;

				FString Key, Value;
				if (Line.TrimStart().Split(TEXT(":"), &Key, &Value))
				{
					Value = Value.TrimStartAndEnd().TrimQuotes();
					if (Key == TEXT("exe")) Exe = Value;
					else if (Key == TEXT("prefix")) Prefix = Value;
				}
			}
			if (!Exe.EndsWith(GameExe) || Prefix.IsEmpty()) continue;

			// The file is named after the game's slug and a timestamp
			FString Slug = FPaths::GetBaseFilename(File);
			int32 Dash;
			if (Slug.FindLastChar('-', Dash) && Slug.Mid(Dash + 1).IsNumeric())
			{
				Slug.LeftInline(Dash);
			}

			OutLaunch.Command = FString::Printf(TEXT("lutris:rungame/%s"), *Slug);
			OutLaunch.ModsFolder = ModsFolderInPrefix(Prefix);
			return true;
		}
	}
	OutError = TEXT("Could not find Whiskerwood in Lutris. Choose the Custom launcher to set the launch command and mods folder");
	return false;
}
#endif

#if PLATFORM_WINDOWS
static bool ResolveWindowsMods(FString& OutFolder, FString& OutError)
{
	const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (LocalAppData.IsEmpty())
	{
		OutError = TEXT("%LOCALAPPDATA% is not set");
		return false;
	}
	OutFolder = LocalAppData / TEXT("Whiskerwood/Saved/mods");
	return true;
}
#endif

bool GameLaunchers::Resolve(FGameLaunch& OutLaunch, FString& OutError)
{
	OutLaunch = FGameLaunch();
	const UWWModToolsSettings* Settings = GetDefault<UWWModToolsSettings>();

	switch (Settings->Launcher)
	{
	case EWWGameLauncher::Steam:
		OutLaunch.Command = FString::Printf(TEXT("steam://rungameid/%s"), SteamAppId);
#if PLATFORM_WINDOWS
		return ResolveWindowsMods(OutLaunch.ModsFolder, OutError);
#elif PLATFORM_LINUX
		return ResolveSteamMods(OutLaunch.ModsFolder, OutError);
#else
		OutError = TEXT("The game's mods folder is unknown on this platform. Choose the Custom launcher and set it");
		return false;
#endif

	case EWWGameLauncher::Heroic:
	case EWWGameLauncher::Lutris:
#if PLATFORM_LINUX
		return Settings->Launcher == EWWGameLauncher::Heroic
			? ResolveHeroic(OutLaunch, OutError)
			: ResolveLutris(OutLaunch, OutError);
#else
		OutError = TEXT("The Heroic and Lutris launchers are only found on Linux. Choose the Custom launcher instead");
		return false;
#endif

	case EWWGameLauncher::Custom:
		OutLaunch.Command = Settings->CustomLaunchCommand.TrimStartAndEnd();
		OutLaunch.ModsFolder = Settings->CustomModsFolder.Path;
		if (OutLaunch.Command.IsEmpty() || OutLaunch.ModsFolder.IsEmpty())
		{
			OutError = TEXT("The Custom launcher needs both a launch command and a mods folder in Whiskerwood Mod Tools settings");
			return false;
		}
		return true;
	}

	OutError = TEXT("Unknown launcher in Whiskerwood Mod Tools settings");
	return false;
}

bool GameLaunchers::Launch(FString& OutError)
{
	FGameLaunch GameLaunch;
	if (!Resolve(GameLaunch, OutError)) return false;

	if (FPaths::FileExists(GameLaunch.Command))
	{
		FProcHandle Proc = FPlatformProcess::CreateProc(*GameLaunch.Command, TEXT(""), true, false, false, nullptr, 0, nullptr, nullptr, nullptr);
		if (!Proc.IsValid())
		{
			OutError = FString::Printf(TEXT("Could not start %s"), *GameLaunch.Command);
			return false;
		}
		return true;
	}

	FString UrlError;
	FPlatformProcess::LaunchURL(*GameLaunch.Command, nullptr, &UrlError);
	if (!UrlError.IsEmpty())
	{
		OutError = UrlError;
		return false;
	}
	return true;
}
