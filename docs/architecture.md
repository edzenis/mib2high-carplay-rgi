# Architecture

## Data path

```text
iPhone navigation app
        |
        v
iAP2 Route Guidance (0x5200..0x5204)
        |
        v
stock K2161 ipod-drvr-iap2.so
        |
        +-- runtime group 0x52 message-table activation
        |
        v
native RGI receive/parser/cache
        |
        v
localhost native -> Java bus
        |
        v
RouteGuidance state
        |
        v
BAPBridge
        |
        v
K2161 CombiBAPServiceNavi
        |
        v
factory Audi HUD
```

The stock iAP2 driver is not patched on disk. The preload library validates the expected K2161 layout and activates the required stock message-table entries in writable process memory.

## iAP2 subscription

The custom `0x5200` StartRouteGuidanceUpdates request is component-qualified for component `0x0010`. The send path deliberately preserves the stock callback order:

```text
incoming 0x2700
-> stock GPS handling
-> stock LocationInformation / real MsgReply
-> custom 0x5200
```

Retries are sent only from a proven stock callback context; the retry worker merely marks an attempt as due.

## Reconnect recovery

A real `0x1D00 StartIdentification` remains the authoritative new-session boundary. Some wireless adapters reconnect without generating a new `0x1D00`, so v1.1 additionally tracks the last valid `0x5201`/`0x5202`/`0x5204` receive time. After 10 seconds of RGI silence, a subsequent stock `0x2700` can create a same-session soft re-arm with a new retry generation.

## Java/BAP ownership

While CarPlay owns route guidance, the Java layer gates stock route-guidance writes so the native navigation service cannot overwrite the CarPlay BAP state. On release of ownership, the stock service is restored.

The K2161 maneuver path uses the combined maneuver/ExitView transaction and publishes numeric maneuver distance separately.

## Display behavior

v1.1 always presents the authoritative real maneuver; distance does not gate whether the maneuver symbol exists. `FOLLOW_STREET` is retained only for the K2161 startup sync transaction or when the actual mapped maneuver is a straight/follow-road maneuver, not as a far-distance placeholder.

The BAP distance bargraph is disabled. Positive metric maneuver distances below 1 km use the direct K2161 unit-0 encoding (`meters * 10`).

The public release is HUD-only: CarPlay route guidance is not rendered as graphical navigation in the Virtual Cockpit.

## Lane guidance

`0x5204` events are cached independently from maneuver slots. `0x5201` supplies the current lane-guidance event/index and display intent. Java resolves the active cached event by ID and calls K2161 `updateLaneGuidance(...)` only when iOS indicates that lane guidance should be shown.
