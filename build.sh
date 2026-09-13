#!/usr/bin/env bash
#
# Build keyfinder-shim and stage it in dist/.
#
# FFTW is built here from a pinned, checksummed tarball rather than taken from
# the host: the binary ships inside a signed .app, so the GPL source offer has
# to name the exact version that went into it. Extra arguments are passed
# through to the CMake configure step.
#
# arm64 only, deliberately. Ultimate ships an arm64 sidecar (PyInstaller with
# native wheels), so an x86_64 slice here would ride inside an app that cannot
# launch on Intel.
#
set -euo pipefail

FFTW_VERSION="3.3.11"
FFTW_URL="https://fftw.org/fftw-${FFTW_VERSION}.tar.gz"
FFTW_SHA256="5630c24cdeb33b131612f7eb4b1a9934234754f9f388ff8617458d0be6f239a1"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
arch="arm64"

if [ "$(uname -m)" != "arm64" ]; then
  echo >&2 "build.sh: this builds arm64 only and needs an Apple Silicon machine"
  exit 2
fi

build_dir="$here/build/$arch"
dist_dir="$here/dist"
fftw_dir="$here/build/fftw"
fftw_prefix="$fftw_dir/prefix-$arch"
output="$dist_dir/keyfinder-shim-${arch}-apple-darwin"

#
# FFTW3: pinned tarball, verified, built static for this one architecture.
#
if [ ! -f "$fftw_prefix/lib/libfftw3.a" ]; then
  mkdir -p "$fftw_dir"
  tarball="$fftw_dir/fftw-${FFTW_VERSION}.tar.gz"

  if [ ! -f "$tarball" ]; then
    echo "Downloading FFTW ${FFTW_VERSION}"
    curl -fsSL --retry 3 -o "$tarball.tmp" "$FFTW_URL"
    mv "$tarball.tmp" "$tarball"
  fi

  actual="$(shasum -a 256 "$tarball" | awk '{print $1}')"
  if [ "$actual" != "$FFTW_SHA256" ]; then
    echo >&2 "ERROR: FFTW tarball checksum mismatch"
    echo >&2 "  expected $FFTW_SHA256"
    echo >&2 "  actual   $actual"
    exit 1
  fi

  src="$fftw_dir/fftw-${FFTW_VERSION}"
  [ -d "$src" ] || tar xzf "$tarball" -C "$fftw_dir"

  echo "Building FFTW ${FFTW_VERSION} for $arch"
  # FFTW 3.3.11 declares cmake_minimum_required(VERSION 3.4), which CMake 4
  # rejects outright; the same allowance libkeyfinder needs.
  cmake -S "$src" -B "$fftw_dir/build-$arch" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_INSTALL_PREFIX="$fftw_prefix" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTS=OFF \
    -DDISABLE_FORTRAN=ON \
    -DENABLE_THREADS=OFF \
    -DENABLE_NEON=ON
  cmake --build "$fftw_dir/build-$arch" --config Release -j "$(sysctl -n hw.ncpu)"
  cmake --install "$fftw_dir/build-$arch"
fi

#
# The shim itself.
#
cmake -S "$here" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$arch" \
  -DFFTW3_ROOT="$fftw_prefix" \
  "$@"
cmake --build "$build_dir" --config Release -j "$(sysctl -n hw.ncpu)"

mkdir -p "$dist_dir"
cp "$build_dir/keyfinder-shim" "$output"
strip -S -x "$output"

got_arch="$(lipo -archs "$output")"
if [ "$got_arch" != "$arch" ]; then
  echo >&2 "ERROR: built $got_arch, expected $arch"
  exit 1
fi

# The shim must not pull in anything from a package manager prefix; libkeyfinder
# and FFTW3 are linked statically on purpose.
bad="$(otool -L "$output" | tail -n +2 | awk '{print $1}' \
  | grep -Ev '^(/usr/lib/libSystem\.B\.dylib|/usr/lib/libc\+\+\.1\.dylib)$' || true)"
if [ -n "$bad" ]; then
  echo >&2 "ERROR: unexpected dynamic dependencies:"
  echo >&2 "$bad"
  exit 1
fi

echo
echo "OK: $output ($got_arch)"
ls -lh "$output"
