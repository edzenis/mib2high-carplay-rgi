# MIB2 High CarPlay RGI

CarPlay turn-by-turn route guidance for Audi **MIB2 High / MHI2**, using Apple iAP2 Route Guidance Information (RGI) and the factory Audi BAP navigation interface to drive the **factory HUD**.

## Tested platform

- Audi Q7 4M
- MIB2 High / MHI2
- Firmware: `MHI2_ER_AUG22_K2161`
- MU software: `1421`

## v1.2.0-dev.3 development checkpoint

v1.2.0-dev.3 has been vehicle-tested on the K2161 target.

Confirmed on the vehicle:

- factory HUD RGI remains operational
- the dev.3 initial/far first-maneuver distance correction is live-confirmed; the HUD showed the correct next-maneuver distance during the tested route
- with `VC_RGI_ENABLED=true`, the Virtual Cockpit enters a navigation presentation while CarPlay route guidance is active
- the same maneuver distance was visible in both HUD and VC during the test (`1.0 km`)
- HUD maneuver graphic: PASS
- HUD maneuver distance: PASS
- VC maneuver distance: PASS
- VC maneuver graphic: not present
- the VC showed a stock map fragment instead of the intended factory maneuver-oriented RGI presentation

### What the dev.3 car test established

The result is no longer treated as a failure to get CarPlay RGI into the cluster. The transport, parser, Java bridge, route-guidance ownership and maneuver-distance path are working far enough for the VC to enter navigation presentation and consume the same maneuver distance as the HUD.

Post-test K2161 inspection showed that `ClusterViewMode` has distinct favored modes for RGI (`1`) and MAP (`3`). dev.3 makes RGI locally valid but does not explicitly select favored RGI mode `1`, which is consistent with the vehicle remaining in a MAP presentation and showing the observed map fragment.

For the follow-up dev.4 work:

- the direct CarPlay RGI BAP type remains `ActiveRGType=0`
- factory RGI should be selected through `ClusterViewMode`, not by forcing the FPK input-listener path
- Audi's stock outward type `4` is associated with the LVDS-map path, not the desired maneuver-oriented RGI mode
- no custom VC renderer is used or planned
- the `VC_RGI_ENABLED=false` behavior still needs to represent true HUD-only / normal full-circle VC operation; the dev.3 map-suppression branch is not considered proven for that requirement

## v1.1 status

v1.1 is the current vehicle-tested stable release for K2161.

Validated on the vehicle:

- Apple Maps and Google Maps route guidance on the factory HUD
- real maneuver symbols from the authoritative CarPlay maneuver list
- first actionable maneuver at route start; the `START_ROUTE` pseudo-step is not used as the HUD maneuver when a real maneuver is already available
- numeric maneuver distance, including direct metric values below 1 km instead of the stock formatter's 50 m floor
- maneuver transitions after completing a turn
- side-street / junction geometry in the HUD maneuver graphic
- automatic recovery of RGI after a wireless CarPlay disconnect/reconnect that does not create a new iAP2 identification session
- route-end clear and normal BAP ownership hand-back behavior used by the existing integration

### Display scope

This release is **HUD-only**. CarPlay route guidance is not output as graphical route guidance in the Virtual Cockpit. While CarPlay owns route guidance, the K2161 integration keeps map visibility/presentation disabled for this path.

### Lane guidance

`0x5204` lane-guidance transport, caching, event selection and BAP output are implemented. The code follows iOS `laneGuidanceIndex` / `laneGuidanceShowing` state and can publish `CombiBAPNaviLaneGuidanceData` to K2161.

No qualifying real lane-guidance event was encountered during the v1.1 vehicle test, so **lane rendering is implemented but not yet live-validated**.

### Known observations

- On one route start, the first real maneuver symbol was correct but the HUD initially showed the destination distance instead of the first-maneuver distance. After passing that first maneuver, subsequent maneuver distances behaved normally.
- After a wireless reconnect, the previous maneuver can remain visible briefly while iOS rebuilds and republishes fresh authoritative route state. RGI recovery itself was successful.
- Waze remains dependent on whether the iOS/Waze session exposes the structured CarPlay RGI stream. A previously tested Waze session reported `sourceSupportsRouteGuidance=0` and produced no normal `0x5202` maneuver stream.

See [CHANGELOG.md](CHANGELOG.md) and [vehicle validation](docs/validation.md).

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

## Credits

The generic RGI parser/bus and parts of the Java route-guidance layer are adapted from **LuKa's `mib2q-carplay-rgi` work**. K2161/MIB2 High uses a different HARMAN integration path and additional stock-driver/BAP work.

See [NOTICE.md](NOTICE.md) for attribution.
