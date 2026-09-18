# Implementation details

## Stock driver integration

The K2161 stock `ipod-drvr-iap2.so` remains unchanged on disk. The native preload validates the expected driver image and activates the stock Route Guidance message group at runtime.

The configured RGI directions are:

```text
receive: 0x5201, 0x5202, 0x5204
send:    0x5200, 0x5203
```

The runtime table enables the receive entries needed by K2161 without replacing the stock parser.

## 0x5200 subscription

The StartRouteGuidanceUpdates request is component-qualified with component ID `0x0010` and the expected RGI capability TLVs.

The sender keeps the stock-safe ordering:

```text
stock 0x2700 callback
-> stock GPS processing
-> real MsgReply
-> custom 0x5200
```

The retry thread never calls stock iAP2 send functions directly. It only marks work due; a later stock callback claims and performs the send.

Initial retry cadence is 2 seconds for the first five attempts and 30 seconds thereafter, until a valid `0x5201`, `0x5202` or `0x5204` is received.

## Wireless reconnect recovery

Each valid RGI receive updates `last_rgi_rx_ms`. If RGI has previously been active, no retry is active, and RGI has been silent for at least 10 seconds, the next `0x2700` can soft-rearm the subscription state. A generation counter prevents an old worker or delayed reply from mutating the new retry epoch.

A genuine `0x1D00 StartIdentification` still resets the subscription as a real session boundary. Per-OCB close callbacks are deliberately not treated as whole-session boundaries.

## Maneuver presentation

The authoritative `maneuver_list` determines the HUD maneuver. iOS can place a `START_ROUTE` pseudo-step at the head of that list. If a later valid real maneuver is already available, v1.1 keeps the pseudo-step in state/cache but selects the first later non-`START_ROUTE` maneuver for presentation.

The real maneuver symbol is not hidden based on distance. The old far-distance `FOLLOW_STREET` substitution and BAP bargraph presentation are not used.

Approach distance is still used for K2161 maneuver-state emphasis and turn-to street text. It does not decide whether the actual maneuver icon or numeric distance is published.

## Distance encoding

For maneuver distance:

```text
meters <= 0
    -> existing invalid/transient handling

metric && meters < 1000
    -> value = meters * 10
       unit  = 0

otherwise
    -> stock BAPDistanceFormatter
```

Destination-distance formatting is unchanged. Non-metric maneuver distance continues to use the stock formatter.

## Maneuver descriptor and junction geometry

Maneuver type, direction, z-level and side-street geometry are converted to `CombiBAPNaviManeuverDescriptor` and sent using the K2161 combined maneuver/ExitView transaction. Side-street geometry has been visually confirmed on the vehicle HUD.

## Lane guidance

The native parser accepts `0x5204 RouteGuidanceLaneGuidanceInformation` and stores lane events by their composed/event index. The active event comes from the current lane-guidance index in `0x5201`; receiving a future/pre-cached `0x5204` does not by itself activate it.

Java gates output on `laneGuidanceShowing`, resolves the matching cached event ID and converts per-lane position/direction/status data into `CombiBAPNaviLaneGuidanceData`.

This implementation is present in v1.1, but a real qualifying lane event has not yet been encountered during vehicle testing.

## HUD-only output

The v1.1 display target is the factory HUD. While CarPlay owns route guidance, K2161 map visibility and map presentation are kept disabled for this path. The release does not provide CarPlay graphical route guidance in the Virtual Cockpit.
