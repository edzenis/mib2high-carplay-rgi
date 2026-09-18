# Compatibility

## Vehicle-tested target

v1.1 has been vehicle-tested on:

- Audi Q7 4M
- MIB2 High / MHI2
- `MHI2_ER_AUG22_K2161`
- MU 1421

The native hook uses K2161-specific offsets and validates the expected stock driver layout. Do not infer compatibility from the shared MHI2 product family alone.

## Navigation applications

### Apple Maps

Factory-HUD route guidance is vehicle-tested.

### Google Maps

Factory-HUD route guidance is vehicle-tested, including maneuver transitions, short-range distance updates and junction/side-street geometry.

### Waze

A previously tested Waze session reported `sourceSupportsRouteGuidance=0` and did not expose the normal structured `0x5202` maneuver stream. No HUD maneuver output was produced in that session. Compatibility therefore depends on the RGI stream exposed by the iOS/Waze combination rather than on a separate Waze-specific renderer in this project.

## Wireless CarPlay adapters

v1.1 includes recovery for adapters that can reconnect CarPlay without presenting a fresh iAP2 `0x1D00 StartIdentification`. A 10-second RGI-silence detector can soft-rearm the existing subscription at the next stock-safe `0x2700` opportunity. This path was vehicle-tested successfully.

## Virtual Cockpit

v1.1 targets the **factory HUD**. It does not output CarPlay graphical route guidance to the Virtual Cockpit. The integration keeps the K2161 map visibility/presentation path disabled while CarPlay owns route guidance.

## Lane guidance

`0x5204` parsing, lane-event caching, active-event resolution and BAP lane output are implemented. No qualifying real lane-guidance event was encountered during the v1.1 vehicle test, so the visual result remains unvalidated.
