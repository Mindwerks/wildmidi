#!/bin/sh
#
# ci-local.sh -- run the GitHub CI jobs locally.
#
# Mirrors the jobs in .github/workflows so changes can be validated before
# committing and pushing.  Toolchains that are not installed are reported as
# SKIP rather than failing the run, so this is useful even on a machine that
# only has the native compilers.
#
# Usage:
#   ./ci-local.sh --all           everything, including the BSD VMs
#   ./ci-local.sh --docker        run the compile jobs in the toolchain container
#   ./ci-local.sh --qemu          also run the FreeBSD/OpenBSD/NetBSD VMs
#   ./ci-local.sh                 run on the host, using whatever is installed
#   ./ci-local.sh native zig      run only the named jobs
#   ./ci-local.sh --list          show the job names and their workflow
#   ./ci-local.sh --docker-build  build (or rebuild) the toolchain image
#   ./ci-local.sh --strict        treat unavailable toolchains as failures
#   ./ci-local.sh --keep          keep build trees for inspection
#
# The BSD jobs boot real VMs under qemu/kvm and are much slower than the rest
# (minutes, not seconds), which is why they need --qemu or --all.  All three
# boot at once.  They need qemu, mtools, rsync, sfdisk and access to /dev/kvm.
#
# --docker is the recommended way to run this: it supplies the cross
# toolchains, so jobs are actually exercised instead of skipped.  The image is
# built once from .ci/Dockerfile.  The AmigaOS job always uses the same
# amigadev/crosstools image that upstream CI uses, in or out of --docker.
#
# Environment:
#   WATCOM      OpenWatcom install root, for the os2/windows Watcom jobs
#   DJGPP_DIR   DJGPP install root (default /opt/djgpp)
#   NDK         Android NDK root (falls back to ANDROID_NDK_LATEST_HOME)
#   JOBS        parallel make/cmake jobs (default: nproc)
#   CI_IMAGE    toolchain image name (default wildmidi-ci:local)
#
# This file is part of WildMIDI and shares its license.

set -u

REPO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$REPO" || exit 1

: "${DJGPP_DIR:=/opt/djgpp}"
: "${NDK:=${ANDROID_NDK_LATEST_HOME:-}}"
: "${JOBS:=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"
: "${CI_IMAGE:=wildmidi-ci:local}"

STRICT=0
KEEP=0
USE_DOCKER=0
USE_QEMU=0
LOGDIR="$REPO/.ci-local"

# --- output helpers ---------------------------------------------------------
# Colour when writing to a terminal.  Inside the container stdout is a pipe,
# so CI_COLOR carries the host's decision in rather than losing it -- without
# that, the jobs run in docker print plain text while the ones run on the host
# come out coloured.
if [ "${CI_COLOR:-}" = 1 ] || { [ -z "${CI_COLOR:-}" ] && [ -t 1 ]; }; then
    C_RED=$(printf '\033[31m'); C_GRN=$(printf '\033[32m')
    C_YEL=$(printf '\033[33m'); C_BLD=$(printf '\033[1m')
    C_RST=$(printf '\033[0m')
else
    C_RED=; C_GRN=; C_YEL=; C_BLD=; C_RST=
fi

PASSED=; FAILED=; SKIPPED=

pass()  { PASSED="$PASSED $1";  printf '%s  PASS%s  %s\n' "$C_GRN" "$C_RST" "$1"; }
fail()  { FAILED="$FAILED $1";  printf '%s  FAIL%s  %s   (log: %s)\n' "$C_RED" "$C_RST" "$1" "$2"; }
skip()  {
    if [ "$STRICT" -eq 1 ]; then
        FAILED="$FAILED $1"
        printf '%s  FAIL%s  %s   (missing: %s)\n' "$C_RED" "$C_RST" "$1" "$2"
    else
        SKIPPED="$SKIPPED $1"
        printf '%s  SKIP%s  %s   (missing: %s)\n' "$C_YEL" "$C_RST" "$1" "$2"
    fi
}

have() { command -v "$1" >/dev/null 2>&1; }

