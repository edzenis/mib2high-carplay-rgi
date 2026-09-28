# CarPlay AltScreen

## Status

This branch is the working AltScreen integration point. The current native build compiles successfully, but this exact build has not yet been vehicle-tested.

The existing semantic CarPlay route-guidance path remains separate. AltScreen work is intended to add a second CarPlay video stream for the instrument cluster without replacing the main CarPlay display.

## Current runtime state

The current implementation advertises a second AirPlay display with:

```text
type=111
geometry=800x480
widthPhysical=0
heightPhysical=0
features=0
primaryInputDevice=0
showsInstruments=true
initialURL=maps:/car/instrumentcluster/map
```

A previous vehicle run confirmed that the AirPlay property hook executes and appends the second display, but no type-111 stream setup was observed afterward.

The current build adds runtime logging for:

```text
PROPERTY_REQUEST
PROPERTY_RETURN
SETUP_REQUEST
SETUP_STREAM
SETUP_RESPONSE_FEATURES
```

This is intended to show exactly which AltScreen properties the phone requests, which stream types it sends during SETUP, and whether the stock session-level response already contains `altScreen` or `viewAreas`.

## Reference comparison

The working reference implementation differs from the current display advertisement in several relevant fields:

```text
current                 reference
--------------------------------------------
widthPhysical=0         non-zero physical width
heightPhysical=0        height derived from aspect ratio
features=0              features=10
primaryInputDevice=0    primaryInputDevice=3
fixed 800x480           runtime cluster geometry
```

The reference session flow also advertises:

```text
enabledFeatures += ["viewAreas", "altScreen"]
```

before the phone requests the type-111 stream. The current implementation leaves the stock `enabledFeatures` response unchanged.

These are the next functional targets after the diagnostic run establishes the exact phone/accessory exchange.

## Native build

Set the QNX SDP environment and run the existing build script:

```sh
export QNX_HOST=/usr/qnx650/host/qnx6/x86
export QNX_TARGET=/usr/qnx650/target/qnx6
export PATH=$QNX_HOST/usr/bin:$PATH

sh scripts/build-native.sh
```

Output:

```text
build/libmib2high-carplay-rgi.so
```

Current diagnostic build SHA-256:

```text
f26c011d315884c93673a6303b86a941b8295a52a04570f07a4824c9ac02c761
```

## Deployment

The branch contains a ready-to-use M.I.B. layout under:

```text
mod/
```

Copy that `mod` directory to the SD card used by M.I.B. The required layout is:

```text
mod/
├── custom.sh
└── mib2high-carplay-rgi/
    └── payload/
        ├── libmib2high-carplay-rgi.so
        └── smartphone_integrator.json
```

Then run the standard M.I.B. custom script entry:

```text
Advanced -> Run /mod/custom.sh
```

First run installs the payload and preserves the previous files under the SD-card backup directory. After installation, another run collects the relevant runtime logs to:

```text
mod/mib2high-carplay-rgi/collected/
```

To restore the previous files, create:

```text
mod/mib2high-carplay-rgi/ROLLBACK
```

and run `/mod/custom.sh` again.

The installer does not patch the configuration on the unit. It installs the prepared configuration file directly and preloads the native library into the CarPlay process.

## Diagnostic decision point

The next vehicle log should make the failure boundary explicit:

```text
only type 110 arrives
    -> capability advertisement / negotiation is still incomplete

type 111 arrives, but no AltScreen session starts
    -> private session setup is the blocker

AltScreen session starts, but no frames arrive
    -> stream/security path is the blocker

frames arrive, but no cluster video
    -> decode/output/routing path is the blocker
```
