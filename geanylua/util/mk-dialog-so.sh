#!/bin/sh

# This will create a separate dynamic shared library from the dialogs module
# that can be used with the Lua standalone interptreter.

gcc -DDIALOG_LIB -Wall -g -shared \
  `pkg-config --libs --cflags gtk+-2.0 lua5.1` gsdlg_lua.c -o dialog.so
