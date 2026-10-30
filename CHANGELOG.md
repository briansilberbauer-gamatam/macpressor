# Changelog

All notable changes to MacPressor are documented here.

## [1.0.1] - 2026-10-30

### Fixed

- **Windows: host bypass could make the plug-in look dead.** The VST3 wrapper
  routes bypassed blocks to `processBlockBypassed`; MacPressor did not override
  it, so the waveform ring and every meter stopped updating while the host had
  the unit bypassed. Bypassed blocks now keep the audio dry but still publish
  scope/meter telemetry, and the editor labels the state **HOST BYPASS / DRY**.
- **Windows: stale bus layout silenced a channel.** `prepareToPlay` snapshotted
  the host's channel count and `processBlock` cleared everything beyond it. Hosts
  that prepare a mono (or not-yet-activated) bus and then deliver stereo buffers
  had the right channel muted and the detector running mono. The engine is now
  always prepared for stereo and processes the channels actually present.
- **Scope window collapsed every time the ring wrapped.** The display treated
  the masked 0..8191 ring position as a sample count, so the window shrank to
  zero after each wrap (and after every `prepareToPlay` reset). The processor now
  publishes a valid-sample count and exposes `copyLatestWaveformSamples()`, which
  always returns the newest full window.
- The scope reports **AWAITING HOST AUDIO** and **NO INPUT SIGNAL / CHECK
  ROUTING** so a silent or mis-routed host bus is no longer mistaken for a
  broken compressor.

### Added

- `MacPressorProcessorTests`: end-to-end processor regressions for stereo/mono
  buffers, prepare-vs-process channel mismatches, huge and zero-length blocks,
  host bypass and the waveform telemetry contract.
- The VST3 host smoke test now asserts real peak reduction through the wrapper.
- The screenshot renderer verifies that the scope contains a live crimson
  waveform (pixel count, column coverage and vertical span) and fails otherwise.
- CI renders and verifies the UI on Linux and (best-effort) on Windows, and
  uploads the rendered PNG; both jobs run the new processor tests.

## [1.0.0] - 2026-10-26

### Added

- Initial public release of MacPressor, a four-voice automatic compressor.
- Source selector: Vocal / Guitar.
- Character selector: Female / Male for vocals; Clean / Overdrive for guitar.
- Voice-specific fixed attack, release, ratio, soft knee, detector high-pass,
  parallel blend and automatic make-up gain.
- Stereo-linked, zero-latency processing with a gentle output safety stage.
- Live output waveform scope, gain-reduction and input/output metering.
- Brushed gunmetal and flush-glass interface with crimson LED edge-glow.
- VST3 and Standalone builds, offline DSP tests, VST3 host smoke test and
  cross-platform GitHub Actions CI/release packaging.
