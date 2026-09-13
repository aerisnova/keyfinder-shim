# Development

Build requirements are in [README.md](README.md#building); there is nothing to
install beyond the Xcode command line tools and CMake. libkeyfinder and FFTW are
both fetched and built by `build.sh`, pinned, so no Homebrew FFTW is involved and
no `-DFFTW3_ROOT` is needed.

```sh
./build.sh
```

The FFTW tarball and its build prefix are cached under `build/fftw/`, so only
the first build pays for it. Delete `build/` to prove a clean build still works.

## Releasing

```sh
git tag -a v1.0.1 -m "Release 1.0.1"
git push origin v1.0.1
```

`.github/workflows/build.yml` runs on one `macos-14` runner, builds, verifies,
and attaches the binary to a GitHub release. It also runs on `workflow_dispatch`
for a dry run without tagging.

Two things the workflow must keep doing, both learned the hard way:

- **Do not ask for an Intel runner.** GitHub's Intel macOS image is retired, so a
  job that requests `macos-13` queues forever instead of failing.
- **Keep the silence check.** It is the cheapest end-to-end proof that the binary
  runs at all; a build can link fine and still be unusable.

## Two CMake workarounds

Both are in `CMakeLists.txt` and both look arbitrary without the reason:

- **libkeyfinder's own `FindFFTW3.cmake` calls `find_library` without constraining
  the suffix**, so it would happily resolve a shared library and give the shim a
  runtime dependency. `CMakeLists.txt` seeds the `FFTW3_LIBRARY` and
  `FFTW3_INCLUDE_DIR` cache entries with the static archive before
  `FetchContent_MakeAvailable`, which makes that `find_library` a no-op, and
  fails the configure if what it resolved is not a `.a`.
- **libkeyfinder and FFTW both declare a `cmake_minimum_required` below 3.5**,
  which CMake 4 rejects outright rather than warning about.
  `CMAKE_POLICY_VERSION_MINIMUM` is set to 3.5 for each to allow it.

## Checking a build

```sh
lipo -archs dist/keyfinder-shim-arm64-apple-darwin
otool -L dist/keyfinder-shim-arm64-apple-darwin   # libSystem and libc++ only

# Silence classifies as silence
dd if=/dev/zero bs=4 count=44100 2>/dev/null \
  | ./dist/keyfinder-shim-arm64-apple-darwin 44100     # -> --
```

`build.sh` already fails the build if either the architecture or the dynamic
dependency list is wrong, so these are for inspecting a binary you were handed.
