#!/bin/sh
# MIB2 High Research Collector
# Generic, read-only collector for Harman/QNX MIB2 High research.
#
# SAFETY MODEL
# - Reads the MIB filesystem only.
# - Writes ONLY to a verified writable removable SD/USB location.
# - Does not remount filesystems, change permissions, stop/start services,
#   patch binaries, or modify vehicle configuration.
# - If writable removable media cannot be verified, collection does not start.
#
# Designed for unknown Audi / VW / Skoda / Seat / Porsche MIB2 High variants.

COLLECTOR_VERSION="1.0.2"
RULES_VERSION="3"
MAX_STRING_SCAN_BYTES="33554432"   # 32 MiB per file

# Optional override for unusual systems. Normally leave unset.
# Example: MIB2_OUTPUT_ROOT=/net/mmx/fs/sda0 sh custom.sh
OUTPUT_OVERRIDE="${MIB2_OUTPUT_ROOT:-}"
SCAN_ROOTS_OVERRIDE="${MIB2_SCAN_ROOTS:-}"
STRING_ROOTS_OVERRIDE="${MIB2_STRING_ROOTS:-}"

# -----------------------------------------------------------------------------
# Very small compatibility layer
# -----------------------------------------------------------------------------

have_cmd() {
    command -v "$1" >/dev/null 2>&1 && return 0
    type "$1" >/dev/null 2>&1 && return 0
    return 1
}

now() {
    if have_cmd date; then
        date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo "NO_TIME"
    else
        echo "NO_TIME"
    fi
}

datestamp() {
    if have_cmd date; then
        date '+%Y%m%d_%H%M%S' 2>/dev/null || echo "NODATE_$$"
    else
        echo "NODATE_$$"
    fi
}

say_raw() {
    echo "$*"
}

LOG_FILE=""
CAPS_FILE=""

log() {
    _msg="$(now) | $*"
    if [ -n "$LOG_FILE" ]; then
        echo "$_msg" >> "$LOG_FILE" 2>/dev/null
    fi
}

say() {
    echo "$*"
    log "SCREEN | $*"
}

separator() {
    say "----------------------------------------"
}

step() {
    say ""
    say "[$1/9] $2"
}

sanitize_token() {
    _in="$1"
    if have_cmd tr; then
        _s="$(echo "$_in" | tr -c 'A-Za-z0-9._-' '_' | tr -s '_' 2>/dev/null)"
        if have_cmd sed; then
            echo "$_s" | sed 's/^_*//;s/_*$//' 2>/dev/null
        else
            echo "$_s"
        fi
    elif have_cmd sed; then
        echo "$_in" | sed 's/[^A-Za-z0-9._-]/_/g;s/^_*//;s/_*$//'
    else
        echo "$_in"
    fi
}

