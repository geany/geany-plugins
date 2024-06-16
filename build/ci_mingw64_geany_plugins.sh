#!/bin/bash
#
# Copyright 2022 The Geany contributors
# License: GPLv2
#
# Helper script to build Geany-Plugins for Windows in a Docker container.
# The following steps are performed:
# - clone Geany-Plugins repository if necessary (i.e. if it is not bind-mounted into the container)
# - cross-compile Geany-Plugins for Windows 64bit and GTK3
# - sign all binaries and installer (if /certificates exist and contains cert.pem and key.pem)
# - create bundle with all dependencies
# - create the NSIS installer in ${OUTPUT_DIRECTORY}
# - test the created NSIS installer
# - test uninstaller and check there is nothing unexpected left after uninstalling
#
# This script has to be executed within the Docker container
# (https://github.com/geany/infrastructure/blob/master/builders/Dockerfile.mingw64).
# The Docker container should have a bind-mount for ${OUTPUT_DIRECTORY}
# where the resulting installer binary is stored.
#
# To test the installer "wine" is used.
# Please note that we need to use wine32 as the created installer and uninstaller
# binaries are 32bit.
#

GEANY_PLUGINS_VERSION=  # will be set below from configure.ac
GEANY_PLUGINS_GIT_REVISION=  # will be set below
OUTPUT_DIRECTORY="/output"
GEANY_PLUGINS_GIT_REPOSITORY="https://github.com/geany/geany-plugins.git"

# rather static values, unlikely to be changed
DEPENDENCY_BUNDLE_DIR="/build/dependencies-bundle"
GEANY_PLUGINS_SOURCE_DIR="/geany-plugins-source"
GEANY_PLUGINS_BUILD_DIR="/build/geany-plugins-build"
GEANY_PLUGINS_RELEASE_DIR="/build/geany-plugins-release"
GEANY_PLUGINS_INSTALLER_FILENAME=  # will be set below
GEANY_PLUGINS_INSTALLATION_DIR_WIN="C:\\geany_plugins_install"
GEANY_PLUGINS_INSTALLATION_DIR=$(winepath --unix ${GEANY_PLUGINS_INSTALLATION_DIR_WIN})
GEANY_PLUGINS_RELEASE_BINARY_PATTERNS=(
	"${GEANY_PLUGINS_RELEASE_DIR}/lib/geany/*.dll"
	"${GEANY_PLUGINS_RELEASE_DIR}/lib/geany-plugins/geanylua/libgeanylua.dll"
	"${GEANY_PLUGINS_RELEASE_DIR}/bin/libgeanypluginutils*.dll"
)

GEANY_INSTALLATION_DIR="/root/.wine/drive_c/geany_install"
GEANY_INSTALLATION_DIR_WIN=$(winepath --windows ${GEANY_INSTALLATION_DIR})
GEANY_INSTALLER_EXECUTABLE=$(ls ${OUTPUT_DIRECTORY}/geany-*_setup.exe)

# CI CFLAGS
CFLAGS="\
	-Wall \
	-Wextra \
	-O2 \
	-Wunused \
	-Wno-unused-parameter \
	-Wunreachable-code \
	-Wformat=2 \
	-Wundef \
	-Wpointer-arith \
	-Wwrite-strings \
	-Waggregate-return \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wmissing-noreturn \
	-Wmissing-format-attribute \
	-Wredundant-decls \
	-Wnested-externs \
	-Wno-deprecated-declarations"

# cross-compilation environment
ARCH="x86_64"
MINGW_ARCH="mingw64"
HOST="x86_64-w64-mingw32"
export LDFLAGS="-static-libgcc ${LDFLAGS}"
export PKG_CONFIG_SYSROOT_DIR="/windows"
export PKG_CONFIG_PATH="/windows/${MINGW_ARCH}/lib/pkgconfig/"
export NOCONFIGURE=1
export JOBS=${JOBS:-1}
export lt_cv_deplibs_check_method='pass_all'

# stop on errors
set -e


log() {
	echo "=========== $(date '+%Y-%m-%d %H:%M:%S %Z') $* ==========="
}


