# Build

## Native library

Requirements:

- your own QNX SDP 6.5 build environment
- ARMv7 little-endian target (`gcc_ntoarmv7le` through `qcc`)
- `QNX_HOST` and `QNX_TARGET` set for that environment

Example:

```sh
export QNX_HOST=/path/to/qnx650/host/qnx6/x86
export QNX_TARGET=/path/to/qnx650/target/qnx6
./scripts/build-native.sh
```

Output:

```text
build/libmib2high-carplay-rgi.so
```

## Java bundle

Requirements:

- JDK 8
- a locally supplied K2161 LSD compile-dependency JAR derived from the target firmware

By default the build script looks for:

```text
java/lib/lsd-K2161-base.jar
```

The OEM-derived JAR is **not distributed** by this repository.

Build with the default location:

```sh
./scripts/build-java.sh
```

or point to the dependency explicitly:

```sh
K2161_BASE_JAR=/path/to/lsd-K2161-base.jar ./scripts/build-java.sh
```

If JDK 8 is not already on `PATH`, set `JAVA_HOME`:

```sh
JAVA_HOME=/path/to/jdk8 ./scripts/build-java.sh
```

Output:

```text
build/mib2high-carplay-rgi.jar
```

The Java sources use source/target level `1.4` because the K2161 LSD runtime uses a legacy JVM.

## Binary compatibility

The fixed SHA-256 check that matters to this project is the stock K2161 `ipod-drvr-iap2.so` compatibility check documented in [implementation details](implementation.md). The native hook relies on firmware-specific structures and offsets and therefore validates the stock driver before applying them.

Build-machine paths, locally generated artifacts and the OEM-derived Java compile dependency are intentionally not pinned to private development-environment hashes.
