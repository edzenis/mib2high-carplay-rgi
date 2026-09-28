#!/bin/sh
set -u

PATH=/bin:/usr/bin:/sbin:/usr/sbin:$PATH
export PATH

SD="/net/mmx/fs/sda0"
APP="/mnt/app"
SYSTEM="/mnt/system"
RCC_SHMEM="/net/rcc/dev/shmem"
DEV_SHMEM="/dev/shmem"

MOD="$SD/mod/mib2high-carplay-rgi"
PAYLOAD="$MOD/payload"
PAYLOAD_SO="$PAYLOAD/libmib2high-carplay-rgi.so"
PAYLOAD_SI="$PAYLOAD/smartphone_integrator.json"

LIVE_SO="$APP/root/hooks/libmib2high-carplay-rgi.so"
LIVE_SI="$SYSTEM/etc/eso/production/smartphone_integrator.json"

BACKUP="$MOD/backup/previous"
COLLECT_ROOT="$MOD/collected"
INSTALLED_FLAG="$MOD/installed.ok"
ROLLBACK_REQUEST="$MOD/ROLLBACK"

ALT_MARKER="$APP/carplay_altscreen"
VERBOSE_MARKER="$APP/carplay_verbose"
PRELOAD_LINE="LD_PRELOAD=/mnt/app/root/hooks/libmib2high-carplay-rgi.so"

APP_REMOUNTED=0
SYSTEM_REMOUNTED=0
SD_WRITE_TEST="$SD/.mib2high-rgi-write-test.$$"

say() { echo "[ALTSCREEN] $*"; }
fail() { say "ERROR: $*"; exit 1; }
file_ok() { [ -f "$1" ] && [ -s "$1" ]; }

restore_mounts() {
    sync
    if [ "$SYSTEM_REMOUNTED" = "1" ]; then
        mount -ur "$SYSTEM" >/dev/null 2>&1 || true
        SYSTEM_REMOUNTED=0
    fi
    if [ "$APP_REMOUNTED" = "1" ]; then
        mount -ur "$APP" >/dev/null 2>&1 || true
        APP_REMOUNTED=0
    fi
}
trap 'restore_mounts' 0 1 2 15

mount_sd_rw() {
    if touch "$SD_WRITE_TEST" 2>/dev/null; then
        rm -f "$SD_WRITE_TEST" 2>/dev/null
        return 0
    fi

    rm -f "$SD_WRITE_TEST" 2>/dev/null
    say "SD is read-only; attempting: mount -uw $SD"
    mount -uw "$SD" >/dev/null 2>&1 || true

    if touch "$SD_WRITE_TEST" 2>/dev/null; then
        rm -f "$SD_WRITE_TEST" 2>/dev/null
        say "SD_WRITE=PASS"
        return 0
    fi

    return 1
}

ensure_rw() {
    MOUNTPOINT="$1"
    WHICH="$2"
    TESTFILE="$MOUNTPOINT/.mib2high-rgi-rw-test.$$"

    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE"
        return 0
    fi

    say "Remounting $MOUNTPOINT read-write..."
    mount -uw "$MOUNTPOINT" >/dev/null 2>&1 || return 1

    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE"
        if [ "$WHICH" = "app" ]; then
            APP_REMOUNTED=1
        else
            SYSTEM_REMOUNTED=1
        fi
        return 0
    fi

    return 1
}

backup_current() {
    mkdir -p "$BACKUP" || fail "cannot create backup directory on SD"

    rm -f "$BACKUP/libmib2high-carplay-rgi.so"
    rm -f "$BACKUP/smartphone_integrator.json"
    rm -f "$BACKUP/had-carplay-altscreen" "$BACKUP/had-carplay-verbose"

    file_ok "$LIVE_SO" || fail "current live .so missing or empty"
    file_ok "$LIVE_SI" || fail "live smartphone_integrator.json missing or empty: $LIVE_SI"

    cp "$LIVE_SO" "$BACKUP/libmib2high-carplay-rgi.so" ||
        fail "cannot back up live .so"

    cp "$LIVE_SI" "$BACKUP/smartphone_integrator.json" ||
        fail "cannot back up smartphone_integrator.json"

    [ -e "$ALT_MARKER" ] && touch "$BACKUP/had-carplay-altscreen"
    [ -e "$VERBOSE_MARKER" ] && touch "$BACKUP/had-carplay-verbose"

    sync
}

