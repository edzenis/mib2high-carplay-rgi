# K2161 GEN2 + Audi LVDS sink build audit

Audited code head: `7d514447f514ce2fc6c2d268ee19c9a8a18f272e`

Compiler:
`arm-unknown-nto-qnx6.5.0eabi-gcc (GCC) 4.9.4`

This checkpoint connects the previously frozen K2161 Harman GEN2 receiver core to the separate Audi renderer over an AV11 loopback transport. It is a compile/static-audit checkpoint only; it has not been installed or runtime-tested in the car.

## Receiver / AV11 producer

Output:
- file: `build/libaltscreen111-k2161.so`
- size: 214464 bytes
- SHA256 from the clean exported-source audit tree: `49ca7db82b2f60f247ba052372292a259d9f0271cc788b2d9ce184d54adae506`
- second consecutive rebuild in the same clean tree: byte-for-byte identical

ELF:
- ELF32 little-endian ARM
- EABI5
- shared object
- debug info present / not stripped
- DT_NEEDED: `libsocket.so.3`

Exported interception surface remains:
- `AirPlayCopyServerInfo`
- `AirPlayReceiverSessionSetup`
- `AirPlayReceiverSessionStart`
- `AirPlayReceiverSessionTearDown`
- `AirPlayReceiverSessionPlatformControl`
- `AirPlayReceiverSessionControl`
- `AES_CBCFrame_Init`

The GEN2 receiver now sends one complete Annex-B access unit per AV11 message to `127.0.0.1:19830`:

```text
u32 magic = 'AV11'
u32 len
u64 timestamp_us
u32 flag       # bit 0 = IDR
[len bytes complete Annex-B H.264 AU]
```

The producer reconnects on demand, uses a bounded socket send timeout, detaches/re-primes the consumer after sink failure, and requests a fresh keyframe when a new consumer is attached. Negotiation, lifecycle hook profiles, K2161 TearDown validation, and exact SetSecurityInfo + 0x38 key capture were not changed.

## Standalone Audi renderer

Source provenance:
- `joeyQuery/MHI2-altScreen`
- pinned reference commit: `59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`
- imported renderer source blob before the K2161 geometry adaptation: `0191a785eb2040eec1cb3926b5c0a2454fa34242`

K2161 adaptation:
- default outputDevice = 1
- default layer/displayable = 58
- default geometry = 800x480 @ 30 fps
- default AV11 port = 19830

Output:
- file: `build/vc_render-k2161`
- size: 29471 bytes
- SHA256 from the clean exported-source audit tree: `73ed71d2c00a1363baa6e0c65950bb5a18a50b08291dbf0406917f2dd6f8a177`
- second consecutive rebuild in the same clean tree: byte-for-byte identical

ELF:
- ELF32 little-endian ARM
- EABI5
- executable
- QNX interpreter `/usr/lib/ldqnx.so.2`
- debug info present / not stripped
- DT_NEEDED: `libsocket.so.3`, `libc.so.3`

The renderer dynamically resolves:
- `libKD.so`
- `libnvss_video.so`
- `kdInitializeNV`
- `NvSSVideoOpen`
- `NvSSVideoStreamConfigure`
- `NvSSVideoGetAttribs`
- `NvSSVideoDecode`
- `NvSSVideoClose`

## Static gates

Passed:
- both targets compile without compiler warnings in the audited build
- both targets rebuild byte-for-byte identically in the same clean exported-source tree
- receiver still exports all seven expected AirPlay/AES interception symbols
- binary string scan finds no `/dev/mlb`, `isoTX2`, `MOST150`, or `direct-ts-remux`
- AV11 producer and renderer agree on port 19830 and the 20-byte little-endian header
- renderer keeps NvSS ownership outside `dio_manager`
- K2161 defaults are outputDevice 1 / layer 58 / 800x480@30

Not claimed by this audit:
- no live Q7 NvSS open/decode test yet
- no DisplayManager context-switch lifecycle has been packaged here
- no unplug/replug/teardown runtime test yet
- no SD installer/package is included in this checkpoint

The next step is deployment packaging and a reversible first-car runtime test.