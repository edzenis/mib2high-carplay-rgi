# K2161 Harman GEN2 receiver-core port

Reference source: harman-f/mhi2_altscreen_carplay @ aa19750911d92f44df8d3fe468516ee1dcaefff9

Target: MHI2_ER_AUG22_K2161

This branch intentionally contains only the CarPlay/AirPlay receiver core. It does **not** contain the Harman MOST / isoTX2 output path and does not yet contain the Audi AV11/NvSS/LVDS sink.

## K2161 hook profile

- SessionSetup: inline ARM hook, expected words 0xe92d4ff0 0xed2d8b02
- SessionStart: inline ARM hook, expected words 0xe92d4ff0 0xed2d8b02
- SessionTearDown: inline ARM hook, expected words 0xe92d4ff0 0xe24dd01c
- SessionPlatformControl: inline ARM hook, expected words 0xe92d4ff0 0xed2d8b04
- SessionControl: inline ARM hook, expected words 0xe92d4ff0 0xe1a06002
- AirPlayCopyServerInfo: ordinary LD_PRELOAD interposition; K2161 _requestProcessInfo calls through PLT
- AES_CBCFrame_Init: ordinary LD_PRELOAD interposition; accept master-key capture only from AirPlayReceiverSessionSetSecurityInfo + 0x38

## K2161 negotiation baseline

The initial profile deliberately reuses the exact Run-8 advertisement identity:
- 800x480 @ 30 fps
- widthPhysical=0, heightPhysical=0
- features=0
- primaryInputDevice=0
- UUID E0CB6FB0-0000-0000-0000-0000C0FFEE58
- initialURL maps:/car/instrumentcluster/map

The Harman GEN2 lifecycle/control/resync logic is retained. The suggestUI normalization is tightened for K2161: only stock kNotHandledErr (-6714) is converted to success while private Stream-111 ownership is active. Other stock errors are preserved.

## Next stage

Attach the existing Audi AV11/NvSS renderer path:
Type111 H.264 -> AV11 -> vc_render -> NvSS outputDevice 1 / layer 58 -> Tegra:HDMI0 -> 2nd LVDS.
