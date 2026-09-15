# Vehicle validation

## Validated session

One full transport/HUD session produced:

```text
225 x 0x5201 Route Guidance Update
23  x 0x5202 Maneuver Update
0   x 0x5204 Lane Guidance
248 total RGI messages
```

Observed transport result:

```text
message-table verification: PASS
0 x RGI "Invalid packet"
0 x "packet_recvlist_pop: No packets in receive list"
sustained authoritative 0x5201 updates
dynamic maneuver changes
dynamic distance updates
natural route-end deactivation
```

## Apple Maps

```text
114 x 0x5201
15  x 0x5202
sourceSupportsRouteGuidance=1
```

Vehicle result:

- factory HUD maneuver arrows: PASS
- distance updates: PASS
- route end clears HUD: PASS

## Google Maps

```text
108 x 0x5201
8   x 0x5202
sourceSupportsRouteGuidance=1
```

Vehicle result:

- factory HUD maneuver arrows: PASS
- distance updates: PASS
- route end clears HUD: PASS

Google Maps was observed reporting `visibleInApp=0` while guidance remained valid; the Java state handling does not use that value alone to tear down an active route.

## Waze

Tested session:

```text
3 x 0x5201
0 x 0x5202
0 x 0x5204
sourceName="Waze"
sourceSupportsRouteGuidance=0
```

Vehicle result: no HUD maneuver arrows.

In this session Waze did not expose the normal structured maneuver stream used by the working Apple Maps and Google Maps paths. This does not by itself indicate a K2161 BAP/HUD output failure.

## Not yet validated

Native Audi navigation hand-back after CarPlay route guidance without an MMI reboot has not yet been validated.
