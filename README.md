# CoreAudio

CoreAudio (`/System/Library/Frameworks/CoreAudio.framework`, public) for pearOS: the audio hardware abstraction (`AudioObject*`, devices, streams, volume, default devices), and the daemon behind it.

**Status: planned; the real HAL is measured (`tools/audio-probe`, `docs/REAL-HAL-PROPERTIES.md`), no daemon yet.** `dyld/machrun/shims.c` only answers "file type not supported" so programs try another decoder; nothing plays or records on pearOS (see STATE.md of the project skill, "Known gaps").

## Layer
Layer 2 (core frameworks). Depends on: `CoreFoundation`, `libdispatch`, `libxpc`, `IOKit` (user side). Used by: `AudioToolbox` (to be written, same layer), `AVFoundation`, `AppKit` (sounds), `ControlCenter` (volume and output choice), the Dock (system sounds).

## Why it exists now: what the Control Center needs
Measured with the real ControlCenter on macOS 26.7.2 and our replacement `apps/controlcenter-cocoa` (docs/development/CONTROLCENTER-REPLACEMENT.md):
| need | Apple API (as called in `apps/controlcenter-cocoa/CCControls.m`) |
|---|---|
| list the output devices | `AudioObjectGetPropertyDataSize` / `AudioObjectGetPropertyData` on `kAudioObjectSystemObject` (1), selector `'dev#'` (`kAudioHardwarePropertyDevices`), scope `'glob'`; a device is an output when selector `'stm#'` with scope `'outp'` has a non-zero size; its name: selector `'lnam'` (`kAudioObjectPropertyName`, a CFString) |
| the default output | selector `'dOut'` (`kAudioHardwarePropertyDefaultOutputDevice`), get and `AudioObjectSetPropertyData` |
| the volume slider | the main volume of the device: `kAudioHardwareServiceDeviceProperty_VirtualMainVolume` (`'vmvc'`, in AudioToolbox's AudioHardwareService); **not done in our Control Center yet** (it stores the value only) |
Not traced yet: the Sound slider of the real panel (which property it writes), the mute state, device-change notifications (`AudioObjectAddPropertyListenerBlock`). The real ControlCenter also reaches `com.apple.coremedia.volumecontroller.xpc` and `com.apple.coremedia.routingcontext.xpc` (its startup trace, research/cc-trace): the routing side belongs to a later repository (CoreMedia).

## Principle: an open-source macOS, not a lookalike
`CoreAudio.framework`, `AudioToolbox` and (the audio of) `AVFoundation` are written here, and `coreaudiod` in `d`, **wire-compatible with Apple's**: the same MIG subsystem and messages between the client library and `coreaudiod`, the same layouts in the shared memory of the IO, the same API, ABI and behaviour.  No protocol of our own.  Checked in both directions: a real Apple client (from the cache, for tests only) against our `coreaudiod`, and our client against the real `coreaudiod` on the Mac VM; `tools/audio-probe` compares the properties one by one.

## Plan (FreeBSD backend)
1. Reconstitute the real protocol: decompile the client side (`HALC_*` in the framework of the cache) and `/usr/sbin/coreaudiod` with REA, observe the messages on the Mac VM (a stub service on `com.apple.audio.coreaudiod` logs ids and sizes first, as `wsbridge` did), write the findings in `docs/`.
2. `coreaudiod` in `d` (Mach service `com.apple.audio.coreaudiod`) owning the objects; devices from `sound(4)`: `/dev/sndstat` (units, play/rec flags), `/dev/dspN` for streams (OSS API), `/dev/mixer` / `SOUND_MIXER_*` for volume and mute, `hw.snd.default_unit`.
3. The property model first (the ids, the selectors and sizes of `docs/REAL-HAL-PROPERTIES.md`, notifications), then streams and the IO (`AudioDeviceCreateIOProcID`, the real-time thread, the shared memory and timestamps as Apple lays them out; needs the Mach primitives of `darwinator`), then `AudioComponentRegistrar` and the system sound server, then AudioToolbox (AudioComponent / AudioUnit, AudioQueue, AudioConverter for PCM, AudioFile / ExtAudioFile) and the audio of AVFoundation.  Codecs only from permissive sources (no GPL/LGPL).
4. Verify as above, in QEMU with an Intel HDA device and `-audiodev wav` (the output is recorded), and against the Mac.
5. Connect Control Center's Sound slider and output list (replacing the temporary FreeBSD bindings in `apps/controlcenter-cocoa/CCPlatform.c`).

## Existing code to reuse
`apps/controlcenter-cocoa/CCPlatform.c` (`ccp_volume_*`, `ccp_sound_*`: temporary FreeBSD bindings), `apps/topbar/ControlCenter.m` (mixer(8) with a background queue so a slider drag does not block).

## Rules for this repository
- Our code: BSD-2-Clause. Apple files are never copied here: reference material is read from the user's own Mac (`macdeps`), notes and measured values are written in our own words (`docs/`).
- No GPL/LGPL code. Upstream code, if any, only under `upstream/` with a `PROVENANCE.md`.
- Before writing a function: search the code index and `where-is.sh` (rule 3); do exactly what Apple does, with evidence recorded here (rule 4).
- Test on the FreeBSD VM; CI is the shared `hygiene` workflow (`pearOS-bsd/.github`).
