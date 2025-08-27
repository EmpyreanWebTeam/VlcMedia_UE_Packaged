namespace UnrealBuildTool.Rules
{
    using System.IO;

    public class VlcMedia : ModuleRules
    {
        public VlcMedia(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            DynamicallyLoadedModuleNames.AddRange(new string[] {
                "Media",
            });

            PrivateDependencyModuleNames.AddRange(new string[] {
                "Core",
                "CoreUObject",
                "MediaUtils",
                "Projects",
                "RenderCore",
                "VlcMediaFactory",
                "MediaAssets",
            });

            PrivateIncludePathModuleNames.AddRange(new string[] {
                "Media",
            });

            PrivateIncludePaths.AddRange(new string[] {
                "VlcMedia/Private",
                "VlcMedia/Private/Player",
                "VlcMedia/Private/Shared",
                "VlcMedia/Private/Vlc",
            });

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                // Folder inside the plugin: Plugins/VlcMedia/ThirdParty/vlc/Win64
                string VlcRoot = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "vlc", "Win64");

                // Delay-load the VLC runtime (so it’s only resolved when needed)
                PublicDelayLoadDLLs.AddRange(new[] { "libvlc.dll", "libvlccore.dll" });

                // Stage runtime files so they are copied into packaged builds & plugin packages
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlc.dll"),     StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlccore.dll"), StagedFileType.NonUFS);

                // Recursively include ALL VLC plugins subfolders (important)
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "plugins", "**"),  StagedFileType.NonUFS);
            }

            // Optional Linux/Mac support (uncomment and mirror the pattern above)
            /*
            else if (Target.Platform == UnrealTargetPlatform.Linux)
            {
                string VlcRoot = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "vlc", "Linux");
                PublicDelayLoadDLLs.AddRange(new[] { "libvlc.so", "libvlccore.so" });
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlc.so"),      StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlccore.so"),  StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "plugins", "**"),  StagedFileType.NonUFS);
            }
            else if (Target.Platform == UnrealTargetPlatform.Mac)
            {
                string VlcRoot = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "vlc", "Mac");
                PublicDelayLoadDLLs.AddRange(new[] { "libvlc.dylib", "libvlccore.dylib" });
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlc.dylib"),     StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "libvlccore.dylib"), StagedFileType.NonUFS);
                RuntimeDependencies.Add(Path.Combine(VlcRoot, "plugins", "**"),    StagedFileType.NonUFS);
            }
            */

            // If your VLC wrapper needs them:
            // bEnableExceptions = true;
            // bUseRTTI = true;
        }
    }
}
