Name:           vgp
Version:        0.1.0
Release:        1%{?dist}
Summary:        GPU-accelerated vector display server and desktop environment

License:        MIT
URL:            https://github.com/theesfeld/VGP
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson >= 0.62
BuildRequires:  ninja-build
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(libseat)
BuildRequires:  pam-devel
BuildRequires:  libvterm-devel
BuildRequires:  scdoc
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description
VGP is a vector-rendered display server for Linux running directly on
DRM/KMS via GBM and EGL. No X11 or Wayland is required.

Ships with a compositor and a bundled desktop environment of F-16
HUD/MFD styled apps: terminal, file manager, text editor, launcher,
system monitor, settings, and status bar.

%package libs
Summary:        VGP client library

%description libs
Shared library used by applications to connect to the VGP display server
over its Unix-domain protocol socket.

%package devel
Summary:        VGP client library -- development files
Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description devel
Headers and pkg-config metadata for libvgp, the VGP client library.

%prep
%autosetup -n VGP-%{version}

%build
%meson
%meson_build

%install
%meson_install

%check
%meson_test || :
desktop-file-validate %{buildroot}%{_datadir}/wayland-sessions/vgp.desktop
for f in %{buildroot}%{_datadir}/applications/vgp-*.desktop; do
  desktop-file-validate "$f"
done
for f in %{buildroot}%{_metainfodir}/*.metainfo.xml; do
  appstream-util validate-relax --nonet "$f" || :
done

%ldconfig_scriptlets libs

%files
%license LICENSE
%doc README.md CHANGELOG.md CONTRIBUTING.md SECURITY.md CODE_OF_CONDUCT.md
%{_bindir}/vgp
%{_bindir}/vgp-bar
%{_bindir}/vgp-edit
%{_bindir}/vgp-files
%{_bindir}/vgp-launcher
%{_bindir}/vgp-monitor
%{_bindir}/vgp-settings
%{_bindir}/vgp-term
%{_bindir}/vgp-view
%{_datadir}/applications/vgp-*.desktop
%{_datadir}/wayland-sessions/vgp.desktop
%{_metainfodir}/io.github.theesfeld.vgp*.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/vgp*.svg
%{_datadir}/vgp/
%{_datadir}/applications/mimeapps.list
%{_datadir}/bash-completion/completions/vgp
%{_datadir}/zsh/site-functions/_vgp
%{_datadir}/fish/vendor_completions.d/vgp.fish
%{_mandir}/man1/vgp*.1*
%{_mandir}/man5/vgp*.5*

%files libs
%license LICENSE
%{_libdir}/libvgp.so.0
%{_libdir}/libvgp.so.*

%files devel
%{_includedir}/vgp/
%{_libdir}/libvgp.so
%{_libdir}/pkgconfig/libvgp.pc

%changelog
* Fri Apr 17 2026 William Theesfeld <tj.theesfeld@citywide.io> - 0.1.0-1
- Initial release.
