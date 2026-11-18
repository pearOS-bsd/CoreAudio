# CoreAudio

CoreAudio (`/System/Library/Frameworks/CoreAudio.framework`, public) for pearOS: the audio hardware abstraction (`AudioObject*`, devices, streams, volume, default devices), and the daemon behind it.

**Status: planned. No code yet.** `dyld/machrun/shims.c` only answers "file type not supported" so programs try another decoder; nothing plays or records on pearOS (see STATE.md of the project skill, "Known gaps").

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

## Plan (FreeBSD backend)
1. `coreaudiod` in `d` (Mach service `com.apple.audio.coreaudiod`; the real one is `/usr/sbin/coreaudiod`, to be read with REA) owning the devices; the client library only talks to it over XPC / Mach.
2. Devices from `sound(4)`: `/dev/sndstat` (units, play/rec flags, default), `/dev/dspN` for streams (OSS API, `SNDCTL_*`), `/dev/mixer` and `mixer(8)` for volume/mute, `sysctl hw.snd.default_unit` for the default device. The earlier Control Center and `apps/controlcenter-cocoa/CCPlatform.c` already use `mixer -s vol` and `/dev/sndstat` (tested with fake files on the FreeBSD VM).
3. Property model first (the `AudioObject` property API with the selectors above and the standard ones), then streams (`AudioDeviceCreateIOProcID`, the IO proc thread), then format conversion (AudioConverter) in AudioToolbox.
4. Compare with the real framework: REA on the dyld-cache image, `dtrace` of a real app, observe the Mac VM (no audio hardware there: `system_profiler SPAudioDataType` is empty; the Mac VM can still show the property calls).

## Existing code to reuse
`apps/controlcenter-cocoa/CCPlatform.c` (`ccp_volume_*`, `ccp_sound_*`: temporary FreeBSD bindings), `apps/topbar/ControlCenter.m` (mixer(8) with a background queue so a slider drag does not block).

## Rules for this repository
- Our code: BSD-2-Clause. Apple files are never copied here: reference material is read from the user's own Mac (`macdeps`), notes and measured values are written in our own words (`docs/`).
- No GPL/LGPL code. Upstream code, if any, only under `upstream/` with a `PROVENANCE.md`.
- Before writing a function: search the code index and `where-is.sh` (rule 3); do exactly what Apple does, with evidence recorded here (rule 4).
- Test on the FreeBSD VM; CI is the shared `hygiene` workflow (`pearOS-bsd/.github`).
