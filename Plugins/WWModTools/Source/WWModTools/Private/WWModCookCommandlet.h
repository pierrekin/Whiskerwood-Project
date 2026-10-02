#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WWModCookCommandlet.generated.h"

// Cooks and installs one mod without the editor's window, the same way the Cook and Install action does:
//   UnrealEditor-Cmd <project> -run=WWModCook -Mod=<ModName>
// The mod's details must already be recorded, since there is nobody to ask for them
UCLASS()
class UWWModCookCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWWModCookCommandlet();

	virtual int32 Main(const FString& Params) override;
};
