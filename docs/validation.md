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

`VC_RGI_ENABLED=true` activates a K2161 Virtual Cockpit navigation presentation while CarPlay route guidance is active.

Observed vehicle state:

```text
HUD maneuver graphic:        PASS
HUD maneuver distance:       PASS (1.0 km in the photographed test)
VC navigation presentation:  ACTIVE
VC maneuver distance:        PASS (same 1.0 km)
VC map area:                 ACTIVE / partial stock map fragment
VC maneuver graphic:         NOT PRESENT
```

This is an important narrowing of the problem. The VC receives enough shared route-guidance state to enter navigation presentation and show the correct maneuver distance. The failure is therefore not classified as missing CarPlay RGI transport, parser output, Java bridge output, BAP ownership, or cluster activation.

### Post-test K2161 state-machine analysis

Offline K2161 inspection after the road test showed:

```text
ClusterViewMode favored 0 = COMPASS
ClusterViewMode favored 1 = RGI
ClusterViewMode favored 2 = KDK
ClusterViewMode favored 3 = MAP
```

`ClusterService.updateRGIString(non-empty)` contributes to `rgiValid`; dev.3 also overlays `DSIResponseContainer.rgActive=true`. That makes the factory RGI path eligible, but dev.3 never explicitly selects favored view mode `1`.

The photographed map-fragment result is therefore consistent with the FPK retaining its MAP favored state while dev.3 simultaneously supplies valid maneuver/distance data. The next development step is to select the factory RGI presentation directly through `ClusterViewMode` while preserving the already-working HUD transaction.

The corresponding BAP conclusion is:

- direct CarPlay maneuver RGI remains `ActiveRGType=0`
- stock FPK code that emits outward type `4` belongs to the Audi LVDS-map pathway; it is not evidence that the direct maneuver-RGI bridge should send type `4`
- the FPK `ClusterInputListener` should not be used to request RGI mode because its validation/fallback path can return to MAP; the direct `ClusterViewMode` state machine is the relevant local presentation selector

No custom maneuver renderer is part of this plan.

### Disabled-switch semantics

The dev.3 `VC_RGI_ENABLED=false` branch was not vehicle-validated as the desired HUD-only / normal full-circle state.

Post-test analysis also showed why this distinction matters: COMPASS and MAP are navigation-content modes inside the VC navigation presentation. They are not equivalent to the user's normal non-navigation/full-circle speedometer presentation. Therefore map suppression or forcing COMPASS is not considered a completed disabled-mode implementation.

## v1.1 vehicle test

The current stable release was tested on the same Audi Q7 4M / K2161 platform.

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

For v1.2.0-dev.3, vehicle testing confirmed that shared CarPlay route-guidance state can activate a VC navigation presentation and carry the correct maneuver distance into the VC. The remaining dev.3 issue is presentation selection: the FPK stayed in a MAP presentation, showing a partial map fragment and no maneuver graphic.

The dev.4 direction is therefore factory RGI presentation selection, not a custom graphics renderer and not a redesign of the working HUD path.

## Application observations

Apple Maps and Google Maps have both produced the structured RGI stream required by this project. In the previously tested Waze session:

```text
sourceName="Waze"
sourceSupportsRouteGuidance=0
0 x 0x5202
0 x 0x5204
```

No HUD maneuver arrows were produced in that session.
