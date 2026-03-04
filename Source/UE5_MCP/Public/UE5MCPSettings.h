// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UE5MCPSettings.generated.h"

/**
 * Project settings for UE5 MCP HTTP server (port and auto-start behavior).
 * Exposed under Edit -> Project Settings -> Plugins -> UE5 MCP.
 */
UCLASS(Config = Engine, defaultconfig, meta = (DisplayName = "UE5 MCP"))
class UE5_MCP_API UUE5MCPSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUE5MCPSettings();

	/** Port for the MCP HTTP server. Valid range: 1-65535. */
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 ServerPort;

	/** If true, the MCP HTTP server starts automatically when the editor launches. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	bool bAutoStartServer;

	/** If true, the Python MCP server (mcp_server.py) starts automatically when the editor launches. */
	UPROPERTY(Config, EditAnywhere, Category = "Python MCP")
	bool bAutoStartPythonMCP;

	/** Port for the Python MCP server. Valid range: 1-65535. */
	UPROPERTY(Config, EditAnywhere, Category = "Python MCP", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 PythonMCPPort;

	/** Optional path to Python interpreter; empty = use system "python". */
	UPROPERTY(Config, EditAnywhere, Category = "Python MCP")
	FString PythonExecutablePath;

	/** If true, when dependencies are missing run pip install -r requirements.txt then try to start MCP. */
	UPROPERTY(Config, EditAnywhere, Category = "Python MCP")
	bool bAutoInstallPythonMCPDependencies;

	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;
};
