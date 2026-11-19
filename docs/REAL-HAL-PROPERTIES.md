# What the real CoreAudio says (macOS 26.7.2 / 27.0, an M1 MacBook Air with a monitor on DisplayPort)

Measured with `tools/audio-probe` (public HAL API only: `AudioObjectHasProperty / GetPropertyDataSize / GetPropertyData / IsPropertySettable`, `AudioObjectShow`, `AudioHardwareServiceGetPropertyData`), 2026-11-19.  The probe asks 70 selectors in 4 scopes (global, input, output, play-through) and 3 elements; this is the part that matters for a clone.  Values are of this machine; sizes and kinds are what a clone must reproduce.  The raw output is not kept in the repository (it names the user's devices).

## Object model
| object | id | class | notes |
|---|---|---|---|
| system object | 1 | `asys` | owns 46 objects (ids from 0x20: plug-ins, the box and transport objects, the devices) |
| device | 0x47 (speakers), 0x4e (microphone), 0x53 (display audio) | `adev` (`vea` in memory), base class `aobj` | built-in devices have transport `'nitl'` (built-in), the display `'trpd'` (DisplayPort); each owns its streams and controls |
| stream | 0x48 / 0x4f / 0x54 (the id after the device) | `astm` | owns nothing; stream class `'astm'`, base `aobj` |
| controls | ids after the streams | `vlme` (volume), `mute`, `dsrc` (data source), `jack` ... | listed by the device's `'ctrl'` property |
The ids are consecutive 32-bit numbers handed out by `coreaudiod` while it enumerates (a device, then its streams, controls).  The user-visible default devices: `'dOut'` (default output) 0x47, `'dIn '` (default input) 0x4e, `'sOut'` (system output) 0x47.

## System object (`kAudioObjectSystemObject`, 1): the properties that answer
`'stdv'` (settable no, UInt32 0, "kAudioObjectPropertyCreator"-like version), `'dev#'` (device list: 3 x UInt32), `'dOut'`, `'dIn '`, `'sOut'` (UInt32, settable yes), `'ownd'` (owned objects), `'clas'` (`'asys'`).  The HAL ignores scope and element on the system object: the same answer in every scope and element.

## Device (the properties of a built-in speaker/microphone and of a display device)
| selector | scopes | size | settable | value on the machine |
|---|---|---|---|---|
| `'stdv'` | all | 4 | no | 1 |
| `'lnam'` (name), `'lmak'` (manufacturer), `'uid '` | all | 8 (a CFStringRef, 8 bytes on 64 bits) | no | "MacBook Air Speakers" / "Apple Inc." / "BuiltInSpeakerDevice" |
| `'name'` (C string, mac-roman) | all | length + 1 | no | the same as `'lnam'` |
| `'tran'` (transport) | all | 4 | no | `'nitl'` built-in, `'trpd'` DisplayPort |
| `'ownd'`, `'ctrl'`, `'stm#'` | global, per scope | n x 4 | no | the owned objects, the controls, the streams of that scope (size 0 where the device has no stream in that scope: an output-only device has `'stm#'` size 0 in input) |
| `'clas'`, `'bcls'` | all | 4 | no | `'adev'`, `'aobj'` |
| `'nsrt'` (nominal sample rate) | all | 8 (Float64) | yes | 48000.0 |
| `'nsr#'` (available rates) | all | n x 16 (AudioValueRange) | no | 44100, 48000, 88200, 96000 ... (speakers: eight ranges) |
| `'bsiz'` (buffer frame size) | global, in, out | 4 | yes | 4096 (output) / 512 (input) |
| `'bsz#'` (buffer size range) | all | 16 | no | 14 to 32768-ish (output), 15 to 4096 (input) |
| `'fsiz'` | all | 4 | yes | 512 |
| `'dflt'`, `'sflt'` (is default / system default) | input and output | 4 | no | 1 for the default one |
| `'ioct'` | all | 8 | no | an opaque pointer-like value (an io counter object) |
| `'srnd'` (preferred stereo channels / channel map) | input or output | 32 / 52 | yes | channel numbers 1, 2 |
| `'vmvc'` / `'volm'` (virtual main volume / volume) | output (speakers), input (microphone) | 4 | yes | Float32 0.0 .. 1.0 (0.625 here) |
| `'mute'` | output / input | 4 | yes | 0 |
| `'lpcm'`, `'rngb'` ... | only where the device has them | | | |

## What follows for the implementation
* One object table with the ids above; a property is found by (object, selector, scope, element), most selectors answer in every scope.
* Volume and mute exist on the speakers (output scope) and the microphone (input scope) and not on a display device.
* The values come from `sound(4)` on FreeBSD (`/dev/sndstat`: units, play/record flags; `hw.snd.default_unit`; `mixer(8)` or `SOUND_MIXER_*` ioctls for volume and mute).
* Not measured yet: the stream properties (`'sfmt'` physical format, `'vfmt'`, `'lat '`, `'sdev'`), what `coreaudiod` sends on a change (listener blocks), the IO proc.
