// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE5_MCP.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UE5_MCPStyle.h"
#include "UE5_MCPCommands.h"
#include "UE5MCPSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "UE5_MCP/API/Route.h"
#include "Widgets/Input/SSpinBox.h"

static const FName UE5_MCPTabName("UE5_MCP");

#define LOCTEXT_NAMESPACE "FUE5_MCPModule"

FString FUE5_MCPModule::ResolvePythonExecutablePath()
{
	const UUE5MCPSettings* Settings = GetDefault<UUE5MCPSettings>();
	if (!Settings->PythonExecutablePath.IsEmpty())
	{
		return Settings->PythonExecutablePath;
	}
	return TEXT("python");
}

bool FUE5_MCPModule::CheckPythonMCPDependencies(const FString& PythonPath, const FString& WorkingDir)
{
	int32 ReturnCode = -1;
	FString StdOut;
	FString StdErr;
	const FString Params = TEXT("-c \"import httpx; import mcp.server.fastmcp; import dotenv\"");
	const bool bExecOk = FPlatformProcess::ExecProcess(
		*PythonPath,
		*Params,
		&ReturnCode,
		&StdOut,
		&StdErr,
		*WorkingDir,
		false);
	return bExecOk && ReturnCode == 0;
}

void FUE5_MCPModule::TryStartPythonMCP()
{
	// If already running (or restarted), stop the previous process first
	StopPythonMCP();

	const UUE5MCPSettings* Settings = GetDefault<UUE5MCPSettings>();
	if (!Settings->bAutoStartPythonMCP)
	{
		return;
	}
	const int32 Port = Settings->PythonMCPPort;
	if (Port < 1 || Port > 65535)
	{
		UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Invalid PythonMCPPort %d (must be 1-65535), Python MCP not started."), Port);
		return;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE5_MCP"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Could not find plugin UE5_MCP, Python MCP not started."));
		return;
	}

	FString ScriptPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Python"), TEXT("MCP"), TEXT("mcp_server.py"));
	if (!FPaths::FileExists(ScriptPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Script not found: %s"), *ScriptPath);
		return;
	}

	// Kill any process already listening on PythonMCPPort (e.g. leftover from previous editor crash)
#if PLATFORM_WINDOWS
	{
		FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("COMSPEC"));
		if (CmdExe.IsEmpty())
		{
			CmdExe = TEXT("cmd.exe");
		}
		FString CmdLine = FString::Printf(
			TEXT("/c \"powershell -NoProfile -Command \\\"Get-NetTCPConnection -LocalPort %d -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue }\\\"\""),
			Port);
		int32 DummyReturnCode = 0;
		FPlatformProcess::ExecProcess(*CmdExe, *CmdLine, &DummyReturnCode, nullptr, nullptr, nullptr, false);
	}
#elif PLATFORM_LINUX || PLATFORM_MAC
	{
		FString ShParams = FString::Printf(TEXT("-c \"lsof -ti:%d | xargs kill -9 2>/dev/null\""), Port);
		int32 DummyReturnCode = 0;
		FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *ShParams, &DummyReturnCode, nullptr, nullptr, nullptr, false);
	}
#endif

	FString PythonPath = ResolvePythonExecutablePath();
	FString WorkingDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Python"), TEXT("MCP"));

	// Dependency check
	bool bDepsOk = CheckPythonMCPDependencies(PythonPath, WorkingDir);
	if (!bDepsOk && Settings->bAutoInstallPythonMCPDependencies)
	{
		// Try auto-install
		FString RequirementsPath = FPaths::Combine(WorkingDir, TEXT("requirements.txt"));
		if (FPaths::FileExists(RequirementsPath))
		{
			int32 PipReturnCode = -1;
			FString PipOut;
			FString PipErr;
			const FString PipParams = TEXT("-m pip install -r requirements.txt");
			if (FPlatformProcess::ExecProcess(*PythonPath, *PipParams, &PipReturnCode, &PipOut, &PipErr, *WorkingDir, false) && PipReturnCode == 0)
			{
				bDepsOk = CheckPythonMCPDependencies(PythonPath, WorkingDir);
				if (!bDepsOk)
				{
					UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Dependencies still missing after pip install. Install manually: pip install httpx mcp python-dotenv"));
					return;
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: pip install failed (code %d). Install manually: pip install httpx mcp python-dotenv"), PipReturnCode);
				return;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: requirements.txt not found at %s"), *RequirementsPath);
			return;
		}
	}
	if (!bDepsOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Python dependencies missing. Install with: pip install httpx mcp python-dotenv"));
		return;
	}

	// Save current env and set for child (cmd then python inherit on Windows)
	FString SavedPort = FPlatformMisc::GetEnvironmentVariable(TEXT("PORT"));
	FString SavedUE5RestApi = FPlatformMisc::GetEnvironmentVariable(TEXT("UE5_REST_API_URL"));
	FPlatformMisc::SetEnvironmentVar(TEXT("PORT"), *FString::FromInt(Port));
	FPlatformMisc::SetEnvironmentVar(TEXT("UE5_REST_API_URL"), *FString::Printf(TEXT("http://localhost:%d"), Settings->ServerPort));

	// Launch via cmd.exe; python runs in background (no console window)
	FString CmdExe = FPlatformMisc::GetEnvironmentVariable(TEXT("COMSPEC"));
	if (CmdExe.IsEmpty())
	{
		CmdExe = TEXT("cmd.exe");
	}
	// /c "python_or_path mcp_server.py" — quote Python path if it contains spaces
	FString CmdParams = FString::Printf(TEXT("/c \"\"%s\" mcp_server.py\""), *PythonPath);

	uint32 ProcessID = 0;
	PythonMCPProcessHandle = FPlatformProcess::CreateProc(
		*CmdExe,
		*CmdParams,
		true,  // bLaunchDetached
		true,  // bLaunchHidden = true so runs in background
		false, // bLaunchReallyHidden
		&ProcessID,
		0,     // PriorityModifier
		*WorkingDir,
		nullptr,
		nullptr);

	// Restore env
	FPlatformMisc::SetEnvironmentVar(TEXT("PORT"), SavedPort.IsEmpty() ? nullptr : *SavedPort);
	FPlatformMisc::SetEnvironmentVar(TEXT("UE5_REST_API_URL"), SavedUE5RestApi.IsEmpty() ? nullptr : *SavedUE5RestApi);

	if (!PythonMCPProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Failed to start Python MCP via cmd (Python: %s)"), *PythonPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UE5 MCP: Started Python MCP on port %d via cmd (PID %u)"), Port, ProcessID);
	}
}

