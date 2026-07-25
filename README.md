# Linux Universal Upscaler & Frame Gen

> **Lossless Scaling für Linux – Universell für JEDES Fenster, Spiel und Emulator**

Ein natives Linux-Projekt, das Echtzeitupscaling und Frame Generation für beliebige Fenster, Emulatoren (RetroArch, Eden) und Steam Games bereitstellt – ohne Steam Launch Commands, ohne Abhängigkeit von einzelnen Spielen.

---

## 🎯 Projekt-Übersicht

### Vision
Ein universelles Tool, das auf Tastenkombination oder via GUI:
1. **Fenster maximiert** → aktives Fenster fullscreen
2. **Frame Gen aktiviert** → LSFG oder ähnliche Methoden zur Framerate-Erhöhung
3. **Upscaling anwendet** → FSR oder ähnliche Algorithmen (konfigurierbar)
4. **Settings speichert** → pro App/Fenster unterschiedliche Profile

### Kernmerkmale
- ✅ **Fenster-unabhängig** – RetroArch, Eden Emulator, Steam, native Apps
- ✅ **Keyboard Shortcuts** – Global Hotkey zum Aktivieren (z.B. Ctrl+Alt+U)
- ✅ **UI Dashboard** – Settings, Profile Management, Live-Vorschau
- ✅ **Plug-and-Play** – Installieren und sofort nutzen
- ✅ **Konfigurierbar** – Frame Gen on/off, Upscaling Methode, Target Resolution
- ✅ **Performance-fokussiert** – Minimal Overhead, optimiert für Gaming

---

## 🏗️ Architektur

```
┌─────────────────────────────────────────────────────────────┐
│              Linux Universal Upscaler (LUU)                 │
└─────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                │             │             │
        ┌───────▼────────┐   │   ┌──────────▼────────┐
        │  Python GUI    │   │   │  Hotkey Daemon    │
        │  + Settings    │   │   │  (Global Input)   │
        └────────────────┘   │   └───────────────────┘
                │             │             │
                └─────────────┼─────────────┘
                              │
                    ┌─────────▼────────┐
                    │  Processing      │
                    │  Pipeline        │
                    └─────────┬────────┘
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
      ┌─────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐
      │ Vulkan      │   │ Screencopy  │   │ X11 Capture│
      │ Layer       │   │ (Wayland)   │   │ (Legacy)   │
      │ (Interceptor)  │             │   │             │
      └─────┬──────┘   └──────┬──────┘   └──────┬──────┘
            │                 │                 │
            └─────────────────┼─────────────────┘
                              │
                ┌─────────────▼────────────┐
                │   Shader Pipeline       │
                │ ┌─────────────────────┐ │
                │ │ Upscale Shader      │ │ (FSR, Lanczos, etc.)
                │ ├─────────────────────┤ │
                │ │ Frame Gen Shader    │ │ (LSFG, DAIN, etc.)
                │ └─────────────────────┘ │
                └─────────────┬────────────┘
                              │
                    ┌─────────▼────────┐
                    │  Output Display  │
                    │  (Enhanced)      │
                    └──────────────────┘
```

### Komponenten

**1. Python GUI & Settings (Entry Point)**
- PyQt6 oder GTK4 UI
- Global Hotkey Listener
- Settings per App (Profile)
- Live On/Off Toggle
- FPS Counter & Performance Stats

**2. Vulkan Layer (C++)**
- Vulkan Layer implementiert via `VK_LAYER_PATH`
- Interceptiert `vkQueuePresentKHR` für Frame Injection
- GLSL Shader Pipeline für Upscaling & Frame Gen
- Minimal invasiv – nur hookt Critical Path

**3. Screencopy Backend (C++/Wayland)**
- Fallback für Non-Vulkan Apps
- Nutzt `wlr-screencopy` Protocol
- Post-Processing via OpenGL/Vulkan
- Für RetroArch, Eden, etc. (wenn Vulkan nicht verfügbar)

