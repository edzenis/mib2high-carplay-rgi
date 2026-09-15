# Implementation details

## Native modules

| File | Role |
|---|---|
| `observer.c` | preload/bootstrap, exact stock-driver base validation, receive observation and native initialization |
| `ident_hook.c` | appends the required iAP2 RouteGuidanceDisplayComponent Identification parameter |
| `message_table.c` | validates and activates stock runtime group `0x52` for `0x5201`, `0x5202`, `0x5204` |
| `send_hook.c` | hooks the verified K2161 send path and emits `0x5200` only after the stock trigger sequence |
| `receive_probe.c` | reconstructs/filters incoming control frames at the verified receive seam |
| `rgi_bridge.c` | feeds complete RGI frames into the parser and publishes normalized updates to Java |
| `diagnostics.c` | bounded diagnostics used by the integration |
| `framework/` | shared bus, logging and iAP2 helpers |
| `routeguidance/` | TLV parser and state cache for RGI updates |

### K2161 binary compatibility checks

- stock `ipod-drvr-iap2.so` size: `731172`
- stock driver SHA256: `fccf1f07f2cfeb6860fc3066971c48aa7dc2c581d03d5efd3a138fbad2974921`
- stock driver Build ID: `f9472afcdab8ca16a2c82d6c229ed7ad`
- `iap2_ctrl_msg_table`: driver offset `0xAB050`, `256 x 12-byte` group entries
- group `0x52` slot: `driver_base + 0xAB428`
- receive observation seam: `driver_base + 0x11428`

These values are firmware-specific. The implementation validates expected stock state and fails closed rather than applying guessed addresses.

## Identification

RGI requires both the message direction advertisement and a `RouteGuidanceDisplayComponent` in Identification.

The K2161 hook appends Identification parameter `0x001E` with component ID `0x0010`. The tested phone accepted the resulting Identification exchange.

## Java modules

| Class | Role |
|---|---|
| `CarplayBus` | Java-side localhost bus server for native RGI updates |
| `RouteGuidance` | authoritative route/maneuver state and lifecycle |
| `ManeuverMapper` | maps iAP2 maneuver semantics to Audi BAP maneuver values |
| `BAPBridge` | sends the current authoritative maneuver and numeric distance to K2161 BAP |
| `K2161GatedCombiService` | controls stock navigation writes while CarPlay owns guidance |
| `K2161RouteGuidanceOwnership` | installs/restores the BAP ownership path |
| `CarPlayLifecycle` | connects guidance lifecycle to the exact K2161 TerminalMode active-device state |
| `Activator` | OSGi bundle entry point and service tracking |

The implementation does not synthesize a fake straight/FOLLOW_STREET maneuver when an authoritative maneuver is unavailable.
