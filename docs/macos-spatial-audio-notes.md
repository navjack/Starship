# macOS Spatial Audio Notes

These notes are for future work on an Atmos-style object/dynamics model. They are not a claim that the current macOS backend outputs Dolby Atmos.

## Build Policy

The macOS fork targets a macOS 26 minimum runtime and should build against the newest SDK available on the developer machine. macOS 27 beta SDKs are expected and should be used for the richest Apple spatial-audio feature surface, but the bundle should still report `minos 26.0` unless the user explicitly raises the runtime floor again.

## Current Invariant

The engine mixer currently owns the audio image. It emits a finished PCM bed:

- stereo: 2 interleaved signed 16-bit channels
- surround: 6 interleaved signed 16-bit channels
- 5.1 order: `L R C LFE Ls Rs`
- 5.1 CoreAudio layout: `kAudioChannelLayoutTag_AudioUnit_5_1`

The macOS spatial backend must preserve that invariant. It may route the 5.1 bed through Apple spatial rendering, but it must not silently reinterpret channels or invent height/object data after the bed has already been mixed.

## What Atmos-Style Work Actually Requires

An Atmos-style implementation is a new mixer/content model, not a backend flag. The existing 5.1 bed has already lost the information needed for objects: source identity, world position, velocity, spread, priority, and dynamic automation.

A clean design should add an explicit spatial scene layer before final bed mixing. The data should be owned per audio frame and should be cheap to consume from the real-time render callback.

Minimum useful object state:

- stable source id
- mono or stereo source buffer pointer/range
- position in listener space or world space plus listener transform
- gain, spread, and optional low-frequency send
- velocity if doppler or motion smoothing is desired
- priority for object budget pressure
- lifetime/update sequence so stale object state cannot be rendered

Object updates must not allocate, take locks, or parse high-level game state on the audio callback. Push compact commands into a bounded queue, build a frame-local scene, then let the backend consume immutable object state.

## Bed Plus Objects

Keep a bed path even if objects are added. Music, ambience, UI, and legacy mixed content should remain a bed. Dynamic sources such as shots, engines, flybys, explosions, and voice callouts are candidates for objects only if their source data is available before panning is baked into the bed.

The fallback path should be deterministic:

- native object renderer available: render bed plus objects
- native object renderer unavailable: fold objects into the existing stereo or 5.1 mixer with the same source positions
- object budget exceeded: fold low-priority objects into the bed, do not drop them arbitrarily

## macOS Renderer Direction

The current native backend is built around SDL-style queue semantics and AudioToolbox output. If future work uses AudioUnit spatial mixing for multiple sources, keep the backend boundary explicit:

- the game/mixer exports a bed and optional spatial objects
- the macOS backend maps that data to AudioUnit inputs or another Apple spatial API
- backend-specific state stays out of the gameplay mixer

Do not add Dolby-specific branding unless there is an actual Dolby pipeline, licensing story, and runtime verification. "Atmos-style object spatialization" is the accurate engineering term until then.

## Validation

Do not use generated tones for this project unless the user explicitly asks. The expected smoke test is natural menu music immediately after launch.

Useful checks:

- SDL backend: menu music audible, first queue logged, first natural nonzero PCM logged
- macOS spatial stereo: menu music audible, first render request logged, first natural nonzero PCM logged
- macOS spatial 5.1: six-channel init logged with `AudioUnit_5_1 L R C LFE Ls Rs`, natural nonzero PCM logged, AirPods Spatial Audio controls remain exposed
- packaged app: `codesign --verify --deep --strict --verbose=2 Starship.app`
- packaged app: `otool -l Starship.app/Contents/MacOS/Starship` reports `minos 26.0`