path_parent() {
    _p="$1"
    case "$_p" in
        */*) echo "${_p%/*}" ;;
        *) echo "." ;;
    esac
}

is_number() {
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

# -----------------------------------------------------------------------------
# Find a removable output location and prove that we can really write to it.
# This happens before collection. No firmware collection occurs until this passes.
# -----------------------------------------------------------------------------

SCRIPT_DIR="${0%/*}"
[ "$SCRIPT_DIR" = "$0" ] && SCRIPT_DIR="."
SCRIPT_DIR_ABS=""
if have_cmd pwd; then
    SCRIPT_DIR_ABS="$(cd "$SCRIPT_DIR" 2>/dev/null && pwd 2>/dev/null)"
fi

looks_removable() {
    case "$1" in
        /net/mmx/fs/sd*|/net/mmx/fs/usb*|/net/mmx/fs/mmc*|\
        /fs/sd*|/fs/usb*|/fs/mmc*|/media/sd*|/media/usb*|/mnt/sd*|/mnt/usb*) return 0 ;;
        *) return 1 ;;
    esac
}

try_output_root() {
    _root="$1"
    [ -n "$_root" ] || return 1
    [ -d "$_root" ] || return 1

    _testdir="$_root/.mib2_collector_test_$$"
    _file1="$_testdir/write_test.txt"
    _file2="$_testdir/write_test_renamed.txt"

    mkdir "$_testdir" >/dev/null 2>&1 || return 1

    echo "MIB2_COLLECTOR_WRITE_TEST_$$" > "$_file1" 2>/dev/null || {
        rm -rf "$_testdir" >/dev/null 2>&1
        return 1
    }

    _readback=""
    if [ -r "$_file1" ]; then
        IFS= read _readback < "$_file1" 2>/dev/null
    fi
    [ "$_readback" = "MIB2_COLLECTOR_WRITE_TEST_$$" ] || {
        rm -rf "$_testdir" >/dev/null 2>&1
        return 1
    }

    # Rename test if mv exists. A failed rename rejects the medium because the
    # final collector folder is renamed after firmware identification.
    if have_cmd mv; then
        mv "$_file1" "$_file2" >/dev/null 2>&1 || {
            rm -rf "$_testdir" >/dev/null 2>&1
            return 1
        }
        [ -r "$_file2" ] || {
            rm -rf "$_testdir" >/dev/null 2>&1
            return 1
        }
    else
        rm -rf "$_testdir" >/dev/null 2>&1
        return 1
    fi

    if have_cmd rm; then
        rm -f "$_file2" >/dev/null 2>&1 || {
            rm -rf "$_testdir" >/dev/null 2>&1
            return 1
        }
    else
        return 1
    fi

    # Re-use the proven directory as the initial working directory.
    echo "$_testdir"
    return 0
}

say_raw ""
say_raw "========================================"
say_raw " MIB2 SOFTWARE COLLECTOR v$COLLECTOR_VERSION"
say_raw "========================================"
say_raw ""
say_raw "Checking the SD card before starting..."

OUTDIR=""
SD_ROOT=""

# Explicit override is trusted only after a real write/read/rename/delete test.
if [ -n "$OUTPUT_OVERRIDE" ]; then
    OUTDIR="$(try_output_root "$OUTPUT_OVERRIDE")"
    [ -n "$OUTDIR" ] && SD_ROOT="$OUTPUT_OVERRIDE"
fi

# If the script is somewhere below a known removable mount, prefer that mount
# root so the result is easy to find at the top of the SD card.
if [ -z "$OUTDIR" ] && [ -n "$SCRIPT_DIR_ABS" ]; then
    for _candidate in \
        /net/mmx/fs/sda0 /net/mmx/fs/sdb0 /net/mmx/fs/sdc0 /net/mmx/fs/sdd0 \
        /net/mmx/fs/sda1 /net/mmx/fs/sdb1 /net/mmx/fs/sdc1 /net/mmx/fs/sdd1 \
        /fs/sda0 /fs/sdb0 /fs/sdc0 /fs/sdd0 \
        /fs/sda1 /fs/sdb1 /fs/sdc1 /fs/sdd1 \
        /fs/sd0 /fs/sd1 /fs/usb0 /fs/usb1 \
        /fs/mmc0 /fs/mmc1
    do
        [ -d "$_candidate" ] || continue
        case "$SCRIPT_DIR_ABS" in
            "$_candidate"|"$_candidate"/*)
                OUTDIR="$(try_output_root "$_candidate")"
                if [ -n "$OUTDIR" ]; then
                    SD_ROOT="$_candidate"
                    break
                fi
                ;;
        esac
    done
fi

# Prefer the directory containing the script for unusual removable mount names.
if [ -z "$OUTDIR" ] && [ -n "$SCRIPT_DIR_ABS" ] && looks_removable "$SCRIPT_DIR_ABS"; then
    OUTDIR="$(try_output_root "$SCRIPT_DIR_ABS")"
    [ -n "$OUTDIR" ] && SD_ROOT="$SCRIPT_DIR_ABS"
fi

# Finally try all known/likely QNX removable-media mount points.
if [ -z "$OUTDIR" ]; then
    for _candidate in \
        /net/mmx/fs/sda0 /net/mmx/fs/sdb0 /net/mmx/fs/sdc0 /net/mmx/fs/sdd0 \
        /net/mmx/fs/sda1 /net/mmx/fs/sdb1 /net/mmx/fs/sdc1 /net/mmx/fs/sdd1 \
        /fs/sda0 /fs/sdb0 /fs/sdc0 /fs/sdd0 \
        /fs/sda1 /fs/sdb1 /fs/sdc1 /fs/sdd1 \
        /fs/sd0 /fs/sd1 /fs/usb0 /fs/usb1 \
        /fs/mmc0 /fs/mmc1 \
        /media/sd* /media/usb* /mnt/sd* /mnt/usb*
    do
        [ -d "$_candidate" ] || continue
        OUTDIR="$(try_output_root "$_candidate")"
        if [ -n "$OUTDIR" ]; then
            SD_ROOT="$_candidate"
            break
        fi
    done
fi

if [ -z "$OUTDIR" ]; then
    say_raw ""
    say_raw "========================================"
    say_raw " COLLECTION NOT STARTED"
    say_raw "========================================"
    say_raw ""
    say_raw "A writable SD/USB card was not found."
    say_raw "No firmware files were changed."
    say_raw ""
    say_raw "Check the SD card on a computer first."
    say_raw "Only run the collector again after the"
    say_raw "card has been fixed or replaced."
    say_raw "========================================"
    exit 2
fi

# Establish all output paths now that removable media is proven writable.
LOG_FILE="$OUTDIR/collector.log"
CAPS_FILE="$OUTDIR/capabilities.txt"
SYSTEM_DIR="$OUTDIR/system"
FILES_DIR="$OUTDIR/files"
DISCOVERY_DIR="$OUTDIR/discovery"
MANIFEST_FILE="$OUTDIR/manifest.tsv"
SOURCES_FILE="$DISCOVERY_DIR/collected_sources.txt"
META_CANDIDATES="$DISCOVERY_DIR/metadata_candidates.txt"
MATCHES_FILE="$DISCOVERY_DIR/relevant_matches.txt"
EVIDENCE_FILE="$DISCOVERY_DIR/string_match_evidence.txt"
DEP_NAMES_FILE="$DISCOVERY_DIR/dependency_names.txt"
INVENTORY_FILE="$DISCOVERY_DIR/filesystem_inventory.txt"

mkdir "$SYSTEM_DIR" >/dev/null 2>&1
mkdir "$FILES_DIR" >/dev/null 2>&1
mkdir "$DISCOVERY_DIR" >/dev/null 2>&1

: > "$LOG_FILE"
: > "$CAPS_FILE"
: > "$MANIFEST_FILE"
: > "$SOURCES_FILE"
: > "$META_CANDIDATES"
: > "$MATCHES_FILE"
: > "$EVIDENCE_FILE"
: > "$DEP_NAMES_FILE"
: > "$INVENTORY_FILE"

echo "access_path|canonical_path|saved_path|size_bytes|reason|checksum_method|checksum" >> "$MANIFEST_FILE"

log "COLLECTOR_START version=$COLLECTOR_VERSION rules=$RULES_VERSION pid=$$"
log "SCRIPT_PATH=$0"
log "SCRIPT_DIR=$SCRIPT_DIR_ABS"
log "SD_ROOT=$SD_ROOT"
log "OUTDIR_INITIAL=$OUTDIR"
log "SD_TEST mkdir=OK write=OK readback=OK rename=OK delete=OK"

# -----------------------------------------------------------------------------
# Capabilities / preflight
# -----------------------------------------------------------------------------

step 1 "Checking system..."

# We need sync before we can ever claim the card is safe to remove.
if ! have_cmd sync; then
    say "This unit is missing a function required"
    say "to save the files safely."
    say "Collection will not start."
    log "CRITICAL missing_command=sync"
    echo "INCOMPLETE: sync command unavailable" > "$OUTDIR/COLLECTION_INCOMPLETE.txt"
    exit 3
fi

step 2 "Checking available tools..."

record_tool() {
    _t="$1"
    if have_cmd "$_t"; then
        _where="$(command -v "$_t" 2>/dev/null)"
        echo "$_t|AVAILABLE|$_where" >> "$CAPS_FILE"
        log "TOOL $_t AVAILABLE path=$_where"
        return 0
    else
        echo "$_t|MISSING|" >> "$CAPS_FILE"
        log "TOOL $_t MISSING"
        return 1
    fi
}

# Check everything we may use, including fallbacks and useful QNX diagnostics.
for _tool in \
    sh echo printf date uname hostname mount df find grep strings awk sed tr \
    basename dirname readlink ls wc cp cat mkdir mv rm sync sleep du file \
    sha256sum md5sum md5 cksum tar gzip head sort uniq pidin use ldd readelf objdump
 do
    record_tool "$_tool" >/dev/null 2>&1
 done

# Test command OPTIONS, not only command presence.
TEST_FILE="$OUTDIR/.capability_test_$$.txt"
echo "CarPlay MHI2_TEST Alpha" > "$TEST_FILE" 2>/dev/null

CAP_MKDIR_P=0
CAP_FIND_NAME=0
CAP_GREP_I=0
CAP_GREP_E=0
CAP_GREP_A=0
CAP_STRINGS=0
CAP_AWK=0
CAP_READLINK=0

_TEST_D="$OUTDIR/.mkdir_p_test_$$/a/b"
if mkdir -p "$_TEST_D" >/dev/null 2>&1; then
    CAP_MKDIR_P=1
    echo "mkdir -p|SUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST mkdir_-p SUPPORTED"
else
    echo "mkdir -p|UNSUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST mkdir_-p UNSUPPORTED"
fi
rm -rf "$OUTDIR/.mkdir_p_test_$$" >/dev/null 2>&1

if have_cmd find && find "$OUTDIR" -name '.capability_test_*' >/dev/null 2>&1; then
    CAP_FIND_NAME=1
    echo "find -name|SUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST find_-name SUPPORTED"
else
    echo "find -name|UNSUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST find_-name UNSUPPORTED"
fi

if have_cmd grep && grep -i 'carplay' "$TEST_FILE" >/dev/null 2>&1; then
    CAP_GREP_I=1
    echo "grep -i|SUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-i SUPPORTED"
else
    echo "grep -i|UNSUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-i UNSUPPORTED"
fi

if have_cmd grep && grep -E 'CarPlay|NoSuchText' "$TEST_FILE" >/dev/null 2>&1; then
    CAP_GREP_E=1
    echo "grep -E|SUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-E SUPPORTED"
else
    echo "grep -E|UNSUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-E UNSUPPORTED"
fi

if have_cmd grep && grep -a 'CarPlay' "$TEST_FILE" >/dev/null 2>&1; then
    CAP_GREP_A=1
    echo "grep -a|SUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-a SUPPORTED"
else
    echo "grep -a|UNSUPPORTED" >> "$CAPS_FILE"
    log "OPTION_TEST grep_-a UNSUPPORTED"
fi

if have_cmd strings && strings "$TEST_FILE" >/dev/null 2>&1; then
    CAP_STRINGS=1
    echo "strings execution|SUPPORTED" >> "$CAPS_FILE"
    log "EXEC_TEST strings SUPPORTED"
else
    echo "strings execution|UNSUPPORTED" >> "$CAPS_FILE"
    log "EXEC_TEST strings UNSUPPORTED"
fi

if have_cmd awk && echo 'ABC' | awk '{x=tolower($0); if (x=="abc") exit 0; exit 1}' >/dev/null 2>&1; then
    CAP_AWK=1
    echo "awk tolower|SUPPORTED" >> "$CAPS_FILE"
    log "EXEC_TEST awk_tolower SUPPORTED"
else
    echo "awk tolower|UNSUPPORTED" >> "$CAPS_FILE"
    log "EXEC_TEST awk_tolower UNSUPPORTED"
fi

if have_cmd readlink; then
    CAP_READLINK=1
    echo "readlink|AVAILABLE" >> "$CAPS_FILE"
fi

rm -f "$TEST_FILE" >/dev/null 2>&1
sync >/dev/null 2>&1

# Determine selected fallback methods.
HASH_METHOD="none"
if have_cmd sha256sum; then
    HASH_METHOD="sha256sum"
elif have_cmd md5sum; then
    HASH_METHOD="md5sum"
elif have_cmd md5; then
    HASH_METHOD="md5"
elif have_cmd cksum; then
    HASH_METHOD="cksum"
fi
log "METHOD hash=$HASH_METHOD"
echo "SELECTED_HASH_METHOD=$HASH_METHOD" >> "$CAPS_FILE"

STRING_MODE="none"
if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
    STRING_MODE="strings_awk"
elif [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_GREP_I" -eq 1 ]; then
    STRING_MODE="strings_grep"
elif [ "$CAP_GREP_A" -eq 1 ] && [ "$CAP_GREP_I" -eq 1 ]; then
    STRING_MODE="grep_binary"
fi
log "METHOD string_scan=$STRING_MODE"
echo "SELECTED_STRING_SCAN=$STRING_MODE" >> "$CAPS_FILE"

# mkdir fallback used after this point.
make_dir() {
    _d="$1"
    [ -d "$_d" ] && return 0
    if [ "$CAP_MKDIR_P" -eq 1 ]; then
        mkdir -p "$_d" >/dev/null 2>&1
        return $?
    fi
    _parent="$(path_parent "$_d")"
    if [ "$_parent" != "$_d" ] && [ ! -d "$_parent" ]; then
        make_dir "$_parent" || return 1
    fi
    mkdir "$_d" >/dev/null 2>&1
}

# File size fallback.
file_size() {
    _f="$1"
    if have_cmd ls; then
        _lsline="$(ls -ln "$_f" 2>/dev/null)"
        set -- $_lsline
        _ls_size="$5"
        if is_number "$_ls_size"; then
            echo "$_ls_size"
            return 0
        fi
    fi
    if have_cmd wc; then
        _wc="$(wc -c < "$_f" 2>/dev/null)"
        set -- $_wc
        if is_number "$1"; then echo "$1"; return 0; fi
    fi
    echo "UNKNOWN"
}

checksum_file() {
    _f="$1"
    case "$HASH_METHOD" in
        sha256sum)
            _o="$(sha256sum "$_f" 2>/dev/null)"; set -- $_o; echo "$1" ;;
        md5sum)
            _o="$(md5sum "$_f" 2>/dev/null)"; set -- $_o; echo "$1" ;;
        md5)
            _o="$(md5 "$_f" 2>/dev/null)"; _last=""; for _x in $_o; do _last="$_x"; done; echo "$_last" ;;
        cksum)
            _o="$(cksum "$_f" 2>/dev/null)"; set -- $_o; echo "$1:$2" ;;
        *) echo "NONE" ;;
    esac
}

# Copy fallback: cp first, cat second. Destination stays on removable media.
copy_bytes() {
    _src="$1"
    _dst="$2"
    if have_cmd cp; then
        cp "$_src" "$_dst" >/dev/null 2>&1 && return 0
    fi
    if have_cmd cat; then
        cat "$_src" > "$_dst" 2>/dev/null && return 0
    fi
    return 1
}

storage_health_check() {
    _health="$OUTDIR/.storage_health_$$"
    echo "$(now) STORAGE_OK" > "$_health" 2>/dev/null || return 1
    [ -r "$_health" ] || return 1
    rm -f "$_health" >/dev/null 2>&1 || return 1
    return 0
}

fatal_storage() {
    sync >/dev/null 2>&1
    say_raw ""
    say_raw "========================================"
    say_raw " STORAGE ERROR - COLLECTION STOPPED"
    say_raw "========================================"
    say_raw "The collector could no longer save files"
    say_raw "to the SD card."
    say_raw "No vehicle firmware was modified."
    say_raw "It is now safe to remove the SD card."
    say_raw "Check the card on a computer first."
    say_raw "Only run the collector again after the"
    say_raw "card has been fixed or replaced."
    say_raw "========================================"
    exit 11
}

# -----------------------------------------------------------------------------
# Interruption handling
# -----------------------------------------------------------------------------

interrupted() {
    _sig="$1"
    log "INTERRUPTED signal=$_sig"
    echo "Collection was interrupted by signal $_sig." > "$OUTDIR/COLLECTION_INCOMPLETE.txt" 2>/dev/null
    sync >/dev/null 2>&1
    say_raw ""
    say_raw "========================================"
    say_raw " COLLECTION STOPPED"
    say_raw "========================================"
    say_raw "The collection did not finish."
    say_raw "Some files may already be on the SD card."
    say_raw "It is now safe to remove the SD card."
    say_raw "========================================"
    exit 10
}

trap 'interrupted HUP' HUP
trap 'interrupted INT' INT
trap 'interrupted TERM' TERM

# -----------------------------------------------------------------------------
# Identify the target infotainment filesystem layout. M.I.B.'s GEM launcher can
# execute from another QNX node while the relevant unit is exposed as /net/mmx.
# -----------------------------------------------------------------------------

TARGET_LAYOUT="local"
if [ -d /net/mmx/eso ] || [ -d /net/mmx/mnt/app/armle ]; then
    TARGET_LAYOUT="net_mmx"
fi
log "TARGET_LAYOUT=$TARGET_LAYOUT"

# -----------------------------------------------------------------------------
# SD information
# -----------------------------------------------------------------------------

step 3 "Checking SD card..."
say "SD card is ready."
say "Please keep the SD card inserted."
log "SD_CONFIRMED root=$SD_ROOT outdir=$OUTDIR"

{
    echo "Collector version: $COLLECTOR_VERSION"
    echo "Rules version: $RULES_VERSION"
    echo "Detected removable root: $SD_ROOT"
    echo "Target filesystem layout: $TARGET_LAYOUT"
    echo "Initial output directory: $OUTDIR"
    echo "Write test: PASS"
    echo "Read-back test: PASS"
    echo "Rename test: PASS"
    echo "Delete test: PASS"
    echo "Sync command: AVAILABLE"
} > "$SYSTEM_DIR/sd_card_check.txt"

if have_cmd df; then
    log "CMD df -k $SD_ROOT"
    df -k "$SD_ROOT" >> "$SYSTEM_DIR/sd_card_check.txt" 2>> "$LOG_FILE"
fi
if have_cmd mount; then
    log "CMD mount"
    mount > "$SYSTEM_DIR/mounts.txt" 2>> "$LOG_FILE"
fi
if have_cmd df; then
    log "CMD df -k"
    df -k > "$SYSTEM_DIR/df.txt" 2>> "$LOG_FILE"
fi

# -----------------------------------------------------------------------------
# General system/firmware identity collection
# -----------------------------------------------------------------------------

step 4 "Reading vehicle and software information..."

run_info() {
    _label="$1"
    _outfile="$2"
    shift 2
    log "CMD $_label"
    echo "### $_label" >> "$_outfile"
    "$@" >> "$_outfile" 2>> "$LOG_FILE"
    _rc=$?
    echo "" >> "$_outfile"
    log "CMD_RESULT $_label rc=$_rc"
    return $_rc
}

: > "$SYSTEM_DIR/system_info.txt"
if have_cmd uname; then run_info "uname -a" "$SYSTEM_DIR/system_info.txt" uname -a; fi
if have_cmd hostname; then run_info "hostname" "$SYSTEM_DIR/system_info.txt" hostname; fi
if have_cmd pidin; then
    run_info "pidin info" "$SYSTEM_DIR/system_info.txt" pidin info
    run_info "pidin ar" "$SYSTEM_DIR/processes.txt" pidin ar
fi
if have_cmd use; then run_info "use -i" "$SYSTEM_DIR/system_info.txt" use -i; fi
if [ -r /proc/version ]; then
    log "READ /proc/version"
    echo "### /proc/version" >> "$SYSTEM_DIR/system_info.txt"
    cat /proc/version >> "$SYSTEM_DIR/system_info.txt" 2>> "$LOG_FILE"
fi

# M.I.B.'s GEM launcher commonly reaches the infotainment filesystem through
# /net/mmx. TARGET_LAYOUT was selected before SD/system information was saved.
DEFAULT_SCAN_ROOTS=""
DEFAULT_META_ROOTS=""
DEFAULT_STRING_ROOTS=""
SUMMARY_ROOTS=""

if [ "$TARGET_LAYOUT" = "net_mmx" ]; then
    for _r in \
        /net/mmx/eso /net/mmx/mnt/eso /net/mmx/mnt/app/eso \
        /net/mmx/mnt/app/armle /net/mmx/mnt/app/usr /net/mmx/mnt/app/root \
        /net/mmx/ifs /net/mmx/etc /net/mmx/usr /net/mmx/lib /net/mmx/bin /net/mmx/sbin /net/mmx/opt
    do
        [ -d "$_r" ] && DEFAULT_SCAN_ROOTS="$DEFAULT_SCAN_ROOTS $_r"
    done
    for _r in /net/mmx/etc /net/mmx/eso /net/mmx/mnt/app/eso /net/mmx/mnt/app/armle /net/mmx/ifs /net/mmx/mnt/app/usr/etc; do
        [ -d "$_r" ] && DEFAULT_META_ROOTS="$DEFAULT_META_ROOTS $_r"
    done
    for _r in \
        /net/mmx/eso/bin /net/mmx/eso/lib /net/mmx/eso/usr/bin /net/mmx/eso/usr/lib \
        /net/mmx/mnt/app/eso/bin /net/mmx/mnt/app/eso/lib \
        /net/mmx/mnt/app/armle/bin /net/mmx/mnt/app/armle/lib \
        /net/mmx/mnt/app/armle/usr/bin /net/mmx/mnt/app/armle/usr/sbin /net/mmx/mnt/app/armle/usr/lib \
        /net/mmx/mnt/app/usr/bin /net/mmx/mnt/app/usr/sbin /net/mmx/mnt/app/usr/lib \
        /net/mmx/usr/bin /net/mmx/usr/sbin /net/mmx/usr/lib /net/mmx/lib /net/mmx/opt
    do
        [ -d "$_r" ] && DEFAULT_STRING_ROOTS="$DEFAULT_STRING_ROOTS $_r"
    done
else
    for _r in \
        /eso /mnt/eso /mnt/app/eso /mnt/app/armle /mnt/app/usr /mnt/app/root \
        /ifs /etc /usr /lib /bin /sbin /opt /armle
    do
        [ -d "$_r" ] && DEFAULT_SCAN_ROOTS="$DEFAULT_SCAN_ROOTS $_r"
    done
    for _r in /etc /eso /mnt/app/eso /mnt/app/armle /ifs /mnt/app/usr/etc /usr/etc /armle; do
        [ -d "$_r" ] && DEFAULT_META_ROOTS="$DEFAULT_META_ROOTS $_r"
    done
    for _r in \
        /eso/bin /eso/lib /eso/usr/bin /eso/usr/lib \
        /mnt/app/eso/bin /mnt/app/eso/lib \
        /mnt/app/armle/bin /mnt/app/armle/lib /mnt/app/armle/usr/bin /mnt/app/armle/usr/sbin /mnt/app/armle/usr/lib \
        /mnt/app/usr/bin /mnt/app/usr/sbin /mnt/app/usr/lib \
        /armle/bin /armle/lib /armle/usr/bin /armle/usr/sbin /armle/usr/lib \
        /usr/bin /usr/sbin /usr/lib /lib /opt
    do
        [ -d "$_r" ] && DEFAULT_STRING_ROOTS="$DEFAULT_STRING_ROOTS $_r"
    done
fi

# Directory summaries are valuable when firmware layouts differ.
SUMMARY_ROOTS="$DEFAULT_SCAN_ROOTS"
for _d in $SUMMARY_ROOTS; do
    [ -d "$_d" ] || continue
    if have_cmd ls; then
        _tag="$(echo "$_d" | tr '/' '_' 2>/dev/null)"
        [ -n "$_tag" ] || _tag="root"
        log "CMD ls -la $_d"
        ls -la "$_d" > "$SYSTEM_DIR/list_${_tag}.txt" 2>> "$LOG_FILE"
    fi
done

# Search roots are deliberately system-code oriented. Removable media and user
# data areas are excluded so the collector does not crawl maps, paired phones,
# music, or the SD card itself.
SCAN_ROOTS=""
if [ -n "$SCAN_ROOTS_OVERRIDE" ]; then
    for _r in $SCAN_ROOTS_OVERRIDE; do
        [ -d "$_r" ] && SCAN_ROOTS="$SCAN_ROOTS $_r"
    done
else
    SCAN_ROOTS="$DEFAULT_SCAN_ROOTS"
fi
log "SCAN_ROOTS=$SCAN_ROOTS"

# Generic enumerator. find is preferred; recursion fallback exists if needed.
walk_fallback() {
    _dir="$1"
    _callback="$2"
    _depth="$3"
    [ "$_depth" -gt 14 ] && return 0

    for _p in "$_dir"/*; do
        [ -f "$_p" ] || [ -d "$_p" ] || continue
        if [ -f "$_p" ]; then
            "$_callback" "$_p"
        elif [ -d "$_p" ]; then
            if [ "$CAP_READLINK" -eq 1 ] && readlink "$_p" >/dev/null 2>&1; then
                log "WALK_SKIP symlink_dir=$_p"
            else
                _next=$((_depth + 1))
                walk_fallback "$_p" "$_callback" "$_next"
            fi
        fi
    done
}

enumerate_files() {
    _root="$1"
    _callback="$2"
    if have_cmd find; then
        # Use plain -print and filter with the shell. This also includes symlinks
        # that resolve to regular files, which is important for versioned .so links.
        find "$_root" -print 2>> "$LOG_FILE" | while IFS= read -r _f; do
            [ -n "$_f" ] || continue
            [ -f "$_f" ] || continue
            "$_callback" "$_f"
        done
    else
        walk_fallback "$_root" "$_callback" 0
    fi
}

# Inventory all safe scan roots so later research can see what existed even if
# a file was not selected for copying.
say "Preparing the firmware file list..."
for _r in $SCAN_ROOTS; do
    log "INVENTORY_START root=$_r"
    echo "### ROOT $_r" >> "$INVENTORY_FILE"
    if have_cmd find; then
        find "$_r" -print >> "$INVENTORY_FILE" 2>> "$LOG_FILE"
    else
        echo "find unavailable; complete path inventory skipped for $_r" >> "$INVENTORY_FILE"
    fi
    log "INVENTORY_END root=$_r"
done

# Metadata-candidate discovery.
metadata_candidate() {
    _f="$1"
    _b="${_f##*/}"
    _lc="$(echo "$_b" | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz' 2>/dev/null)"
    case "$_lc" in
        *version*|*release*|*build*|*train*|*variant*|*production*|*software*|*sw_*|*device*|*metainfo*|*manifest*)
            _sz="$(file_size "$_f")"
            if is_number "$_sz" && [ "$_sz" -gt 4194304 ]; then
                log "META_SKIP too_large source=$_f size=$_sz"
                return 0
            fi
            echo "$_f" >> "$META_CANDIDATES"
            log "META_CANDIDATE source=$_f size=$_sz"
            ;;
    esac
}

