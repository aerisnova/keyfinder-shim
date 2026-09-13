/*
  keyfinder-shim — musical key detection over raw PCM on stdin.

  Copyright (C) 2026 Aeris Nova

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation, either version 3 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program.  If not, see <https://www.gnu.org/licenses/>.

  See LICENSE for the full text and NOTICE.md for attribution.
*/

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

#include "constants.h"
#include "keyfinder.h"
#include "toneprofiles.h"
#include "workspace.h"

namespace {

const char* camelotOf(KeyFinder::key_t key) {
  switch (key) {
    case KeyFinder::A_MAJOR:       return "11B";
    case KeyFinder::A_MINOR:       return "8A";
    case KeyFinder::B_FLAT_MAJOR:  return "6B";
    case KeyFinder::B_FLAT_MINOR:  return "3A";
    case KeyFinder::B_MAJOR:       return "1B";
    case KeyFinder::B_MINOR:       return "10A";
    case KeyFinder::C_MAJOR:       return "8B";
    case KeyFinder::C_MINOR:       return "5A";
    case KeyFinder::D_FLAT_MAJOR:  return "3B";
    case KeyFinder::D_FLAT_MINOR:  return "12A";
    case KeyFinder::D_MAJOR:       return "10B";
    case KeyFinder::D_MINOR:       return "7A";
    case KeyFinder::E_FLAT_MAJOR:  return "5B";
    case KeyFinder::E_FLAT_MINOR:  return "2A";
    case KeyFinder::E_MAJOR:       return "12B";
    case KeyFinder::E_MINOR:       return "9A";
    case KeyFinder::F_MAJOR:       return "7B";
    case KeyFinder::F_MINOR:       return "4A";
    case KeyFinder::G_FLAT_MAJOR:  return "2B";
    case KeyFinder::G_FLAT_MINOR:  return "11A";
    case KeyFinder::G_MAJOR:       return "9B";
    case KeyFinder::G_MINOR:       return "6A";
    case KeyFinder::A_FLAT_MAJOR:  return "4B";
    case KeyFinder::A_FLAT_MINOR:  return "1A";
    case KeyFinder::SILENCE:       return "--";
  }
  return nullptr;
}

float decodeFloat32LE(const unsigned char* b) {
  const uint32_t bits = static_cast<uint32_t>(b[0]) |
                        (static_cast<uint32_t>(b[1]) << 8) |
                        (static_cast<uint32_t>(b[2]) << 16) |
                        (static_cast<uint32_t>(b[3]) << 24);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool readStdinInto(KeyFinder::AudioData& audio, unsigned int& sampleCount) {
  unsigned char buffer[1 << 16];
  size_t carry = 0;
  sampleCount = 0;

  for (;;) {
    const size_t got = std::fread(buffer + carry, 1, sizeof(buffer) - carry, stdin);
    const size_t available = carry + got;
    const size_t whole = available / 4;

    if (whole > 0) {
      audio.addToSampleCount(static_cast<unsigned int>(whole));
      for (size_t i = 0; i < whole; ++i) {
        const float value = decodeFloat32LE(buffer + i * 4);
        audio.setSample(sampleCount++, std::isfinite(value) ? static_cast<double>(value) : 0.0);
      }
    }

    carry = available - whole * 4;
    if (carry > 0) {
      std::memmove(buffer, buffer + whole * 4, carry);
    }

    if (got == 0) {
      if (std::ferror(stdin)) {
        std::fprintf(stderr, "keyfinder-shim: read error on stdin: %s\n", std::strerror(errno));
        return false;
      }
      break;
    }
  }

  if (carry != 0) {
    std::fprintf(stderr,
                 "keyfinder-shim: stdin ended with %zu trailing byte(s); "
                 "input must be a whole number of float32 samples\n",
                 carry);
    return false;
  }
  return true;
}

// The scores mode mirrors KeyClassifier::classify exactly: cosine similarity of
// the collapsed chromagram against the major and minor tone profiles at every
// semitone offset, silence (score 0) as the floor. Kept in step with
// libkeyfinder's keyclassifier.cpp so the reported winner never diverges from
// what keyOfAudio would have returned.
int runWithScores(KeyFinder::AudioData& audio) {
  KeyFinder::KeyFinder finder;
  KeyFinder::Workspace workspace;
  finder.progressiveChromagram(audio, workspace);
  finder.finalChromagram(workspace);
  const std::vector<double> chroma = workspace.chromagram->collapseToOneHop();

  const KeyFinder::ToneProfile major(KeyFinder::toneProfileMajor());
  const KeyFinder::ToneProfile minor(KeyFinder::toneProfileMinor());

  double scores[KEYS];
  for (unsigned int i = 0; i < SEMITONES; ++i) {
    scores[i * 2] = major.cosineSimilarity(chroma, i);
    scores[i * 2 + 1] = minor.cosineSimilarity(chroma, i);
  }

  double bestScore = 0.0;
  KeyFinder::key_t bestMatch = KeyFinder::SILENCE;
  for (unsigned int i = 0; i < KEYS; ++i) {
    if (scores[i] > bestScore) {
      bestScore = scores[i];
      bestMatch = static_cast<KeyFinder::key_t>(i);
    }
  }

  std::printf("%s\n", camelotOf(bestMatch));
  std::printf("scores");
  for (unsigned int i = 0; i < KEYS; ++i) {
    std::printf(" %s:%.10g", camelotOf(static_cast<KeyFinder::key_t>(i)), scores[i]);
  }
  std::printf("\n");
  std::printf("chroma");
  for (size_t i = 0; i < chroma.size(); ++i) {
    std::printf(" %.10g", chroma[i]);
  }
  std::printf("\n");
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  bool scoresMode = false;
  if (argc == 3 && std::strcmp(argv[2], "--scores") == 0) {
    scoresMode = true;
  } else if (argc != 2) {
    std::fprintf(stderr, "usage: keyfinder-shim <sample_rate> [--scores]\n");
    std::fprintf(stderr, "  reads mono float32 little-endian PCM on stdin until EOF\n");
    return 2;
  }

  errno = 0;
  char* end = nullptr;
  const long rate = std::strtol(argv[1], &end, 10);
  if (errno != 0 || end == argv[1] || *end != '\0' || rate < 1 || rate > 768000) {
    std::fprintf(stderr, "keyfinder-shim: invalid sample rate '%s'\n", argv[1]);
    return 2;
  }

  try {
    KeyFinder::AudioData audio;
    audio.setChannels(1);
    audio.setFrameRate(static_cast<unsigned int>(rate));

    unsigned int sampleCount = 0;
    if (!readStdinInto(audio, sampleCount)) {
      return 1;
    }
    if (sampleCount == 0) {
      std::printf("--\n");
      return 0;
    }

    if (scoresMode) {
      return runWithScores(audio);
    }

    KeyFinder::KeyFinder finder;
    const char* camelot = camelotOf(finder.keyOfAudio(audio));
    if (camelot == nullptr) {
      std::fprintf(stderr, "keyfinder-shim: unrecognised key returned by libkeyfinder\n");
      return 1;
    }
    std::printf("%s\n", camelot);
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "keyfinder-shim: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "keyfinder-shim: unknown error\n");
    return 1;
  }
}
