# Architecture

## Overview

```
┌─────────────────────────────────────────────┐
│             VGP Server (vgp)                │
│                                             │
│  ┌─────────┐ ┌──────────┐ ┌─────────────┐  │
│  │ DRM/KMS │ │ NanoVG   │ │ Compositor  │  │
│  │ + GBM   │ │ GLES 3.0 │ │ + Tiling    │  │
│  └────┬────┘ └────┬─────┘ └──────┬──────┘  │
│       │           │              │          │
│  ┌────┴───┐  ┌────┴──────┐  ┌───┴──────┐  │
│  │libinput│  │ Shader Mgr│  │ IPC      │  │
│  │+ xkb   │  │ + Themes  │  │ Server   │  │
│  └────────┘  └───────────┘  └──────────┘  │
│                                             │
│  ┌─────────┐ ┌────────┐ ┌──────────────┐  │
│  │libseat  │ │ D-Bus  │ │ Lock Screen  │  │
│  │Session  │ │Notify  │ │ + Animations │  │
│  └─────────┘ └────────┘ └──────────────┘  │
└──────────────────┬──────────────────────────┘
                   │ Unix domain socket
    ┌──────────────┼──────────────┐
    │              │              │
 vgp-term    vgp-settings   vgp-files
```

## Rendering Pipeline

1. **DRM/KMS**: Opens GPU via libseat, creates GBM surfaces per output
2. **EGL**: Creates OpenGL ES 3.0 context on GBM surfaces
3. **NanoVG**: Tessellates vector paths into GPU triangles
4. **Shaders**: Custom GLSL fragment shaders for backgrounds, rendered before NanoVG frame
5. **Compositing**: NanoVG draws decorations, text, UI elements as vector paths
6. **Scanout**: `eglSwapBuffers` → GBM BO → `drmModeSetCrtc`/`PageFlip`

## Cell Grid Protocol

Terminals don't render pixels. They send a cell grid:

```c
typedef struct vgp_cell {
    uint32_t codepoint;   // Unicode character
    uint8_t  fg_r, fg_g, fg_b;
    uint8_t  bg_r, bg_g, bg_b;
    uint8_t  attrs;       // bold, italic, underline, etc.
    uint8_t  width;       // cell width (1 or 2 for wide chars)
} vgp_cell_t;             // 12 bytes per cell
```

An 80x24 terminal sends 23,040 bytes per frame (vs 1,600,000 bytes for pixel data). The server renders each character with NanoVG at native output resolution.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/server/main.c` | Entry point, signal handling |
| `src/server/server.c` | Server state, init, event dispatch |
| `src/server/drm.c` | DRM/KMS, output enumeration, page flips |
| `src/server/backend_gpu.c` | NanoVG/EGL/GBM GPU backend |
| `src/server/renderer.c` | Frame rendering, decoration, cell grid |
| `src/server/compositor.c` | Window management, z-order, focus |
| `src/server/tiling.c` | Tiling layout algorithms |
| `src/server/ipc.c` | Unix socket IPC server |
| `src/server/config.c` | TOML config parser |
| `src/server/shader_loader.c` | GLSL shader compilation |
| `src/server/lockscreen.c` | Lock screen |
| `src/libvgp/vgp.c` | Client library |
| `src/libvgp-ui/vgp-ui.c` | Cell grid UI toolkit |
| `src/vgp-term/term.c` | Terminal emulator |

---

← **[Shaders](Shaders)** · Next: **[FAQ](FAQ)** →
