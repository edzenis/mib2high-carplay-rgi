#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SRC="$ROOT/java/src"
LIB="${K2161_BASE_JAR:-$ROOT/java/lib/lsd-K2161-base.jar}"
OUTDIR="$ROOT/build/java"
CLASSES="$OUTDIR/classes"
JAR_OUT="$ROOT/build/mib2high-carplay-rgi.jar"

if [ -n "${JAVA_HOME:-}" ]; then
    JAVAC="$JAVA_HOME/bin/javac"
    JAR_TOOL="$JAVA_HOME/bin/jar"
else
    JAVAC=$(command -v javac || true)
    JAR_TOOL=$(command -v jar || true)
fi

[ -f "$LIB" ] || {
    echo "ERROR: missing K2161 base JAR: $LIB" >&2
    echo "See docs/build.md. The OEM-derived compile dependency is not distributed." >&2
    exit 1
}
[ -n "$JAVAC" ] && [ -x "$JAVAC" ] || {
    echo "ERROR: JDK 8 javac not found. Set JAVA_HOME or add JDK 8 to PATH." >&2
    exit 1
}
[ -n "$JAR_TOOL" ] && [ -x "$JAR_TOOL" ] || {
    echo "ERROR: JDK 8 jar tool not found. Set JAVA_HOME or add JDK 8 to PATH." >&2
    exit 1
}

case "$("$JAVAC" -version 2>&1)" in
    "javac 1.8."*) ;;
    *)
        echo "ERROR: JDK 8 is required by this build." >&2
        exit 1
        ;;
esac

rm -rf "$OUTDIR"
mkdir -p "$CLASSES" "$ROOT/build"

"$JAVAC" \
    -source 1.4 \
    -target 1.4 \
    -encoding UTF-8 \
    -cp "$LIB" \
    -sourcepath "$SRC" \
    -d "$CLASSES" \
    "$SRC/framework/Log.java" \
    "$SRC/framework/CarplayBus.java" \
    "$SRC/routeguidance/K2161GatedCombiService.java" \
    "$SRC/routeguidance/K2161RouteGuidanceOwnership.java" \
    "$SRC/routeguidance/RouteGuidance.java" \
    "$SRC/routeguidance/BAPBridge.java" \
    "$SRC/routeguidance/ManeuverMapper.java" \
    "$SRC/routeguidance/SideStreets.java" \
    "$SRC/app/CarPlayLifecycle.java" \
    "$SRC/app/Activator.java"

(
    cd "$CLASSES"
    "$JAR_TOOL" cf "$JAR_OUT" .
)

echo "OUTPUT=$JAR_OUT"
sha256sum "$JAR_OUT"
