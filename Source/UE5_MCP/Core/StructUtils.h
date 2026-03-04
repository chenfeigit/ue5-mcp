#pragma once

// Utility functions for resolving UScriptStruct by name (including engine structs via load).
class StructUtils
{
public:
	/** Find or load a UScriptStruct by name. Supports /Script/Module.StructName and Module.StructName. */
	static UScriptStruct* FindStructByName(const FString& StructTypeName);
};
