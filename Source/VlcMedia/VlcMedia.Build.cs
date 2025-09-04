using UnrealBuildTool;
using System.IO;

public class VlcMedia : ModuleRules
{
    public VlcMedia(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        DynamicallyLoadedModuleNames.AddRange(new[] { "Media" });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core","CoreUObject","MediaUtils","Projects","RenderCore",
            "VlcMediaFactory","MediaAssets"
        });

        PrivateIncludePathModuleNames.AddRange(new[] { "Media" });

        PrivateIncludePaths.AddRange(new[]
        {
            "VlcMedia/Private",
            "VlcMedia/Private/Player",
            "VlcMedia/Private/Shared",
            "VlcMedia/Private/Vlc",
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string Base = "$(PluginDir)/ThirdParty/vlc/Win64";

            // Copy these into the packaged plugin/game
            RuntimeDependencies.Add($"{Base}/libvlc.dll", StagedFileType.NonUFS);
            RuntimeDependencies.Add($"{Base}/libvlccore.dll", StagedFileType.NonUFS);
            RuntimeDependencies.Add($"{Base}/plugins/**", StagedFileType.NonUFS);

            PublicDelayLoadDLLs.Add("libvlc.dll");
            PublicDelayLoadDLLs.Add("libvlccore.dll");
        }

    }
}