META_ROOTS=""
if [ -n "$SCAN_ROOTS_OVERRIDE" ]; then
    META_ROOTS="$SCAN_ROOTS"
else
    META_ROOTS="$DEFAULT_META_ROOTS"
fi
log "META_ROOTS=$META_ROOTS"
for _r in $META_ROOTS; do
    enumerate_files "$_r" metadata_candidate
done

SOFTWARE_TRAIN=""
MEDIA_DRIVER_VERSION=""
IDENTITY_RAW="$DISCOVERY_DIR/identity_candidates.txt"
: > "$IDENTITY_RAW"

extract_identity_from_file() {
    _f="$1"
    [ -r "$_f" ] || return 0
    log "IDENTITY_SCAN source=$_f"

    if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
        strings "$_f" 2>/dev/null | awk '
        {
            for(i=1;i<=NF;i++) {
                t=$i
                gsub(/[",();:<>{}\[\]]/, "", t)
                if (t ~ /MHI2_/) { sub(/^.*MHI2_/, "MHI2_", t); print t }
                else if (t ~ /MHS2_/) { sub(/^.*MHS2_/, "MHS2_", t); print t }
                else if (t ~ /MST2_/) { sub(/^.*MST2_/, "MST2_", t); print t }
                else if (t ~ /MIB2_/) { sub(/^.*MIB2_/, "MIB2_", t); print t }
                else if (t ~ /DEV_MMX2_/) { sub(/^.*DEV_MMX2_/, "DEV_MMX2_", t); print t }
                else if (t ~ /DEV_MMX_/) { sub(/^.*DEV_MMX_/, "DEV_MMX_", t); print t }
            }
        }' | while IFS= read -r _id; do
            [ -n "$_id" ] || continue
            echo "$_f|$_id" >> "$IDENTITY_RAW"
        done
    elif [ "$CAP_GREP_A" -eq 1 ]; then
        grep -a 'MHI2_\|MHS2_\|MST2_\|MIB2_\|DEV_MMX' "$_f" 2>/dev/null | while IFS= read -r _id; do
            echo "$_f|$_id" >> "$IDENTITY_RAW"
        done
    fi
}

while IFS= read -r _mf; do
    [ -n "$_mf" ] && extract_identity_from_file "$_mf"
done < "$META_CANDIDATES"

# Choose first clean train/media tokens from the raw identity data.
while IFS='|' read -r _src _tok; do
    [ -n "$_tok" ] || continue
    case "$_tok" in
        MHI2_*|MHS2_*|MST2_*|MIB2_*)
            if [ -z "$SOFTWARE_TRAIN" ]; then SOFTWARE_TRAIN="$_tok"; fi
            ;;
        DEV_MMX2_*|DEV_MMX_*)
            if [ -z "$MEDIA_DRIVER_VERSION" ]; then MEDIA_DRIVER_VERSION="$_tok"; fi
            ;;
    esac
