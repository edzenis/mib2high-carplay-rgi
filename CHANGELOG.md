# Changelog

## v1.2.0-dev.4 — factory VC RGI presentation candidate

### Changed

- keeps one runtime `VC_RGI_ENABLED` switch in the source, defaulted to `true`; both enabled and disabled paths remain compiled into the same JAR
- enabled path keeps the proven CarPlay HUD transaction (`RGStatus=1`, `ActiveRGType=0`) and explicitly selects the factory K2161 `ClusterViewMode` RGI favored view (`1`)
- enabled path propagates the factory RG-active state before selecting RGI so the OEM state machine sees valid route-guidance data
- does not use a custom VC renderer, EGL graphics path, LVDS maneuver renderer or synthetic maneuver artwork
- preserves and restores the user's pre-CarPlay favored VC state around the CarPlay ownership window
- development disabled path no longer treats COMPASS as equivalent to the desired normal/full-circle VC state
- disabled path leaves favored VC navigation mode untouched, keeps VC route-guidance status inactive, and uses the separate DDP2 `updateHUDDisplayContent(true)` request as the HUD-only experiment
- route-guidance ownership/gating remains in place so stock native guidance writes cannot overwrite the CarPlay transaction while CarPlay owns route guidance

### Build status

- Java source builds successfully against the K2161 LSD compile dependency
- candidate JAR bytecode contains both `FACTORY_RGI` and `HUD_ONLY_DDP2` branches with `VC_RGI_ENABLED=true` set by the static initializer
- current development JAR SHA256 from the local validated build: `542cb3ec7f7cb2edfaebb1e89b53a46c131ce9f4163d4d78de838e489df1569d`
- native `.so` is unchanged from dev.3

### Vehicle validation

- not yet vehicle-tested
- primary dev.4 test target: factory Audi maneuver-oriented RGI in the Virtual Cockpit while retaining the already-working HUD maneuver graphic and distance
- disabled/HUD-only DDP2 behavior remains experimental until separately tested on the vehicle

## v1.2.0-dev.3 — presentation alignment + VC switch

### Changed

- resolves one authoritative presentation maneuver for descriptor, distance and turn-to text
- when iOS keeps `START_ROUTE` at the head while a later real maneuver is presented, rejects an absent or implausibly larger top-level maneuver distance when the selected 0x5202 slot has a usable distance
- adds presentation-distance diagnostics showing live versus selected-slot distance and which source was sent
- adds `VC_RGI_ENABLED` as a development switch; enabled leaves the shared K2161 maneuver presentation available to the VC/FPK, while disabled applies the then-current best-effort map visibility/presentation suppression
- keeps the shared maneuver descriptor/distance BAP transaction active in both modes because the HUD requires the same route-guidance writes

### Vehicle validation

- vehicle-tested on Audi Q7 4M / `MHI2_ER_AUG22_K2161`, MU 1421
- factory HUD RGI remains operational with dev.3
- initial/far first-maneuver distance presentation: live-confirmed; the tested route showed the correct next-maneuver distance on the HUD
- `VC_RGI_ENABLED=true` activates a Virtual Cockpit navigation presentation on this vehicle
- the same maneuver distance was visible in both HUD and VC during the test (`1.0 km`)
- HUD maneuver graphic: PASS
- HUD maneuver distance: PASS
- VC maneuver distance: PASS
- VC maneuver graphic: NOT PRESENT
- the VC showed a stock map fragment instead of the intended factory maneuver-oriented RGI view

### Post-test K2161 analysis

The dev.3 car test narrowed the remaining VC issue to presentation selection rather than transport or maneuver-data generation.

Offline K2161 inspection after the test showed:

- `ClusterViewMode` uses favored view mode `1` for factory RGI and `3` for MAP
- dev.3 makes local RGI state valid (`rgActive` + non-empty RGI string) but does not select favored view mode `1`
- the observed map-fragment result is therefore consistent with the FPK remaining in its MAP favored state while the same BAP maneuver distance is also available to the cluster
- direct CarPlay maneuver RGI remains `ActiveRGType=0`; Audi stock FPK code emitting outward type `4` belongs to the LVDS-map path and is not a reason to change the direct RGI type to `4`
- the desired VC-enabled follow-up should select the factory RGI view through `ClusterViewMode`, not through `ClusterInputListener`, which validates FPK requests and can fall back to MAP
- no custom VC renderer is required or desired for this path

The dev.3 disabled branch is also not considered a proven implementation of the desired normal/full-circle VC state. Map suppression or COMPASS selection controls navigation content, not necessarily the global non-navigation/full-circle presentation. That behavior remains a dev.4 concern.

### Known limitation

- dev.3 reaches and activates a VC/FPK navigation presentation, but it does not explicitly select the factory RGI favored view; the resulting state is a partial MAP presentation without the maneuver graphic
- `VC_RGI_ENABLED=false` in dev.3 is not vehicle-validated as a true HUD-only/full-circle mode

## v1.1 — vehicle-tested K2161 release

### Changed

- presents the first real authoritative maneuver instead of treating iOS `START_ROUTE` as the HUD maneuver when a real maneuver is already available
- keeps the real current maneuver active at long range rather than substituting a distance-based `FOLLOW_STREET` placeholder
- keeps the maneuver distance numeric; the BAP distance bargraph is not used
- for positive metric maneuver distances below 1 km, sends the direct K2161 unit-0 value (`meters * 10`) instead of accepting the stock turn formatter's short-range clamp
- adds generation-guarded RGI subscription recovery for wireless reconnects that occur without a new `0x1D00 StartIdentification`
- after 10 seconds of RGI silence, the next normal stock `0x2700` opportunity can soft-rearm the `0x5200` subscription; retry cadence remains 2 s for the first five attempts and 30 s thereafter
- preserves the stock-safe send sequence: stock `0x2700` handling -> real `MsgReply` -> custom component-qualified `0x5200`
- keeps CarPlay route guidance HUD-only on this K2161 integration
- retains `0x5204` lane-guidance parsing/caching/BAP output; live lane rendering remains unverified because no qualifying event was encountered

### Vehicle validation

- first actionable maneuver: PASS
- sub-1 km metric distance: PASS
- wireless reconnect recovery: PASS
- maneuver transitions: PASS
- side-street/junction geometry: PASS
- Virtual Cockpit CarPlay graphical route guidance: not produced by this release
- lane guidance: implemented, not yet live-tested

### Known observations

- one tested route start showed the correct first maneuver symbol with destination distance until the first maneuver was passed; later maneuver distances were correct
- after reconnect, the last pre-disconnect maneuver may remain visible briefly until fresh authoritative route state arrives

## v1.0.0 — initial public release

- Apple Maps factory-HUD maneuver arrows and numeric distance
- Google Maps factory-HUD maneuver arrows and numeric distance
- route-end HUD clear
- sustained iAP2 RGI reception on K2161 after runtime activation of stock group `0x52`
- source release for `MHI2_ER_AUG22_K2161`, MU 1421
