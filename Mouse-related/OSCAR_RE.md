# Oscar Editor — Reverse Engineering & Rewrite Plan

## 1. Binary Profile

| Property | Value |
|---|---|
| **File** | `OscarEditor.exe` |
| **Size** | 3,547,136 bytes (3.4 MB) |
| **Format** | PE32 i386 (Windows GUI) |
| **Linker** | Microsoft Linker v5.0 |
| **Built** | Mon Mar 3 11:42:27 2014 |
| **Language** | Borland C++ Builder (VCL) + STL |
| **PDB** | Stripped to external PDB |
| **Subsystem** | Windows GUI (2) |
| **Image Base** | 0x00400000 |

## 2. Vendor & Product

**A4Tech / Bloody Oscar Editor** — Gaming mouse configuration utility supporting multiple chipset families:

| Chipset | Class | Notes |
|---|---|---|
| X7 | `THidWindow_X7` | Mainline A4Tech gaming |
| KX | `THidWindow_KX` | Bloody K-series |
| GX | `THidWindow_GX` | Bloody G-series |
| LX | `THidWindow_LX` | Bloody L-series |

## 3. HID Communication Protocol

### API Layer (Windows DLL imports found)
```
CreateFileA / CreateFileW      → Open HID device handle
HidD_GetFeature                → Read feature report from device
HidD_SetFeature                → Write feature report to device
WriteFile                      → Output report (interrupt OUT)
ReadFile                       → Input report (interrupt IN)
DeviceIoControl                → IOCTL_HID_* operations
SetupDi*                       → Device enumeration
```

### Key Functions (RTTI names)
| Function | Purpose |
|---|---|
| `HID_EE_Write16` | Write 16-bit value to device EEPROM via HID (called ~280+ times) |
| `@@Hidprocesser@Finalize` | HID processing thread cleanup |
| `@@Hidprocesser@Initialize` | HID processing thread setup |
| `THidThreadManager` | Manages HID background threads per device |
| `THidWindow_*` | Per-device HID communication window |

### HID Transaction Pattern (reconstructed)
```
1. CreateFile("\\?\hid#vid_####&pid_#####")    → device handle
2. HidD_GetFeature(hid, buffer, bufsize)        → read config/status
3. Modify buffer in place
4. HidD_SetFeature(hid, buffer, bufsize)        → write config/command
5. CloseHandle(hid)
```

### Memory/EEPROM Map (from HID_EE_Write16 usage patterns)
The 280+ calls to `HID_EE_Write16` with different parameters suggest a structured EEPROM map:

| Range | Content |
|---|---|
| 0x0000-0x00FF | Device info, serial, VID/PID |
| 0x0100-0x01FF | DPI profiles (5-6 stages) |
| 0x0200-0x02FF | Button mapping (STRUCT_TRIGGER[113]) |
| 0x0300-0x03FF | Macro storage |
| 0x0400-0x04FF | Lighting / OSD config |
| 0x0500-0x05FF | Report rate, LOD, debounce |

*Note: Exact addresses need dynamic analysis with a real device connected.*

## 4. Plugin Architecture (8 DLLs)

| DLL | Purpose |
|---|---|
| `DLL_AnalyzeGesturesInOne.dll` | Single-button gesture recognition |
| `DLL_AnalyzeGesturesInRight.dll` | Right-button gesture recognition |
| `DLL_MouseDeviceManager.dll` | Device connect/disconnect/management |
| `DLL_MouseEventHook.dll` | Global mouse event hooking |
| `DLL_PenSuit.dll` | Pen / drawing tablet input |
| `DLL_ScrollbarControl.dll` | Custom scrollbar rendering |
| `DLL_Wheel4D.dll` | 4-direction tilt wheel control |
| `DLL_ZoomControl.dll` | Zoom gesture control |

All DLLs are Borland C++ Builder compiled, communicate via shared VCL memory (`ShareVar` export).

## 5. Application Architecture