done < "$IDENTITY_RAW"

[ -n "$SOFTWARE_TRAIN" ] || SOFTWARE_TRAIN="UNKNOWN_TRAIN"
[ -n "$MEDIA_DRIVER_VERSION" ] || MEDIA_DRIVER_VERSION="UNKNOWN_MEDIA_DRIVER"

# Derive the usual VAG train version token (for example P4523 or K2161) when present.
SOFTWARE_VERSION="UNKNOWN"
_oldifs="$IFS"
IFS='_'
for _tok in $SOFTWARE_TRAIN; do
    case "$_tok" in
        P[0-9]*|K[0-9]*) SOFTWARE_VERSION="$_tok" ;;
    esac
done
IFS="$_oldifs"

BRAND="UNKNOWN"
case "$SOFTWARE_TRAIN $MEDIA_DRIVER_VERSION" in
    *SKG*|*SKODA*) BRAND="SKODA" ;;
    *AUG*|*AUDI*|*AU_ER*) BRAND="AUDI" ;;
    *VWG*|*VOLKSWAGEN*|*VW_ER*) BRAND="VW" ;;
    *SEG*|*SEAT*) BRAND="SEAT" ;;
    *POG*|*PORSCHE*) BRAND="PORSCHE" ;;
esac

OS_INFO="UNKNOWN"
if have_cmd uname; then
    OS_INFO="$(uname -s 2>/dev/null)_$(uname -r 2>/dev/null)"
