#!/bin/sh
#
# bsd-vm.sh -- build WildMIDI inside a BSD VM under QEMU/KVM.
#
# This is the local equivalent of the bsd.yml workflow, which uses
# cross-platform-actions/action.  That action also runs QEMU on a plain Linux
# runner, so the mechanism here is the same: boot the same disk image the
# action uses, hand the guest an ssh key through a small FAT disk, rsync the
# work tree in, and run the build over ssh.
#
# Usage: bsd-vm.sh <freebsd|openbsd|netbsd> <repo-dir> <ssh-port> <cache-dir>
#                  [--fetch-only|--refresh]
#
# --fetch-only downloads the disk image and stops, so that several VMs can be
# started afterwards without competing for bandwidth.
# --refresh discards the cached "prepared" image (the one with cmake already
# installed) and builds it again, e.g. to pick up newer packages.
#
# This file is part of WildMIDI and shares its license.

set -u

OS=${1:?usage: bsd-vm.sh <os> <repo> <port> <cache>}
REPO=${2:?}
PORT=${3:?}
CACHE=${4:?}
MODE=${5:-}

# Image versions.  Each builder repo has its own release tag, so these do not
# share a version -- check the builder's releases when bumping an OS version.
case $OS in
freebsd)
    OSVER=14.4
    BUILDER_TAG=v0.15.0
    CMAKE_OPTS="-DWANT_OSS=ON"
    SETUP_CMD="sudo pkg update -q"
    INSTALL_CMD="sudo pkg install -y cmake"
    ;;
openbsd)
    OSVER=7.9
    BUILDER_TAG=v0.13.0
    CMAKE_OPTS="-DWANT_SNDIO=ON"
    SETUP_CMD="sudo pkg_add -u"
    INSTALL_CMD="sudo pkg_add cmake"
    ;;
netbsd)
    OSVER=10.1
    BUILDER_TAG=v0.6.0
    CMAKE_OPTS="-DWANT_NETBSD=ON"
    # Mirrors bsd.yml: pkgin needs PKG_PATH, and the build needs ASLR off.
    SETUP_CMD='export PATH="/usr/pkg/sbin:/usr/pkg/bin:/sbin:$PATH";
               export PKG_PATH="https://cdn.netBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -p)/$(uname -r|cut -f "1 2" -d.)/All/";
               sudo -E sysctl -w security.pax.aslr.enabled=0 >/dev/null;
               sudo -E sysctl -w security.pax.aslr.global=0 >/dev/null;
               sudo -E pkgin -y update'
    INSTALL_CMD='export PATH="/usr/pkg/sbin:/usr/pkg/bin:/sbin:$PATH";
                 export PKG_PATH="https://cdn.netBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -p)/$(uname -r|cut -f "1 2" -d.)/All/";
                 sudo -E pkgin -y install cmake'
    ;;
*)
    echo "bsd-vm.sh: unknown os '$OS'" >&2
    exit 2
    ;;
esac

IMAGE=$OS-$OSVER-x86-64.qcow2
IMAGE_URL=https://github.com/cross-platform-actions/$OS-builder/releases/download/$BUILDER_TAG/$IMAGE

WORK=$(mktemp -d "${TMPDIR:-/tmp}/wm-$OS-XXXXXX") || exit 1
QEMU_PID=

