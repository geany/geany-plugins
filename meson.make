.SILENT:
.PHONY: all install clean help update-version clear-version configure build
.NOTPARALLEL: all

all: configure build

install:
	cd meson_build && \
	meson install && \
	:

clean:
	rm -rf meson_build && \
	:

help:
	echo "Please provide a target:" ; \
	echo "  [default]   - configure and meson_build" ; \
	echo "   install    - install to system" ; \
	echo "   clean      - delete meson_build dir" ; \
	:

update-version:
	version=$(shell git describe --tags --abbrev=7 | sed -E 's/^[^0-9]*//;s/-([0-9]*-g.*)$$/.r\1/;s/-/./g') && \
	meson rewrite kwargs set project / version $$version && \
	echo "project version: $$version" && \
	:

clear-version:
	meson rewrite kwargs delete project / version - && \
	:

configure:
	meson setup meson_build --prefix="$${PREFIX:-/usr}" && \
	:

build:
	cd meson_build && \
	ninja && \
	:
