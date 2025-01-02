#!/bin/sh
#
# Fetch and extract Geany-Plugins dependencies for Windows/MSYS2
# This script will download (or use Pacman's cache) to extract
# plugin dependencies as defined below. To be run within a MSYS2
# shell. The extracted files will be placed into the current
# directory.

ABI=x86_64  # do not change, i686 is not supported any longer
use_cache="no"
make_zip="no"
gtkv="3"
run_pi="y"
cross="no"

# Wine commands for 64bit binaries, used only when "-x" is set
EXE_WRAPPER_64="mingw-w64-x86_64-wine"

# ctags - binary for GeanyCTags plugin
# ctpl-git - for GeanyGenDoc plugin
# enchant, hunspell - for SpellCheck plugin
# lua51 - for GeanyLua plugin
# gnupg, gpgme - for GeanyPG plugin
# libsoup3 - for UpdateChecker & GeniusPaste plugins
# libgit2 - for GitChangeBar plugin
# gtkspell3 - for GeanyVC plugin
# the rest is dependency-dependency
packages="
ca-certificates
ctags
ctpl-git
curl
enchant
glib-networking
gnupg
gpgme
gsettings-desktop-schemas
http-parser
hunspell
libassuan
libgcrypt
libgit2
libgpg-error
libidn2
libproxy
libpsl
libsoup3
libssh2
libsystre
libunistring
lua51
nghttp2
openssl
p11-kit
readline
sqlite3
termcap
"

gtk3_dependency_pkgs="
gtkspell3
"
gtk4_dependency_pkgs=""
package_urls=""


handle_command_line_options() {
	for opt in "$@"; do
		case "$opt" in
		"-c"|"--cache")
			use_cache="yes"
			;;
		"-z"|"--zip")
			make_zip="yes"
			;;
		"-3")
			gtkv="3"
			;;
		"-4")
			gtkv="4"
			;;
		"-n")
			run_pi=""
			;;
		"-x")
			cross="yes"
			;;
		"-h"|"--help")
			echo "gtk-bundle-from-msys2.sh [-c] [-h] [-z] [-3 | -4] [-x] [CACHEDIR]"
			echo "      -c Use pacman cache. Otherwise pacman will download"
			echo "         archive files"
			echo "      -h Show this help screen"
			echo "      -n Do not run post install scripts of the packages"
			echo "      -z Create a zip afterwards"
			echo "      -3 Prefer gtk3"
			echo "      -4 Prefer gtk4"
			echo "      -x Set when the script is executed in a cross-compilation context (e.g. to use wine)"
			echo "CACHEDIR Directory where to look for cached packages (default: /var/cache/pacman/pkg)"
			exit 1
			;;
		*)
			cachedir="$opt"
			;;
		esac
	done
}

set -e  # stop on errors
# enable extended globbing as we need it in _getpkg
shopt -s extglob

initialize() {
	if [ -z "$cachedir" ]; then
		cachedir="/var/cache/pacman/pkg"
	fi

	if [ "$use_cache" = "yes" ] && ! [ -d "$cachedir" ]; then
		echo "Cache dir \"$cachedir\" not a directory"
		exit 1
	fi

	if [ "$cross" != "yes" ]; then
		# if running natively, we do not need wine or any other wrappers
		EXE_WRAPPER_64=""
	fi

	gtk="gtk$gtkv"
	eval "gtk_dependency_pkgs=\${${gtk}_dependency_pkgs}"

	pkgs="
${packages}
${gtk_dependency_pkgs}
"
}

_getpkg() {
	if [ "$use_cache" = "yes" ]; then
		package_info=$(pacman -Qi mingw-w64-$ABI-$1)
		package_version=$(echo "$package_info" | grep "^Version " | cut -d':' -f 2 | tr -d '[[:space:]]')
		# use @(gz|xz|zst) to filter out signature files (e.g. mingw-w64-x86_64-...-any.pkg.tar.zst.sig)
		ls $cachedir/mingw-w64-${ABI}-${1}-${package_version}-*.tar.@(gz|xz|zst) | sort -V | tail -n 1
	else
		# -dd to ignore dependencies as we listed them already above in $packages and
		# make pacman ignore its possibly existing cache (otherwise we would get an URL to the cache)
		pacman -Sddp --cachedir /nonexistent mingw-w64-${ABI}-${1}
	fi
}

_remember_package_source() {
	if [ "$use_cache" = "yes" ]; then
		package_url=$(pacman -Sddp mingw-w64-${ABI}-${2})
	else
		package_url="${1}"
	fi
	package_urls="${package_urls}\n${package_url}"
}

extract_packages() {
	# extract packages
	for i in $pkgs; do
		pkg=$(_getpkg $i)
		_remember_package_source $pkg $i
		if [ "$use_cache" = "yes" ]; then
			if [ -e "$pkg" ]; then
				echo "Extracting $pkg from cache"
				tar xaf $pkg
			else
				echo "ERROR: File $pkg not found"
				exit 1
			fi
		else
			echo "Download $pkg using curl"
			filename=$(basename "$pkg")
			curl -s -o "$filename" -L "$pkg"
			tar xf "$filename"
			rm "$filename"
		fi
		if [ "$make_zip" = "yes" -a "$i" = "$gtk" ]; then
			VERSION=$(grep ^pkgver .PKGINFO | sed -e 's,^pkgver = ,,' -e 's,-.*$,,')
		fi
		rm -f .INSTALL .MTREE .PKGINFO .BUILDINFO
	done
}