restore_previous() {
    say "Restoring previous state..."

    file_ok "$BACKUP/libmib2high-carplay-rgi.so" ||
        fail "backup .so missing or empty"

    file_ok "$BACKUP/smartphone_integrator.json" ||
        fail "backup smartphone_integrator.json missing or empty"

    # Use rename for the shared object so we do not modify the inode of a
    # library that may already be mapped by a running process.
    RESTORE_SO="$LIVE_SO.mib2high-rgi.restore"
    rm -f "$RESTORE_SO"
    cp "$BACKUP/libmib2high-carplay-rgi.so" "$RESTORE_SO" ||
        fail "cannot stage previous .so"
    chmod 755 "$RESTORE_SO" >/dev/null 2>&1 || true
    mv -f "$RESTORE_SO" "$LIVE_SO" ||
        fail "cannot restore previous .so"

    # JSON is deliberately restored by direct copy.
    cp "$BACKUP/smartphone_integrator.json" "$LIVE_SI" ||
        fail "cannot restore smartphone_integrator.json"

    if [ -e "$BACKUP/had-carplay-altscreen" ]; then
        touch "$ALT_MARKER"
    else
        rm -f "$ALT_MARKER"
    fi

    if [ -e "$BACKUP/had-carplay-verbose" ]; then
        touch "$VERBOSE_MARKER"
    else
        rm -f "$VERBOSE_MARKER"
    fi

    rm -f "$INSTALLED_FLAG"
    sync
}

install_payload() {
    say "MODE=INSTALL"

    file_ok "$PAYLOAD_SO" ||
        fail "payload .so missing or empty: $PAYLOAD_SO"

    file_ok "$PAYLOAD_SI" ||
        fail "payload smartphone_integrator.json missing or empty: $PAYLOAD_SI"

    grep "$PRELOAD_LINE" "$PAYLOAD_SI" >/dev/null 2>&1 ||
        fail "payload smartphone_integrator.json does not contain expected LD_PRELOAD"

    grep '"exec":"dio_manager"' "$PAYLOAD_SI" >/dev/null 2>&1 ||
        fail "payload smartphone_integrator.json does not contain CarPlay dio_manager entry"

    grep 'IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production' "$PAYLOAD_SI" >/dev/null 2>&1 ||
        fail "payload smartphone_integrator.json does not contain expected CarPlay config path"

    mount_sd_rw || fail "SD is not writable"
    ensure_rw "$APP" app || fail "cannot make /mnt/app writable"
    ensure_rw "$SYSTEM" system || fail "cannot make /mnt/system writable"

    backup_current

    # Shared library: stage + rename intentionally. The old .so may already
    # be mapped by a running process, so do not truncate its live inode.
    TMP_SO="$LIVE_SO.mib2high-rgi.new"
    rm -f "$TMP_SO"

    say "Installing AltScreen .so..."
    cp "$PAYLOAD_SO" "$TMP_SO" || fail "failed to stage .so"
    chmod 755 "$TMP_SO" >/dev/null 2>&1 || true
    file_ok "$TMP_SO" || fail "staged .so missing or empty"

    mv -f "$TMP_SO" "$LIVE_SO" || {
        restore_previous
        fail "failed to install .so"
    }

    chmod 755 "$LIVE_SO" >/dev/null 2>&1 || true

    # Install the prepared configuration without editing it on the unit.
    say "Installing prepared smartphone_integrator.json..."
    cp "$PAYLOAD_SI" "$LIVE_SI" || {
        restore_previous
        fail "failed to install smartphone_integrator.json"
    }

    file_ok "$LIVE_SO" || {
        restore_previous
        fail "installed .so missing or empty"
    }

    file_ok "$LIVE_SI" || {
        restore_previous
        fail "installed smartphone_integrator.json missing or empty"
    }

    grep "$PRELOAD_LINE" "$LIVE_SI" >/dev/null 2>&1 || {
        restore_previous
        fail "installed smartphone_integrator.json is missing expected LD_PRELOAD"
    }

    touch "$ALT_MARKER" || {
        restore_previous
        fail "cannot create AltScreen marker"
    }

    touch "$VERBOSE_MARKER" || {
        restore_previous
        fail "cannot create verbose marker"
    }

    sync

    : > "$INSTALLED_FLAG" || {
        restore_previous
        fail "cannot create install-state flag on SD"
    }

    say "JSON_INSTALL=DIRECT_PAYLOAD_COPY"
    say "PRELOAD=ON"
    say "ALTSCREEN_MARKER=ON"
    say "VERBOSE_LOGGING=ON"
    say "INSTALL_RESULT=PASS"
    say "Reboot MMI manually before testing."
}