fi

TRAIN_SAFE="$(sanitize_token "$SOFTWARE_TRAIN")"
BRAND_SAFE="$(sanitize_token "$BRAND")"
DATE_SAFE="$(datestamp)"
FINAL_BASE="MIB2_${BRAND_SAFE}_${TRAIN_SAFE}_${DATE_SAFE}"
[ -n "$FINAL_BASE" ] || FINAL_BASE="MIB2_UNKNOWN_$(datestamp)"

# Write identity before renaming the folder.
{
    echo "Collector version: $COLLECTOR_VERSION"
    echo "Rules version: $RULES_VERSION"
    echo "Brand: $BRAND"
    echo "Software train: $SOFTWARE_TRAIN"
    echo "Software version token: $SOFTWARE_VERSION"
    echo "Media driver version: $MEDIA_DRIVER_VERSION"
    echo "OS: $OS_INFO"
    echo "Collection time: $(now)"
    echo "Source script: $0"
    echo "Suggested archive name: ${FINAL_BASE}.zip"
    echo ""
    echo "Notes:"
    echo "- Brand is inferred from discovered software identifiers when possible."
    echo "- UNKNOWN means the collector did not find a trustworthy value."
    echo "- Raw identity candidates are in discovery/identity_candidates.txt."
} > "$SYSTEM_DIR/identity.txt"

