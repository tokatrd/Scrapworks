# AGENTS.md

Project: `lovense-shenanigans`
Location: `/home/toka/Documents/Projects/lovense-shenanigans`

## Purpose

Lovense OBS Toolset - Windows OBS plugins converted to Linux compatibility.

## Component Status

| Component | Linux Status | Notes |
|-----------|--------------|-------|
| obs-multi-rtmp | **Skipped** | Native Linux version exists |
| lovense-obs-webcam | **Converted** | V4L2 camera support added |
| lovense-obs-websocket | **Converted** | V4L2 camera source ID added |
| lovense-smart-object | **Not Converted** | Heavy dlib/OpenCV deps, needs Linux libs |
| lovense-relay-server | **Not Converted** | DirectShow/IPC Windows-only |
| lovense-ai-camera | **Not Converted** | DirectShow is Windows-only |
| lovense-video-freeback | **Not Converted** | CefSharp is Windows-only |

## Converted Components

### lovense-obs-websocket

Changes:
- Added `v4l2_input` camera source ID for Linux (WSRequestHandler_StudioMode.cpp)

Build (OBS 28+):
```bash
cd lovense-toolset-source/lovense-obs-websocket
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Install:
```bash
cmake --install build
```

### lovense-webcam

Changes:
- Added V4L2 camera source ID (`v4l2_input`) for Linux (UntilHelp.cpp)
- Added Linux resource path handling (UntilHelp.cpp)
- Added Linux camera info parsing for V4L2 (UntilHelp.cpp)
- Fixed Qt includes for cross-platform (RemoteControlDialog.cpp)

Build (OBS 28+):
```bash
cd lovense-toolset-source/lovense-webcam
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Not Converted (Requires Significant Work)

### lovense-relay-server
- Depends on DirectShow (Windows-only video capture)
- Uses Windows IPC (Inter-Process Communication)
- Windows-specific process management

### lovense-ai-camera
- Uses DirectShow for video capture
- Windows COM interfaces
- No Linux equivalent without major rewrite

### lovense-video-freeback
- Uses CefSharp (Windows-only Chromium Embedded Framework)
- Windows-specific browser integration

### lovense-smart-object
- Depends on dlib (needs Linux build)
- Depends on OpenCV (needs Linux build)
- ONNX Runtime/TensorRT integration

## Build Prerequisites

### For Converted Components
```bash
# Arch Linux
sudo pacman -S obs-studio qt6-base qt6-svg cmake

# Ubuntu/Debian
sudo apt install obs-studio qt6-base-dev libqt6svg6-dev cmake
```

### For Full Toolset (if building non-converted components)
```bash
# Additional dependencies needed
sudo pacman -S opencv dlib onnxruntime
```

## Development

### Testing Changes
1. Build the component
2. Copy `.so` to OBS plugin directory:
   ```bash
   cp build/*.so ~/.config/obs-studio/plugins/
   ```
3. Restart OBS and verify plugin loads

### Debug Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
