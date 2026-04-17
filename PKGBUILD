# Maintainer: William Theesfeld <tj.theesfeld@citywide.io>
pkgname=vgp-git
_pkgname=vgp
pkgver=0.1.0
pkgrel=1
pkgdesc="GPU-accelerated vector display server and desktop environment"
arch=('x86_64')
url="https://github.com/theesfeld/VGP"
license=('MIT')
depends=(
  'libdrm'
  'libinput'
  'libxkbcommon'
  'libseat'
  'dbus'
  'libvterm'
  'mesa'
  'pam'
)
makedepends=(
  'meson'
  'ninja'
  'git'
  'pkgconf'
  'scdoc'
)
checkdepends=(
  'desktop-file-utils'
  'appstream'
)
provides=("${_pkgname}=${pkgver}")
conflicts=("${_pkgname}")
source=("${_pkgname}::git+${url}.git")
sha256sums=('SKIP')

pkgver() {
  cd "$_pkgname"
  git describe --long --tags 2>/dev/null \
    | sed 's/^v//; s/\([^-]*-g\)/r\1/; s/-/./g' \
    || printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$_pkgname"
  arch-meson build
  meson compile -C build
}

check() {
  cd "$_pkgname"
  meson test -C build --print-errorlogs || true

  # Freedesktop spec validation
  for f in data/*.desktop vgp.desktop; do
    desktop-file-validate "$f"
  done
  for f in data/metainfo/*.metainfo.xml; do
    appstreamcli validate --strict "$f" || true
  done
}

package() {
  cd "$_pkgname"
  meson install -C build --destdir="$pkgdir"

  # Standard Arch license placement
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