**4. X11 Legacy Support (Optional Phase 2)**
- X11 Screenshot API
- Xrandr für Display-Info
- Fallback für ältere Systeme

**5. Config System (JSON)**
- Global Settings: `~/.config/luu/settings.json`
- App Profiles: `~/.config/luu/profiles.d/`
- Hotkey Mappings
- Shader Auswahl

---

## 🛠️ Tech Stack

```
Frontend:
  - Python 3.10+
  - PyQt6 oder GTK4
  - pynput/keyboard (Hotkey Listening)

Backend:
  - C++17/20
  - Vulkan SDK (>= 1.3)
  - OpenGL 4.6+
  - CMake 3.20+

Shaders:
  - GLSL 450+
  - Compute Shaders (optional, für Performance)

System Integration:
  - systemd (optional Service)
  - D-Bus (optional RPC)

Dependencies:
  - libvulkan-dev
  - libwayland-dev
  - libx11-dev (X11 Support)
  - libxrandr-dev (X11 Display)
  - glfw3 oder SDL2 (für Window Management)
```

---

## 📋 MVP Roadmap

### Phase 1: Foundation (2-3 Wochen)
- [ ] Python GUI Scaffold
  - [ ] Main Window Layout
  - [ ] Settings Panel UI
  - [ ] Profile Manager
- [ ] Global Hotkey System
  - [ ] Linux Input Listener
  - [ ] Hotkey Configuration
  - [ ] Test: Alt+U triggern
- [ ] Config System
  - [ ] JSON Parser
  - [ ] Default Configs
  - [ ] Profile Persistence
- [ ] Basic Window Detection
  - [ ] X11/Wayland Window Info
  - [ ] Active Window Tracking
  - [ ] WM_CLASS Detection für App Profiles

**Deliverable:** GUI läuft, Hotkey funktioniert, Settings speichern sich

---

### Phase 2: Capture & Processing (3-4 Wochen)
- [ ] Screen Capture Foundation
  - [ ] Screencopy (Wayland) Wrapper
  - [ ] X11 Screenshot Fallback
  - [ ] Performance Optimization (Memory Efficient)
- [ ] Shader Pipeline
  - [ ] Upscale Shader (einfacher: NN → später FSR)
  - [ ] Frame Gen Shader (einfacher: Interpolation)
  - [ ] Shader Composition
- [ ] Basic OpenGL Renderer
  - [ ] Capture → Texture Upload
  - [ ] Shader Application
  - [ ] Display Output
- [ ] Vulkan Layer Skeleton
  - [ ] VkLayer boilerplate
  - [ ] vkQueuePresentKHR Hook
  - [ ] Shader Pass Integration

**Deliverable:** Screencopy Screenshot kann upscaled werden, Hotkey triggert Verarbeitung

---

### Phase 3: Vulkan Integration (3-4 Wochen)
- [ ] Full Vulkan Layer Implementation
  - [ ] Frame Injection (LSFG)
  - [ ] Render Pass Optimization
  - [ ] VSync Handling
- [ ] Advanced Upscaling
  - [ ] FSR Integration oder Port
  - [ ] Multiple Upscale Modes
  - [ ] Quality Settings
- [ ] Game Compatibility Testing
  - [ ] Proton/Proton-Experimental
  - [ ] Native Linux Games
  - [ ] Steam Runtime Compatibility
- [ ] Performance Tuning
  - [ ] Latency Profiling
  - [ ] GPU Memory Optimization
  - [ ] CPU/GPU Load Balancing

**Deliverable:** Steam Game lässt sich mit Upscaling + Frame Gen spielen

---

### Phase 4: Polish & Release (2-3 Wochen)
- [ ] Emulator Integration Tests
  - [ ] RetroArch
  - [ ] Eden Emulator
  - [ ] Other Popular Emus
- [ ] Error Handling & Logging
- [ ] User Documentation
- [ ] Installation Scripts (AUR Package, Flatpak optional)
- [ ] Performance Benchmarks
- [ ] Bug Fixes & Refinement

