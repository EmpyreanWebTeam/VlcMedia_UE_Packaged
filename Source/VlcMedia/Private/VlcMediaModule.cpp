// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IVlcMediaModule.h"
#include "VlcMediaPrivate.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"        // SetEnvironmentVar
#include "HAL/PlatformProcess.h"     // CreateProc, GetDllHandle, PushDllDirectory
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "Containers/StringConv.h"    // FTCHARToUTF8
#include "Templates/UniquePtr.h"

#include "Vlc.h"
#include "VlcMediaPlayer.h"

DEFINE_LOG_CATEGORY(LogVlcMedia);

#define LOCTEXT_NAMESPACE "FVlcMediaModule"

#if PLATFORM_WINDOWS
// ---------- Helpers ----------

static FString PSEscape(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT("'"), TEXT("''"));
	return Out;
}

// Expand Resources/_runtime/UnzipThis.zip to ExtractDir (Saved/VlcMedia/vlc/Win64)
static bool ExpandZipWithPowershell(const FString& Zip, const FString& ExtractDir)
{
	IFileManager::Get().MakeDirectory(*ExtractDir, /*Tree*/ true);

	const FString Cmd = FString::Printf(
		TEXT("-NoProfile -NonInteractive -ExecutionPolicy Bypass ")
		TEXT("-Command \"Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\""),
		*PSEscape(Zip), *PSEscape(ExtractDir)
	);

	FProcHandle Proc = FPlatformProcess::CreateProc(TEXT("powershell.exe"), *Cmd, true, false, false, nullptr, 0, nullptr, nullptr);
	if (!Proc.IsValid())
	{
		UE_LOG(LogVlcMedia, Error, TEXT("Failed to launch PowerShell for Expand-Archive."));
		return false;
	}

	FPlatformProcess::WaitForProc(Proc);
	int32 ReturnCode = 1;
	FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
	FPlatformProcess::CloseProc(Proc);

	return ReturnCode == 0;
}

// If zip extracts into ExtractDir/UnzipThis/*, move contents up to ExtractDir and remove wrapper
static void FlattenUnzipThisWrapper(const FString& ExtractDir)
{
	const FString WrapperDir = FPaths::Combine(ExtractDir, TEXT("UnzipThis"));
	if (!IFileManager::Get().DirectoryExists(*WrapperDir))
	{
		return; // nothing to flatten
	}

	UE_LOG(LogVlcMedia, Display, TEXT("Flattening UnzipThis wrapper: %s -> %s"), *WrapperDir, *ExtractDir);

	// Collect all files under WrapperDir
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *WrapperDir, TEXT("*.*"), true, false, false);

	// Move each file relative to WrapperDir up into ExtractDir
	FString WrapperNorm = WrapperDir;
	FPaths::NormalizeDirectoryName(WrapperNorm);

	for (const FString& SrcFileRaw : Files)
	{
		FString SrcFile = SrcFileRaw;
		FPaths::NormalizeFilename(SrcFile);

		FString Rel = SrcFile;
		Rel.RemoveFromStart(WrapperNorm + TEXT("/")); // safe after Normalize*

		const FString DstFile = FPaths::Combine(ExtractDir, Rel);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DstFile), /*Tree*/ true);

		// Move (overwrite if exists, even if read-only)
		const bool bMoved = IFileManager::Get().Move(*DstFile, *SrcFile, /*Replace*/ true, /*EvenIfReadOnly*/ true, /*Attributes*/ false, /*bDoNotRetryOrError*/ false);
		if (!bMoved)
		{
			UE_LOG(LogVlcMedia, Warning, TEXT("Failed to move file from %s to %s"), *SrcFile, *DstFile);
		}
	}

	// Remove the wrapper directory
	IFileManager::Get().DeleteDirectory(*WrapperDir, /*RequireExists*/ false, /*Tree*/ true);
}
#endif // PLATFORM_WINDOWS


/**
 * Implements the VlcMedia module.
 */
class FVlcMediaModule : public IVlcMediaModule
{
public:
	FVlcMediaModule()
		: Initialized(false)
	{ }

public:
	//~ IVlcMediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!Initialized)
		{
			return nullptr;
		}

		return MakeShared<FVlcMediaPlayer, ESPMode::ThreadSafe>(EventSink, VlcInstance);
	}

