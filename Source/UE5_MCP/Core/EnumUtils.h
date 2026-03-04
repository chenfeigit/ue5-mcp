#pragma once

// Utility functions for resolving UEnum by name (including engine enums via load).
class EnumUtils
{
public:
	/** Find or load a UEnum by name. Supports /Script/Module.EnumName and Module.EnumName. */
	static UEnum* FindEnumByName(const FString& EnumTypeName);
};
