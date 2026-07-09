# Agent Notes

## macOS Spatial Audio

The current macOS 26+ spatial-audio work has two separate goals:

- Keep reliable sound first. SDL is a real macOS backend and remains the control path/fallback for comparison.
- Use the native macOS spatial path as the macOS default for the existing stereo and 5.1 mixer output.
- Build with the newest macOS SDK available, including macOS 27 beta SDKs, but keep the app runtime floor at macOS 26 unless the user explicitly changes that policy again.

Do not describe the current 5.1 bed as Dolby Atmos. The engine invariant is still simple PCM output: stereo emits 2 interleaved signed 16-bit channels, and Surround 5.1 emits 6 interleaved signed 16-bit channels in `L R C LFE Ls Rs` order. The native macOS backend spatializes that 5.1 bed and now has a first positional object path for non-default SFX sources: SFX channels `0..15` map to mono `AUSpatialMixer` point-source buses `1..16`, with bus `0` reserved for the bed. This is object-style positional spatialization, not Dolby certification.

Current caveat: objectized SFX are copied before pan is baked, but they are still also present in the legacy bed. Do not remove them from the bed until the point-source path is verified in real gameplay, or gameplay SFX can become silent if a backend rejects object rendering.

For future Atmos-style work, read [docs/macos-spatial-audio-notes.md](docs/macos-spatial-audio-notes.md) before touching the mixer or macOS backend.