public:
	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		// bootstrap LibVLC glue (UE side)
		if (!FVlc::Initialize())
		{
			UE_LOG(LogVlcMedia, Error, TEXT("Failed to initialize LibVLC bootstrap"));
			return;
		}

		UE_LOG(LogVlcMedia, Log, TEXT("Initialized LibVLC %s (%s - %s)"),
			ANSI_TO_TCHAR(FVlc::GetVersion()),
			ANSI_TO_TCHAR(FVlc::GetChangeset()),
			ANSI_TO_TCHAR(FVlc::GetCompiler())
		);

#if UE_BUILD_DEBUG
		// backup old log file
		const FString LogFilePath = FPaths::Combine(FPaths::ProjectLogDir(), TEXT("vlc.log"));
		FOutputDeviceFile::CreateBackupCopy(*LogFilePath);
		IFileManager::Get().Delete(*LogFilePath);
#endif

		const auto Settings = GetDefault<UVlcMediaSettings>();

		// Default plugins path (Saved) in case non-Windows or nothing found
		FString FinalPluginsDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VlcMedia/vlc/Win64/plugins"));

// ------------------------------ RUNTIME SELF-INSTALL (Win64) ------------------------------
#if PLATFORM_WINDOWS
	UE_LOG(LogVlcMedia, Display, TEXT("==== VlcMedia Startup (UE %s) ===="), *FEngineVersion::Current().ToString());

	FString PluginBaseDir;
	if (TSharedPtr<IPlugin> P = IPluginManager::Get().FindPlugin(TEXT("VlcMedia")))
	{
		PluginBaseDir = P->GetBaseDir();
	}
	UE_LOG(LogVlcMedia, Display, TEXT("Plugin base dir: %s"), *PluginBaseDir);

	const FString ZipPath = FPaths::Combine(PluginBaseDir, TEXT("Resources/_runtime/UnzipThis.zip"));

	// saved extract location (default)
	const FString SavedExtractDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VlcMedia/vlc/Win64"));
	// plugin-root location
	const FString PluginRootDir = PluginBaseDir;
	// plugin ThirdParty expected location
	const FString PluginTPDir = FPaths::Combine(PluginBaseDir, TEXT("ThirdParty"), TEXT("vlc"), TEXT("Win64"));

	UE_LOG(LogVlcMedia, Display, TEXT("ZipPath: %s"), *ZipPath);
	UE_LOG(LogVlcMedia, Display, TEXT("SavedExtractDir: %s"), *SavedExtractDir);
	UE_LOG(LogVlcMedia, Display, TEXT("PluginRootDir: %s"), *PluginRootDir);
	UE_LOG(LogVlcMedia, Display, TEXT("PluginTPDir: %s"), *PluginTPDir);

	// Check Saved, plugin root, and plugin ThirdParty for an existing runtime
	const bool bHaveLibVLC_Saved      = FPaths::FileExists(FPaths::Combine(SavedExtractDir, TEXT("libvlc.dll")));
	const bool bHaveCore_Saved        = FPaths::FileExists(FPaths::Combine(SavedExtractDir, TEXT("libvlccore.dll")));
	const bool bHavePluginsDir_Saved  = IFileManager::Get().DirectoryExists(*FPaths::Combine(SavedExtractDir, TEXT("plugins")));

	const bool bHaveLibVLC_PluginRoot = FPaths::FileExists(FPaths::Combine(PluginRootDir, TEXT("libvlc.dll")));
	const bool bHaveCore_PluginRoot   = FPaths::FileExists(FPaths::Combine(PluginRootDir, TEXT("libvlccore.dll")));
	const bool bHavePluginsDir_PluginRoot = IFileManager::Get().DirectoryExists(*FPaths::Combine(PluginRootDir, TEXT("plugins")));

	const bool bHaveLibVLC_PluginTP   = FPaths::FileExists(FPaths::Combine(PluginTPDir, TEXT("libvlc.dll")));
	const bool bHaveCore_PluginTP     = FPaths::FileExists(FPaths::Combine(PluginTPDir, TEXT("libvlccore.dll")));
	const bool bHavePluginsDir_PluginTP = IFileManager::Get().DirectoryExists(*FPaths::Combine(PluginTPDir, TEXT("plugins")));

	UE_LOG(LogVlcMedia, Display, TEXT("Pre-extract check -> saved(libvlc:%d libvlccore:%d plugins:%d) pluginRoot(libvlc:%d libvlccore:%d plugins:%d) pluginTP(libvlc:%d libvlccore:%d plugins:%d)"),
		bHaveLibVLC_Saved ? 1 : 0, bHaveCore_Saved ? 1 : 0, bHavePluginsDir_Saved ? 1 : 0,
		bHaveLibVLC_PluginRoot ? 1 : 0, bHaveCore_PluginRoot ? 1 : 0, bHavePluginsDir_PluginRoot ? 1 : 0,
		bHaveLibVLC_PluginTP ? 1 : 0, bHaveCore_PluginTP ? 1 : 0, bHavePluginsDir_PluginTP ? 1 : 0
	);

	// Decide where we will load runtime from. Default to SavedExtractDir; if nothing is in Saved but plugin root has runtime, use plugin root.
	FString ExtractDir = SavedExtractDir;
	FString PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));

	const bool bSavedComplete = bHaveLibVLC_Saved && bHaveCore_Saved && bHavePluginsDir_Saved;
	const bool bPluginComplete = bHaveLibVLC_PluginRoot && bHaveCore_PluginRoot && bHavePluginsDir_PluginRoot;
	const bool bPluginTPComplete = bHaveLibVLC_PluginTP && bHaveCore_PluginTP && bHavePluginsDir_PluginTP;

	bool bNeedExtract = !bSavedComplete;

	if (bNeedExtract)
	{
		if (!FPaths::FileExists(ZipPath))
		{
			// If the zip is missing but plugin root already contains the runtime, use that
			if (bPluginTPComplete)
			{
				ExtractDir = PluginTPDir;
				PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));
				UE_LOG(LogVlcMedia, Display, TEXT("Using runtime found in plugin ThirdParty: %s"), *PluginTPDir);
				bNeedExtract = false;
			}
			else if (bPluginComplete)
			{
				ExtractDir = PluginRootDir;
				PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));
				UE_LOG(LogVlcMedia, Display, TEXT("Using runtime found in plugin root: %s"), *PluginRootDir);
				bNeedExtract = false;
			}
			else
			{
				UE_LOG(LogVlcMedia, Error, TEXT("VLC runtime zip NOT FOUND at: %s"), *ZipPath);
			}
		}
		else
		{
			// Try extracting into the plugin ThirdParty first (so packaged plugin gets a usable runtime at runtime)
			bool bExtractedToPluginTP = false;
			if (IFileManager::Get().IsReadOnly(*PluginTPDir) == false) // quick hint, will create dir below anyway
			{
				UE_LOG(LogVlcMedia, Display, TEXT("Attempting to extract VLC runtime into plugin ThirdParty: %s"), *PluginTPDir);
				if (ExpandZipWithPowershell(ZipPath, PluginTPDir))
				{
					// If the zip had a nested UnzipThis wrapper, flatten it
					FlattenUnzipThisWrapper(PluginTPDir);

					const bool bNowLib  = FPaths::FileExists(FPaths::Combine(PluginTPDir, TEXT("libvlc.dll")));
					const bool bNowCore = FPaths::FileExists(FPaths::Combine(PluginTPDir, TEXT("libvlccore.dll")));
					const bool bNowPlug = IFileManager::Get().DirectoryExists(*FPaths::Combine(PluginTPDir, TEXT("plugins")));

					if (bNowLib && bNowCore && bNowPlug)
					{
						UE_LOG(LogVlcMedia, Display, TEXT("Extracted VLC runtime into plugin ThirdParty. Post-extract -> libvlc:%d libvlccore:%d plugins:%d"),
							bNowLib ? 1 : 0, bNowCore ? 1 : 0, bNowPlug ? 1 : 0);

						ExtractDir = PluginTPDir;
						PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));
						bExtractedToPluginTP = true;
						bNeedExtract = false;
					}
					else
					{
						UE_LOG(LogVlcMedia, Warning, TEXT("Extraction to plugin ThirdParty did not produce expected files; falling back to Saved."));
					}
				}
				else
				{
					UE_LOG(LogVlcMedia, Warning, TEXT("Expand-Archive into plugin ThirdParty FAILED -> %s"), *PluginTPDir);
				}
			}

			// If extraction into plugin ThirdParty wasn't done/failed, fall back to SavedExtractDir
			if (!bExtractedToPluginTP)
			{
				UE_LOG(LogVlcMedia, Display, TEXT("Extracting VLC runtime into Saved: %s"), *SavedExtractDir);
				if (!ExpandZipWithPowershell(ZipPath, SavedExtractDir))
				{
					UE_LOG(LogVlcMedia, Error, TEXT("Expand-Archive FAILED -> %s"), *SavedExtractDir);
				}
				else
				{
					FlattenUnzipThisWrapper(SavedExtractDir);

					const bool bNowLib  = FPaths::FileExists(FPaths::Combine(SavedExtractDir, TEXT("libvlc.dll")));
					const bool bNowCore = FPaths::FileExists(FPaths::Combine(SavedExtractDir, TEXT("libvlccore.dll")));
					const bool bNowPlug = IFileManager::Get().DirectoryExists(*FPaths::Combine(SavedExtractDir, TEXT("plugins")));

					UE_LOG(LogVlcMedia, Display, TEXT("Extracted VLC runtime to Saved. Post-extract -> libvlc:%d libvlccore:%d plugins:%d"),
						bNowLib ? 1 : 0, bNowCore ? 1 : 0, bNowPlug ? 1 : 0);
				}
			}
		}
	}

	// If plugin ThirdParty contains the runtime, prefer it. Fallback to plugin root if present.
	if (bPluginTPComplete)
	{
		ExtractDir = PluginTPDir;
		PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));
	}
	else if (bPluginComplete)
	{
		ExtractDir = PluginRootDir;
		PluginsDir = FPaths::Combine(ExtractDir, TEXT("plugins"));
	}

	// Expose chosen plugins path for later argv construction and lib loading
	FinalPluginsDir = PluginsDir;

	// Load from chosen ExtractDir and point VLC at plugins
	UE_LOG(LogVlcMedia, Display, TEXT("Final runtime dir chosen: %s"), *ExtractDir);
	FPlatformProcess::PushDllDirectory(*ExtractDir);

	void* VLCCore = FPlatformProcess::GetDllHandle(*FPaths::Combine(ExtractDir, TEXT("libvlccore.dll")));
	UE_LOG(LogVlcMedia, Display, TEXT("Load libvlccore -> %s"), VLCCore ? TEXT("OK") : TEXT("FAIL"));

	void* VLC = FPlatformProcess::GetDllHandle(*FPaths::Combine(ExtractDir, TEXT("libvlc.dll")));
	UE_LOG(LogVlcMedia, Display, TEXT("Load libvlc -> %s"), VLC ? TEXT("OK") : TEXT("FAIL"));

	FPlatformMisc::SetEnvironmentVar(TEXT("VLC_PLUGIN_PATH"), *PluginsDir);
	UE_LOG(LogVlcMedia, Display, TEXT("VLC_PLUGIN_PATH=%s"), *PluginsDir);