# --- docker -----------------------------------------------------------------
docker_build() {
    have docker || { printf 'docker is not installed\n' >&2; return 1; }
    printf '%s>> building %s from .ci/Dockerfile%s\n' "$C_BLD" "$CI_IMAGE" "$C_RST"
    docker build -t "$CI_IMAGE" -f "$REPO/.ci/Dockerfile" "$REPO/.ci"
}

# Re-run this script inside the toolchain image.  The repo is bind-mounted and
# we run as the caller's uid so build products are not left owned by root.
docker_run() {
    have docker || { printf 'docker is not installed\n' >&2; return 1; }

    if ! docker image inspect "$CI_IMAGE" >/dev/null 2>&1; then
        printf '%simage %s not found, building it once%s\n' \
               "$C_YEL" "$CI_IMAGE" "$C_RST"
        docker_build || return 1
    fi

    # The amiga job shells out to docker itself, which we cannot nest, so it
    # is run on the host afterwards instead.
    _inner=$(printf '%s ' "$@")
    docker run --rm \
        -v "$REPO":/src \
        -u "$(id -u):$(id -g)" \
        -e HOME=/tmp \
        -e "JOBS=$JOBS" \
        -e "CI_COLOR=$([ -n "$C_GRN" ] && echo 1 || echo 0)" \
        -w /src \
        "$CI_IMAGE" \
        ./ci-local.sh --no-amiga $_inner
    return $?
}

# Run a job: run_job <name> <command-string>.  Output goes to a log; only
# failures print it, so a clean run stays readable.
run_job() {
    _name=$1
    _cmd=$2
    _log="$LOGDIR/$_name.log"

    printf '%s>> %s%s\n' "$C_BLD" "$_name" "$C_RST"
    if ( eval "$_cmd" ) >"$_log" 2>&1; then
        pass "$_name"
        return 0
    fi
    fail "$_name" "$_log"
    printf '     --- last 20 lines ---\n'
    tail -20 "$_log" | sed 's/^/     /'
    return 1
}

# --- job definitions --------------------------------------------------------
# Each job is <name>:<workflow>:<description>.  Keep in sync with .github/workflows.
JOB_LIST="
native:main.yml:cmake build + ctest with gcc and clang
zig:zig.yml:zig build and zig build test
mingw32:cross.yml:MinGW-w64 i686 static+shared
mingw64:cross.yml:MinGW-w64 x86_64 static+shared
djgpp:cross.yml:DJGPP DOS build
amiga:cross.yml:AmigaOS m68k library (docker)
os2emx:cross.yml:OS/2 EMX cross build
android:cross.yml:Android NDK build
watcom-os2:watcom.yml:OpenWatcom OS/2 build
watcom-win32:watcom.yml:OpenWatcom Win32 build
freebsd:bsd.yml:FreeBSD 14.4 in qemu (--qemu)
openbsd:bsd.yml:OpenBSD 7.9 in qemu (--qemu)
netbsd:bsd.yml:NetBSD 10.1 in qemu (--qemu)
"

list_jobs() {
    printf '%sAvailable jobs%s\n' "$C_BLD" "$C_RST"
    echo "$JOB_LIST" | while IFS=: read -r n w d; do
        [ -n "$n" ] || continue
        printf '  %-14s %-12s %s\n' "$n" "$w" "$d"
    done
}

# --- individual jobs --------------------------------------------------------

# main.yml: cmake configure, build, ctest, install -- once per compiler.
job_native() {
    _rc=0
    for cc in gcc clang; do
        if ! have "$cc"; then
            skip "native($cc)" "$cc"
            continue
        fi
        _b="$REPO/build-ci-$cc"
        rm -rf "$_b"
        run_job "native($cc)" "
            CC=$cc cmake -B '$_b' \
                -DWANT_PLAYER=ON -DWANT_STATIC=ON -DWANT_ALSA=ON -DWANT_OSS=ON \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX='$_b/out' &&
            cmake --build '$_b' -j$JOBS --config Release &&
            ctest --test-dir '$_b' --output-on-failure -C Release &&
            cmake --install '$_b'
        " || _rc=1
        [ "$KEEP" -eq 1 ] || rm -rf "$_b"
    done
    return $_rc
}