**Deliverable:** Stable Release, Installation Guide, Known Issues Docs

---

## 📦 Verzeichnisstruktur

```
linux-universal-upscaler/
│
├── README.md                 # Diese Datei
├── ARCHITECTURE.md           # Detaillierte Tech-Dokumentation
├── BUILDING.md              # Build Instructions
├── CMakeLists.txt           # C++ Build Config
│
├── src/
│   ├── gui/                 # Python Frontend
│   │   ├── main.py          # Entry Point (PyQt App)
│   │   ├── settings_ui.py   # Settings Dialog
│   │   ├── profile_manager.py
│   │   ├── hotkey_listener.py
│   │   └── assets/          # Icons, Styles, etc.
│   │
│   ├── core/                # C++ Backend Core
│   │   ├── config.cpp/h     # Config Parser
│   │   ├── window_manager.cpp/h  # Window Tracking
│   │   └── ipc.cpp/h        # GUI ↔ Backend Communication
│   │
│   ├── capture/             # Screen Capture
│   │   ├── capture_base.cpp/h
│   │   ├── wayland_capture.cpp/h
│   │   ├── x11_capture.cpp/h
│   │   └── CMakeLists.txt
│   │
│   ├── vulkan/              # Vulkan Layer
│   │   ├── vk_layer.cpp/h   # Layer Entrypoint
│   │   ├── hooks.cpp/h      # API Hooks
│   │   ├── frame_injector.cpp/h
│   │   ├── CMakeLists.txt
│   │   └── layer/
│   │       └── VK_LAYER_LinuxUniversalUpscaler.json
│   │
│   ├── shaders/             # GLSL Shader Library
│   │   ├── upscale.frag     # FSR / Lanczos
│   │   ├── framegen.frag    # LSFG / Interpolation
│   │   ├── util.glsl        # Common Utils
│   │   └── compile.sh       # Shader to SPIR-V
│   │
│   ├── render/              # Rendering Backend
│   │   ├── renderer.cpp/h
│   │   ├── shader_program.cpp/h
│   │   ├── texture.cpp/h
│   │   └── CMakeLists.txt
│   │
│   └── common/              # Shared Utilities
│       ├── logger.cpp/h
│       ├── performance.cpp/h
│       └── platform.cpp/h
│
├── config/
│   ├── settings.json        # Global Settings Template
│   ├── profiles/
│   │   ├── default.json
│   │   ├── retroarch.json   # Pre-configured
│   │   └── steam.json
│   └── shaders/             # Compiled Shader Binaries
│
├── scripts/
│   ├── build.sh             # Build Helper
│   ├── install.sh           # Installation Script
│   ├── test_hotkey.py       # Test Suite
│   └── benchmark.py         # Performance Profiling
│
├── docs/
│   ├── USER_GUIDE.md        # How to Use
│   ├── TROUBLESHOOTING.md   # Common Issues
│   ├── API.md               # IPC / Plugin API (Future)
│   └── PERFORMANCE.md       # Tuning Guide
│
└── tests/
    ├── unit/                # C++ Unit Tests (gtest)
    ├── integration/         # End-to-End Tests
    └── fixtures/            # Test Data
```

---

## 🚀 Quick Start (Development)

### Voraussetzungen
```bash
# Ubuntu/Debian
sudo apt install vulkan-tools libvulkan-dev libwayland-dev libx11-dev \
  libxrandr-dev libglew-dev libglfw3-dev cmake python3-pyqt6 python3-pip git

# Arch
sudo pacman -S vulkan-headers vulkan-tools wayland libx11 xorg-server \
  glew glfw cmake python-pyqt6 base-devel
```

Note: `wlr-screencopy-unstable-v1.xml` (used by `src/capture/`) is vendored
in `protocols/`, so building against it does not require the `wlr-protocols`
package to be installed - only `wayland-scanner` (part of `wayland`/`wayland-utils`)
is needed at build time to generate its bindings. The screencopy capture
itself only works on wlroots-based Wayland compositors (Hyprland, Sway, ...).