void FUE5_MCPModule::StopPythonMCP()
{
	if (PythonMCPProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(PythonMCPProcessHandle, true);
		FPlatformProcess::CloseProc(PythonMCPProcessHandle);
		PythonMCPProcessHandle = FProcHandle();
	}
}

void FUE5_MCPModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FUE5_MCPStyle::Initialize();
	FUE5_MCPStyle::ReloadTextures();

	FUE5_MCPCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FUE5_MCPCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FUE5_MCPModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUE5_MCPModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UE5_MCPTabName, FOnSpawnTab::CreateRaw(this, &FUE5_MCPModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FUE5_MCPTabTitle", "UE5_MCP"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "UE5 MCP",
			LOCTEXT("UE5MCPSettingsName", "UE5 MCP"),
			LOCTEXT("UE5MCPSettingsDescription", "Configure the MCP HTTP server port and auto-start on editor launch."),
			GetMutableDefault<UUE5MCPSettings>());
	}
#endif

	const UUE5MCPSettings* Settings = GetDefault<UUE5MCPSettings>();
	if (Settings->bAutoStartServer)
	{
		const int32 Port = Settings->ServerPort;
		if (Port >= 1 && Port <= 65535)
		{
			TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Port);
			Router::Bind(Router);
			FHttpServerModule::Get().StartAllListeners();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UE5 MCP: Invalid ServerPort %d (must be 1-65535), server not started."), Port);
		}
	}

	TryStartPythonMCP();
}


void FUE5_MCPModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	StopPythonMCP();
	FHttpServerModule::Get().StopAllListeners();

#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "UE5 MCP");
	}
#endif

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FUE5_MCPStyle::Shutdown();

	FUE5_MCPCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UE5_MCPTabName);
}

TSharedRef<SDockTab> FUE5_MCPModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedPtr<SSpinBox<int32>> PortSpinBox;
	
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
	
				+ SVerticalBox::Slot()
				.Padding(5)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Port:"))
				]
	
				// Number box
				+ SVerticalBox::Slot()
				.Padding(5)
				[
					SAssignNew(PortSpinBox, SSpinBox<int32>)
					.MinValue(1)          // valid port range
					.MaxValue(65535)
					.Value(GetDefault<UUE5MCPSettings>()->ServerPort)
				]
	
				// Button
				+ SVerticalBox::Slot()
				.Padding(5)
				[
					SNew(SButton)
					.Text(FText::FromString("Run Server"))
					.OnClicked_Lambda([PortSpinBox]() -> FReply
					{
						if (PortSpinBox.IsValid())
						{
							int32 Port = PortSpinBox->GetValue();
						
							if (Port <= 0 || Port > 65535)
							{
								UE_LOG(LogTemp, Error, TEXT("Invalid port number: %d"), Port);
							}
							else
							{
								auto Router = FHttpServerModule::Get().GetHttpRouter(Port);
								Router::Bind(Router);
								FHttpServerModule::Get().StartAllListeners();
							}
						}
						return FReply::Handled();
					})
				]
	
				+ SVerticalBox::Slot()
				.Padding(5)
				[
					SNew(SButton)
					.Text(FText::FromString("Stop Server"))
					.OnClicked_Lambda([PortSpinBox]() -> FReply
					{
						FHttpServerModule::Get().StopAllListeners();
						return FReply::Handled();
					})
				]
			]
		];
	
}

void FUE5_MCPModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(UE5_MCPTabName);
}

void FUE5_MCPModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FUE5_MCPCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FUE5_MCPCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUE5_MCPModule, UE5_MCP)