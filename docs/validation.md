# Vehicle validation

## Baseline transport validation

An earlier full transport/HUD session produced:

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

## v1.2.0-dev.3 vehicle test

The development checkpoint was tested on the Audi Q7 4M / `MHI2_ER_AUG22_K2161`, MU 1421 platform.

### Passed

- factory HUD RGI remains operational
- initial/far first-maneuver distance correction is live-confirmed on the tested route
- the existing CarPlay route-guidance transport, Java bridge and HUD BAP path continue to operate with the dev.3 presentation changes
- the same next-maneuver distance was visible simultaneously in HUD and VC during the test (`1.0 km`)

### Virtual Cockpit result

`VC_RGI_ENABLED=true` does activate a K2161 Virtual Cockpit navigation/map presentation while CarPlay route guidance is active.

Observed vehicle state:

```text
HUD maneuver graphic: active
HUD maneuver distance: 1.0 km
VC navigation/map presentation: active
VC maneuver distance: 1.0 km
VC map area: visible
VC maneuver graphic: not present
```

The VC therefore receives enough shared route-guidance state to enter a navigation presentation and show the correct maneuver distance, but the resulting presentation is incomplete: only a map fragment is shown instead of the intended maneuver-oriented route-guidance graphic.

The remaining VC work is to identify the exact K2161 map/presentation state needed for the desired FPK maneuver view. This is no longer classified as a failure to activate VC navigation mode.

## v1.1 vehicle test

The current release was tested on the same Audi Q7 4M / K2161 platform.

### Passed

- first actionable maneuver appears at route start when iOS has already supplied a real maneuver after `START_ROUTE`
- normal left/right/roundabout maneuver symbols render on the factory HUD
- numeric maneuver distance works below 1 km using the direct metric encoding; 70 m, 60 m and other close-range values were observed correctly
- maneuver transitions work after passing a maneuver
- side-street / junction geometry is rendered as part of the HUD maneuver graphic
- wireless CarPlay reconnect recovery works: after leaving the running vehicle and later returning, RGI subscription recovery eventually restored fresh route guidance
- route guidance transport remains on the stock K2161 iAP2 driver; the driver is not modified on disk

### Reconnect details

The tested adapter can reconnect without a new iAP2 identification boundary. v1.1 detects stale RGI after 10 seconds of silence and re-arms the `0x5200` subscription from a stock-safe `0x2700` callback path. Fresh `0x5201`/`0x5202` traffic cancelled the retry sequence and route guidance resumed.

A presentation quirk remains: the last maneuver visible before disconnect can remain on the HUD briefly after reconnect while iOS rebuilds and republishes the authoritative maneuver list.

### First-route distance observation

In one Google Maps route, the first real maneuver was a roundabout roughly 15 km away while the destination was roughly 24 km away. The HUD showed the correct roundabout symbol but initially displayed the destination distance. After passing that roundabout, the next maneuver and its distance behaved normally. This is tracked as an initial-route presentation observation rather than a general long-distance formatter failure.

## Lane guidance

The release contains the `0x5204` lane-guidance path, including separate event caching, active `laneGuidanceIndex` resolution, `laneGuidanceShowing` gating and K2161 BAP lane output.

No qualifying lane-guidance event occurred during the v1.1 road test. Therefore:

```text
implementation: present
offline path: verified
live visual rendering: not yet validated
```

## Virtual Cockpit

CarPlay graphical route guidance is not an output target of v1.1. The validated display target is the factory HUD.

For v1.2.0-dev.3, vehicle testing confirmed that the shared route-guidance state can activate a VC navigation/map presentation and carry the correct maneuver distance into the VC. The remaining issue is presentation selection: the VC currently shows only a map fragment and does not display the intended maneuver graphic.

## Application observations

Apple Maps and Google Maps have both produced the structured RGI stream required by this project. In the previously tested Waze session:

```text
sourceName="Waze"
sourceSupportsRouteGuidance=0
0 x 0x5202
0 x 0x5204
```

No HUD maneuver arrows were produced in that session.
