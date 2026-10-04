# K2161 Harman GEN2 -> Audi LVDS port

Reference receiver source: `harman-f/mhi2_altscreen_carplay @ aa19750911d92f44df8d3fe468516ee1dcaefff9`

Target: `MHI2_ER_AUG22_K2161`

Renderer reference: `joeyQuery/MHI2-altScreen @ 59b7fa6d1b81e0da266cf4f70b4cf54c34bc03dc`

## Current architecture

```text
iPhone
  -> Harman GEN2 negotiation/lifecycle
  -> K2161 validated AirPlay hooks
  -> Type-111 AES + H.264
  -> complete Annex-B AU
  -> AV11 over 127.0.0.1:19830
  -> standalone vc_render
  -> NvSS outputDevice 1 / layer 58
  -> Tegra:HDMI0 / 2nd LVDS
  -> Virtual Cockpit
```

The old Harman MOST path is intentionally not part of this branch. The receiver and renderer binaries contain no `/dev/mlb`, `isoTX2`, `MOST150`, or `direct-ts-remux` dependency.

## K2161 receiver profile retained

- SessionSetup: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b02`
- SessionStart: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b02`
- SessionTearDown: inline ARM hook, expected words `0xe92d4ff0 0xe24dd01c`
- SessionPlatformControl: inline ARM hook, expected words `0xe92d4ff0 0xed2d8b04`
- SessionControl: inline ARM hook, expected words `0xe92d4ff0 0xe1a06002`
- AirPlayCopyServerInfo: ordinary LD_PRELOAD interposition
- AES_CBCFrame_Init: master-key capture accepted only from `AirPlayReceiverSessionSetSecurityInfo + 0x38`

Negotiation remains:
- 800x480 @ 30 fps
- widthPhysical=0, heightPhysical=0
- features=0
- primaryInputDevice=0
- UUID `E0CB6FB0-0000-0000-0000-0000C0FFEE58`
- initialURL `maps:/car/instrumentcluster/map`
- suggestUI normalizes only stock `kNotHandledErr (-6714)` while private Stream-111 ownership is active

## AV11 sink boundary

The old raw-H.264 listener boundary was removed from the production path. The receiver is now the AV11 client and `vc_render` is the listener.

Each message is one complete Annex-B AU:

```text
u32 'AV11'
u32 AU length
u64 monotonic timestamp in microseconds
u32 flags (bit 0 = IDR)
AU bytes
```

The sink connection is deliberately independent of CarPlay negotiation. If the renderer is absent, the receiver stays alive and retries. On a failed/stale sink connection, the video consumer is detached and later reattached so the stream is re-primed from an IDR instead of continuing from an arbitrary P-frame.

## Audi renderer

`native/vc_render-k2161/vc_render.c` is the separate-process NvSS renderer derived from the pinned Joey reference. K2161 defaults are:

```text
outputDevice = 1
layer        = 58
width        = 800
height       = 480
fps          = 30
port         = 19830
```

Keeping NvSS ownership in a separate process is intentional; the receiver hook inside `dio_manager` does not own the second NvSS handle.

## Current checkpoint

Source integration + QNX compile/static audit: complete.

Car installation: not done in this checkpoint.

The next step is a separate reversible SD package/runtime-test checkpoint. It must start the standalone renderer, load the receiver hook, switch/restore the proven K2161 cluster display context safely, collect logs, and provide clean rollback.