next_collection_dir() {
    N=1
    while [ "$N" -le 99 ]; do
        if [ "$N" -lt 10 ]; then
            D="$COLLECT_ROOT/run-0$N"
        else
            D="$COLLECT_ROOT/run-$N"
        fi

        if [ ! -e "$D" ]; then
            echo "$D"
            return 0
        fi

        N=$((N + 1))
    done
    return 1
}

copy_if_exists() {
    SRC="$1"
    DEST="$2"
    [ -f "$SRC" ] && cp "$SRC" "$DEST/" >/dev/null 2>&1
}

collect_logs() {
    say "MODE=COLLECT"
    mount_sd_rw || fail "SD is not writable"

    RUN_DIR=`next_collection_dir` || fail "run-01..run-99 already exist"

    mkdir -p "$RUN_DIR/live" "$RUN_DIR/logs" "$RUN_DIR/rcc-shmem" "$RUN_DIR/dev-shmem" ||
        fail "cannot create collection directory"

    copy_if_exists "$LIVE_SO" "$RUN_DIR/live"
    copy_if_exists "$LIVE_SI" "$RUN_DIR/live"

    copy_if_exists "/tmp/carplay_hook.log" "$RUN_DIR/logs"
    copy_if_exists "/tmp/carplay_hook.log.1" "$RUN_DIR/logs"
    copy_if_exists "/tmp/carplay_hook.log.2" "$RUN_DIR/logs"
    copy_if_exists "/tmp/carplay_hook.log.3" "$RUN_DIR/logs"

    for F in "$RCC_SHMEM"/*mib2high* "$RCC_SHMEM"/*iap2* "$RCC_SHMEM"/*carplay*; do
        [ -f "$F" ] && cp "$F" "$RUN_DIR/rcc-shmem/" >/dev/null 2>&1
    done

    for F in "$DEV_SHMEM"/*mib2high* "$DEV_SHMEM"/*iap2* "$DEV_SHMEM"/*carplay*; do
        [ -f "$F" ] && cp "$F" "$RUN_DIR/dev-shmem/" >/dev/null 2>&1
    done

    {
        echo "MIB2 High CarPlay AltScreen collection"
        echo "LIVE_SO=$LIVE_SO"
        echo "LIVE_SI=$LIVE_SI"
        echo "ALTSCREEN_MARKER=`[ -e "$ALT_MARKER" ] && echo 1 || echo 0`"
        echo "VERBOSE_MARKER=`[ -e "$VERBOSE_MARKER" ] && echo 1 || echo 0`"

        if grep "$PRELOAD_LINE" "$LIVE_SI" >/dev/null 2>&1; then
            echo "PRELOAD=1"
        else
            echo "PRELOAD=0"
        fi

        date 2>/dev/null || true
    } > "$RUN_DIR/report.txt"

    sync
    say "COLLECT_RESULT=PASS"
    say "Saved to: $RUN_DIR"
}

rollback() {
    say "MODE=ROLLBACK"

    mount_sd_rw || fail "SD is not writable"
    ensure_rw "$APP" app || fail "cannot make /mnt/app writable"
    ensure_rw "$SYSTEM" system || fail "cannot make /mnt/system writable"

    restore_previous
    rm -f "$ROLLBACK_REQUEST"
    sync

    say "ROLLBACK_RESULT=PASS"
    say "Previous .so and smartphone_integrator.json restored."
    say "Reboot MMI manually."
}

echo
say "CarPlay AltScreen installer / collector"
say "================================================="

[ -d "$SD" ] || fail "SD path is unavailable: $SD"

if [ -f "$ROLLBACK_REQUEST" ]; then
    rollback
elif [ -f "$INSTALLED_FLAG" ]; then
    collect_logs
else
    install_payload
fi

say "Finished."
exit 0