git_clone_geany_plugins_if_necessary() {
	if [ -d ${GEANY_PLUGINS_SOURCE_DIR} ]; then
		log "Copying Geany-Plugins source"
		cp --archive ${GEANY_PLUGINS_SOURCE_DIR}/ ${GEANY_PLUGINS_BUILD_DIR}/
		chown --recursive $(id -u):$(id -g) ${GEANY_PLUGINS_BUILD_DIR}/
	else
		log "Cloning Geany-Plugins repository from ${GEANY_PLUGINS_GIT_REPOSITORY}"
		git clone --depth 1 ${GEANY_PLUGINS_GIT_REPOSITORY} ${GEANY_PLUGINS_BUILD_DIR}
	fi
}


parse_geany_plugins_version() {
	GEANY_PLUGINS_VERSION=$(sed -n -E -e 's/^AC_INIT.\[geany-plugins\], \[(.+)\]./\1/p' ${GEANY_PLUGINS_BUILD_DIR}/configure.ac)
	GEANY_PLUGINS_GIT_REVISION=$(cd ${GEANY_PLUGINS_BUILD_DIR} && git rev-parse --short --revs-only HEAD 2>/dev/null || true)
	TIMESTAMP=$(date +%Y%m%d%H%M%S)
	# add pull request number if this is a CI and a PR build
	if [ "${GITHUB_PULL_REQUEST}" ]; then
		GEANY_PLUGINS_VERSION="${GEANY_PLUGINS_VERSION}_ci_${TIMESTAMP}_${GEANY_PLUGINS_GIT_REVISION}_pr${GITHUB_PULL_REQUEST}"
	elif [ "${CI}" -a "${GEANY_PLUGINS_GIT_REVISION}" ]; then
		GEANY_PLUGINS_VERSION="${GEANY_PLUGINS_VERSION}_ci_${TIMESTAMP}_${GEANY_PLUGINS_GIT_REVISION}"
	elif [ "${GEANY_PLUGINS_GIT_REVISION}" ]; then
		GEANY_PLUGINS_VERSION="${GEANY_PLUGINS_VERSION}_git_${TIMESTAMP}_${GEANY_PLUGINS_GIT_REVISION}"
	fi
	GEANY_PLUGINS_INSTALLER_FILENAME="geany-plugins-${GEANY_PLUGINS_VERSION}_setup.exe"
}


log_environment() {
	log "Using environment"
	CONFIGURE_OPTIONS="--disable-silent-rules --host=${HOST} --prefix=${GEANY_PLUGINS_RELEASE_DIR} --with-geany-libdir=${GEANY_PLUGINS_RELEASE_DIR}/lib"
	echo "Geany-Plugins version        : ${GEANY_PLUGINS_VERSION}"
	echo "Geany-Plugins GIT revision   : ${GEANY_PLUGINS_GIT_REVISION}"
	echo "Geany installer              : ${GEANY_INSTALLER_EXECUTABLE}"
	echo "PATH                         : ${PATH}"
	echo "HOST                         : ${HOST}"
	echo "GCC                          : $(${HOST}-gcc -dumpfullversion) ($(${HOST}-gcc -dumpversion))"
	echo "G++                          : $(${HOST}-g++ -dumpfullversion) ($(${HOST}-g++ -dumpversion))"
	echo "Libstdc++                    : $(dpkg-query --showformat='${Version}' --show libstdc++6:i386)"
	echo "GLib                         : $(pkg-config --modversion glib-2.0)"
	echo "GTK                          : $(pkg-config --modversion gtk+-3.0)"
	echo "CFLAGS                       : ${CFLAGS}"
	echo "Configure                    : ${CONFIGURE_OPTIONS}"
}


