#!/bin/bash
#
# requires root
#
cd $(dirname $0)
[ "$EUID" -ne 0 ] && { echo "run as root" >&2; exit 2; }

for i in $(find . -name "*.png")
do 
  cp $i /usr/share/icons/hicolor/$(dirname $i)/
  cp $i /usr/local/share/icons/hicolor/$(dirname $i)/
done

update-icon-caches /usr/share/icons/*
update-icon-caches /usr/local/share/icons/*