#endif
// ------------------------------------------------------------------------------------------

		// ---------------------- Build argv for libvlc_new ----------------------
		TArray<FString> ArgStrings;

		// caching
		ArgStrings.Add(FString::Printf(TEXT("--disc-caching=%d"),    (int32)Settings->DiscCaching.GetTotalMilliseconds()));
		ArgStrings.Add(FString::Printf(TEXT("--file-caching=%d"),    (int32)Settings->FileCaching.GetTotalMilliseconds()));
		ArgStrings.Add(FString::Printf(TEXT("--live-caching=%d"),    (int32)Settings->LiveCaching.GetTotalMilliseconds()));
		ArgStrings.Add(FString::Printf(TEXT("--network-caching=%d"), (int32)Settings->NetworkCaching.GetTotalMilliseconds()));

		// config / logging
		ArgStrings.Add(TEXT("--ignore-config"));

#if UE_BUILD_DEBUG
		ArgStrings.Add(TEXT("--file-logging"));
		ArgStrings.Add(FString::Printf(TEXT("--logfile=%s"), *FPaths::Combine(FPaths::ProjectLogDir(), TEXT("vlc.log"))));
#endif

#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
		ArgStrings.Add(TEXT("--verbose=2"));
#else
		ArgStrings.Add(TEXT("--quiet"));
