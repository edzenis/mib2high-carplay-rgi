# Compatibility

## Validated

| Item | Status |
|---|---|
| Audi Q7 4M | validated |
| MIB2 High / MHI2 | validated |
| `MHI2_ER_AUG22_K2161` | validated |
| MU 1421 | validated |
| Apple Maps | HUD guidance validated |
| Google Maps | HUD guidance validated |
| Waze | no usable normal maneuver stream in the tested session |
| `0x5204` lane guidance | transport/parser present; no live frame observed |

## Firmware specificity

The native hook uses exact, validated K2161 driver structures and offsets. Another MHI2 firmware may have a different stock driver build, table layout, symbols or call-site offsets.

Do not infer compatibility from the shared MHI2 product family alone.

## Virtual Cockpit

v1.0.0 does not claim validated Virtual Cockpit graphical route-guidance output. The validated display target is the factory HUD.