# zig.yml.  CI installs a current zig; an older local one fails on the
# language itself rather than on anything in this repo, so check first.
job_zig() {
    have zig || { skip zig zig; return 0; }

    _need=$(sed -n 's/.*minimum_zig_version[^"]*"\([^"]*\)".*/\1/p' build.zig.zon 2>/dev/null)
    _got=$(zig version 2>/dev/null)
    if [ -n "$_need" ] && [ -n "$_got" ]; then
        # Sort the two versions and see if ours is the lower one.
        _low=$(printf '%s\n%s\n' "$_need" "$_got" | sort -V | head -1)
        if [ "$_low" = "$_got" ] && [ "$_got" != "$_need" ]; then
            skip zig "zig >= $_need (have $_got)"
            return 0
        fi
    fi
    run_job zig "zig build --summary all && zig build test --summary all"
}

# cross.yml: mingw.  CI runs 'static shared'.
job_mingw32() { _mingw i686-w64-mingw32 mingw32; }
job_mingw64() { _mingw x86_64-w64-mingw32 mingw64; }
_mingw() {
    _cross=$1; _name=$2
    have "$_cross-gcc" || { skip "$_name" "$_cross-gcc"; return 0; }
    run_job "$_name" "
        make -C '$REPO/mingw' CROSS=$_cross clean >/dev/null 2>&1;
        make -C '$REPO/mingw' -j$JOBS CROSS=$_cross static shared &&
        make -C '$REPO/mingw' CROSS=$_cross distclean
    "
}

# cross.yml: djgpp
job_djgpp() {
    _dj=$DJGPP_DIR/bin
    if ! have i586-pc-msdosdjgpp-gcc && [ ! -x "$_dj/i586-pc-msdosdjgpp-gcc" ]; then
        skip djgpp "i586-pc-msdosdjgpp-gcc (set DJGPP_DIR)"
        return 0
    fi
    run_job djgpp "
        PATH='$DJGPP_DIR/i586-pc-msdosdjgpp/bin:$_dj':\$PATH;
        export PATH;
        make -C '$REPO/djgpp' CROSS=i586-pc-msdosdjgpp USE_DXE=0 clean >/dev/null 2>&1;
        make -C '$REPO/djgpp' -j$JOBS CROSS=i586-pc-msdosdjgpp USE_DXE=0 &&
        make -C '$REPO/djgpp' CROSS=i586-pc-msdosdjgpp USE_DXE=0 distclean
    "
}

# cross.yml: amiga-m68k, which CI runs inside a container.  Library only --
# the player does not link there (clib2 libm gap), same as CI.
job_amiga() {
    have docker || { skip amiga docker; return 0; }
    run_job amiga "
        docker run --rm -v '$REPO':/src -w /src/amiga \
            amigadev/crosstools:m68k-amigaos \
            sh -c 'make libWildMidi.a AOS3=1 CROSS=m68k-amigaos -j$JOBS && make distclean'
    "
}

# The Amiga player sources are not built by CI, so compile them here to keep
# that backend honest.  Only runs when the container image is already present.
job_amiga_player() {
    have docker || return 0
    docker image inspect amigadev/crosstools:m68k-amigaos >/dev/null 2>&1 || return 0
    run_job "amiga(playlist.c)" "
        docker run --rm -v '$REPO':/src -w /src/amiga \
            amigadev/crosstools:m68k-amigaos \
            m68k-amigaos-gcc -c -o /tmp/playlist.o ../src/player/playlist.c \
                -I. -I../include -DWILDMIDI_AMIGA -Wall -Wextra -Werror
    "
}

# cross.yml: os2-emx
job_os2emx() {
    have i686-pc-os2-emx-gcc || { skip os2emx i686-pc-os2-emx-gcc; return 0; }
    run_job os2emx "
        make -C '$REPO/os2' -f makefile.emx -j$JOBS CROSS=i686-pc-os2-emx &&
        make -C '$REPO/os2' -f makefile.emx distclean
    "
}

# cross.yml: android.  CI builds through $ANDROID_NDK_LATEST_HOME.
job_android() {
    if [ -z "$NDK" ] || [ ! -x "$NDK/ndk-build" ]; then
        skip android "Android NDK (set NDK)"
        return 0
    fi
    run_job android "
        '$NDK/ndk-build' -C '$REPO/android' \
            NDK_PROJECT_PATH=. \
            APP_BUILD_SCRIPT=jni/Android.mk \
            NDK_APPLICATION_MK=jni/Application.mk &&
        rm -rf '$REPO/android/libs' '$REPO/android/obj'
    "
}

