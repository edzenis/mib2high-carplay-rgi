# MIB2 High CarPlay RGI

CarPlay turn-by-turn route guidance for Audi **MIB2 High / MHI2**, using Apple iAP2 Route Guidance Information (RGI) and the factory Audi BAP navigation interface to drive the **factory HUD**.

## Tested platform

- Audi Q7 4M
- MIB2 High / MHI2
- Firmware: `MHI2_ER_AUG22_K2161`
- MU software: `1421`

## v1.0.0 status

Validated on the vehicle:

- **Apple Maps:** maneuver arrows, numeric distance updates and route-end clear on the factory HUD
- **Google Maps:** maneuver arrows, numeric distance updates and route-end clear on the factory HUD
- sustained iAP2 RGI reception after runtime activation of stock group `0x52`
- clean route-guidance deactivation in the tested Apple Maps and Google Maps sessions

Known limitations:

- **Waze:** in the tested session, `sourceSupportsRouteGuidance=0`; no normal `0x5202` maneuver stream was observed and no HUD arrows were produced
- **Lane guidance (`0x5204`):** transport and parser support are present, but no real lane-guidance frame was observed in the validation routes
- **Native Audi navigation hand-back:** starting native Audi guidance after CarPlay guidance without rebooting the MMI has not yet been validated
- Virtual Cockpit graphical guidance is not claimed by this release; the validated output is the factory HUD

## Architecture

```text
iPhone navigation app
        |
        v
iAP2 Route Guidance
        |
        v
stock K2161 ipod-drvr-iap2.so
        |
        +-- runtime group 0x52 message-table activation
        |
        v
receive observer / RGI parser
        |
        v
localhost native -> Java bus
        |
        v
RouteGuidance -> BAPBridge
        |
        v
K2161 CombiBAPServiceNavi
        |
        v
factory Audi HUD
```

The stock iAP2 driver is **not modified on disk**. The K2161-specific native code validates the expected stock layout and changes only the required writable runtime state in memory.

See [architecture](docs/architecture.md) and [implementation details](docs/implementation.md).

## Build outputs

```text
scripts/build-native.sh
  -> build/libmib2high-carplay-rgi.so

scripts/build-java.sh
  -> build/mib2high-carplay-rgi.jar
```

The native build requires your own QNX SDP 6.5 environment. The Java build requires JDK 8 and a K2161 LSD compile-dependency JAR derived from the target firmware; the OEM-derived JAR is intentionally **not distributed**.

See [build instructions](docs/build.md).

## Installation

This repository contains source and integration documentation, not an automated MMI installer. Runtime integration requires:

1. the RGI message-direction entries in `iap2.cfg`;
2. process-specific preload of the native library into stock `mm-ipod`;
3. the Java bundle registered with the K2161 LSD/OSGi runtime.

See [installation requirements](docs/installation.md).

## Validation

One validated session captured **225 `0x5201` + 23 `0x5202` messages**, with no RGI `Invalid packet` failures and no receive-list exhaustion. See [validation](docs/validation.md).

## Credits

The generic RGI parser/bus and parts of the Java route-guidance layer are adapted from **LuKa's `mib2q-carplay-rgi` work**. K2161/MIB2 High uses a different HARMAN integration path and additional stock-driver/BAP work.

See [NOTICE.md](NOTICE.md) for attribution.
