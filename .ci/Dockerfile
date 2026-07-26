# Toolchain image for ci-local.sh -- reproduces the compilers used by the
# GitHub workflows so changes can be validated without pushing.
#
# Covers: native gcc/clang, MinGW-w64 (i686 + x86_64), DJGPP, OS/2 EMX,
#         OpenWatcom (OS/2 + Win32), Zig.
#
# The AmigaOS m68k toolchain is not installed here: upstream CI runs that job
# in the amigadev/crosstools container, and ci-local.sh uses the same image.
#
# Build with:  ./ci-local.sh --docker-build      (or docker build -f .ci/Dockerfile)

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Native toolchains + the bits the cross toolchains need at runtime.
# libfl2 is required by DJGPP, exactly as in cross.yml.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        cmake \
        make \
        git \
        ca-certificates \
        curl \
        xz-utils \
        bzip2 \
        unzip \
        file \
        libfl2 \
        libasound2-dev \
        libopenal-dev \
        gcc-mingw-w64 \
        binutils-mingw-w64 \
    && rm -rf /var/lib/apt/lists/*

# --- DJGPP (DOS) ------------------------------------------------------------
# Same release and checksum as .github/workflows/cross.yml.
ARG DJGPP_URL=https://github.com/andrewwutw/build-djgpp/releases/download/v3.4/djgpp-linux64-gcc1220.tar.bz2
ARG DJGPP_SHA=8464f17017d6ab1b2bb2df4ed82357b5bf692e6e2b7fee37e315638f3d505f00
RUN curl -fsL "$DJGPP_URL" -o /tmp/djgpp.tar.bz2 \
    && echo "$DJGPP_SHA  /tmp/djgpp.tar.bz2" | sha256sum -c - \
    && tar xj -C /opt -f /tmp/djgpp.tar.bz2 \
    && rm /tmp/djgpp.tar.bz2

# --- OS/2 EMX ---------------------------------------------------------------
# What komh/setup-cross-os2emx installs in CI.
ARG OS2EMX_VER=1.4.0
RUN curl -fsL "https://github.com/komh/cross-os2emx/releases/download/${OS2EMX_VER}/cross-os2emx-${OS2EMX_VER}-linux-x64.tar.gz" \
        -o /tmp/os2emx.tar.gz \
    && mkdir -p /opt/cross-os2emx \
    && tar xz -C /opt/cross-os2emx --strip-components=1 -f /tmp/os2emx.tar.gz \
    && rm /tmp/os2emx.tar.gz

# The base toolchain has no OS/2 Toolkit headers, so out_dart.c (MMPM/2 DART
# output) cannot build.  setup-cross-os2emx installs these by default; do the
# same, then point gcc at them exactly as the action's GCCOPT does.
RUN cd /tmp \
    && PATH=/opt/cross-os2emx/opt/os2emx/bin:$PATH \
       installrpmzip pthread os2tk45 \
    && rm -rf /tmp/installrpmzip.zip*

# --- OpenWatcom (OS/2 + Win32) ---------------------------------------------
# The open-watcom/setup-watcom action installs this snapshot.
RUN curl -fsL https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz \
        -o /tmp/ow.tar.xz \
    && mkdir -p /opt/watcom \
    && tar xJ -C /opt/watcom -f /tmp/ow.tar.xz \
    && rm /tmp/ow.tar.xz \
    && chmod -R +x /opt/watcom/binl64 /opt/watcom/binl 2>/dev/null || true

# --- Android NDK ------------------------------------------------------------
# cross.yml uses $ANDROID_NDK_LATEST_HOME, which on the ubuntu-24.04 runner
# image is r29.  Only the linux-x86_64 host toolchain is kept; the prebuilt
# toolchains for darwin/windows hosts are dead weight here and roughly halve
# the unpacked size when removed.
ARG NDK_VER=r29
RUN curl -fsL "https://dl.google.com/android/repository/android-ndk-${NDK_VER}-linux.zip" \
        -o /tmp/ndk.zip \
    && mkdir -p /opt/android \
    && unzip -q /tmp/ndk.zip -d /opt/android \
    && mv "/opt/android/android-ndk-${NDK_VER}" /opt/android/ndk \
    && rm /tmp/ndk.zip \
    && rm -rf /opt/android/ndk/toolchains/llvm/prebuilt/darwin-x86_64 \
              /opt/android/ndk/toolchains/llvm/prebuilt/windows-x86_64 \
              /opt/android/ndk/simpleperf \
              /opt/android/ndk/shader-tools \
              /opt/android/ndk/toolchains/llvm/prebuilt/linux-x86_64/musl \
    && ndkbin=/opt/android/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin \
    && rm -f "$ndkbin"/clangd "$ndkbin"/clang-tidy "$ndkbin"/clang-check \
             "$ndkbin"/clang-scan-deps "$ndkbin"/llvm-bolt "$ndkbin"/*-bolt* \
             "$ndkbin"/clang-format "$ndkbin"/clang-refactor \
    && /opt/android/ndk/ndk-build --version >/dev/null

# --- Zig --------------------------------------------------------------------
# build.zig.zon requires >= 0.16.0, newer than any apt package.
ARG ZIG_VER=0.16.0
RUN set -eux; \
    for base in \
        "https://ziglang.org/download/${ZIG_VER}/zig-x86_64-linux-${ZIG_VER}.tar.xz" \
        "https://ziglang.org/download/${ZIG_VER}/zig-linux-x86_64-${ZIG_VER}.tar.xz" ; do \
        if curl -fsL "$base" -o /tmp/zig.tar.xz; then break; fi; \
    done; \
    mkdir -p /opt/zig; \
    tar xJ -C /opt/zig --strip-components=1 -f /tmp/zig.tar.xz; \
    rm /tmp/zig.tar.xz

ENV WATCOM=/opt/watcom \
    EDPATH=/opt/watcom/eddat \
    WIPFC=/opt/watcom/wipfc \
    DJGPP_DIR=/opt/djgpp \
    GCCOPT="-idirafter /opt/cross-os2emx/opt/os2emx/i686-pc-os2-emx/include/os2tk45" \
    NDK=/opt/android/ndk \
    ANDROID_NDK_LATEST_HOME=/opt/android/ndk

# Order matters: /usr/bin must win for plain "gcc"/"g++", otherwise the DJGPP
# and OS/2 sysroots shadow the native compiler and the native job silently
# cross-compiles.  Each cross toolchain is reached through its prefixed name,
# so only the prefixed binaries need to be on PATH -- and djgpp's own
# unprefixed bin directory is deliberately left off.
ENV PATH=/usr/local/bin:/usr/bin:/bin:/opt/watcom/binl64:/opt/watcom/binl:/opt/djgpp/bin:/opt/cross-os2emx/opt/os2emx/bin:/opt/zig:/usr/local/sbin:/usr/sbin:/sbin

# Building as an arbitrary uid keeps output files owned by the caller.
WORKDIR /src