# watcom.yml.  wmake needs WATCOM set and its binaries on PATH.
_watcom_env() {
    [ -n "${WATCOM:-}" ] || return 1
    for d in binl64 binl; do
        [ -d "$WATCOM/$d" ] && PATH="$WATCOM/$d:$PATH"
    done
    export WATCOM PATH
    export EDPATH="$WATCOM/eddat" WIPFC="$WATCOM/wipfc"
    have wmake
}

job_watcom_os2() {
    _watcom_env || { skip watcom-os2 "wmake (set WATCOM)"; return 0; }
    run_job watcom-os2 "
        cd '$REPO/os2' &&
        wmake -f makefile.wat &&
        wmake -f makefile.wat distclean
    "
}

job_watcom_win32() {
    _watcom_env || { skip watcom-win32 "wmake (set WATCOM)"; return 0; }
    run_job watcom-win32 "
        cd '$REPO/win32wat' &&
        wmake &&
        wmake distclean
    "
}

# bsd.yml.  Each OS boots its own qemu VM, so they can all run at once; a VM
# spends most of its wall time booting and fetching packages rather than using
# the cpu.  Gated behind --qemu because it is far slower than the other jobs.
BSD_CACHE="${BSD_CACHE:-$LOGDIR/vm-images}"

bsd_missing() {
    _m=
    have qemu-system-x86_64 || _m="$_m qemu-system-x86_64"
    have qemu-img           || _m="$_m qemu-img"
    have mformat            || _m="$_m mtools"
    have rsync              || _m="$_m rsync"
    have sfdisk             || _m="$_m sfdisk"
    [ -r /dev/kvm ] && [ -w /dev/kvm ] || _m="$_m /dev/kvm"
    printf '%s' "${_m# }"
}

job_bsd() {
    _want=$1              # space separated list of freebsd/openbsd/netbsd
    _miss=$(bsd_missing)
    if [ -n "$_miss" ]; then
        for _o in $_want; do skip "$_o" "$_miss"; done
        return 0
    fi


    # Fetch any missing images first.  Downloading half a gigabyte while the
    # other VMs are trying to boot starves them of I/O and makes them look
    # like they hung.
    for _o in $_want; do
        .ci/bsd-vm.sh "$_o" "$REPO" 0 "$BSD_CACHE" --fetch-only \
            >>"$LOGDIR/$_o.log" 2>&1 || true
    done

    # Launch each VM in the background on its own ssh port.
    _port=2222
    _pids=
    for _o in $_want; do
        printf '%s>> %s (qemu)%s\n' "$C_BLD" "$_o" "$C_RST"
        ( .ci/bsd-vm.sh "$_o" "$REPO" "$_port" "$BSD_CACHE" ) \
            >"$LOGDIR/$_o.log" 2>&1 &
        _pids="$_pids $_o:$!"
        _port=$((_port + 1))
    done

    # Collect them.  Waiting on each in turn is fine: we need every result
    # before the summary either way.
    _rc=0
    for _e in $_pids; do
        _o=${_e%%:*}
        _p=${_e##*:}
        if wait "$_p"; then
            pass "$_o"
        else
            fail "$_o" "$LOGDIR/$_o.log"
            printf '     --- last 20 lines ---\n'
            tail -20 "$LOGDIR/$_o.log" | sed 's/^/     /'
            _rc=1
        fi
    done
    return $_rc
}

# --- argument handling ------------------------------------------------------
WANTED=
NO_AMIGA=0
ARGS_FOR_DOCKER=
while [ $# -gt 0 ]; do
    case $1 in
        --list|-l) list_jobs; exit 0 ;;
        --docker)  USE_DOCKER=1 ;;
        --qemu)    USE_QEMU=1 ;;
        --all)     USE_DOCKER=1; USE_QEMU=1 ;;
        --docker-build)
            docker_build; exit $? ;;
        --no-amiga) NO_AMIGA=1 ;;
        --strict)  STRICT=1; ARGS_FOR_DOCKER="$ARGS_FOR_DOCKER --strict" ;;
        --keep)    KEEP=1;   ARGS_FOR_DOCKER="$ARGS_FOR_DOCKER --keep" ;;
        -h|--help)
            sed -n '3,33p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
        *)  WANTED="$WANTED $1"; ARGS_FOR_DOCKER="$ARGS_FOR_DOCKER $1" ;;
    esac
    shift
