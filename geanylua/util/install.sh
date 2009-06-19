#!/bin/sh

# Script to install GeanyLua in the user's local Geany config directory

PLUGINS_DIR="$HOME/.geany/plugins"
DATA_DIR="$PLUGINS_DIR/geanylua"

PLUGIN="geanylua.so"
LIBRARY="libgeanylua.so"

[ -f "$PLUGIN" ] || PLUGIN=".libs/$PLUGIN"
[ -f "$LIBRARY" ] || LIBRARY=".libs/$LIBRARY"

mkdir -p  "$DATA_DIR" || exit

cp "$PLUGIN" "$PLUGINS_DIR" || exit
cp "$LIBRARY" "$DATA_DIR" || exit
cp -a examples/. "$DATA_DIR/examples" || exit
cp -a docs/. "$DATA_DIR/support" || exit

echo "Installation complete."