log "IDENTITY brand=$BRAND train=$SOFTWARE_TRAIN software_version=$SOFTWARE_VERSION media_driver=$MEDIA_DRIVER_VERSION os=$OS_INFO"

# Rename the working folder to a concrete, useful research name.
FINAL_DIR="$SD_ROOT/$FINAL_BASE"
_suffix=1
while [ -d "$FINAL_DIR" ] || [ -f "$FINAL_DIR" ]; do
    _suffix=$((_suffix + 1))
    FINAL_DIR="$SD_ROOT/${FINAL_BASE}_$_suffix"
done

if mv "$OUTDIR" "$FINAL_DIR" >/dev/null 2>&1; then
    OLD_OUTDIR="$OUTDIR"
    OUTDIR="$FINAL_DIR"
    LOG_FILE="$OUTDIR/collector.log"
    CAPS_FILE="$OUTDIR/capabilities.txt"
    SYSTEM_DIR="$OUTDIR/system"
    FILES_DIR="$OUTDIR/files"
    DISCOVERY_DIR="$OUTDIR/discovery"
    MANIFEST_FILE="$OUTDIR/manifest.tsv"
    SOURCES_FILE="$DISCOVERY_DIR/collected_sources.txt"
    META_CANDIDATES="$DISCOVERY_DIR/metadata_candidates.txt"
    MATCHES_FILE="$DISCOVERY_DIR/relevant_matches.txt"
    EVIDENCE_FILE="$DISCOVERY_DIR/string_match_evidence.txt"
    DEP_NAMES_FILE="$DISCOVERY_DIR/dependency_names.txt"
    INVENTORY_FILE="$DISCOVERY_DIR/filesystem_inventory.txt"
    IDENTITY_RAW="$DISCOVERY_DIR/identity_candidates.txt"
    log "OUTDIR_RENAMED from=$OLD_OUTDIR to=$OUTDIR"
else
    log "OUTDIR_RENAME_FAILED requested=$FINAL_DIR keeping=$OUTDIR"
fi

# Save a copy of the exact collector script when possible.
if [ -r "$0" ]; then
    if copy_bytes "$0" "$OUTDIR/collector_source.sh"; then
        log "COLLECTOR_SOURCE_COPIED source=$0"
    else
        log "COLLECTOR_SOURCE_COPY_FAILED source=$0"
    fi
fi

# -----------------------------------------------------------------------------
# Core copying / manifest functions
# -----------------------------------------------------------------------------

canonical_source_path() {
    case "$1" in
        /net/mmx/*) echo "${1#/net/mmx}" ;;
        *) echo "$1" ;;
    esac
}

copy_one() {
    _src="$1"
    _reason="$2"

    [ -r "$_src" ] || {
        log "COPY_SKIP unreadable source=$_src reason=$_reason"
        return 1
    }
    [ -f "$_src" ] || {
        log "COPY_SKIP not_regular source=$_src reason=$_reason"
        return 1
    }

    _canonical="$(canonical_source_path "$_src")"
    _rel="${_canonical#/}"
    _dst="$FILES_DIR/$_rel"
    _parent="$(path_parent "$_dst")"

    # Existing destination means this source was already collected by another rule.
    if [ -f "$_dst" ]; then
        log "COPY_DUPLICATE source=$_src destination=$_dst reason=$_reason"
        return 0
    fi

    make_dir "$_parent" || {
        log "COPY_FAIL mkdir destination_parent=$_parent source=$_src"
        return 1
    }

    _size="$(file_size "$_src")"
    log "COPY_START source=$_src destination=$_dst reason=$_reason size=$_size"

    if ! copy_bytes "$_src" "$_dst"; then
        log "COPY_FAIL source=$_src destination=$_dst reason=$_reason"
        rm -f "$_dst" >/dev/null 2>&1
        if ! storage_health_check; then
            log "STORAGE_HEALTH after_copy_failure=FAILED source=$_src"
            fatal_storage
        fi
        return 1
    fi

    _saved_size="$(file_size "$_dst")"
    _sum="$(checksum_file "$_dst")"

    echo "$_src|$_canonical|$_dst|$_saved_size|$_reason|$HASH_METHOD|$_sum" >> "$MANIFEST_FILE"
    echo "$_src" >> "$SOURCES_FILE"
    echo "$_src|$_reason" >> "$MATCHES_FILE"
    log "COPY_OK source=$_src destination=$_dst size=$_saved_size checksum_method=$HASH_METHOD checksum=$_sum reason=$_reason"

    # Record symlink target information where readlink exists. cp/cat normally
    # gives us usable bytes; the link relationship itself is preserved here.
    if [ "$CAP_READLINK" -eq 1 ]; then
        _target="$(readlink "$_src" 2>/dev/null)"
        if [ -n "$_target" ]; then
            echo "$_src -> $_target" >> "$DISCOVERY_DIR/symlinks.txt"
            log "SYMLINK source=$_src target=$_target"
            # The copied file already contains usable bytes because cp/cat follows
            # regular-file symlinks. We intentionally do not recursively copy the
            # textual target path here; that avoids any ../ path traversal risk.
        fi
    fi

    return 0
}

# Copy small system metadata candidates as well; they are often where train,
# hardware, MU, region, and build information lives.
while IFS= read -r _mf; do
    [ -n "$_mf" ] || continue
    copy_one "$_mf" "system_metadata"
done < "$META_CANDIDATES"

# -----------------------------------------------------------------------------
# Relevant component discovery and collection
# -----------------------------------------------------------------------------

step 5 "Searching for relevant software..."
say "This may take a few minutes."

if storage_health_check; then
    log "STORAGE_HEALTH before_discovery=OK"
else
    log "STORAGE_HEALTH before_discovery=FAILED"
    fatal_storage
fi

is_known_exact_name() {
    case "$1" in
        libairplay.so|libairplay.so.*|ipod-drvr-iap2.so|ipod-drvr-iap2.so.*|\
        libiap2client.so|libiap2client.so.*|mm-ipod|dio_manager|irc_hb_bridge)
            return 0 ;;
        *) return 1 ;;
    esac
}

name_reason() {
    _path="$1"
    _base="${_path##*/}"
    _lc="$(echo "$_base" | tr 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz' 2>/dev/null)"

    if is_known_exact_name "$_base"; then
        echo "known_component"
        return 0
    fi

    case "$_lc" in
        *carplay*|*airplay*|*iap2*|*ipod*|*bap*|*fpk*|*cluster*|*kombiinstrument*|\
        *maneuver*|*manoeuvre*|*guidance*|*route*|*projection*|*mirrorlink*|\
        *androidauto*|*android_auto*|*smartphone*|*navigation*|*navdata*|*nav_*)
            echo "filename_match"
            return 0 ;;
    esac
    return 1
}

