# Agent Notes

## macOS Spatial Audio

The current macOS 26+ spatial-audio work has two separate goals:

- Keep reliable sound first. SDL is a real macOS backend and remains the control path/fallback for comparison.
- Use the native macOS spatial path as the macOS default for the existing stereo and 5.1 mixer output.
- Build with the newest macOS SDK available, including macOS 27 beta SDKs, but keep the app runtime floor at macOS 26 unless the user explicitly changes that policy again.

Do not describe the current 5.1 bed as Dolby Atmos. The current engine invariant is still simple PCM output: stereo emits 2 interleaved signed 16-bit channels, and Surround 5.1 emits 6 interleaved signed 16-bit channels in `L R C LFE Ls Rs` order. The native macOS backend can spatialize that 5.1 bed, but it does not carry height channels, object metadata, dynamic object motion, or Dolby certification.

For future Atmos-style work, read [docs/macos-spatial-audio-notes.md](docs/macos-spatial-audio-notes.md) before touching the mixer or macOS backend.
