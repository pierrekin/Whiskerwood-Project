#if WITH_COOKED_EDITOR_ENGINE

#include "CookedCustomSerializers.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace CookedCustomSerializers
{
    FStructSerializeFn FindStructSerializer(const FString& StructPath)
    {
        return nullptr;
    }
}

#endif