name_scan_file() {
    _f="$1"
    _reason="$(name_reason "$_f")"
    if [ -n "$_reason" ]; then
        copy_one "$_f" "$_reason"
    fi
}

for _r in $SCAN_ROOTS; do
    log "NAME_SCAN_START root=$_r"
    enumerate_files "$_r" name_scan_file
    log "NAME_SCAN_END root=$_r"
done

# High-signal string test. This intentionally avoids generic words such as
# "display" or "map" that would match huge parts of the HMI.
has_relevant_strings() {
    _f="$1"
    case "$STRING_MODE" in
        strings_awk)
            strings "$_f" 2>/dev/null | awk '
            {
                s=tolower($0)
                if (index(s,"carplay") || index(s,"airplay") || index(s,"iap2") ||
                    index(s,"routeguidance") || index(s,"route guidance") ||
                    index(s,"maneuver") || index(s,"manoeuvre") ||
                    index(s,"turn-by-turn") || index(s,"turn by turn") ||
                    index(s,"instrument cluster") || index(s,"kombiinstrument") ||
                    index(s,"navigation guidance") || index(s,"android auto") ||
                    index(s,"androidauto") || index(s,"mirrorlink") ||
                    index(s,"projection manager") || index(s,"bap") || index(s,"fpk")) {
                    found=1
                    exit
                }
            }
            END { if (found) exit 0; else exit 1 }'
            return $?
            ;;
        strings_grep)
            strings "$_f" 2>/dev/null | grep -i 'carplay' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'iap2' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'airplay' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'maneuver' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'route guidance' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'instrument cluster' >/dev/null 2>&1 && return 0
            strings "$_f" 2>/dev/null | grep -i 'android auto' >/dev/null 2>&1 && return 0
            return 1
            ;;
        grep_binary)
            grep -a -i 'carplay' "$_f" >/dev/null 2>&1 && return 0
            grep -a -i 'iap2' "$_f" >/dev/null 2>&1 && return 0
            grep -a -i 'airplay' "$_f" >/dev/null 2>&1 && return 0
            grep -a -i 'maneuver' "$_f" >/dev/null 2>&1 && return 0
            return 1
            ;;
        *) return 1 ;;
    esac
}

write_string_evidence() {
    _f="$1"
    echo "### $_f" >> "$EVIDENCE_FILE"
    if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
        strings "$_f" 2>/dev/null | awk '
        {
            s=tolower($0)
            if (index(s,"carplay") || index(s,"airplay") || index(s,"iap2") ||
                index(s,"routeguidance") || index(s,"route guidance") ||
                index(s,"maneuver") || index(s,"manoeuvre") ||
                index(s,"turn-by-turn") || index(s,"turn by turn") ||
                index(s,"instrument cluster") || index(s,"kombiinstrument") ||
                index(s,"navigation guidance") || index(s,"android auto") ||
                index(s,"androidauto") || index(s,"mirrorlink") ||
                index(s,"projection manager") || index(s,"bap") || index(s,"fpk")) {
                print
                n++
                if (n>=20) exit
            }
        }' >> "$EVIDENCE_FILE"
    fi
    echo "" >> "$EVIDENCE_FILE"
}

string_scan_file() {
    _f="$1"
    _size="$(file_size "$_f")"
    if is_number "$_size" && [ "$_size" -gt "$MAX_STRING_SCAN_BYTES" ]; then
        log "STRING_SKIP too_large source=$_f size=$_size"
        return 0
    fi

    if has_relevant_strings "$_f"; then
        log "STRING_MATCH source=$_f size=$_size"
        write_string_evidence "$_f"
        copy_one "$_f" "string_match"
    fi
}

# String scans are intentionally limited to code/library trees. This keeps the
# first run useful without crawling map databases or user data.
STRING_ROOTS=""
if [ -n "$STRING_ROOTS_OVERRIDE" ]; then
    for _r in $STRING_ROOTS_OVERRIDE; do
        [ -d "$_r" ] && STRING_ROOTS="$STRING_ROOTS $_r"
    done
else
    STRING_ROOTS="$DEFAULT_STRING_ROOTS"
fi
log "STRING_ROOTS=$STRING_ROOTS"

if [ "$STRING_MODE" != "none" ]; then
    say "Checking software contents..."
    for _r in $STRING_ROOTS; do
        log "STRING_SCAN_START root=$_r mode=$STRING_MODE"
        enumerate_files "$_r" string_scan_file
        log "STRING_SCAN_END root=$_r"
    done
else
    log "STRING_SCAN_SKIPPED no_supported_method"
fi

# -----------------------------------------------------------------------------
# Direct shared-library dependency expansion
# -----------------------------------------------------------------------------

step 6 "Copying relevant files..."
say "Checking for related software files..."