### Core Classes (RTTI-extracted)

```
TApp                          → Application singleton
  ├── TSoftwareSetting        → Persisted settings
  ├── TFunctionEnable         → Feature toggles (GUI, macros, etc.)
  ├── TRunTime                → Runtime state management
  ├── TFileManager            → File I/O (INI, firmware)
  ├── TManageInternet         → FTP/TCP update service
  │    └── Server: ServerClient.5-link.com:50777
  ├── TManageWheel4D          → Tilt wheel manager
  └── TAP_Manage              → Action/macro profile management

THidThreadManager             → HID communication
  ├── THidWindow_KX           → KX chipset
  ├── THidWindow_GX           → GX chipset
  ├── THidWindow_LX           → LX chipset
  └── THidWindow_X7           → X7 chipset

TGesture8in1 / TGesture16in1  → Gesture recognition
TGesturesUserDefine           → User-defined gestures

TMacroContent / TScriptContent → Macro/script engine
AP_DoScript                   → Script execution entry point
AP_ScriptThreadUnit           → Background script runner
```

### Trigger/Button System
- `STRUCT_TRIGGER[113]` — 113 button/event trigger definitions
- `TEventDefine[113]` — Event-to-action mapping array
- `CCompilerTriggerParam[113]` — Compiled macro parameters
- `TMouseEvent` — Mouse event descriptor

### Supported Sensors (DPI)
ADNS6010, ADNS6090, ADNS3060, ADNS3080, ADNS3081, ADNS9500,
ADNS7010, ADNS7530, ADNS7550, PAN3102, PAN3204, PAN3204L, PAN3305, PAN3385

## 6. UI Structure (from INI files + RTTI)

```
Main Window                  → TabControl with:
  ├── Key Configuration      → Button mapping (5+ buttons)
  ├── Advanced Settings      → DPI, report rate, LOD, debounce
  ├── Macro Editor           → Record/edit macros
  ├── Profile Manager        → Save/load profiles (5 profiles)
  ├── OSD Settings           → On-screen display
  ├── Battery                → Wireless mouse battery
  └── Support                → Firmware update, internet

Forms (VCL names):
  - TForm (main)
  - DLLSTRUCT_ReportRateForm
  - DLLSTRUCT_OSDForm
  - DLLSTRUCT_OSDText
  - DLLSTRUCT_Fixed4DPIForm
  - DLLSTRUCT_KeyboardPanel
  - DLLSTRUCT_TextForm
  - DLLSTRUCT_MessageBox / MessageConfirm
  - DLLSTRUCT_UpgradeHint / UpgradeProgress
  - DLLSTRUCT_PenSetForm
  - DLLSTRUCT_Wheel4DEx
  - DLLSTRUCT_MouseGestures
```

## 7. Data Flow

```
User Input (Click/Key/Gesture)
  → THidThreadManager
    → TEventDefine lookup (113 triggers)
      → AP_DoScript (macro/script)
        → HID_EE_Write16 (device communication)
          → HidD_SetFeature → USB → Mouse MCU → EEPROM

INI Files (persistent storage):
  ├── Custom.ini         → User settings, DPI, GUI config
  ├── Main.ini           → DPI profiles per sensor
  ├── FunctionDefine.ini → Device PID mapping
  ├── DefaultScript.ini  → Default macro scripts
  ├── Inform.ini         → App info
  └── Internet.ini       → Update server config
```

## 8. Rewrite Plan

### Language Choice: **Zig**

**Why Zig is "as low level as possible" (short of assembly):**
- No hidden control flow — zero exceptions, zero destructors, no unwinding
- No preprocessor, no hidden stack frames
- Manual memory management — every allocation is explicit
- Direct syscall access via `linux.linux_syscall`
- C ABI compatible — can link directly with `libusb`, `xcb`, etc.
- Single static binary output — no libc dependency with `-freference-trace`
- `comptime` — zero-cost abstractions at compile time, nothing at runtime
- No runtime at all (no Go-style runtime, no Rust-style panicking)

