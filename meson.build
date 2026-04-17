project('vgp', 'c',
  version : '0.1.0',
  license : 'MIT',
  license_files : ['LICENSE'],
  meson_version : '>=0.62',
  default_options : [
    'c_std=c17',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

cc = meson.get_compiler('c')

# Publish the project version as a C macro to every compiled file.
add_project_arguments(
  '-DVGP_VERSION="@0@"'.format(meson.project_version()),
  language : 'c',
)

# Reproducible-build support: no __DATE__ / __TIME__ / __FILE__ path
# embedding, use the relative source path so builds are deterministic
# when SOURCE_DATE_EPOCH is set.
if cc.has_argument('-Wdate-time')
  add_project_arguments('-Wdate-time', language : 'c')
endif
if cc.has_argument('-ffile-prefix-map=@0@=.'.format(meson.project_source_root()))
  add_project_arguments(
    '-ffile-prefix-map=@0@=.'.format(meson.project_source_root()),
    language : 'c',
  )
endif

# ============================================================
# Dependencies
# ============================================================

dep_drm   = dependency('libdrm')
dep_input = dependency('libinput')
dep_udev  = dependency('libudev')
dep_xkb   = dependency('xkbcommon')
dep_math  = cc.find_library('m')
dep_dl    = cc.find_library('dl')
dep_dbus  = dependency('dbus-1', required : false)
dep_seat  = dependency('libseat')
dep_pam   = cc.find_library('pam')

# plutovg as a subproject
plutovg_proj = subproject('plutovg')
dep_plutovg  = plutovg_proj.get_variable('plutovg_dep')

# libvterm (for terminal emulator)
dep_vterm = dependency('vterm', required : false)

# GPU backend dependencies (optional)
dep_gbm    = dependency('gbm', required : false)
dep_egl    = dependency('egl', required : false)
dep_glesv2 = dependency('glesv2', required : false)

gpu_backend_available = dep_gbm.found() and dep_egl.found() and dep_glesv2.found()

# POSIX feature test macros (needed for clock_gettime, sigaction, etc.)
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')

# Build options
if get_option('debug_log')
  add_project_arguments('-DVGP_DEBUG', language : 'c')
endif

if get_option('sanitizers')
  add_project_arguments('-fsanitize=address,undefined', language : 'c')
  add_project_link_arguments('-fsanitize=address,undefined', language : 'c')
endif

# ============================================================
# Include directories
# ============================================================

vgp_inc = include_directories('include', 'src')
vgp_gfx_inc = include_directories('src/libvgp-gfx')

# ============================================================
# VGP Server
# ============================================================

vgp_server_sources = files(
  'src/server/main.c',
  'src/server/server.c',
  'src/server/loop.c',
  'src/server/timer.c',
  'src/server/vt.c',
  'src/server/drm.c',
  'src/server/input.c',
  'src/server/keyboard.c',
  'src/server/cursor.c',
  'src/server/renderer.c',
  'src/server/compositor.c',
  'src/server/window.c',
  'src/server/decoration.c',
  'src/server/desktop.c',
  'src/server/theme.c',
  'src/server/ipc.c',
  'src/server/config.c',
  'src/server/keybind.c',
  'src/server/spawn.c',
  'src/server/notify.c',
  'src/server/seat.c',
  'src/server/animation.c',
  'src/server/lockscreen.c',
  'src/server/tiling.c',
  'src/server/menu.c',
  'src/server/ipc_control.c',
  'src/server/session.c',
  'src/server/power.c',
  'src/server/hotreload.c',
  'src/server/calendar.c',
  'src/server/backend_cpu.c',
  'src/server/arena.c',
  'src/server/pool.c',
  'src/server/panel.c',
  'src/server/fbo_glass.c',
  'src/libvgp-gfx/vgp-stroke-font.c',
  'src/libvgp/xdg.c',
)

# Add GPU backend sources if available
gpu_deps = []
nanovg_inc = include_directories('include/nanovg')

if gpu_backend_available
  add_project_arguments('-DVGP_HAS_GPU_BACKEND', language : 'c')
  vgp_server_sources += files(
    'src/server/backend_gpu.c',
    'src/server/nanovg_impl.c',
    'src/server/shader_loader.c',
  )
  gpu_deps = [dep_gbm, dep_egl, dep_glesv2]
endif

executable('vgp',
  vgp_server_sources,
  include_directories : [vgp_inc, nanovg_inc, vgp_gfx_inc],
  dependencies : [
    dep_drm,
    dep_input,
    dep_udev,
    dep_xkb,
    dep_plutovg,
    dep_math,
    dep_dl,
    dep_dbus,
    dep_seat,
    dep_pam,
  ] + gpu_deps,
  install : true,
)

# ============================================================
# Client library (libvgp)
# ============================================================

libvgp = shared_library('vgp',
  files('src/libvgp/vgp.c', 'src/libvgp/xdg.c'),
  include_directories : vgp_inc,
  version : meson.project_version(),
  soversion : '0',
  install : true,
)

dep_libvgp = declare_dependency(
  link_with : libvgp,
  include_directories : vgp_inc,
)

# Public headers
install_headers(
  'include/vgp/vgp.h',
  'include/vgp/protocol.h',
  'include/vgp/types.h',
  'include/vgp/log.h',
  'include/vgp/xdg.h',
  subdir : 'vgp',
)

# pkg-config file generated from data/libvgp.pc.in
pkg_conf = configuration_data()
pkg_conf.set('prefix',     get_option('prefix'))
pkg_conf.set('libdir',     get_option('prefix') / get_option('libdir'))
pkg_conf.set('includedir', get_option('prefix') / get_option('includedir'))
pkg_conf.set('version',    meson.project_version())

configure_file(
  input : 'data/libvgp.pc.in',
  output : 'libvgp.pc',
  configuration : pkg_conf,
  install : true,
  install_dir : get_option('libdir') / 'pkgconfig',
)

# ============================================================
# Shared UI library for VGP apps (cell grid, legacy)
# ============================================================

vgp_ui_inc = include_directories('src/libvgp-ui')

libvgp_ui = static_library('vgp-ui',
  files('src/libvgp-ui/vgp-ui.c', 'src/libvgp-ui/config-writer.c'),
  include_directories : [vgp_inc, vgp_ui_inc],
  dependencies : [dep_libvgp],
)

dep_vgp_ui = declare_dependency(
  link_with : libvgp_ui,
  include_directories : [vgp_inc, vgp_ui_inc],
  dependencies : [dep_libvgp],
)

# ============================================================
# Graphical UI library for VGP apps (draw commands, GPU)
# ============================================================

libvgp_gfx = static_library('vgp-gfx',
  files('src/libvgp-gfx/vgp-gfx.c', 'src/libvgp-gfx/vgp-gfx-widgets.c',
        'src/libvgp-gfx/vgp-gfx-3d.c'),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_libvgp, dep_math],
)

dep_vgp_gfx = declare_dependency(
  link_with : libvgp_gfx,
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_libvgp],
)

