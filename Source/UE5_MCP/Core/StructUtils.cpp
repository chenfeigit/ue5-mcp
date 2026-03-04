#include "StructUtils.h"

#include "Internationalization/Regex.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

UScriptStruct* StructUtils::FindStructByName(const FString& StructTypeName)
{
	// 1. Already loaded?
	UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *StructTypeName);
	if (Struct)
		return Struct;

	// 2. C++ struct with /Script/Module.StructName format
	if (StructTypeName.StartsWith(TEXT("/Script/")))
	{
		Struct = Cast<UScriptStruct>(StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *StructTypeName));
		if (Struct)
			return Struct;
		// Fall through to short-name fallback (step 4) instead of returning nullptr
	}

	// 3. Module.StructName format (normalize to /Script/Module.StructName)
	FRegexPattern Pattern(TEXT("^\\w+\\.\\w+$"));
	FRegexMatcher Matcher(Pattern, StructTypeName);
	if (Matcher.FindNext())
	{
		const FString NormalizedPath = FString::Printf(TEXT("/Script/%s"), *StructTypeName);
		Struct = Cast<UScriptStruct>(StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *NormalizedPath));
		if (Struct)
			return Struct;
	}

	// 4. Fallback: find by short name with NativeFirst (matches engine FindObjectByTypePath behavior)
	int32 LastDot = INDEX_NONE;
	StructTypeName.FindLastChar(TCHAR('.'), LastDot);
	const FString ShortName = (LastDot != INDEX_NONE && LastDot + 1 < StructTypeName.Len())
		? StructTypeName.RightChop(LastDot + 1)
		: StructTypeName;
	if (!ShortName.IsEmpty())
	{
		UObject* Found = StaticFindFirstObject(
			UScriptStruct::StaticClass(),
			FStringView(ShortName),
			EFindFirstObjectOptions::NativeFirst,
			ELogVerbosity::NoLogging,
			nullptr);
		Struct = Cast<UScriptStruct>(Found);
		if (!Struct && ShortName.Len() >= 2 && ShortName[0] == TCHAR('F') && ShortName[1] >= TCHAR('A') && ShortName[1] <= TCHAR('Z'))
		{
			// UE registers many C++ structs as "StructName" (no leading F); try without F prefix
			const FString NameWithoutF = ShortName.RightChop(1);
			Found = StaticFindFirstObject(
				UScriptStruct::StaticClass(),
				FStringView(NameWithoutF),
				EFindFirstObjectOptions::NativeFirst,
				ELogVerbosity::NoLogging,
				nullptr);
			Struct = Cast<UScriptStruct>(Found);
		}
		if (Struct)
			return Struct;
	}

	return nullptr;
}
