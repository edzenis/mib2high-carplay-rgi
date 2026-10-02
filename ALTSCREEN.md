# Audi K2161 CarPlay AltScreen port

## Status

Experimental CarPlay Auxiliary / ScreenAlt port for Audi MIB2 High.

Target:

- Audi Q7 4M
- MHI2_ER_AUG22_K2161
- MU 1421
- QNX 6.5 ARM

This work adapts the GEN2 Stream-111 architecture from harman-f/mhi2_altscreen_carplay to Audi K2161.

The normal CarPlay main display, Stream 110, remains stock-owned.

## Current checkpoint

Run-04 is the current vehicle-tested diagnostic checkpoint.

Confirmed on K2161:

- LD_PRELOAD AirPlay lifecycle interposition works
- stock CarPlay Stream 110 remains operational
- executable-text patching is not used
- direct ARM text patching was abandoned after it caused dio_manager failure
- Setup / Start / TearDown / PlatformControl / SessionControl interception works
- K2161 screen AES master material is captured through AirPlay_DeriveAESKeySHA512ForScreen
- AirPlayReceiverSessionSendCommand is resolved lazily
- modesChanged and navigation state are observed
- secondary AltScreen capability is advertised
- showUI completes successfully
- forceKeyFrame completes successfully

Run-04 control state:

```text
control_session=1
command_ready=1
projection_desired=1
shown_ack=1
last_dispatched_request=2
last_completed_request=2
last_completion_status=0
```

## Secondary display profile

```text
type=111
widthPixels=1010
heightPixels=376
widthPhysical=200
heightPhysical=74
maxFPS=30
features=0
primaryInputDevice=<absent>
initialURL=maps:/car/instrumentcluster
ViewAreas=enabled
```

Root enabledFeatures includes altScreen and viewAreas.

Advertised cluster URL roles:

```text
maps:/car/instrumentcluster
maps:/car/instrumentcluster/map
maps:/car/instrumentcluster/instructioncard
```

## Current blocker

The sender has not yet initiated Type-111 SETUP.

Observed SETUP requests remain Type 110, while stream_gen/source_aus/source_idrs/delivered_aus remain zero.

Therefore the current failure boundary is before the Stream-111 H.264 receive path.

The remux / isoTX2 / MOST path has not yet been exercised by real Type-111 CarPlay video on K2161.

## Run-04 diagnostic bootstrap

Run-04 arms GEN2 control at SessionStart and issues showUI / forceKeyFrame.

This proved the K2161 command path works, but later sender-side research indicates showUI is not what creates the Auxiliary / ScreenAlt stream.

This behavior is therefore diagnostic, not the intended final bootstrap.

## Wireless adapter diagnostic

Run-04 reported sender metadata:

```text
model=iPhone12,1
osBuildVersion=21B101
sourceVersion=320.17.6
```

The actual test handset is an iPhone 17 Pro running iOS 27.

The next controlled test uses the same Run-04 software with the wireless adapter removed and the iPhone connected directly by USB.

Primary question: does direct USB produce SETUP type=111?

## Prepared media path

```text
iPhone CarPlay
 -> Auxiliary / ScreenAlt Type 111
 -> AES-CTR screen stream
 -> AVCC H.264 -> Annex-B
 -> direct-ts-remux
 -> MPEG-TS
 -> /dev/mlb/isoTX2
 -> MOST150
 -> Virtual Cockpit
```

## Current binaries

Run-04 libcarplay-altscreen.so:

```text
98519a4a2ee9997baae0f6cff182d015d2b9c0b09121608c585f793dcd404a56
```

direct-ts-remux:

```text
b761a8741682e3cbc6e05f5fe1705475c34c4805d1cd1ce09333274a301e735e
```

isoTX2 gate:

```text
05673010a88c25022145ffb4e75d3715eaf686f4127ac188e91a52f512b9d957
```

## Deployment

The existing tracked mod/custom.sh is from the earlier development flow and is NOT the exact Run-04 GEN2 installer.

Do not use it as a Run-04 installer.

The current reversible installer will be added separately after the exact deployment scripts are recovered or recreated and reviewed.