# ============================================================
# Terminal emulator (vgp-term)
# ============================================================

if dep_vterm.found()
  executable('vgp-term',
    files(
      'src/vgp-term/main.c',
      'src/vgp-term/term.c',
      'src/vgp-term/pty.c',
      'src/vgp-term/render.c',
      'src/vgp-term/color.c',
    ),
    include_directories : vgp_inc,
    dependencies : [dep_libvgp, dep_vterm],
    install : true,
  )
endif

# ============================================================
# Application launcher (vgp-launcher)
# ============================================================

executable('vgp-launcher',
  files(
    'src/vgp-launcher/main.c',
    'src/vgp-launcher/launcher.c',
    'src/vgp-launcher/desktop.c',
    'src/vgp-launcher/render.c',
  ),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_vgp_gfx],
  install : true,
)

# (libvgp-ui and libvgp-gfx defined above, before launcher)

# ============================================================
# VGP Settings
# ============================================================

config_writer = static_library('config-writer',
  files('src/libvgp-ui/config-writer.c'),
  include_directories : [vgp_inc, vgp_ui_inc],
)

executable('vgp-settings',
  files('src/vgp-settings/main.c'),
  include_directories : [vgp_inc, vgp_gfx_inc, vgp_ui_inc],
  dependencies : [dep_vgp_gfx],
  link_with : [config_writer],
  install : true,
)

# ============================================================
# VGP System Monitor
# ============================================================

executable('vgp-monitor',
  files('src/vgp-monitor/main.c'),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_vgp_gfx, dep_math],
  install : true,
)

# ============================================================
# VGP File Manager
# ============================================================

executable('vgp-files',
  files('src/vgp-files/main.c'),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_vgp_gfx],
  install : true,
)

# ============================================================
# VGP Image Viewer
# ============================================================

# ============================================================
# VGP Text Editor
# ============================================================

executable('vgp-edit',
  files('src/vgp-edit/main.c'),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_vgp_gfx],
  install : true,
)

# ============================================================
# VGP Image Viewer
# ============================================================

executable('vgp-view',
  files('src/vgp-view/main.c'),
  include_directories : [vgp_inc, nanovg_inc],
  dependencies : [dep_libvgp, dep_math],
  install : true,
)

# ============================================================
# VGP Bar (standalone scriptable status bar)
# ============================================================

