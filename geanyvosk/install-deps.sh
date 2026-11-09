#!/usr/bin/env bash
# Install build and runtime dependencies for geanyvosk.
# Requires sudo for system-wide installation.

set -euo pipefail

VOSK_VERSION="0.3.45"
VOSK_ARCH="x86_64"
VOSK_TARBALL="vosk-linux-$VOSK_ARCH-$VOSK_VERSION.zip"
VOSK_URL="https://github.com/alphacep/vosk-api/releases/download/v$VOSK_VERSION/$VOSK_TARBALL"
VOSK_DIR="vosk-linux-$VOSK_ARCH-$VOSK_VERSION"

die() { echo "ERROR: $*" >&2; exit 1; }

# ── ALSA dev headers ──────────────────────────────────────────────────────────

echo "==> Installing ALSA development headers..."
sudo apt-get install -y libasound2-dev

# ── Vosk shared library + header ──────────────────────────────────────────────

if [ -f /usr/local/lib/libvosk.so ] && [ -f /usr/local/include/vosk_api.h ]; then
    echo "==> Vosk already installed at /usr/local/lib/libvosk.so"
else
    echo "==> Fetching Vosk $VOSK_VERSION..."
    TMP=$(mktemp -d)
    cleanup() { rm -rf $TMP; }
    trap cleanup EXIT

    if command -v wget >/dev/null 2>&1; then
        wget -q --show-progress -O $TMP/$VOSK_TARBALL $VOSK_URL
    elif command -v curl >/dev/null 2>&1; then
        curl -L --progress-bar -o $TMP/$VOSK_TARBALL $VOSK_URL
    else
        die "Neither wget nor curl found. Install one and re-run."
    fi

    unzip -q $TMP/$VOSK_TARBALL -d $TMP

    [ -f $TMP/$VOSK_DIR/libvosk.so ]  || die "libvosk.so not found in archive"
    [ -f $TMP/$VOSK_DIR/vosk_api.h ]  || die "vosk_api.h not found in archive"

    echo "==> Installing Vosk to /usr/local ..."
    sudo install -m 755 $TMP/$VOSK_DIR/libvosk.so  /usr/local/lib/libvosk.so
    sudo install -m 644 $TMP/$VOSK_DIR/vosk_api.h  /usr/local/include/vosk_api.h
    sudo ldconfig
fi

# ── Vosk speech model ─────────────────────────────────────────────────────────

MODEL_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/geanyvosk/model"
if [ -d $MODEL_DIR ]; then
    echo "==> Vosk model already present at $MODEL_DIR"
else
    echo ""
    echo "==> No Vosk model found at $MODEL_DIR"
    echo "    Download a small English model and install it:"
    echo ""
    echo "    MODEL_URL=https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
    echo "    wget -q --show-progress \$MODEL_URL"
    echo "    unzip vosk-model-small-en-us-0.15.zip"
    echo "    mkdir -p $MODEL_DIR"
    echo "    mv vosk-model-small-en-us-0.15/* $MODEL_DIR"
    echo ""
    echo "    Larger models: https://alphacephei.com/vosk/models"
fi

echo ""
echo "==> Done. Re-run ./configure && make to rebuild geanyvosk with Vosk support."
