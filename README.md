# MacPressor

![MacPressor interface](Resources/MacPressor-UI.png)

**Four voices. Two switches. No knobs.**

MacPressor is a JUCE 8 / VST3 automatic compressor built around a deliberately
minimal interface. There is no threshold, ratio, attack or release to set. Pick
the source and the character; the compressor loads a fixed, source-matched
curve and tracks the gain reduction with automatic make-up gain.

## Controls

| Switch | Position | Voice |
| :--- | :--- | :--- |
| **Instrument** | Vocal | Vocal female / male curves |
| | Guitar | Guitar clean / overdrive curves |
| **Character** | Vocal | **Female** or **Male** |
| | Guitar | **Clean** or **Overdrive** |

The header `<` `>` chevrons and the four-position voice rail are shortcuts for
the same single `Voice` parameter exposed to the host. There are no other
plugin parameters. VST3 hosts may also display the standard VST3 **Bypass**
parameter, which is required by the format and is not part of MacPressor's
interface.

## Voice design

Each voice has its own fixed detection high-pass, soft-knee threshold, ratio,
attack/release ballistics, parallel blend and automatic make-up law:

| Voice | Attack | Release | Ratio | Character |
| :--- | ---: | ---: | ---: | :--- |
| Vocal / Female | 8 ms | 140 ms | 3.0:1 | gentle grip, open release |
| Vocal / Male | 13 ms | 180 ms | 3.3:1 | firmer grip, steady body |
| Guitar / Clean | 24 ms | 240 ms | 2.4:1 | pick attack preserved |
| Guitar / Overdrive | 4 ms | 95 ms | 4.0:1 | fast, tight leveling |

A stereo-linked detector keeps the image locked. Make-up gain follows the
gain-reduction envelope so switching voices or source levels stays musical.
A gentle output safety stage catches overs without acting like a second
limiter.

## Install

Build outputs live in `dist/`:

- `MacPressor.vst3` — copy to your VST3 folder (`~/.vst3` on Linux/macOS,
  `C:\Program Files\Common Files\VST3` on Windows).
- `MacPressor` / `MacPressor.exe` / `MacPressor.app` — standalone host.

## Build (Linux)

The project reuses the local JUCE 8.0.8 and sysroot caches when present:

```bash
./scripts/setup-linux.sh
./scripts/build.sh
```

The build produces the VST3, Standalone, DSP tests, VST3 host smoke test and
headless screenshot renderer. The script follows the local sysroot layout from
`../plugin_poc` so the build works offline.

## Tests

```bash
./build/MacPressorTests_artefacts/Release/MacPressorTests
./build/MacPressorHostSmokeTest_artefacts/Release/MacPressorHostSmokeTest \
  dist/MacPressor.vst3
```

The host test verifies that the VST3 exposes exactly one parameter, that all
four voices process audio, that latency is zero, and that state round-trips.

Render a UI preview:

```bash
xvfb-run -a ./build/MacPressorScreenshot_artefacts/Release/MacPressorScreenshot \
  --out=dist/macpressor-ui.png
```

## Visual language

Brushed gunmetal chassis, flush screwless glass overlay, muted crimson LED
edge-glow, a central obsidian live-waveform scope, and two low-profile
machined source switches. No visible fasteners.

## CI / releases

`.github/workflows/build.yml` builds Windows x64, Linux x64 and a macOS
Universal binary, runs the tests and packages `.zip` + `.sha256` artifacts.
Pushing a `v*` tag (or running the workflow manually with a release tag)
publishes a GitHub Release.

## License

MacPressor source is provided under the MIT license. The bundled Lato fonts
are distributed under the SIL Open Font License; see `Resources/Lato-OFL.txt`.