executable('vgp-bar',
  files('src/vgp-bar/main.c'),
  include_directories : [vgp_inc, vgp_gfx_inc],
  dependencies : [dep_vgp_gfx],
  install : true,
)

# ============================================================
# Desktop files (for greeters and launchers)
# ============================================================

install_data('vgp.desktop',
  install_dir : '/usr/share/wayland-sessions',
)

install_data(
  'data/vgp-term.desktop',
  'data/vgp-settings.desktop',
  'data/vgp-files.desktop',
  'data/vgp-monitor.desktop',
  'data/vgp-launcher.desktop',
  'data/vgp-edit.desktop',
  'data/vgp-bar.desktop',
  install_dir : get_option('datadir') / 'applications',
)

# ============================================================
# XDG data files: shaders + themes installed to $datadir/vgp/
# (per XDG_DATA_DIRS). Users can override in $XDG_DATA_HOME/vgp/
# or $XDG_CONFIG_HOME/vgp/.
# ============================================================

install_subdir('themes',
  install_dir : get_option('datadir') / 'vgp',
  exclude_directories : ['.git'],
)

install_subdir('data',
  install_dir : get_option('datadir') / 'vgp',
  exclude_files : [
    'vgp-term.desktop', 'vgp-settings.desktop', 'vgp-files.desktop',
    'vgp-monitor.desktop', 'vgp-launcher.desktop', 'vgp-edit.desktop',
    'vgp-bar.desktop', 'libvgp.pc.in',
  ],
  exclude_directories : ['metainfo'],
)

# ============================================================
# AppStream MetaInfo (for software centres, flatpak catalogs)
# ============================================================

install_data(
  'data/metainfo/io.github.theesfeld.vgp.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.term.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.edit.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.files.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.monitor.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.settings.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.launcher.metainfo.xml',
  'data/metainfo/io.github.theesfeld.vgp.bar.metainfo.xml',
  install_dir : get_option('datadir') / 'metainfo',
)

# ============================================================
# Man pages (generated from scdoc sources when available)
# ============================================================

scdoc_prog = find_program('scdoc', required : false)
if scdoc_prog.found()
  man_pages = {
    'vgp.1'          : 'man/vgp.1.scd',
    'vgp-term.1'     : 'man/vgp-term.1.scd',
    'vgp-edit.1'     : 'man/vgp-edit.1.scd',
    'vgp-files.1'    : 'man/vgp-files.1.scd',
    'vgp-launcher.1' : 'man/vgp-launcher.1.scd',
    'vgp-monitor.1'  : 'man/vgp-monitor.1.scd',
    'vgp-settings.1' : 'man/vgp-settings.1.scd',
    'vgp-bar.1'      : 'man/vgp-bar.1.scd',
    'vgp.conf.5'     : 'man/vgp.conf.5.scd',
  }
  foreach page, src : man_pages
    section = page.split('.')[-1]
    custom_target(page,
      input   : src,
      output  : page,
      command : [scdoc_prog],
      feed    : true,
      capture : true,
      install : true,
      install_dir : get_option('mandir') / 'man' + section,
    )
  endforeach
else
  message('scdoc not found; man pages will not be built')
endif

# ============================================================
# Icons (SVG, scalable, hicolor)
# ============================================================

install_data(
  'data/icons/hicolor/scalable/apps/vgp.svg',
  'data/icons/hicolor/scalable/apps/vgp-term.svg',
  'data/icons/hicolor/scalable/apps/vgp-edit.svg',
  'data/icons/hicolor/scalable/apps/vgp-files.svg',
  'data/icons/hicolor/scalable/apps/vgp-launcher.svg',
  'data/icons/hicolor/scalable/apps/vgp-monitor.svg',
  'data/icons/hicolor/scalable/apps/vgp-settings.svg',
  'data/icons/hicolor/scalable/apps/vgp-bar.svg',
  install_dir : get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

# ============================================================
# Shell completions (bash / zsh / fish)
# ============================================================

install_data('data/completions/bash/vgp',
  install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)
install_data('data/completions/zsh/_vgp',
  install_dir : get_option('datadir') / 'zsh' / 'site-functions',
)
install_data('data/completions/fish/vgp.fish',
  install_dir : get_option('datadir') / 'fish' / 'vendor_completions.d',
)

# ============================================================
# Default MIME associations
# ============================================================

install_data('data/mimeapps.list',
  install_dir : get_option('datadir') / 'applications',
)

# ============================================================
# Localization (gettext) -- scaffolding. Strings are marked with
# _("...") as translations are added; run `meson compile vgp-pot`
# to regenerate the template.
# ============================================================

subdir('po')