move_extracted_files() {
	echo "Move extracted data to destination directory"
	if [ -d mingw64 ]; then
		for d in bin etc home include lib libexec locale sbin share ssl var; do
			if [ -d "mingw64/$d" ]; then
				rm -rf $d
				# prevent sporadic 'permission denied' errors on my system, not sure why they happen
				sleep 0.5
				mv mingw64/$d .
			fi
		done
		rmdir mingw64
	fi
}

delayed_post_install() {
	if [ "$run_pi" ]; then
		echo "Execute delayed post install tasks"
		# Commands have been collected manually from the various .INSTALL scripts

		# ca-certificates
		DEST=etc/pki/ca-trust/extracted
		# OpenSSL PEM bundle that includes trust flags
		${EXE_WRAPPER_64} bin/p11-kit.exe extract --format=openssl-bundle --filter=certificates --overwrite $DEST/openssl/ca-bundle.trust.crt
		${EXE_WRAPPER_64} bin/p11-kit.exe extract --format=pem-bundle --filter=ca-anchors --overwrite --purpose server-auth $DEST/pem/tls-ca-bundle.pem
		${EXE_WRAPPER_64} bin/p11-kit.exe extract --format=pem-bundle --filter=ca-anchors --overwrite --purpose email $DEST/pem/email-ca-bundle.pem
		${EXE_WRAPPER_64} bin/p11-kit.exe extract --format=pem-bundle --filter=ca-anchors --overwrite --purpose code-signing $DEST/pem/objsign-ca-bundle.pem
		${EXE_WRAPPER_64} bin/p11-kit.exe extract --format=java-cacerts --filter=ca-anchors --overwrite --purpose server-auth $DEST/java/cacerts
		mkdir -p ssl/certs
		cp -f $DEST/pem/tls-ca-bundle.pem ssl/certs/ca-bundle.crt
		cp -f $DEST/pem/tls-ca-bundle.pem ssl/cert.pem
		cp -f $DEST/openssl/ca-bundle.trust.crt ssl/certs/ca-bundle.trust.crt
	fi
}

cleanup_unnecessary_files() {
	echo "Cleanup unnecessary files"
	# etc: cleanup unnecessary files
	rm -rf etc/bash_completion.d
	rm -rf etc/dbus-1
	rm -rf etc/pkcs11
	rm -rf etc/xml
	# include: cleanup development files
	rm -rf include
	# lib: cleanup development and other unnecessary files
	rm -rf lib/cmake
	rm -rf lib/pkgconfig
	rm -rf lib/girepository-1.0
	rm -rf lib/lua
	rm -rf lib/p11-kit
	rm -rf lib/python3.9
	rm -rf lib/sqlite3.37.*
	find lib -name '*.h' -delete
	find lib -name '*.a' -delete
	find lib -name '*.typelib' -delete
	find lib -name '*.def' -delete
	find lib -name '*.sh' -delete
	find libexec -name '*.exe' -delete
	# enchant: remove libenchant_zemberek.dll which should not packaged at all and is additionally packaged in the wrong location
	# See https://github.com/Alexpux/MINGW-packages/issues/2632
	rm -f lib/bin/libenchant_zemberek.dll
	# enchant: remove aspell engine (it would require the aspell library which we don't need)
	rm -f lib/enchant/libenchant_aspell.dll
	# sbin: cleanup sbin files
	rm -rf sbin
	# share: cleanup other unnecessary files
	rm -rf share/aclocal
	rm -rf share/bash-completion
	rm -rf share/cmake
	rm -rf share/common-lisp
	rm -rf share/dbus-1
	rm -rf share/doc
	rm -rf share/emacs
	rm -rf share/GConf
	rm -rf share/gir-1.0
	rm -rf share/gnupg
	rm -rf share/gtk-doc
	rm -rf share/info
	rm -rf share/lua
	rm -rf share/man
	rm -rf share/nghttp2
	rm -rf share/readline
	rm -rf share/sqlite
	rm -rf share/vala
	rm -rf share/zsh
	# ssl: cleanup ssl files
	rm -rf ssl/*.cnf
	rm -rf ssl/*.cnf.dist
	rm -rf ssl/*.pem
	# bin: cleanup binaries and libs (delete anything except *.dll and binaries we need)
	find bin \
		! -name '*.dll' \
		! -name ctags.exe \
		! -name gpg.exe \
		! -name gpgme-w32spawn.exe \
		! -name gpgme-tool.exe \
		! -name gpgconf.exe \
		-type f \
		-delete
	# cleanup empty directories
	find . -type d -empty -delete
}

create_bundle_dependency_info_file() {
	filename="ReadMe.Dependencies.Geany-Plugins.txt"
	cat << EOF > "${filename}"
This installation contains dependencies for Geany-Plugins which are
distributed as binaries (usually .dll files) as well as additional
files necessary for these dependencies.
Following is a list of all included binary packages with their
full download URL as used to create this installation.
EOF
	echo -e "${package_urls}" >> "${filename}"
}

create_zip_archive() {
	if [ "$make_zip" = "yes" ]; then
		echo "Packing the bundle"
		zip -r plugins-$gtk.zip bin etc include lib locale share var
	fi
}


# main()
handle_command_line_options $@
initialize
extract_packages
move_extracted_files
delayed_post_install
cleanup_unnecessary_files
create_bundle_dependency_info_file
create_zip_archive