**Alternative considered and rejected:**
- **C**: Viable but lacks comptime, has the preprocessor, and is less safe
- **Rust**: Has panics, unwinding, hidden control flow — violates "as low level as possible"
- **Assembly**: Impractical for a full GUI HID tool — would take years

### Architecture (Linux-native)

```
┌──────────────────────────────────────────────────┐
│                  oscar (Zig binary)               │
├──────────────────────────────────────────────────┤
│  CLI Layer: argparse (built-in Zig)              │
│  ├── oscar list-devices                          │
│  ├── oscar profile set --dpi 800 --polling 1000  │
│  ├── oscar macro record --output macro.bin       │
│  ├── oscar flash firmware.bin                    │
│  ├── oscar gui (launch GTK/XCB GUI)              │
│  └── oscar daemon (tray icon, hotplug)           │
├──────────────────────────────────────────────────┤
│  GUI Layer: GTK4 (C bindings) or XCB directly    │
│  ├── Main window (notebook/tabbed)              │
│  ├── Device panel                                │
│  ├── DPI/Profile editor                          │
│  ├── Button mapping editor                       │
│  ├── Macro editor                                │
│  └── OSD preview                                 │
├──────────────────────────────────────────────────┤
│  HID Layer: libusb (via Zig-translated headers)  │
│  ├── Device enumeration (vid:pid filter)         │
│  ├── HID Feature Report I/O                     │
│  ├── Interrupt IN/OUT                           │
│  └── Hotplug detection                          │
├──────────────────────────────────────────────────┤
│  Protocol Layer (reimplemented from RE)          │
│  ├── EEPROM read/write                          │
│  ├── DPI table management                       │
│  ├── Button trigger mapping                     │
│  ├── Macro command compiler                     │
│  └── Firmware flashing                          │
├──────────────────────────────────────────────────┤
│  Profile Storage: JSON/YAML/INI                  │
│  └── ~/.config/oscar/*.json                      │
└──────────────────────────────────────────────────┘
```

### Implementation Phases

| Phase | Scope | Est. |
|---|---|---|
| **P1** | Device detection (enumerate HID, match known VID/PID) | 2 days |
| **P2** | Feature report R/W (read current config from mouse) | 2 days |
| **P3** | DPI/Profile editor (read + write DPI stages) | 3 days |
| **P4** | Button mapping (trigger table read/write) | 3 days |
| **P5** | Macro engine (record, compile, replay) | 5 days |
| **P6** | GUI frontend (GTK4 or TUI) | 5 days |
| **P7** | CLI tool (headless mode, scripting) | 2 days |
| **P8** | Hotplug daemon (udev, tray icon) | 2 days |
| **P9** | Firmware flashing | 3 days |
| **Total** | | **~27 days** |

### HID Protocol Info Needed (to be obtained with real device)

1. Vendor HID usage page / report IDs for each operation
2. EEPROM read command structure
3. EEPROM write command structure (beyond HID_EE_Write16 hints)
4. DPI table byte layout
5. Button trigger entry format (STRUCT_TRIGGER layout)
6. Macro binary format
7. Firmware upload protocol (BridgeToUser.exe role)

## 9. Dependencies (Linux)

| Component | Dependency | Purpose |
|---|---|---|
| HID access | `libusb-1.0` (no sudo needed with udev rules) | USB device communication |
| OR | `hidraw` via sysfs + udev | Kernel HID interface |
| GUI | `GTK4` + `libxcb` (or just XCB) | Cross-platform UI |
| Config | Zig stdlib JSON parser | Profile storage |
| Networking | Zig stdlib `std.http` | Firmware updates |

No runtime, no garbage collector, no VM — just a static binary.

---

*Analysis date: 2026-07-09*
*Toolset: objdump (binutils 2.46.1), strings, manual PE analysis*