#endif

		// output
		ArgStrings.Add(TEXT("--aout")); ArgStrings.Add(TEXT("amem"));
		ArgStrings.Add(TEXT("--intf")); ArgStrings.Add(TEXT("dummy"));
		ArgStrings.Add(TEXT("--text-renderer")); ArgStrings.Add(TEXT("dummy"));
		ArgStrings.Add(TEXT("--vout")); ArgStrings.Add(TEXT("vmem"));

		// performance / features
		ArgStrings.Add(TEXT("--drop-late-frames"));
		ArgStrings.Add(TEXT("--no-disable-screensaver"));
		ArgStrings.Add(TEXT("--no-plugins-cache"));
		ArgStrings.Add(TEXT("--no-snapshot-preview"));
		ArgStrings.Add(TEXT("--no-video-title-show"));

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		ArgStrings.Add(TEXT("--no-stats"));
#endif

#if PLATFORM_LINUX
		ArgStrings.Add(TEXT("--no-xlib"));
#endif

#if PLATFORM_WINDOWS
		// Always point VLC at the extracted plugins (editor + packaged)
		ArgStrings.Add(FString::Printf(TEXT("--plugin-path=%s"), *FinalPluginsDir));
#endif

		// Convert to UTF-8 and call libvlc_new
		TArray<TUniquePtr<FTCHARToUTF8>> Converters;
		Converters.Reserve(ArgStrings.Num());

		TArray<const ANSICHAR*> Argv;
		Argv.Reserve(ArgStrings.Num());

		for (const FString& S : ArgStrings)
		{
			TUniquePtr<FTCHARToUTF8> Conv = MakeUnique<FTCHARToUTF8>(*S);
			Argv.Add(Conv->Get());
			Converters.Add(MoveTemp(Conv)); // keep storage alive
		}

		const int Argc = Argv.Num();
		VlcInstance = FVlc::New(Argc, Argv.GetData());

		if (VlcInstance == nullptr)
		{
			UE_LOG(LogVlcMedia, Warning, TEXT("Failed to create VLC instance (%s)"), ANSI_TO_TCHAR(FVlc::Errmsg()));
			FVlc::Shutdown();
			return;
		}

		// register logging callback
		FVlc::LogSet(VlcInstance, &FVlcMediaModule::HandleVlcLog, nullptr);

		Initialized = true;
	}

	virtual void ShutdownModule() override
	{
		if (!Initialized)
		{
			return;
		}

		Initialized = false;

		// unregister logging callback
		FVlc::LogUnset(VlcInstance);

		// release LibVLC instance
		FVlc::Release((FLibvlcInstance*)VlcInstance);
		VlcInstance = nullptr;

		// shut down LibVLC
		FVlc::Shutdown();
	}

