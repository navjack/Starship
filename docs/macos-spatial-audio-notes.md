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

## Current macOS Native Backend

The native macOS backend uses AudioToolbox with SDL-style queue semantics. Stereo is rendered to the default output. Surround 5.1 is rendered through `AUSpatialMixer` as an `AmbienceBed` with:

- input layout `kAudioChannelLayoutTag_AudioUnit_5_1`
- source mode `kSpatialMixerSourceMode_AmbienceBed`
- spatialization algorithm `kSpatializationAlgorithm_UseOutputType`
- output type `kSpatialMixerOutputType_Headphones`
- head tracking enabled through `kAudioUnitProperty_SpatialMixerEnableHeadTracking`
- personalized HRTF mode set to system-auto

The backend receives a compact `StarshipSpatialAudioFrame` each audio update. That frame mirrors active SFX identity, channel, legacy view-relative position, optional ship-relative position, distance, pan, gain, reverb, and listener angles after the game has chosen active SFX channels. The legacy `x/y/z` values remain camera/view-relative because the original panner and bed mixer depend on that convention. Object audio uses the optional ship-relative coordinates when available.

The backend also has a first positional object path for macOS 5.1:

- bus 0 remains the 5.1 ambience bed
- buses 1-16 are mono `AUSpatialMixer` point-source inputs, one per SFX sequencer channel
- moving SFX with non-default `sfxSource` pointers are copied from `DMEM_TEMP` after final resample/gain and before `AudioSynth_ProcessEnvelope` bakes pan into the bed
- legacy `x/y/z` positions come from the same view-relative source coordinates used by the existing SFX pan logic, including render/camera transform helpers such as `Object_SetSfxSourceToPos`, `Object_UpdateSfxSource`, and `Object_SetSfxSourceToView`
- ship-relative object coordinates are stored in a fixed source registry keyed by the existing `sfxSource` pointer whenever the game knows the object's world position; direct object-position helpers, central actor/boss/effect/item/scenery draw paths, and projectile updates publish these coordinates
- `AUSpatialMixer` global head yaw/pitch/roll stay at zero for object coordinates; AirPods head tracking then applies the user's physical head motion as the only extra rotation
- player projectile launch sounds use the projectile `sfxSource`, and active projectile sources are refreshed from their visual object position during `PlayerShot_UpdateShot`
- default-source/system/comms-style sounds stay in the bed so they remain centered/in-head
- accepted object SFX update their normal envelope state but skip `aEnvMixer` for that audio slice, so the sound is owned by the object bus instead of duplicated in the bed
- if the native backend is unavailable or cannot queue the whole PCM block, the object submit fails and the legacy bed mix runs unchanged
- object bus ownership is tied to the SFX channel plus source token; when a channel is reused before the prior object's queued PCM drains, the new SFX remains in the bed instead of retargeting old object audio to a new position

This is positional object augmentation, not Dolby Atmos.

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

For real object motion, add mono point-source inputs to `AUSpatialMixer` and feed them pre-pan SFX sample streams. Do not try to recover object audio by splitting the finished 5.1 bed. A practical implementation path is:

- keep music, UI, voice, ambience, and low-priority legacy content in the bed
- preserve per-note or per-SFX PCM for object candidates before `AudioSynth_ProcessEnvelope` bakes pan into channel volumes
- assign stable object buses from the active SFX table; the current macOS path maps SFX channel `0..15` to SpatialMixer buses `1..16`
- update `kSpatialMixerParam_Azimuth`, `kSpatialMixerParam_Elevation`, `kSpatialMixerParam_Distance`, `kSpatialMixerParam_Gain`, and reverb blend per object bus
- remove accepted object SFX from the bed only when the backend confirms ownership of the whole PCM block; otherwise keep the bed path audible
- fold objects back into the bed deterministically when bus budget is exhausted
- keep AirPods head tracking as a listener transform, not as a substitute for source coordinates

Do not add Dolby-specific branding unless there is an actual Dolby pipeline, licensing story, and runtime verification. "Atmos-style object spatialization" is the accurate engineering term until then.

## Validation

Do not use generated tones for this project unless the user explicitly asks. The expected smoke test is natural menu music immediately after launch.

Useful checks:

- SDL backend: menu music audible, first queue logged, first natural nonzero PCM logged
- macOS spatial stereo: menu music audible, first render request logged, first natural nonzero PCM logged
- macOS spatial 5.1: six-channel init logged with `AudioUnit_5_1 L R C LFE Ls Rs`, natural nonzero PCM logged, AirPods Spatial Audio controls remain exposed
- macOS object path: in real gameplay, look for `macOS spatial backend SFX object PCM` and `macOS spatial backend first object render request`; the object PCM line includes `xyz`, `azimuth`, `elevation`, and `distanceHint`
- macOS object path: accepted object PCM logs include `positionSpace=ship-relative` when the new ship-centered source coordinates are active, or `positionSpace=legacy-view` for sources that still only have legacy coordinates
- macOS object path: the backend logs the first 8 accepted object PCM blocks by default; set `STARSHIP_SPATIAL_OBJECT_LOG_LIMIT` to a higher integer before launch if a longer human-test trace is needed
- packaged app: `codesign --verify --deep --strict --verbose=2 Starship.app`
- packaged app: `otool -l Starship.app/Contents/MacOS/Starship` reports `minos 26.0`
