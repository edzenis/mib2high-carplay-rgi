# K2161 GEN2 receiver-core build audit

Source branch: `k2161-gen2-core`
Source head used for first local build: `2b33e0d74c3adfb02ff70e3b9082c3214eab6c3c`

Compiler:
`arm-unknown-nto-qnx6.5.0eabi-gcc (GCC) 4.9.4`

Output:
- file: `build/libaltscreen111-k2161.so`
- size: 213291 bytes
- SHA256: `4d1057d4f7ccf4eb4f9b9486905023567701a9f3a7b648ffb516ec8b40ef069a`
- second independent local rebuild: byte-for-byte identical

ELF:
- ELF32 little-endian ARM
- EABI5
- shared object
- ARMv7-A
- softfp build
- not stripped
- SONAME `libaltscreen111-k2161.so`
- DT_NEEDED: `libsocket.so.3` (same behavior as the Luka-GCC4.9 Run-8 control build)

Exported interception surface:
- `AirPlayCopyServerInfo`
- `AirPlayReceiverSessionSetup`
- `AirPlayReceiverSessionStart`
- `AirPlayReceiverSessionTearDown`
- `AirPlayReceiverSessionPlatformControl`
- `AirPlayReceiverSessionControl`
- `AES_CBCFrame_Init`

Static gates:
- no `/dev/mlb`, `isoTX2`, `MOST150`, or `direct-ts-remux` strings
- K2161 TearDown prologue validation is `0xe92d4ff0 0xe24dd01c`
- exact master-key caller gate is `AirPlayReceiverSessionSetSecurityInfo + 0x38`
- K2161 Run-8 profile is 800x480@30, physical 0x0, features 0, primaryInputDevice 0
- suggestUI only normalizes stock `kNotHandledErr (-6714)`; other stock errors remain authoritative
- five lifecycle/control inline hooks fail closed if live prologue validation fails

This artifact is receiver-core only. Audi AV11/NvSS/LVDS output integration is deliberately the next stage.
