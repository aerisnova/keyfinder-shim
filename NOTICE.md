# Licensing

keyfinder-shim is free software under the **GNU General Public License, version 3
or (at your option) any later version**. The full text is in [LICENSE](LICENSE).

    keyfinder-shim — musical key detection over raw PCM on stdin
    Copyright (C) 2026 Aeris Nova

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <https://www.gnu.org/licenses/>.

## Why GPL

Linkage. keyfinder-shim statically links two GPL libraries, so the combined work
is GPL too — GPL-3.0-or-later, because that is the stronger of the two:

| Component | Version shipped | Licence | Copyright |
| --- | --- | --- | --- |
| [libkeyfinder](https://github.com/mixxxdj/libkeyfinder) | 2.2.8, commit `b33b5a88e04a5182dd19c38c57762925631118fd` | GPL-3.0-or-later | Ibrahim Sha'ath and the Mixxx project |
| [FFTW](https://www.fftw.org/) | 3.3.11 | GPL-2.0-or-later | Matteo Frigo and the Massachusetts Institute of Technology |

FFTW is GPL-2.0-**or-later**, so it can be used under GPL-3; that option is what
makes the combination possible, and GPL-3 then governs the whole.

Both are pinned by something upstream cannot move — a commit, and a SHA-256 of
the release tarball. See [README](README.md#dependencies). That is a
licensing requirement as much as a build one: a source offer has to name the
version that was actually linked in.

## What this licence does not reach

keyfinder-shim is a separate executable. It is started as a subprocess and
spoken to over a pipe: an integer argument in, a byte stream in, one line of
text out. It shares no code, no headers and no address space with whatever
starts it.

A program that merely runs it is therefore not a derivative work of it, and is
not covered by this licence. That separation is the entire reason this exists as
its own program rather than as a library call, and the interface is kept
deliberately thin — nothing in the contract exposes a libkeyfinder type — so
that the boundary stays obvious. See [README](README.md#licence).

## Distributing a binary

If you ship a keyfinder-shim binary inside something else, GPL-3 §4 and §6 apply
to that binary: pass on a copy of [LICENSE](LICENSE), and either include the
corresponding source or give a written offer for it. The pinned versions above
are what the offer has to name.