`luu_capture_preview` runs a **continuous** live loop (re-capture + upscale
+ display every frame, paced by vsync), reading `capture_output` from
`settings.json` to pick which monitor to capture (see `hyprctl monitors` /
`wlr-randr` for names, e.g. `DP-2`). If `capture_output` is left empty, it
auto-picks the first output the compositor reports and logs which one - on
a single-monitor setup, or if the preview window happens to overlap the
captured output, the capture will recursively include the window itself
(a self-capture mirror). Point `capture_output` at a *different* monitor
than the one showing the preview window to avoid that.

Setting `capture_target: "window"` (default: `"output"`) captures one
specific window instead of a whole monitor - correctly composited even if
another window overlaps it on screen, since it's captured directly rather
than scraped from the visible screen region. Uses the standardized
`ext-foreign-toplevel-list-v1` / `ext-image-capture-source-v1` /
`ext-image-copy-capture-v1` protocols (part of `wayland-protocols` 1.49+,
not Hyprland-specific, though Hyprland is what this was built/tested
against) rather than `wlr-screencopy`. Pick a window via the Settings
dialog's "Choose window..." button, or run
`luu_capture_preview --list-windows` directly for the raw JSON list. A
picked window's `capture_window_id` is only valid **while that window
stays open** - closing it (even reopening "the same" window) invalidates
the id, and `capture_target: "window"` will fail with a clear error
listing what's currently open instead of silently capturing the wrong
thing. This makes window selection more session-scoped than
`capture_output` (a monitor name survives reboots; a window id doesn't
survive closing that window) - re-pick after closing/reopening the target.

Setting `framegen_method: "interpolation"` (with `frame_gen_enabled: true`)
turns on the frame-gen MVP: a real capture only every other vsync tick,
crossfaded 50/50 with the previous one on the ticks in between, instead of
re-displaying a stale frame. `framegen_method: "lsfg"` (the default) is
still just a placeholder name for a future real motion-compensated
interpolator - leaving it set logs a note and runs without frame gen.

`upscale_mode: "fsr"` (the default) is a real implementation of AMD's
FidelityFX Super Resolution 1.0 (EASU + RCAS), ported to plain GLSL from
the MIT-licensed reference at
[GPUOpen-Effects/FidelityFX-FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR)
(`src/shaders/fsr_easu.frag`, `fsr_rcas.frag`) - not just filtered
sampling. `quality` now has a real effect in this mode too: it sets
RCAS's sharpening strength (low -> ultra = mild -> strongest).
`upscale_mode: "lanczos"` is still the older filtered-sampling placeholder
(`"bilinear"`/`"nearest"` are simple, honest GPU-filter modes, unchanged) -
a real Lanczos kernel is a natural follow-up, not bundled into this round.

### Build & Run
```bash
# Clone & Setup
git clone https://github.com/yourusername/linux-universal-upscaler.git
cd linux-universal-upscaler
mkdir build && cd build

# Compile C++ Backend
cmake ..
make -j$(nproc)

# Run GUI (Development)
cd ../src/gui
python3 main.py
```

### Environment Setup (Development)
The layer's JSON manifest is generated into the *build* tree (not checked
into `src/vulkan/layer/`), with an absolute path to the `.so` built
alongside it - point `VK_LAYER_PATH` there, not at the source tree:
```bash
export VK_LAYER_PATH=/path/to/linux-universal-upscaler/build/src/vulkan/layer:$VK_LAYER_PATH

# Starten (Skeleton: passthrough + einmal pro Sekunde ein stderr-Log der
# Present-Rate, keine sichtbare Veränderung)
VK_INSTANCE_LAYERS=VK_LAYER_LinuxUniversalUpscaler ~/game_or_app
```

---

## 🎮 Nutzungsszenarien

