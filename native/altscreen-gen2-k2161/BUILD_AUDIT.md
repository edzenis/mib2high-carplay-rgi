# K2161 GEN2 + AV11/LVDS build audit

Base receiver checkpoint: `k2161-gen2-core @ 0d2b8e41642672cba8babdace95b725a00990455`

Compiler:
`arm-unknown-nto-qnx6.5.0eabi-gcc (GCC) 4.9.4`

## Receiver + AV11 producer

Output:
- file: `build/libaltscreen111-k2161.so`
- size: 214464 bytes
- SHA256: `fb7fbc00665ba816a1171de7028fa166253ab832984ac53dd070b58e6189bd5f`
- second independent rebuild: byte-for-byte identical

ELF:
- ELF32 little-endian ARM
- EABI5
- shared object
- not stripped
- SONAME `libaltscreen111-k2161.so`
- DT_NEEDED: `libsocket.so.3`

Exported interception surface remains:
- `AirPlayCopyServerInfo`
- `AirPlayReceiverSessionSetup`
- `AirPlayReceiverSessionStart`
- `AirPlayReceiverSessionTearDown`
- `AirPlayReceiverSessionPlatformControl`
- `AirPlayReceiverSessionControl`
- `AES_CBCFrame_Init`

AV11 producer gates:
- default target `127.0.0.1:19830`
- complete AU only (`ticket.offset == 0`)
- max AU bounded by `ALT111_AU_LIMIT` (2 MiB)
- one AV11 header per complete Annex-B AU
- IDR flag comes from validated core ticket
- send timeout is configured; renderer failure detaches/re-primes the consumer instead of tearing down the receiver session

## Standalone renderer

Source basis: `joeyQuery/MHI2-altScreen @ 59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`

Output:
- file: `build/vc_render-k2161`
- size: 29471 bytes
- SHA256: `d50b4fa3fbd635bc59e2ea8f4c3a0c5f1446642d9c3333d39bf775b555fbd61c`
- second independent rebuild: byte-for-byte identical

ELF:
- ELF32 little-endian ARM
- EABI5
- executable
- QNX interpreter `/usr/lib/ldqnx.so.2`
- not stripped
- DT_NEEDED: `libsocket.so.3`, `libc.so.3`

Defaults:
- loopback port 19830
- outputDevice 1
- layer 58
- 800x480@30

The renderer dynamically loads:
- `libKD.so`
- `libnvss_video.so`

and resolves:
- `kdInitializeNV`
- `NvSSVideoOpen`
- `NvSSVideoStreamConfigure`
- `NvSSVideoGetAttribs`
- `NvSSVideoDecode`
- `NvSSVideoClose`

## Static gates

- no `/dev/mlb`, `isoTX2`, `MOST150`, or `direct-ts-remux` dependency in executable source or receiver binary
- K2161 TearDown prologue validation remains `0xe92d4ff0 0xe24dd01c`
- exact master-key caller gate remains `AirPlayReceiverSessionSetSecurityInfo + 0x38`
- K2161 profile remains 800x480@30, physical 0x0, features=0, primaryInputDevice=0
- suggestUI still only normalizes stock `-6714`
- five lifecycle/control inline hooks remain fail-closed on prologue mismatch
- receiver core contains no direct NvSS ownership; NvSS stays in the standalone renderer process

This checkpoint is ready for packaging and controlled vehicle runtime validation, not yet declared vehicle-proven.
