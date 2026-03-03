// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE5MCPSettings.h"

UUE5MCPSettings::UUE5MCPSettings()
{
	ServerPort = 8080;
	bAutoStartServer = true;
}

FName UUE5MCPSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

FText UUE5MCPSettings::GetSectionText() const
{
	return NSLOCTEXT("UE5MCP", "SettingsSection", "UE5 MCP");
}