### Szenario 1: Steam Game mit Upscaling
```
1. Spielfenster öffnen
2. Alt+U drücken (konfigurierbar)
3. Fenster maximiert, Frame Gen + Upscaling aktiv
4. 1440p → 4K mit 120fps (statt 60fps native)
5. Alt+U zum Deaktivieren
```

### Szenario 2: RetroArch Emulation
```
1. RetroArch lädt NES/SNES Spiel
2. Alt+U drücken
3. 640x480 → 1440p hochskaliert + Interpolation für smoothere Animation
4. Performance Monitor zeigt +2-3ms Overhead
```

### Szenario 3: Config pro App
```
~/.config/luu/profiles.d/retroarch.json:
{
  "window_class": "retroarch",
  "upscale_mode": "fsr",
  "target_resolution": [1440, 1080],
  "frame_gen_enabled": true,
  "framegen_method": "lsfg",
  "quality": "ultra"
}
```

---

## 🔧 Development Workflow

### 1. Feature Development
```bash
# Feature Branch
git checkout -b feature/vulkan-layer-v2
# Make changes
# Test locally
git push origin feature/vulkan-layer-v2
```

### 2. Testing
```bash
# Unit Tests
cd build && make test

# Manual Testing
./scripts/test_hotkey.py
./scripts/benchmark.py

# Visual Profiling
VK_LAYER_PATH=... VK_INSTANCE_LAYERS=... vkgpuinfo
```

### 3. Shader Development
```bash
# Edit Shaders
vim src/shaders/upscale.frag

# Compile to SPIR-V
./src/shaders/compile.sh

# Test in Live App (auto-reload)
```

---

## 📊 Performance Targets

| Target | Metric | Threshold |
|--------|--------|-----------|
| Latency | Frame Injection Delay | < 2ms |
| Memory | GPU VRAM per App | < 200MB |
| CPU | Thread Overhead | < 10% single core |
| FPS | Framerate with Frame Gen | +60% (60 → 96fps typical) |
| Compatibility | Game Support | > 90% Proton + Native |

---

## 🐛 Debugging

### Hotkey nicht funktioniert?
```bash
# Check Input Events
sudo evtest
# Oder direkt mit pynput testen
python3 scripts/test_hotkey.py
```

### Vulkan Layer lädt nicht?
```bash
# Layer-Debug aktivieren
VK_LAYER_PATH=... VK_INSTANCE_LAYERS=VK_LAYER_LinuxUniversalUpscaler \
  VK_LOADER_DEBUG=all glxinfo 2>&1 | grep "Layer"
```

### Performance Probleme?
```bash
# Profiler starten
python3 scripts/benchmark.py --profile

# GPU Auslastung
nvidia-smi -l 1  # NVIDIA
watch -n1 "radeontop"  # AMD
```

---

## 📝 Contributing

Willkommen! Folgende Areas sind offen:
- **Vulkan Expertise** – Layer Optimization
- **Shader Dev** – FSR/DAIN Ports
- **Frame Gen** – LSFG Implementierung
- **Testing** – Game Compatibility Testing
- **Python** – GUI Improvements
- **Docs** – Guides & Troubleshooting

---

## 📄 License

MIT License – Siehe LICENSE Datei

---

## 🤝 Support & Community

- **Issues**: GitHub Issues für Bugs & Feature Requests
- **Discussions**: GitHub Discussions für Fragen
- **Discord**: (Optional) Community Server Link

---

## 🎓 Ressourcen & Referenzen

- [Vulkan Layers Spec](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/loader/LoaderAndLayerInterface.md)
- [AMD FSR Specification](https://github.com/GPUOpen-Effects/FidelityFX-FSR)
- [Wayland Protocols](https://wayland.freedesktop.org/docs/html/)
- [LSFG Paper](https://arxiv.org/abs/2011.01852) (wenn verfügbar)
- [PancakeTAS lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) – Reference Implementation

---

**Last Updated:** 2026-07-25  
**Status:** Planning Phase  
**Next:** Start Phase 1 Development in Claude Code
