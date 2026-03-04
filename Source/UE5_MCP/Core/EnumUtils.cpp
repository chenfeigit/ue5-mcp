#include "EnumUtils.h"

#include "Internationalization/Regex.h"
#include "UObject/UObjectGlobals.h"

UEnum* EnumUtils::FindEnumByName(const FString& EnumTypeName)
{
	// 1. Already loaded?
	UEnum* Enum = FindObject<UEnum>(nullptr, *EnumTypeName);
	if (Enum)
		return Enum;

	// 2. C++ enum with /Script/Module.EnumName format
	if (EnumTypeName.StartsWith(TEXT("/Script/")))
	{
		Enum = Cast<UEnum>(StaticLoadObject(UEnum::StaticClass(), nullptr, *EnumTypeName));
		if (Enum)
			return Enum;
		// Fall through to short-name fallback (step 4) instead of returning nullptr
	}

	// 3. Module.EnumName format (normalize to /Script/Module.EnumName)
	FRegexPattern Pattern(TEXT("^\\w+\\.\\w+$"));
	FRegexMatcher Matcher(Pattern, EnumTypeName);
	if (Matcher.FindNext())
	{
		const FString NormalizedPath = FString::Printf(TEXT("/Script/%s"), *EnumTypeName);
		Enum = Cast<UEnum>(StaticLoadObject(UEnum::StaticClass(), nullptr, *NormalizedPath));
		if (Enum)
			return Enum;
	}

	// 4. Fallback: find by short name with NativeFirst (matches engine FindObjectByTypePath behavior)
	int32 LastDot = INDEX_NONE;
	EnumTypeName.FindLastChar(TCHAR('.'), LastDot);
	const FString ShortName = (LastDot != INDEX_NONE && LastDot + 1 < EnumTypeName.Len())
		? EnumTypeName.RightChop(LastDot + 1)
		: EnumTypeName;
	if (!ShortName.IsEmpty())
	{
		UObject* Found = StaticFindFirstObject(
			UEnum::StaticClass(),
			FStringView(ShortName),
			EFindFirstObjectOptions::NativeFirst,
			ELogVerbosity::NoLogging,
			nullptr);
		Enum = Cast<UEnum>(Found);
		if (Enum)
			return Enum;
	}

	return nullptr;
}