patch_version_information() {
	log "Patching version information"

	if [ -z "${GEANY_PLUGINS_GIT_REVISION}" ]; then
		return
	fi

	# parse version string and decrement the patch and/or minor levels to keep nightly build
	# versions below the next release version
	regex='^([0-9]*)[.]([0-9]*)([.]([0-9]*))?'
	if [[ ${GEANY_PLUGINS_VERSION} =~ $regex ]]; then
		MAJOR="${BASH_REMATCH[1]}"
		MINOR="${BASH_REMATCH[2]}"
		PATCH="${BASH_REMATCH[4]}"
		if [ -z "${PATCH}" ] || [ "${PATCH}" = "0" ]; then
			if [ "${MINOR}" = "0" ]; then
				MAJOR="$((MAJOR-1))"
				MINOR="99"
			else
				MINOR="$((MINOR-1))"
			fi
			PATCH="99"
		else
			PATCH="$((PATCH-1))"
		fi
	else
		echo "Could not extract or parse version tag" >&2
		exit 1
	fi
	# replace version information in configure.ac and for Windows binaries
	sed -i -E "s/^AC_INIT.\[geany-plugins\], \[(.+)\],/AC_INIT(\[geany-plugins\], \[${GEANY_PLUGINS_VERSION}\],/" ${GEANY_PLUGINS_BUILD_DIR}/configure.ac
	sed -i -E "s/^!define PRODUCT_VERSION \"(.+)\"/!define PRODUCT_VERSION \"${GEANY_PLUGINS_VERSION}\"/" ${GEANY_PLUGINS_BUILD_DIR}/build/geany-plugins.nsi
	sed -i -E "s/^!define PRODUCT_VERSION_ID \"(.+)\"/!define PRODUCT_VERSION_ID \"${MAJOR}.${MINOR}.${PATCH}.90\"/" ${GEANY_PLUGINS_BUILD_DIR}/build/geany-plugins.nsi
	sed -i -E "s/^!define REQUIRED_GEANY_VERSION \"(.+)\"/!define REQUIRED_GEANY_VERSION \"${MAJOR}.${MINOR}\"/" ${GEANY_PLUGINS_BUILD_DIR}/build/geany-plugins.nsi
}


install_dependencies() {
	log "Installing build dependencies"
	pacman --noconfirm -Syu
	pacman --noconfirm -S \
		mingw-w64-${ARCH}-check \
		mingw-w64-${ARCH}-cppcheck \
		mingw-w64-${ARCH}-ctpl-git \
		mingw-w64-${ARCH}-enchant \
		mingw-w64-${ARCH}-gpgme \
		mingw-w64-${ARCH}-gtkspell3 \
		mingw-w64-${ARCH}-libgit2 \
		mingw-w64-${ARCH}-libsoup3 \
		mingw-w64-${ARCH}-lua51
}


install_geany() {
	log "Installing Geany (using wine)"
	if [ -z "${GEANY_INSTALLER_EXECUTABLE}" -o ! -f "${GEANY_INSTALLER_EXECUTABLE}" ]; then
		echo "No Geany installer found"
		exit 1
	fi
	mingw-w64-i686-wine ${GEANY_INSTALLER_EXECUTABLE} /S /D=${GEANY_INSTALLATION_DIR_WIN}

	# TODO the following steps are way too hacky: installing Geany from the installer is basically
	# what we want for CI tests but the installed "geany.pc" file isn't really suitable
	# for cross-compiling

	# add the Geany installation directory to the PKG_CONFIG_PATH
	export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${GEANY_INSTALLATION_DIR}/lib/pkgconfig/"
	# patch Geany pkg-config prefix: pkg-config always prepend "/windows" to the "prefix" variable, so we
	# need to add ../ to get out of the hardcoded "/windows" path element
	sed -i "s%^\(prefix=\).*$%\1/..${GEANY_INSTALLATION_DIR}%" ${GEANY_INSTALLATION_DIR}/lib/pkgconfig/geany.pc
	sed -i "s%^\(exec_prefix=\).*$%\1/..${GEANY_INSTALLATION_DIR}%" ${GEANY_INSTALLATION_DIR}/lib/pkgconfig/geany.pc
	# we need "${exec_prefix}/bin" additionally for libgeany-0.dll which is installed into bin/ and replace -lgeany by -lgeany-0
	sed -i "s%^Libs: -L\${libdir} -lgeany$%Libs: -L\${libdir} -L\${exec_prefix}/bin -lgeany-0%" ${GEANY_INSTALLATION_DIR}/lib/pkgconfig/geany.pc
}


build_geany_plugins() {
	cd ${GEANY_PLUGINS_BUILD_DIR}
	log "Running autogen.sh"
	./autogen.sh
	log "Running configure"
	./configure ${CONFIGURE_OPTIONS}
	log "Running make"
	make -j ${JOBS}
	log "Running install-strip"
	make install-strip
}


sign_file() {
	echo "Sign file $1"
	if [ -f /certificates/cert.pem ] && [ -f /certificates/key.pem ]; then
		osslsigncode sign \
			-certs /certificates/cert.pem \
			-key /certificates/key.pem \
			-n "Geany-Plugins Binary" \
			-i "https://www.geany.org/" \
			-ts http://zeitstempel.dfn.de/ \
			-h sha512 \
			-in ${1} \
			-out ${1}-signed
		mv ${1}-signed ${1}
	else
		echo "Skip signing due to missing certificate"
	fi
}