private:
	/** Handles log messages from LibVLC. */
	static void HandleVlcLog(void* /*Data*/, ELibvlcLogLevel Level, FLibvlcLog* Context, const char* Format, va_list Args)
	{
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
		const auto Settings = GetDefault<UVlcMediaSettings>();

		// filter unwanted messages
		if ((uint8)Level < (uint8)Settings->LogLevel)
		{
			return;
		}

		FString LogContext;

		// get context information
		if (Context != nullptr)
		{
			const char* Module = nullptr;
			const char* File = nullptr;
			unsigned Line = 0;

			FVlc::LogGetContext(Context, &Module, &File, &Line);
			LogContext = FString::Printf(TEXT("%s: "), (Module != nullptr) ? ANSI_TO_TCHAR(Module) : TEXT("unknown module"));

			if (Settings->ShowLogContext)
			{
				LogContext += FString::Printf(TEXT("%s, line %s: "),
					(File != nullptr) ? ANSI_TO_TCHAR(File) : TEXT("unknown file"),
					(Line != 0) ? *FString::Printf(TEXT("%i"), Line) : TEXT("n/a")
				);
			}
		}
		else
		{
			LogContext = TEXT("generic: ");
		}

		// forward message to log
		ANSICHAR Message[1024];
		FCStringAnsi::GetVarArgs(Message, UE_ARRAY_COUNT(Message), Format, Args);

		switch (Level)
		{
		case ELibvlcLogLevel::Debug:
			UE_LOG(LogVlcMedia, VeryVerbose, TEXT("%s%s"), *LogContext, ANSI_TO_TCHAR(Message));
			break;

		case ELibvlcLogLevel::Error:
			UE_LOG(LogVlcMedia, Error, TEXT("%s%s"), *LogContext, ANSI_TO_TCHAR(Message));
			break;

		case ELibvlcLogLevel::Notice:
			UE_LOG(LogVlcMedia, Verbose, TEXT("%s%s"), *LogContext, ANSI_TO_TCHAR(Message));
			break;

		case ELibvlcLogLevel::Warning:
			UE_LOG(LogVlcMedia, Warning, TEXT("%s%s"), *LogContext, ANSI_TO_TCHAR(Message));
			break;

		default:
			UE_LOG(LogVlcMedia, Log, TEXT("%s%s"), *LogContext, ANSI_TO_TCHAR(Message));
			break;
		}
#endif
	}

private:
	bool Initialized = false;
	FLibvlcInstance* VlcInstance = nullptr;
};

IMPLEMENT_MODULE(FVlcMediaModule, VlcMedia);

#undef LOCTEXT_NAMESPACE
