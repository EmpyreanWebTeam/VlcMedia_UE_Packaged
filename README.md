![License: Proprietary](https://img.shields.io/badge/License-Proprietary-red.svg)
![No Redistribution](https://img.shields.io/badge/Redistribution-Prohibited-critical)
![UE5.4 Supported](https://img.shields.io/badge/UE5.4-Supported-brightgreen)


# 📺 VlcMedia Plugin for Unreal Engine [WIP]

This plugin enables Unreal Engine to stream video content using VLC Media Player functionality. Designed for Blueprint-only projects, it includes everything you need to drop in and start playing streaming `.m3u8` sources with no source compilation required.

---

## ✅ Features

- Stream `.m3u8` and live video URLs in UE 5.4+
- Auto-play support via included `BP_TV` Blueprint
- Packaged plugin — no C++ compilation needed
- Integrates MediaPlayer, Texture, and Material setup automatically

---

✅ Supported Platforms & Requirements

The VlcMedia (libVLC) Plugin for Unreal Engine is currently focused on stable Windows x64 support. Additional platforms may be added in the future.

🖥️ Supported Operating Systems
Platform	Status	Notes
Windows 10 / Windows 11 (64-bit)	✅ Officially Supported	Fully tested in Editor + Packaged builds. Recommended.

macOS	❌ Not Supported	No prebuilt libVLC binaries provided; untested.

Linux	⚠️ Not Supported	May work with custom libVLC builds, but not tested.

Android	❌ Not Supported	Requires separate libVLC integration (not yet implemented).

iOS	❌ Not Supported	Unreal + libVLC on iOS requires significant additional work.

🧩 Supported Unreal Engine Versions
Unreal Version	Status	Notes
5.4.4	✅ Primary Target	Latest precompiled release is built for 5.4.4.

5.2.1	⚠️ Legacy Support	Older source tag available but no longer actively developed.

Other UE versions	❌ Unsupported	No active builds or testing outside 5.4.4.
🎥 VLC / libVLC Requirements

A 64-bit VLC installation is required on Windows
The plugin dynamically loads:

libvlc.dll

libvlccore.dll

If VLC is not installed (or the 32-bit version is installed), Unreal will not be able to initialize the media player.

📦 Distribution Type

Precompiled Plugin (Recommended)
Download from Releases → drop into
YourProject/Plugins/VlcMedia/.

Source Version (5.2.1 Only)
Provided for historical/compatibility reasons; not maintained.

📝 Summary

The current official target configuration is:

Unreal Engine 5.4.4 + Windows 10/11 64-bit + VLC x64 installed

All other platforms are considered untested or unsupported for now.


## 📦 Installation
1. Copy (or clone) the **VlcMedia** folder into your project’s `Plugins/` directory:

```
YourProject/
├─ Plugins/
│  └─ VlcMedia/
│     ├─ Binaries/
│     │  └─ Win64/…            # precompiled plugin binaries
│     ├─ Config/               # optional
│     ├─ Content/
│     │  ├─ Blueprints/
│     │  │  └─ BP_VlcTV.uasset
│     │  ├─ Media/
│     │  │  ├─ MP_VlcTV.uasset       # Media Player
│     │  │  └─ MT_VlcTV.uasset       # Media Texture
│     │  ├─ Materials/
│     │  │  └─ M_Video_Unlit_Rot.uasset
│     │  ├─ Icons/ (optional)
│     │  └─ Widgets/ (optional)
│     ├─ Resources/
│     │  └─ Icon128.png
│     ├─ VlcMedia.uplugin
│     └─ README.md
```

2. Open (or restart) your project.
3. Enable the plugin if it isn’t auto-loaded.

> **Note**  
> - If you plan to package **local files**, put them under `Content/Movies/` in **your project** (not inside the plugin).  
> - If your build of the plugin is *content-only*, the `Binaries/` folder may not be present; that’s fine.

---

## 🧠 Setup Guide

This setup follows [this tutorial](https://www.youtube.com/watch?v=nNNzUf3zNjM&t=2s) from Timo Helmers for Blueprint-based media playback using streaming URLs.

### 1. Included Blueprint: `BP_TV`

Drag the `BP_TV` Blueprint into your level. It auto-plays video on BeginPlay using an index from a predefined Media Source array.

#### 🔧 To customize:
- Open `BP_TV`
- Replace the default entries in the `ChannelList` array with your own `Stream Media Source` assets.
- Adjust the `ChannelIndex` integer variable to select which one auto-plays.

### 2. Blueprint Logic

The following Blueprint handles media loading and playback:

![TV Blueprint](https://raw.githubusercontent.com/Jon1969Edwards/VlcMedia_UnrealEngine/main/docs/BP_TV_AutoPlay.png)

---

## 📌 Notes

- If playback fails, check that:
- Your `.m3u8` or stream URL is public and working
- VLC is installed and supported (used under-the-hood)
- Media Source is properly assigned

- StreamMediaSources used in Blueprints must be stored in the main `Content/` folder (not inside the plugin) unless manually linked.

---

## 🧪 Example Streams (For Testing)

Use any of these publicly accessible streams:
- [NASA TV](https://nasatv-lh.akamaihd.net/i/NASA_101@319270/master.m3u8)
- [DW English](https://dwstream4-lh.akamaihd.net/i/dwstream4_live@123456/master.m3u8)

> ⚠️ Some streams may block cross-origin requests. Use trusted sources for production.


## Documentation
[Wiki documentation](https://github.com/Jon1969Edwards/VlcMedia_UnrealEngine/wiki/%F0%9F%8F%A0-Home)

## Support
- For issues, please open an issue on the plugin's GitHub page or contact the author.
- Check the [Wiki documentation](https://github.com/Jon1969Edwards/VlcMedia_UnrealEngine/wiki/%F0%9F%8F%A0-Home)
- Check the [VideoLAN documentation](https://www.videolan.org/doc/) for supported formats.

## Credits
- Developed by Jon Edwards

- Special thanks to Charles (age 9) for testing + feature ideas 🎮

- Uses VLC backend via VideoLAN / libVLC

- <a href='https://ko-fi.com/Z8Z81F4OEC' target='_blank'><img height='36' style='border:0px;height:36px;' src='https://storage.ko-fi.com/cdn/kofi6.png?v=6' border='0' alt='Buy Me a Beer at ko-fi.com' /></a>