extract_deps() {
    _src="$1"
    [ -r "$_src" ] || return 0

    if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
        strings "$_src" 2>/dev/null | awk '
        {
            for (i=1; i<=NF; i++) {
                t=$i
                gsub(/[",();:<>{}\[\]]/, "", t)
                if (t ~ /^lib[A-Za-z0-9_.+-]*\.so([.][A-Za-z0-9_.+-]+)?$/)
                    print t
            }
        }' >> "$DEP_NAMES_FILE"
    elif [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_GREP_I" -eq 1 ]; then
        strings "$_src" 2>/dev/null | grep '\.so' >> "$DISCOVERY_DIR/dependency_raw_strings.txt" 2>/dev/null
    fi
}

while IFS= read -r _src; do
    [ -n "$_src" ] || continue
    extract_deps "$_src"
done < "$SOURCES_FILE"

find_and_copy_basename() {
    _name="$1"
    [ -n "$_name" ] || return 0

    if [ "$CAP_FIND_NAME" -eq 1 ]; then
        for _r in $SCAN_ROOTS; do
            find "$_r" -name "$_name" -print 2>> "$LOG_FILE" | while IFS= read -r _hit; do
                [ -n "$_hit" ] || continue
                [ -f "$_hit" ] || continue
                copy_one "$_hit" "direct_dependency:$_name"
            done
        done
    else
        # Fallback: enumerate and compare basenames.
        for _r in $SCAN_ROOTS; do
            if have_cmd find; then
                find "$_r" -print 2>> "$LOG_FILE" | while IFS= read -r _hit; do
                    [ -f "$_hit" ] || continue
                    [ "${_hit##*/}" = "$_name" ] && copy_one "$_hit" "direct_dependency:$_name"
                done
            fi
        done
    fi
}

# Duplicates are harmless: copy_one detects an already saved destination.
while IFS= read -r _dep; do
    [ -n "$_dep" ] || continue
    log "DEPENDENCY_LOOKUP name=$_dep"
    find_and_copy_basename "$_dep"
done < "$DEP_NAMES_FILE"

# -----------------------------------------------------------------------------
# Final accounting / verification
# -----------------------------------------------------------------------------

step 7 "Recording file information..."

# Hash the collector source itself if it was successfully copied.
if [ -f "$OUTDIR/collector_source.sh" ]; then
    _collector_sum="$(checksum_file "$OUTDIR/collector_source.sh")"
    echo "Collector source checksum method: $HASH_METHOD" >> "$SYSTEM_DIR/identity.txt"
    echo "Collector source checksum: $_collector_sum" >> "$SYSTEM_DIR/identity.txt"
    log "COLLECTOR_SOURCE_HASH method=$HASH_METHOD checksum=$_collector_sum"
fi

# Record final file count / size using whatever the unit supports.
FILE_COUNT="UNKNOWN"
if have_cmd find && have_cmd wc; then
    _fc="$(find "$FILES_DIR" -type f -print 2>/dev/null | wc -l 2>/dev/null)"
    set -- $_fc
    [ -n "$1" ] && FILE_COUNT="$1"
fi
COLLECTION_KB="UNKNOWN"
if have_cmd du; then
    _du="$(du -sk "$FILES_DIR" 2>/dev/null)"
    set -- $_du
    [ -n "$1" ] && COLLECTION_KB="$1"
fi

{
    echo "Collected file count: $FILE_COUNT"
    echo "Collected files size (KiB): $COLLECTION_KB"
    echo "Manifest: manifest.tsv"
    echo "Full operation log: collector.log"
    echo "Capabilities: capabilities.txt"
    echo "Filesystem inventory: discovery/filesystem_inventory.txt"
    echo "Relevant matches: discovery/relevant_matches.txt"
    echo "String evidence: discovery/string_match_evidence.txt"
    echo "Raw identity candidates: discovery/identity_candidates.txt"
} > "$SYSTEM_DIR/collection_summary.txt"

log "SUMMARY files=$FILE_COUNT size_kib=$COLLECTION_KB"

step 8 "Finalizing collection..."

if storage_health_check; then
    log "STORAGE_HEALTH before_finalize=OK"
else
    log "STORAGE_HEALTH before_finalize=FAILED"
    fatal_storage
fi

# Re-write identity with the actual folder/archive suggestion in case a suffix
# was added because an older collection already existed.
ACTUAL_BASE="${OUTDIR##*/}"
{
    echo ""
    echo "Final collection folder: $ACTUAL_BASE"
    echo "Suggested archive name: ${ACTUAL_BASE}.zip"
} >> "$SYSTEM_DIR/identity.txt"

# Completion marker is written BEFORE the final sync and verified after it.
COMPLETE_FILE="$OUTDIR/COLLECTION_COMPLETE.txt"
{
    echo "MIB2 COLLECTION COMPLETE"
    echo "Collector version: $COLLECTOR_VERSION"
    echo "Brand: $BRAND"
    echo "Software train: $SOFTWARE_TRAIN"
    echo "Software version token: $SOFTWARE_VERSION"
    echo "Media driver version: $MEDIA_DRIVER_VERSION"
    echo "Collected files: $FILE_COUNT"
    echo "Collected size KiB: $COLLECTION_KB"
    echo "Completed: $(now)"
    echo "Suggested archive name: ${ACTUAL_BASE}.zip"
    echo "Status: COMPLETE"
} > "$COMPLETE_FILE"
log "COMPLETE_MARKER_WRITTEN path=$COMPLETE_FILE"

step 9 "Saving data to SD card..."
say "Finishing the saved files..."
log "SYNC_START pass=1"
sync >/dev/null 2>&1
log "SYNC_END pass=1"
if have_cmd sleep; then sleep 2; fi
log "SYNC_START pass=2"
sync >/dev/null 2>&1
log "SYNC_END pass=2"

# Final read-back. Retry internally so the end user is never told to keep
# launching Custom Script for the same unresolved problem.
VERIFY_STATUS=""
VERIFY_ATTEMPT=1
while [ "$VERIFY_ATTEMPT" -le 3 ]; do
    VERIFY_STATUS=""
    log "FINAL_VERIFY_START attempt=$VERIFY_ATTEMPT path=$COMPLETE_FILE"

    if [ -r "$COMPLETE_FILE" ]; then
        if have_cmd grep && grep 'Status: COMPLETE' "$COMPLETE_FILE" >/dev/null 2>&1; then
            VERIFY_STATUS="OK"
        else
            # grep fallback: shell read line-by-line.
            while IFS= read -r _line; do
                [ "$_line" = "Status: COMPLETE" ] && VERIFY_STATUS="OK"
            done < "$COMPLETE_FILE"
        fi
    fi

    if [ "$VERIFY_STATUS" = "OK" ]; then
        log "FINAL_VERIFY_OK attempt=$VERIFY_ATTEMPT path=$COMPLETE_FILE"
        break
    fi

    log "FINAL_VERIFY_RETRY attempt=$VERIFY_ATTEMPT path=$COMPLETE_FILE"
    sync >/dev/null 2>&1
    if have_cmd sleep; then sleep 2; fi
    VERIFY_ATTEMPT=$((VERIFY_ATTEMPT + 1))
 done

if [ "$VERIFY_STATUS" != "OK" ]; then
    log "FINAL_VERIFY_FAILED attempts=3 complete_marker=$COMPLETE_FILE"
    echo "The collector could not verify the saved result after 3 attempts." > "$OUTDIR/COLLECTION_INCOMPLETE.txt" 2>/dev/null
    sync >/dev/null 2>&1
    if have_cmd sleep; then sleep 2; fi
    sync >/dev/null 2>&1

    say_raw ""
    say_raw "----------------------------------------"
    say_raw "COLLECTION NOT COMPLETED"
    say_raw "----------------------------------------"
    say_raw "The collector tried several times but"
    say_raw "could not confirm that everything was"
    say_raw "saved correctly."
    say_raw ""
    say_raw "It is now safe to remove the SD card."
    say_raw ""
    say_raw "Do not run Custom Script again yet."
    say_raw "Check the SD card on a computer and"
    say_raw "send the collection folder to the"
    say_raw "project maintainer for inspection."
    say_raw "----------------------------------------"
    exit 4
fi

log "FINAL_VERIFY_OK complete_marker=$COMPLETE_FILE"
log "COLLECTOR_END status=COMPLETE outdir=$OUTDIR"
sync >/dev/null 2>&1

# IMPORTANT: use say_raw below. The final sync is already complete, so there
# must be no more writes to the SD card after the SAFE TO REMOVE message.
say_raw ""
say_raw "========================================"
say_raw " COLLECTION COMPLETE"
say_raw "========================================"
say_raw ""
say_raw "All collected data has been saved."
say_raw "Folder: $ACTUAL_BASE"
say_raw ""
say_raw "It is now SAFE TO REMOVE THE SD CARD."
say_raw ""
say_raw "Copy or ZIP the complete folder above"
say_raw "and send it to the project maintainer."
say_raw "========================================"

# Keep the final message visible for a moment. No SD access occurs here.
if have_cmd sleep; then sleep 8; fi
exit 0
