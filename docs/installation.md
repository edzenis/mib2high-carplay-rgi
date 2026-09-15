# Installation requirements

This repository does not include an automated installer. Integration into an MHI2 unit should be performed only by someone familiar with the target firmware and recovery procedure.

## 1. iAP2 message directions

For K2161, the required RGI direction split is:

```ini
[ident]
recvmsgs=0x5201,0x5202,0x5204
sendmsgs=0x5200,0x5203
```

Direction is from the MMI/accessory point of view:

```text
MMI -> iPhone: 0x5200, 0x5203
iPhone -> MMI: 0x5201, 0x5202, 0x5204
```

Do not treat `0x5203` as an incoming route-end event.

The complete OEM `iap2.cfg` is not distributed by this repository.

## 2. Native preload

The native library must be preloaded **only** into stock `mm-ipod`.

Stock executable:

```text
/mnt/app/armle/usr/sbin/mm-ipod
```

Build output:

```text
build/libmib2high-carplay-rgi.so
```

Place the library in persistent storage on the MMI and configure a process-specific launcher to set `LD_PRELOAD` for `mm-ipod` only.

The launcher should:

1. verify that the preload library exists;
2. set `LD_PRELOAD` only in its own process environment;
3. preserve an existing `LD_PRELOAD` value;
4. `execv()` the stock `mm-ipod` path with the original argument vector;
5. fall back to stock `mm-ipod` if the preload library is unavailable.

The stock USB launcher driver field is executed as a program name; it cannot be replaced by a shell expression such as `LD_PRELOAD=... mm-ipod`.

A launcher and automated installer are not included in this repository.

## 3. Java bundle

Build output:

```text
build/mib2high-carplay-rgi.jar
```

Deploy the JAR to the K2161 LSD/OSGi environment and register this bundle entry point:

```text
org.mib2high.carplay.rgi.Activator
```

The OEM bundle registry is not distributed by this repository.

## 4. Safety / compatibility

This implementation is tied to the validated K2161 stock driver layout. Do not apply the documented addresses blindly to another firmware. The native code validates the expected stock state and fails closed when the supported layout is not present.
