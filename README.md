# keyfinder-shim

A single-purpose command-line binary that reports the musical key of raw PCM
audio, in Camelot notation. It wraps [libkeyfinder][lkf], the key detection
library used by Mixxx.

## Contract

```
keyfinder-shim <sample_rate> [--scores]
```

- `sample_rate` — integer Hz, 1 to 768000.
- **stdin** — mono, float32 little-endian PCM, read in chunks until EOF. No
  header, no container, no decoding: the caller decodes the file and sends
  samples. The total byte count must be a multiple of 4.
- **stdout** — one line: the key in Camelot notation (`8A`, `12B`, …), or `--`
  when libkeyfinder reports `SILENCE`. Empty input also gives `--`.
- **stderr** — a message, only on failure.
- **Exit code** — `0` on success, `2` for a usage or argument error, `1` for a
  read error, a truncated final sample, or an error from libkeyfinder.

Non-finite samples (NaN, infinity) are replaced with 0.0 rather than rejected;
libkeyfinder throws on them.

With `--scores`, two more lines follow the key (empty input still prints only
`--`):

```
scores 11B:<s> 8A:<s> ... (24 camelot:score pairs, libkeyfinder key_t order)
chroma <72 space-separated band magnitudes>
```

The scores are the classifier's cosine similarities of the collapsed
chromagram against the major and minor tone profiles; the first line is always
the argmax of that vector (silence, printed as `--`, floors it at 0). The
chroma line is the input to that classification, so alternative profiles or
mode-bias post-processing can be evaluated offline without re-decoding audio.

Example:

```sh
keyfinder-shim 44100 < mono_f32.raw
```

### Camelot mapping

| libkeyfinder | Camelot | libkeyfinder | Camelot |
| --- | --- | --- | --- |
| `A_MAJOR` | 11B | `A_MINOR` | 8A |
| `B_FLAT_MAJOR` | 6B | `B_FLAT_MINOR` | 3A |
| `B_MAJOR` | 1B | `B_MINOR` | 10A |
| `C_MAJOR` | 8B | `C_MINOR` | 5A |
| `D_FLAT_MAJOR` | 3B | `D_FLAT_MINOR` | 12A |
| `D_MAJOR` | 10B | `D_MINOR` | 7A |
| `E_FLAT_MAJOR` | 5B | `E_FLAT_MINOR` | 2A |
| `E_MAJOR` | 12B | `E_MINOR` | 9A |
| `F_MAJOR` | 7B | `F_MINOR` | 4A |
| `G_FLAT_MAJOR` | 2B | `G_FLAT_MINOR` | 11A |
| `G_MAJOR` | 9B | `G_MINOR` | 6A |
| `A_FLAT_MAJOR` | 4B | `A_FLAT_MINOR` | 1A |
| `SILENCE` | `--` | | |

## Why a separate process

libkeyfinder is GPL-3.0-or-later and FFTW is GPL-2.0-or-later, and both are
linked statically here. Anything that linked this code would take on the same
licence.

Running it as its own executable avoids that. The caller starts a subprocess and
talks to it over a pipe, sharing no code, no headers and no address space, so it
is not a derivative work and keeps its own licence. That is why the contract
above is as thin as it is: an integer argument, a byte stream in, one line out,
with no library type anywhere in it.

## Building

Requirements:

- macOS on Apple Silicon, with the Xcode command line tools.
- CMake 3.20 or newer.
- Network access on the first build — libkeyfinder and FFTW are both fetched.

```sh
./build.sh
```

The binary lands in `dist/keyfinder-shim-arm64-apple-darwin`, stripped. The build
fails rather than warns if the result is the wrong architecture, or if it links
anything beyond `libSystem` and `libc++` — a dynamic dependency on a package
manager prefix would make the binary unusable anywhere else.

Extra arguments are passed through to the CMake configure step. `build/` and
`dist/` are not committed; the binary is reproducible from the pinned sources
below.

arm64 only. Nothing in the source is architecture-specific, so this is a
packaging choice rather than a limitation.

## Dependencies

Both are linked statically, and both are pinned by something upstream cannot
move — so a given commit here always produces the same combination.

| Component | Version | Pinned by |
| --- | --- | --- |
| [libkeyfinder](https://github.com/mixxxdj/libkeyfinder) | 2.2.8 | commit `b33b5a88e04a5182dd19c38c57762925631118fd` |
| [FFTW](https://www.fftw.org/) | 3.3.11 | SHA-256 of the release tarball |

libkeyfinder is pinned by commit rather than by its `2.2.8` tag, because a tag is
a movable ref: upstream re-pointing it would change what builds here with no diff
to show for it.

FFTW is fetched from `fftw.org` and checksummed before extraction, then built
static and double precision. A host or Homebrew FFTW is not used, so the build
does not depend on what the machine happens to have installed.

## Licence

GPL-3.0-or-later. The full text is in [LICENSE](LICENSE); [NOTICE.md](NOTICE.md)
has attribution, the scope of the licence, and what shipping the binary inside
something else requires.

[lkf]: https://github.com/mixxxdj/libkeyfinder
