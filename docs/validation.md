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
- initial/far first-maneuver distance presentation is treated as PASS for this checkpoint; reopen only if later vehicle evidence reproduces the old `START_ROUTE`/wrong-far-distance behavior
- the existing CarPlay route-guidance transport, Java bridge and HUD BAP path continue to operate with the dev.3 presentation changes

### Virtual Cockpit result

`VC_RGI_ENABLED=true` does not by itself activate the K2161 Virtual Cockpit/FPK navigation presentation.

Observed vehicle state while CarPlay route guidance was active:

```text
HUD RGI: active
VC route-guidance layout: not active
VC state: normal no-navigation layout
transient RGI flicker: not observed
```

The dev.3 switch therefore leaves the shared VC/FPK presentation available but does not actively force the cluster into its route-guidance layout. Explicit VC/FPK presentation activation remains unresolved.

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

For v1.2.0-dev.3, the shared/simple VC presentation was left available, but vehicle testing showed that this alone does not activate the Virtual Cockpit route-guidance layout.

## Application observations

Apple Maps and Google Maps have both produced the structured RGI stream required by this project. In the previously tested Waze session:

```text
sourceName="Waze"
sourceSupportsRouteGuidance=0
0 x 0x5202
0 x 0x5204
```

No HUD maneuver arrows were produced in that session.
