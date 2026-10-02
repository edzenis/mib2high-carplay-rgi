# AltScreen source provenance

This Audi K2161 implementation is adapted from the GEN2 Stream-111 work in:

harman-f/mhi2_altscreen_carplay

Initial porting baseline:

c2f811f1a5c84dae3a62f4cf9b4a9e65fc3f7b3c

K2161-specific work includes:

- LD_PRELOAD lifecycle interposition instead of executable-text patching
- K2161 AirPlay symbol/runtime adaptation
- screen-key capture through AirPlay_DeriveAESKeySHA512ForScreen
- lazy AirPlayReceiverSessionSendCommand resolution
- preservation of stock Stream 110
- K2161 lifecycle and negotiation diagnostics

Upstream licensing and copyright terms remain applicable to adapted material.
