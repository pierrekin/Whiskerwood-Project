// Changes the types of a game's cooked Blueprints in memory, as the project's
// Config/BlueprintTypeOverrides.json lists them. A cooked Blueprint has no graph the editor can change
// and save, so a project that needs one of its types otherwise, such as a mod that passes its own
// copy of a game class to a game Blueprint's function, has this plugin change the loaded class
// each time the editor or a commandlet starts.
//
// It runs on the OnAllModuleLoadingPhasesComplete broadcast, which comes after every module of
// the PostEngineInit phase has started, when the game's pak is mounted and Suzie has built the
// game's native classes, and before a commandlet's Main runs or the editor opens a Blueprint.
// OnPostEngineInit is too early: a commandlet broadcasts it before those modules load. Each class
// it changes is loaded then and kept from garbage collection, so it is never loaded again without
// the change.
//
// For now an override widens the class of an object property, a function's parameter or a
// class's variable, to a base class of the one it has:
//
//   {"widen": [{"owner": "/Game/UI/Cell.Cell_C", "function": "SetData", "property": "Control",
//               "from": "/Game/UI/Control.Control_C", "to": "/Script/Game.WidgetBase"}]}
//
// Without "function" the property is the class's own variable. Every override must apply, as the
// file says: the property must hold an object of the class "from", and "to" must be a base of it.
// One that does not is an error, and the log's last BlueprintTypeOverrides line says how many applied.

#include "Dom/JsonObject.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintTypeOverrides, Log, All);

namespace
{
	const TCHAR* FileName = TEXT("BlueprintTypeOverrides.json");

	// Applies one widening; an error says why it cannot.
	bool Widen(const FJsonObject& Override, FString& OutError)
	{
		FString OwnerPath, FunctionName, PropertyName, FromPath, ToPath;
		if (!Override.TryGetStringField(TEXT("owner"), OwnerPath) || !Override.TryGetStringField(TEXT("property"), PropertyName)
			|| !Override.TryGetStringField(TEXT("from"), FromPath) || !Override.TryGetStringField(TEXT("to"), ToPath))
		{
			OutError = TEXT("a widening needs owner, property, from and to");
			return false;
		}
		const bool bParameter = Override.TryGetStringField(TEXT("function"), FunctionName);
		const FString Shown = bParameter ? FString::Printf(TEXT("%s:%s's parameter %s"), *OwnerPath, *FunctionName, *PropertyName)
										 : FString::Printf(TEXT("%s.%s"), *OwnerPath, *PropertyName);

		UClass* Owner = LoadObject<UClass>(nullptr, *OwnerPath);
		if (!Owner)
		{
			OutError = FString::Printf(TEXT("%s: the class %s does not load"), *Shown, *OwnerPath);
			return false;
		}
		const UStruct* Scope = Owner;
		if (bParameter)
		{
			const UFunction* Function = Owner->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::ExcludeSuper);
			if (!Function)
			{
				OutError = FString::Printf(TEXT("%s: %s declares no function %s"), *Shown, *OwnerPath, *FunctionName);
				return false;
			}
			Scope = Function;
		}
		FObjectPropertyBase* Property = nullptr;
		for (TFieldIterator<FProperty> It(Scope, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->GetFName() == FName(*PropertyName))
			{
				Property = CastField<FObjectPropertyBase>(*It);
				if (!Property)
				{
					OutError = FString::Printf(TEXT("%s is a %s, not an object property"), *Shown, *It->GetClass()->GetName());
					return false;
				}
				break;
			}
		}
		if (!Property)
		{
			OutError = FString::Printf(TEXT("%s: no such property"), *Shown);
			return false;
		}
		if (bParameter && !Property->HasAnyPropertyFlags(CPF_Parm))
		{
			OutError = FString::Printf(TEXT("%s is not a parameter"), *Shown);
			return false;
		}
		if (!Property->PropertyClass || Property->PropertyClass->GetPathName() != FromPath)
		{
			OutError = FString::Printf(TEXT("%s holds %s, not %s; the game may have changed"), *Shown, *GetPathNameSafe(Property->PropertyClass), *FromPath);
			return false;
		}
		UClass* To = LoadObject<UClass>(nullptr, *ToPath);
		if (!To)
		{
			OutError = FString::Printf(TEXT("%s: the class %s does not load"), *Shown, *ToPath);
			return false;
		}
		if (!Property->PropertyClass->IsChildOf(To))
		{
			OutError = FString::Printf(TEXT("%s: %s is not a base of %s"), *Shown, *ToPath, *FromPath);
			return false;
		}
		Property->SetPropertyClass(To);
		// A class unloaded and loaded again would have its cooked type back.
		Owner->AddToRoot();
		UE_LOG(LogBlueprintTypeOverrides, Display, TEXT("BlueprintTypeOverrides: widened %s from %s to %s"), *Shown, *FromPath, *ToPath);
		return true;
	}

	void Apply()
	{
		const FString Path = FPaths::ProjectConfigDir() / FileName;
		if (!FPaths::FileExists(Path))
		{
			UE_LOG(LogBlueprintTypeOverrides, Display, TEXT("BlueprintTypeOverrides: the project has no %s, so no overrides"), *Path);
			return;
		}
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			UE_LOG(LogBlueprintTypeOverrides, Error, TEXT("BlueprintTypeOverrides: cannot read %s as JSON"), *Path);
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Widenings = nullptr;
		if (!Root->TryGetArrayField(TEXT("widen"), Widenings))
		{
			UE_LOG(LogBlueprintTypeOverrides, Error, TEXT("BlueprintTypeOverrides: %s has no widen list"), *Path);
			return;
		}
		int32 Applied = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Widenings)
		{
			const TSharedPtr<FJsonObject>* Override = nullptr;
			FString Error = TEXT("a widening that is not an object");
			if (Value->TryGetObject(Override) && Widen(**Override, Error))
			{
				++Applied;
			}
			else
			{
				UE_LOG(LogBlueprintTypeOverrides, Error, TEXT("BlueprintTypeOverrides: %s"), *Error);
			}
		}
		UE_LOG(LogBlueprintTypeOverrides, Display, TEXT("BlueprintTypeOverrides: %d of %d overrides applied from %s"), Applied, Widenings->Num(), *Path);
	}
}

class FBlueprintTypeOverridesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddStatic(&Apply);
	}
};

IMPLEMENT_MODULE(FBlueprintTypeOverridesModule, BlueprintTypeOverrides)
