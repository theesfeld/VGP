# Architecture

## Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        VGP Server (vgp)                     │
│                                                             │
│  ┌─────────┐ ┌──────────┐ ┌──────────────┐ ┌────────────┐   │
│  │ DRM/KMS │ │ NanoVG   │ │ FBO Glass    │ │ Compositor │   │
│  │  + GBM  │ │ GLES 3.0 │ │ + blur chain │ │  + Tiling  │   │
│  └────┬────┘ └────┬─────┘ └──────┬───────┘ └─────┬──────┘   │
│       │           │              │               │          │
│  ┌────┴───┐  ┌────┴──────┐  ┌───┴───────┐  ┌────┴──────┐    │
│  │libinput│  │ Shader Mgr│  │ Hotreload │  │ IPC       │    │
│  │+ xkb   │  │ + Themes  │  │ (inotify) │  │ + Control │    │
│  └────────┘  └───────────┘  └───────────┘  └───────────┘    │
│                                                             │
│  ┌─────────┐ ┌────────────┐ ┌───────────┐ ┌─────────────┐   │
│  │ libseat │ │ D-Bus      │ │ XDG       │ │ Lock Screen │   │
│  │ Session │ │ Notify     │ │ Autostart │ │ + PAM       │   │
│  └─────────┘ └────────────┘ └───────────┘ └─────────────┘   │
└────────────────────────────┬────────────────────────────────┘
                             │ Unix-domain socket (libvgp protocol)
     ┌────────────┬──────────┼──────────┬────────────┬────────────┐
     │            │          │          │            │            │
  vgp-term  vgp-launcher  vgp-files  vgp-edit   vgp-monitor  vgp-settings
                                                                 │
                                                              vgp-bar