done

# In --docker mode, hand off to the container for everything except the amiga
# jobs, which need their own image and so cannot be nested; those run here on
# the host afterwards, with their own short summary.
if [ "$USE_DOCKER" -eq 1 ]; then
    docker_run $ARGS_FOR_DOCKER
    _drc=$?

    if [ -z "$WANTED" ] || printf '%s' "$WANTED" | grep -q amiga; then
        mkdir -p "$LOGDIR"
        printf '\n%s>> amiga jobs (amigadev/crosstools image)%s\n' "$C_BLD" "$C_RST"
        job_amiga
        job_amiga_player
    fi

    # BSD VMs also stay outside the container: qemu needs /dev/kvm.
    _bsds=
    for _o in freebsd openbsd netbsd; do
        { [ -z "$WANTED" ] || printf '%s' "$WANTED" | grep -q "$_o"; } \
            && _bsds="$_bsds $_o"
    done
    if [ -n "$_bsds" ]; then
        mkdir -p "$LOGDIR"
        if [ "$USE_QEMU" -eq 1 ] || [ -n "$WANTED" ]; then
            job_bsd "$_bsds"
        else
            for _o in $_bsds; do skip "$_o" "not requested (use --qemu)"; done
        fi
    fi

    set -- $FAILED
    if [ $# -ne 0 ]; then
        _drc=1
        printf '%shost-side failures:%s%s\n' "$C_RED" "$C_RST" "$FAILED"
    fi

    if [ "$_drc" -ne 0 ]; then
        printf '\n%sCI would fail. Logs in %s%s\n' "$C_RED" "$LOGDIR" "$C_RST"
    fi
    exit $_drc
fi

wanted() {
    [ -z "$WANTED" ] && return 0
    for w in $WANTED; do
        [ "$w" = "$1" ] && return 0
    done
    return 1
}

# --- run --------------------------------------------------------------------
mkdir -p "$LOGDIR"
printf '%sWildMIDI local CI%s  (repo: %s, jobs: %s)\n\n' "$C_BLD" "$C_RST" "$REPO" "$JOBS"

wanted native       && job_native
wanted zig          && job_zig
wanted mingw32      && job_mingw32
wanted mingw64      && job_mingw64
wanted djgpp        && job_djgpp
wanted amiga        && [ "$NO_AMIGA" -eq 0 ] && { job_amiga; job_amiga_player; }
wanted os2emx       && job_os2emx
wanted android      && job_android
wanted watcom-os2   && job_watcom_os2
wanted watcom-win32 && job_watcom_win32

# BSD VMs run on the host: qemu needs /dev/kvm, which the toolchain container
# does not get.  Only run when asked, and only when not already inside it.
if [ "$NO_AMIGA" -eq 0 ]; then
    _bsds=
    for _o in freebsd openbsd netbsd; do
        wanted "$_o" && _bsds="$_bsds $_o"
    done
    if [ -n "$_bsds" ]; then
        if [ "$USE_QEMU" -eq 1 ] || [ -n "$WANTED" ]; then
            job_bsd "$_bsds"
        else
            for _o in $_bsds; do skip "$_o" "not requested (use --qemu)"; done
        fi
    fi
fi

# --- summary ----------------------------------------------------------------
echo
printf '%s---- summary ----%s\n' "$C_BLD" "$C_RST"
set -- $PASSED;  printf 'passed:  %s\n' "$#"
set -- $SKIPPED; printf 'skipped: %s%s\n' "$#" "${SKIPPED:+ -$SKIPPED}"
set -- $FAILED;  _nfail=$#
printf 'failed:  %s%s\n' "$_nfail" "${FAILED:+ -$FAILED}"

if [ "$_nfail" -ne 0 ]; then
    printf '\n%sCI would fail. Logs in %s%s\n' "$C_RED" "$LOGDIR" "$C_RST"
    exit 1
fi
printf '\n%sAll available jobs passed.%s\n' "$C_GRN" "$C_RST"
exit 0
