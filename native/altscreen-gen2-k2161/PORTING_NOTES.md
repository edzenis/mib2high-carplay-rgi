# K2161 Harman GEN2 + Audi AV11/LVDS sink

Reference receiver source: `harman-f/mhi2_altscreen_carplay @ aa19750911d92f44df8d3fe468516ee1dcaefff9`

Audi target: `MHI2_ER_AUG22_K2161`

Base checkpoint: `k2161-gen2-core @ 0d2b8e41642672cba8babdace95b725a00990455`

This stage keeps the validated K2161 Harman GEN2 receiver core and attaches a separate Audi renderer process. It intentionally does **not** restore Harman's MOST / isoTX2 / direct-ts-remux path.

## Receiver invariants retained

- SessionSetup: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b02`
- SessionStart: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b02`
- SessionTearDown: inline ARM hook, expected words `0xe92d4ff0 0xe24dd01c`
- SessionPlatformControl: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b04`
- SessionControl: inline ARM hook, expected words `0xe92d4ff0 0xe1a06002`
- `AirPlayCopyServerInfo`: ordinary LD_PRELOAD interposition
- `AES_CBCFrame_Init`: master key accepted only from `AirPlayReceiverSessionSetSecurityInfo + 0x38`
- negotiation profile: 800x480@30, physical 0x0, features=0, primaryInputDevice=0
- suggestUI: only stock `kNotHandledErr (-6714)` is normalized while private Stream-111 ownership is active

## Audi sink architecture

```text
Type-111 AVCC H.264
 -> validated complete AU
 -> Annex-B AU (+ SPS/PPS on first IDR from core priming)
 -> AV11 frame: u32 magic | u32 len | u64 timestamp | u32 flag | AU
 -> TCP 127.0.0.1:19830
 -> standalone vc_render-k2161
 -> NvSSVideoOpen(outputDevice=1, layer=58)
 -> NvSSVideoStreamConfigure(800x480@30)
 -> NvSSVideoDecode
 -> Tegra:HDMI0 / 2nd LVDS / Virtual Cockpit
```

The receiver is the TCP client and `vc_render` is the listener. Renderer absence/restart does not own or tear down the CarPlay receiver session. Reconnect is attempted on loopback and a fresh AltScreen consumer generation is attached after reconnection. The core refuses partial-AU offsets at the AV11 boundary and re-primes instead of framing a continuation as a new AU.

The first IDR after consumer attach is already gated by `alt111_video`: non-IDR pictures are dropped until priming, and the committed codec config is prepended to the first priming IDR. This provides the renderer with SPS/PPS + IDR without importing Joey's MU0678 session code.

## Standalone renderer provenance

Renderer basis: `joeyQuery/MHI2-altScreen @ 59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`, `src/vc_render/vc_render.c`.

K2161 adaptation is deliberately minimal:

- default geometry changed from Joey's A4 1440x456 to the current K2161 negotiation baseline 800x480
- outputDevice remains 1
- layer remains 58
- port remains 19830
- QNX 6.5 build uses `-Wc,-std=gnu99`

The renderer remains a separate process so `dio_manager` never owns a second NvSS handle.

## Remaining live proof

This checkpoint is compiled and statically audited only. Before calling it vehicle-proven, the first live package must confirm on the Q7:

- `vc_render` resolves `libKD.so` / `libnvss_video.so`
- `NvSSVideoOpen(1, layer 58)` succeeds while stock Stream-110 is active
- real Stream-111 AUs reach AV11 and decode
- display/context 77 exposes layer 58 on the Virtual Cockpit
- unplug/replug and CarPlay teardown leave stock Stream-110 and cluster restoration stable

The SD installer/package remains a separate step.
