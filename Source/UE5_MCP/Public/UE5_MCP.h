// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FUE5_MCPModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Resolves Python executable path: settings override, else "python". */
	static FString ResolvePythonExecutablePath();

	/** Returns true if the given Python has mcp_server dependencies (httpx, mcp, dotenv). */
	static bool CheckPythonMCPDependencies(const FString& PythonPath, const FString& WorkingDir);

	/** Starts the Python MCP server process if bAutoStartPythonMCP is true. */
	void TryStartPythonMCP();

	/** Stops the Python MCP server process if one was started. */
	void StopPythonMCP();

private:
	TSharedPtr<class FUICommandList> PluginCommands;

	/** Handle of the auto-started Python MCP process, if any. */
	FProcHandle PythonMCPProcessHandle;
};
