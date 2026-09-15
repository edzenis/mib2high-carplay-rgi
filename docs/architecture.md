# Architecture

## End-to-end path

```text
iPhone navigation app
  -> Apple iAP2 Route Guidance Information
  -> stock K2161 ipod-drvr-iap2.so
  -> stock runtime control-message validation
  -> K2161 receive observer at driver_base + 0x11428
  -> RGI parser/cache
  -> localhost TCP bus (127.0.0.1:19810)
  -> Java RouteGuidance
  -> BAPBridge
  -> de.audi.atip.interapp.combi.bap.CombiBAPServiceNavi
  -> factory Audi HUD
```

## Why K2161 needs the runtime message-table patch

The K2161 stock iAP2 driver's control-message table has an empty group `0x52` entry. Without a valid group definition, incoming Route Guidance messages reach the stock unsupported-message path and return `EINVAL (22)`. During vehicle testing this caused receive progress to stop after a repeatable initial burst.

The native hook validates the expected stock table/layout first and then installs an in-memory definition for group `0x52`:

```text
index 1 -> 0x5201  flags 0x00000002
index 2 -> 0x5202  flags 0x00000002
index 4 -> 0x5204  flags 0x00000002
```

The sender is fail-closed: Route Guidance is not armed unless the table patch verifies successfully.

No stock iAP2 driver file is modified on disk.

## Route-guidance startup

The required send ordering is intentionally preserved:

```text
iPhone 0x2700
  -> stock gps_info_send
  -> stock 0xFFFB LocationInformation
  -> real MsgReply
  -> custom 0x5200 StartRouteGuidanceUpdates
```

The custom `0x5200` is not sent ahead of the stock behavior.

## Java side

`Activator` tracks the exact K2161 service:

```text
de.audi.atip.interapp.combi.bap.CombiBAPServiceNavi
```

The TerminalMode lifecycle adapter tracks:

```text
de.audi.atip.interapp.terminalmode.ITerminalModeUpdateService
```

and validates the exact K2161 implementation before reflectively acquiring its `IDeviceManager`.

`K2161RouteGuidanceOwnership` and `K2161GatedCombiService` arbitrate access to the stock navigation BAP service while CarPlay route guidance owns the HUD state.