cleanup() {
    [ -n "$QEMU_PID" ] && kill "$QEMU_PID" 2>/dev/null
    # Give qemu a moment to release the disks before the directory goes.
    [ -n "$QEMU_PID" ] && { sleep 2; kill -9 "$QEMU_PID" 2>/dev/null; }
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

say() { echo "[$OS] $*"; }

# --- image ------------------------------------------------------------------
mkdir -p "$CACHE"
if [ ! -f "$CACHE/$IMAGE" ]; then
    say "downloading $IMAGE"
    curl -fsSL --retry 3 -o "$CACHE/$IMAGE.part" "$IMAGE_URL" || exit 1
    mv "$CACHE/$IMAGE.part" "$CACHE/$IMAGE"
fi

[ "$MODE" = "--fetch-only" ] && exit 0

# Installing cmake in the guest is by far the slowest part of a run -- around
# a minute on NetBSD, whose pkgin pulls in eight packages and upgrades a few
# of the base ones on the way.  So do it once and keep the result: PREPARED is
# a copy of the pristine image with cmake already in it, and normal runs
# overlay that instead.  Delete it (or pass --refresh) to rebuild.
PRISTINE=$(cd "$CACHE" && pwd)/$IMAGE
PREPARED=$(cd "$CACHE" && pwd)/$OS-$OSVER-prepared.qcow2

[ "$MODE" = "--refresh" ] && rm -f "$PREPARED"

if [ -f "$PREPARED" ]; then
    BASE=$PREPARED
    NEED_PACKAGES=0
else
    BASE=$PRISTINE
    NEED_PACKAGES=1
fi

# A qcow2 overlay keeps the base image pristine and makes this run throwaway,
# which also means parallel runs cannot corrupt each other.
say "preparing disk"
if [ "$NEED_PACKAGES" = 1 ]; then
    # First run for this OS: build the prepared image in place, so what we
    # install now survives for later runs.
    qemu-img create -q -f qcow2 -F qcow2 -b "$PRISTINE" "$WORK/disk.qcow2" >/dev/null || exit 1
else
    qemu-img create -q -f qcow2 -F qcow2 -b "$BASE" "$WORK/disk.qcow2" >/dev/null || exit 1
fi

# --- ssh key, handed over on a small FAT disk -------------------------------
# The action does this with losetup+mkfs.fat+mount, which needs root; mtools
# writes the same filesystem without it.
#
# The layout differs per guest, matching what the action passes to losetup:
# FreeBSD reads a partitioned disk (offset 1MiB), while OpenBSD and NetBSD
# expect the filesystem to start at sector 0 with no partition table.  Get
# this wrong and the guest boots fine but never installs the key, so the only
# symptom is ssh refusing the key forever.
ssh-keygen -t ed25519 -f "$WORK/key" -q -N "" || exit 1
truncate -s 40m "$WORK/res.raw" || exit 1

if [ "$OS" = freebsd ]; then
    printf 'label: dos\nstart=2048, type=c\n' | sfdisk -q "$WORK/res.raw" >/dev/null 2>&1 || exit 1
    RES_IMG="$WORK/res.raw@@1M"
else
    RES_IMG="$WORK/res.raw"
fi
# The volume label is RES and the key file is a lowercase "keys": the action
# does `copyFileSync(publicKey, mountPath + '/keys')` on a disk labelled RES,
# and the guests' provision.sh reads that path.  Spelling it KEYS gets the
# guest booting happily but never authorised, which looks like a hang.
mformat -i "$RES_IMG" -F -v RES :: || exit 1
mcopy -i "$RES_IMG" "$WORK/key.pub" ::/keys || exit 1

# --- boot -------------------------------------------------------------------
# Disk, network and firmware flags are per-guest, matching what the action's
# operating_systems/<os>/qemu_vm.ts sets up:
#
#   FreeBSD  virtio-blk (it looks for vtbd0; SCSI drops it at mountroot>)
#   OpenBSD  scsi + e1000 nic + UEFI, and -pdpe1gb to disable huge pages,
#            without which it will not boot (qemu issue 1091)
#   NetBSD   scsi, and ipv6 off on the user network
case $OS in
freebsd)
    DISK_FLAGS="-device virtio-blk-pci,drive=drive0,bootindex=0
                -drive if=none,file=$WORK/disk.qcow2,id=drive0,cache=unsafe,format=qcow2
                -device virtio-blk-pci,drive=drive1,bootindex=1
                -drive if=none,file=$WORK/res.raw,id=drive1,cache=unsafe,format=raw"
    NET_DEV=virtio-net-pci
    NETDEV="user,id=user.0,hostfwd=tcp::$PORT-:22"
    CPU_FLAGS=host
    FIRMWARE=
    ;;
*)
    DISK_FLAGS="-device virtio-scsi-pci
                -device scsi-hd,drive=drive0,bootindex=0
                -drive if=none,file=$WORK/disk.qcow2,id=drive0,cache=unsafe,format=qcow2
                -device scsi-hd,drive=drive1,bootindex=1
                -drive if=none,file=$WORK/res.raw,id=drive1,cache=unsafe,format=raw"
    if [ "$OS" = openbsd ]; then
        NET_DEV=e1000
        NETDEV="user,id=user.0,hostfwd=tcp::$PORT-:22"
        CPU_FLAGS=host,-pdpe1gb
        OVMF=${OVMF:-/usr/share/OVMF/OVMF_CODE_4M.fd}
        if [ ! -r "$OVMF" ]; then
            say "need UEFI firmware; set OVMF=/path/to/OVMF_CODE.fd"
            exit 1
        fi
        FIRMWARE="-drive if=pflash,format=raw,unit=0,file=$OVMF,readonly=on"
    else
        NET_DEV=virtio-net-pci
        NETDEV="user,id=user.0,ipv6=off,hostfwd=tcp::$PORT-:22"
        CPU_FLAGS=host
        FIRMWARE=
    fi
    ;;
esac