sign_geany_plugins_binaries() {
	log "Signing Geany-Plugins binary files"
	for binary_file_pattern in ${GEANY_PLUGINS_RELEASE_BINARY_PATTERNS[@]}; do
		for binary_file in $(ls ${binary_file_pattern}); do
			sign_file ${binary_file}
		done
	done
}


create_dependencies_bundle() {
	log "Creating Geany-Plugins dependencies bundle"
	mkdir ${DEPENDENCY_BUNDLE_DIR}
	cd ${DEPENDENCY_BUNDLE_DIR}
	bash ${GEANY_PLUGINS_BUILD_DIR}/build/gtk-bundle-from-msys2.sh -x -3
}


create_installer() {
	log "Creating NSIS installer"
	makensis \
		-V3 \
		-WX \
		-DGEANY_PLUGINS_INSTALLER_NAME="${GEANY_PLUGINS_INSTALLER_FILENAME}" \
		-DGEANY_PLUGINS_RELEASE_DIR=${GEANY_PLUGINS_RELEASE_DIR} \
		-DDEPENDENCY_BUNDLE_DIR=${DEPENDENCY_BUNDLE_DIR} \
		${GEANY_PLUGINS_BUILD_DIR}/build/geany-plugins.nsi
}


sign_installer() {
	log "Signing NSIS installer"
	sign_file ${GEANY_PLUGINS_BUILD_DIR}/build/${GEANY_PLUGINS_INSTALLER_FILENAME}
}


test_installer() {
	log "Test NSIS installer"
	# perform a silent install and check for installed files
	exiftool -FileName -FileType -FileVersion -FileVersionNumber ${GEANY_PLUGINS_BUILD_DIR}/build/${GEANY_PLUGINS_INSTALLER_FILENAME}
	# install Geany-Plugins
	mingw-w64-i686-wine ${GEANY_PLUGINS_BUILD_DIR}/build/${GEANY_PLUGINS_INSTALLER_FILENAME} /S /D=${GEANY_PLUGINS_INSTALLATION_DIR_WIN}
	# check if we have something installed
	ls -l ${GEANY_PLUGINS_INSTALLATION_DIR}/uninst-plugins.exe || exit 1
	ls -l ${GEANY_PLUGINS_INSTALLATION_DIR}/lib/geany/addons.dll || exit 1
}


test_uninstaller() {
	log "Test NSIS uninstaller"
	# uninstall Geany-Plugins and test if everything is clean
	mingw-w64-i686-wine ${GEANY_PLUGINS_INSTALLATION_DIR}/uninst-plugins.exe /S
	sleep 15  # it seems the uninstaller returns earlier than the files are actually removed, so wait a moment
	rest=$(find ${GEANY_PLUGINS_INSTALLATION_DIR} \( -path '*share/locale' -o -path '*share/licenses' \) -prune -false -o -true -printf '%P ')
	if [ "${rest}" != " share share/locale share/licenses " ]; then
		# do not treat this as error, it might happen if upstream dependencies have been updated,
		# the list of files to be removed needs to be updated only before releases
		echo "WARNING: Uninstaller failed to uninstall the following files files: ${rest}"
	fi
}


copy_installer_and_bundle() {
	log "Copying NSIS installer and dependencies bundle"
	cp ${GEANY_PLUGINS_BUILD_DIR}/build/${GEANY_PLUGINS_INSTALLER_FILENAME} ${OUTPUT_DIRECTORY}
	cd ${DEPENDENCY_BUNDLE_DIR}
	zip --quiet --recurse-paths ${OUTPUT_DIRECTORY}/geany_plugins_dependencies_bundle_$(date '+%Y%m%dT%H%M%S').zip *
}


main() {
	git_clone_geany_plugins_if_necessary

	parse_geany_plugins_version
	log_environment
	patch_version_information
	install_dependencies
	install_geany
	build_geany_plugins
	sign_geany_plugins_binaries

	create_dependencies_bundle
	create_installer
	sign_installer
	copy_installer_and_bundle

	test_installer
	test_uninstaller

	log "Done."
}


main