```

## Rendering pipeline (per frame, per output)

1. **DRM/KMS** — `libseat`-brokered GPU access, one `gbm_surface` per
   output via `backend_gpu.c`.
2. **EGL** — OpenGL ES 3.0 context bound to the GBM surface.
3. **Frame begin** — `nvgBeginFrame` starts a NanoVG frame on the
   default framebuffer.
4. **Scene + glass (FBO pipeline, `fbo_glass.c`)** — when enabled:
   1. `nvgEndFrame` to release NanoVG state.
   2. Render the background shader (e.g. `clouds.frag`) into a
      full-res scene FBO.
   3. Two-stage box-blur downsample: scene → half-res → quarter-res.
   4. Blit the full-res scene onto the default framebuffer as the
      base layer.
   5. For every visible window rect, draw the glass fragment shader:
      rounded-rect SDF alpha, Fresnel edge brightness, IOR-offset
      sample into the blur chain, quadratic chromatic aberration,
      top scatter, focus halo.
   6. `nvgBeginFrame` resumes NanoVG.

   Disabled via `VGP_FBO=0`; falls back to line-stacked fake-glass.
5. **Decoration** — NanoVG draws titlebars, buttons, etched black
   title text, the panel and its widgets, cell-grid content from
   terminals, raw draw commands from GUI clients, and the cursor.
6. **Frame end** — `nvgEndFrame`, `eglSwapBuffers`,
   `gbm_surface_lock_front_buffer`, convert to a DRM framebuffer ID,
   `drmModeSetCrtc` / async page-flip.

## Inter-process protocol

Clients link `libvgp` and connect to the compositor via a Unix-domain
socket at `$XDG_RUNTIME_DIR/vgp-0`. The wire format is a fixed
16-byte header + variable body:

```c
typedef struct {
    uint32_t magic;       // VGP_PROTOCOL_MAGIC
    uint16_t type;        // VGP_MSG_*
    uint16_t flags;
    uint32_t length;      // body size
    uint32_t window_id;   // target window (0 = session-global)
} vgp_msg_header_t;
```

Messages are typed (create/destroy window, cell-grid update, draw
commands, input events, theme broadcast, clipboard, notifications).
Full enumeration: `include/vgp/protocol.h`.

### Cell-grid channel

Terminals and TUI apps send a cell grid — one struct per character —
instead of a pixel buffer:

```c
typedef struct vgp_cell {
    uint32_t codepoint;   // Unicode character
    uint8_t  fg_r, fg_g, fg_b;
    uint8_t  bg_r, bg_g, bg_b;
    uint8_t  attrs;       // bold, italic, underline, strike, reverse
    uint8_t  width;       // 1, 2 (wide), or 0 (continuation)
} vgp_cell_t;             // 12 bytes per cell
```

An 80×24 terminal is ~23 KB / frame vs 1.6 MB of pixel data. The
compositor renders every glyph as vector strokes via the VGP stroke
font at native output resolution.

### Draw-command channel

GUI clients (`vgp-settings`, `vgp-files`, `vgp-launcher`,
`vgp-monitor`, `vgp-edit`, `vgp-bar`) send a byte stream of typed
draw commands — `rect`, `rounded_rect`, `line`, `text`, `text_bold`,
`circle`, `clear`, etc. — executed on the compositor's NanoVG
context. See `VGP_DCMD_*` in `include/vgp/protocol.h` and the
helpers in `src/libvgp-gfx/vgp-gfx.c`.

## Separate control socket

The compositor also listens on a second Unix socket at
`$XDG_RUNTIME_DIR/vgp-ctl` (`src/server/ipc_control.c`) for
out-of-band control: theme reload, lock, logout, shutdown dispatch.
This is how `vgp-settings` tells the compositor to re-read config.

## Key source files

| File                                 | Purpose                                 |
| ---                                  | ---                                     |
| `src/server/main.c`                  | Entry point, CLI flags, signal handling |
| `src/server/server.c`                | Server state, init, event dispatch      |
| `src/server/loop.c`, `timer.c`       | Epoll event loop, timers                |
| `src/server/drm.c`                   | DRM/KMS, output enumeration, page flips |
| `src/server/backend_gpu.c`           | EGL / GBM / NanoVG GPU backend          |
| `src/server/backend_cpu.c`           | plutovg CPU fallback                    |
| `src/server/renderer.c`              | Frame-render orchestration, decoration  |
| `src/server/fbo_glass.c`             | Scene FBO + blur chain + glass shader   |
| `src/server/shader_loader.c`         | GLSL compile, uniform binding           |
| `src/server/compositor.c`            | Window management, z-order, focus       |
| `src/server/tiling.c`                | Tiling layout algorithms                |
| `src/server/panel.c`                 | Status-bar widgets                      |
| `src/server/ipc.c`                   | Main client socket                      |
| `src/server/ipc_control.c`           | Control socket                          |
| `src/server/config.c`                | TOML config parser                      |
| `src/server/theme.c`                 | Theme-file parser                       |
| `src/server/keybind.c`               | Keybind parsing + action dispatch       |
| `src/server/spawn.c`                 | Process spawn + XDG Autostart scanner   |
| `src/server/hotreload.c`             | inotify watcher for config / shaders    |
| `src/server/notify.c`                | `org.freedesktop.Notifications` D-Bus   |
| `src/server/power.c`                 | Idle timeout + auto-lock                |
| `src/server/lockscreen.c`            | PAM-authenticated lock screen           |
| `src/server/session.c`               | Window-layout save / restore            |
| `src/libvgp/vgp.c`                   | Client library (protocol + connect)     |
| `src/libvgp/xdg.c`                   | XDG Base Directory helpers              |
| `src/libvgp-gfx/vgp-gfx.c`           | Client draw-command encoder             |
| `src/libvgp-gfx/vgp-stroke-font.c`   | 4×7 vector stroke font                  |
| `src/libvgp-gfx/vgp-hud.h`           | HUD palette + phosphor-glow helpers     |
| `src/libvgp-ui/vgp-ui.c`             | Cell-grid UI toolkit                    |
| `src/vgp-term/term.c`                | Terminal emulator (libvterm-backed)     |

---

← **[Shaders](Shaders)** · Next: **[FAQ](FAQ)** →
