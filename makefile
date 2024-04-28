.SILENT:
.PHONY: all install clean help update-version clear-version configure build
.NOTPARALLEL: all

all: configure build

install:
	cd build_meson && \
	meson install && \
	:

clean:
	rm -rf build_meson && \
	:

help:
	echo "Please provide a target:" ; \
	echo "  [default]   - configure and build_meson" ; \
	echo "   install    - install to system" ; \
	echo "   clean      - delete build_meson dir" ; \
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
	meson setup build_meson --prefix="$${PREFIX:-/usr}" && \
	:

build:
	cd build_meson && \
	ninja && \
	:
