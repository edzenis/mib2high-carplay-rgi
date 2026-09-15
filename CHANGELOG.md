# Changelog

## v1.1.0 — HUD presentation update

Status: vehicle-tested development version; public release pending.

### Changed

- revised HUD maneuver presentation to keep the real current maneuver active instead of substituting a synthetic follow-street state
- revised numeric-distance presentation so distance remains associated with the active maneuver
- added route-guidance ownership/presentation changes intended to preserve HUD guidance while preventing CarPlay route-guidance presentation from taking over the Virtual Cockpit
- added reconnect/session recovery changes

### Known issues

- the first maneuver can be missing when guidance starts; the following maneuver appears after the first maneuver is completed
- CarPlay reconnect can succeed without restoring HUD RGI, including when a route was already active
- HUD distance was observed to stop at approximately 50 m instead of counting down closer to zero
- Virtual Cockpit behavior has not yet been vehicle-validated

These issues were discovered during v1.1.0 vehicle testing and are candidates for the next corrective update.

## v1.0.0 — Initial public release

- Apple Maps factory-HUD maneuver arrows and numeric distance
- Google Maps factory-HUD maneuver arrows and numeric distance
- route-end HUD clear
- sustained iAP2 RGI reception on K2161 after runtime activation of stock group `0x52`
- source release for `MHI2_ER_AUG22_K2161`, MU 1421

Known limitations in v1.0.0 include the tested Waze session not exposing a normal structured maneuver stream, no observed `0x5204` lane-guidance frame, and unvalidated native Audi navigation hand-back without reboot.
