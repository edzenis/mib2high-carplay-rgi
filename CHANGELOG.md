# Changelog

## v1.2.0-dev.3 — presentation alignment + VC switch

### Changed

- resolves one authoritative presentation maneuver for descriptor, distance and turn-to text
- when iOS keeps `START_ROUTE` at the head while a later real maneuver is presented, rejects an absent or implausibly larger top-level maneuver distance when the selected 0x5202 slot has a usable distance
- adds presentation-distance diagnostics showing live versus selected-slot distance and which source was sent
- adds `VC_RGI_ENABLED` as a compile-time switch; enabled leaves the shared K2161 maneuver presentation available to the VC/FPK, while disabled applies the existing best-effort map visibility/presentation suppression
- keeps the shared maneuver descriptor/distance BAP transaction active in both modes because the HUD requires the same route-guidance writes

### Vehicle validation

- vehicle-tested on Audi Q7 4M / `MHI2_ER_AUG22_K2161`, MU 1421
- factory HUD RGI remains operational with dev.3
- initial/far first-maneuver distance presentation: live-confirmed; the tested route showed the correct next-maneuver distance on the HUD
- `VC_RGI_ENABLED=true` does activate a Virtual Cockpit navigation/map presentation on this vehicle
- the same maneuver distance was visible in both HUD and VC during the test (`1.0 km`)
- the VC presentation is incomplete: a map fragment is shown, but the expected maneuver graphic is not presented

### Known limitation

- dev.3 reaches and activates a VC/FPK navigation presentation, but the resulting state is not yet the intended maneuver-oriented route-guidance view; exact K2161 map/presentation state control remains unresolved

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