say "booting vm (ssh port $PORT)"
# shellcheck disable=SC2086
qemu-system-x86_64 \
    -machine type=q35,accel=kvm -cpu "$CPU_FLAGS" \
    -smp "${VM_CPUS:-2}" -m "${VM_RAM:-2048}" \
    -device "$NET_DEV,netdev=user.0,addr=0x03" -netdev "$NETDEV" \
    -display none -monitor none -serial "file:$WORK/serial.log" \
    -boot strict=off \
    $FIRMWARE $DISK_FLAGS >"$WORK/qemu.log" 2>&1 &
QEMU_PID=$!

SSHOPT="-i $WORK/key -p $PORT -o StrictHostKeyChecking=no
        -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
        -o ConnectTimeout=5 -o ServerAliveInterval=30"

# Wait for sshd.  OpenBSD and NetBSD boot a good deal slower than FreeBSD,
# and slower again when several VMs are competing for the same disk, so this
# waits generously.  NetBSD keeps its console on vga rather than serial, so
# an empty serial.log there is normal and not a sign of trouble.
say "waiting for ssh"
i=0
until ssh $SSHOPT runner@127.0.0.1 true 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt "${VM_BOOT_TRIES:-200}" ]; then
        say "vm did not come up after $((i * 3))s"
        [ -s "$WORK/serial.log" ] && { say "last serial output:"; tail -25 "$WORK/serial.log"; }
        exit 1
    fi
    kill -0 "$QEMU_PID" 2>/dev/null || { say "qemu died"; cat "$WORK/qemu.log"; exit 1; }
    sleep 3
done
say "up after $((i * 3))s"

# --- build ------------------------------------------------------------------
say "syncing work tree"
rsync -az --delete -e "ssh $SSHOPT" \
      --exclude '.git' --exclude 'build*' --exclude '.ci-local' \
      --exclude 'android/obj' --exclude 'android/libs' \
      "$REPO/" runner@127.0.0.1:wildmidi/ || exit 1

if [ "$NEED_PACKAGES" = 1 ]; then
    say "installing cmake (first run for $OS; caching the result)"
    ssh $SSHOPT runner@127.0.0.1 "$SETUP_CMD" >>"$WORK/build.log" 2>&1
    ssh $SSHOPT runner@127.0.0.1 "$INSTALL_CMD" >>"$WORK/build.log" 2>&1 || {
        say "could not install cmake"; tail -20 "$WORK/build.log"; exit 1
    }
    SAVE_PREPARED=1
else
    say "cmake already in the cached image"
    SAVE_PREPARED=0
fi

say "building"
ssh $SSHOPT runner@127.0.0.1 "
    export PATH=/usr/pkg/sbin:/usr/pkg/bin:/sbin:\$PATH
    cd wildmidi &&
    cmake -B build -DCMAKE_INSTALL_PREFIX=build/out \
        -DCMAKE_BUILD_TYPE=Release \
        -DWANT_PLAYER=ON -DWANT_STATIC=ON $CMAKE_OPTS &&
    cmake --build build --config Release &&
    ctest --test-dir build --output-on-failure &&
    cmake --install build
" 2>&1 || { say "build failed"; exit 1; }

# Keep the guest as it is now, minus the work tree, so the next run starts
# with cmake already installed.  Only done once the build has succeeded: an
# image built from a broken run is worse than no cache at all.
if [ "$SAVE_PREPARED" = 1 ]; then
    say "saving prepared image for future runs"
    ssh $SSHOPT runner@127.0.0.1 "rm -rf wildmidi" >/dev/null 2>&1

    # Shut down cleanly so the guest filesystem is consistent, then let qemu
    # release the disk before touching it.
    ssh $SSHOPT runner@127.0.0.1 "sudo shutdown -p now || sudo shutdown -h now" \
        >/dev/null 2>&1
    i=0
    while kill -0 "$QEMU_PID" 2>/dev/null && [ "$i" -lt 60 ]; do
        i=$((i + 1)); sleep 2
    done
    kill -0 "$QEMU_PID" 2>/dev/null && { kill "$QEMU_PID" 2>/dev/null; sleep 3; }
    QEMU_PID=

    # Flatten the overlay so the cached image does not depend on $WORK.  -c
    # matters: without it the result is written out uncompressed and ends up
    # several times the size of the image it came from.
    if qemu-img convert -q -c -O qcow2 "$WORK/disk.qcow2" "$PREPARED.part" 2>/dev/null; then
        mv "$PREPARED.part" "$PREPARED"
        say "cached $(basename "$PREPARED")"
    else
        rm -f "$PREPARED.part"
        say "could not save prepared image; runs stay slow but correct"
    fi
fi

say "ok"
exit 0
