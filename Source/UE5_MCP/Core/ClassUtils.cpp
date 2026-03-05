#include "ClassUtils.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"

namespace
{
	// UE naming prefixes: A (Actor), U (UObject), F (struct), E (enum), I (interface)
	static const TArray<FString> UEClassPrefixes = { TEXT("A"), TEXT("U"), TEXT("F"), TEXT("E"), TEXT("I") };

	/** Returns true if Name is empty or already starts with a UE class prefix. */
	static bool HasUEPrefix(const FString& Name)
	{
		if (Name.Len() == 0) return true;
		TCHAR First = Name[0];
		return First == TEXT('A') || First == TEXT('U') || First == TEXT('F') || First == TEXT('E') || First == TEXT('I');
	}

	/** Try loading UClass by prepending A/U/F/E/I to ClassNamePart; ModulePart is e.g. "/Script/Engine". Returns nullptr if not found or if name already has prefix. */
	static UClass* TryLoadClassWithUEPrefixes(const FString& ModulePart, const FString& ClassNamePart)
	{
		if (HasUEPrefix(ClassNamePart)) return nullptr;
		for (int32 i = 0; i < UEClassPrefixes.Num(); ++i)
		{
			FString WithPrefix = ModulePart + TEXT(".") + UEClassPrefixes[i] + ClassNamePart;
			UClass* Class = StaticLoadClass(UObject::StaticClass(), nullptr, *WithPrefix);
			if (Class)
			{
				UE_LOG(LogTemp, Log, TEXT("Loaded C++ class with prefix: %s"), *WithPrefix);
				return Class;
			}
		}
		return nullptr;
	}
}

UClass* ClassUtils::FindClassByName(const FString& ClassFullName)
{
	// 1. Already loaded?
	UClass* Class = FindObject<UClass>(nullptr, *ClassFullName);
	if (Class)
		return Class;

	// 2. Blueprint-generated class format like /Game/Path/To/Blueprint.Blueprint_C
	FRegexPattern BPPattern(TEXT("^/Game/.+\\..+_C$"));
	FRegexMatcher BPMatcher(BPPattern, ClassFullName);
	if (BPMatcher.FindNext())
	{
		Class = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassFullName);
		UE_LOG(LogTemp, Log, TEXT("Trying to load Blueprint class: %s"), *ClassFullName);
		return Class;
	}

	// 3. C++ class with /Script/Module.Class format (try as-is, then with A/U/F/E/I prefix if not found)
	if (ClassFullName.StartsWith(TEXT("/Script/")))
	{
		Class = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassFullName);
		if (Class) return Class;
		int32 DotIndex;
		if (ClassFullName.FindLastChar(TEXT('.'), DotIndex))
		{
			FString ModulePart = ClassFullName.Left(DotIndex);
			FString ClassNamePart = ClassFullName.Mid(DotIndex + 1);
			Class = TryLoadClassWithUEPrefixes(ModulePart, ClassNamePart);
		}
		return Class;
	}

	// 4. C++ class in ModuleName.ClassName format (prepend /Script/, then try A/U/F/E/I prefix if not found)
	FRegexPattern CppPattern(TEXT("^\\w+\\.\\w+$"));
	FRegexMatcher Matcher(CppPattern, ClassFullName);
	if (Matcher.FindNext())
	{
		FString FullPath = FString::Printf(TEXT("/Script/%s"), *ClassFullName);
		Class = StaticLoadClass(UObject::StaticClass(), nullptr, *FullPath);
		if (!Class)
		{
			int32 DotIdx;
			if (ClassFullName.FindLastChar(TEXT('.'), DotIdx))
			{
				FString ModulePart = FString(TEXT("/Script/")) + ClassFullName.Left(DotIdx);
				FString ClassNamePart = ClassFullName.Mid(DotIdx + 1);
				Class = TryLoadClassWithUEPrefixes(ModulePart, ClassNamePart);
			}
		}
		return Class;
	}

	// 5. Try adding _C for Blueprint asset path
	int32 LastSlash = -1;
	if (ClassFullName.StartsWith(TEXT("/Game/")) &&
		!ClassFullName.EndsWith(TEXT("_C")) &&
		ClassFullName.FindLastChar(TEXT('/'), LastSlash))
	{
		FString AssetName = ClassFullName.Mid(LastSlash + 1);
		FString BPClassPath = FString::Printf(TEXT("%s.%s_C"), *ClassFullName, *AssetName);
		Class = StaticLoadClass(UObject::StaticClass(), nullptr, *BPClassPath);
		UE_LOG(LogTemp, Log, TEXT("Trying to load Blueprint class: %s"), *BPClassPath);
		return Class;
	}

	return nullptr;
}


FString ClassUtils::CreateBlueprintFromClass(const FString& ParentClassFullName, const FString& BlueprintPath)
{
	UE_LOG(LogTemp, Log, TEXT("Creating Blueprint: %s from Parent Class: %s"), *BlueprintPath, *ParentClassFullName);
	
	UClass* ParentClass = FindClassByName(ParentClassFullName);
	
	if (!ParentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Class not found: %s"), *ParentClassFullName);
		throw std::runtime_error("Class not found");
	}

	auto lastSlashIndex = -1;
	FString AssetName;
	FString PackageName;
	if (BlueprintPath.FindLastChar('/', lastSlashIndex))
	{
		PackageName = BlueprintPath.Left(lastSlashIndex);
		AssetName = BlueprintPath.Mid(lastSlashIndex + 1);
	}

	FString UniquePackageName;
	FString UniqueAssetName;
	
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName + "/" + AssetName, TEXT(""), UniquePackageName, UniqueAssetName);

	// Create package
	UPackage* Package = CreatePackage(*UniquePackageName);

	// Create Blueprint
	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		*UniqueAssetName,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		FName("BlueprintCreation")
	);

	if (!NewBP)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create Blueprint: %s/%s"), *UniquePackageName, *UniqueAssetName);
		throw std::runtime_error("Failed to create Blueprint");
	}
	
	// Notify asset registry & mark dirty
	FAssetRegistryModule::AssetCreated(NewBP);
	if (!Package->MarkPackageDirty())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to mark package dirty: %s/%s"), *UniquePackageName, *UniqueAssetName);
		throw std::runtime_error("Failed to save Blueprint");
	}

	// Save to disk
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	SaveArgs.bWarnOfLongFilename = true;

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		UniquePackageName, FPackageName::GetAssetPackageExtension()
	);

	
	if (!UPackage::SavePackage(Package, NewBP, *PackageFileName, SaveArgs))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save Blueprint: %s"), *PackageFileName);
		throw std::runtime_error("Failed to save Blueprint");
	}

	UE_LOG(LogTemp, Log, TEXT("Blueprint created: %s"), *PackageFileName);
	return UniquePackageName;
}

