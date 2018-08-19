#!/bin/sh
#
# Fetch and extract Geany-Plugins dependencies for Windows/MSYS2
# This script will download (or use Pacman's cache) to extract
# plugin dependencies as defined below. To be run within a MSYS2
# shell. The extracted files will be placed into the current
# directory.

ABI=i686
use_cache="no"
make_zip="no"
gtkv="3"

# ctags - binary for GeanyCTags plugin
# ctpl-git - for GeanyGenDoc plugin
# enchant, hunspell - for SpellCheck plugin
# curl, glib-networking, gnutls, icu, sqlite3, webkitgtk2/3 for WebHelper and Markdown plugins
# lua51 - for GeanyLua plugin
# gnupg, gpgme - for GeanyPG plugin
# libsoup - for UpdateChecker plugin
# libgit2 - for GitChangeBar plugin
# libxml2 - for PrettyPrinter plugin
# gtkspell - for GeanyVC plugin
# the rest is dependency-dependency
packages="
ctags
ctpl-git
curl
dbus
dbus-glib
enchant
geoclue
giflib
glib-networking
gmp
gnupg
gnutls
gpgme
gstreamer
gst-plugins-base
http-parser
hunspell
icu
libassuan
libgcrypt
libgpg-error
libgit2
libidn2
libjpeg-turbo
libogg
libsoup
libssh2
libsystre
libtasn1
libtheora
libtiff
libtre-git
libunistring
libvorbis
libvorbisidec-svn
libwebp
libxml2
libxslt
lua51
nettle
nghttp2
openssl
orc
p11-kit
readline
rtmpdump-git
sqlite3
termcap
xz
"

gtk2_dependency_pkgs="
gtkspell
webkitgtk2
"
gtk3_dependency_pkgs="
gtkspell3
webkitgtk3
"

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
		"-2")
			gtkv="2"
			;;
		"-3")
			gtkv="3"
			;;
		"-h"|"--help")
			echo "gtk-bundle-from-msys2.sh [-c] [-h] [-z] [-2 | -3] [CACHEDIR]"
			echo "      -c Use pacman cache. Otherwise pacman will download"
			echo "         archive files"
			echo "      -h Show this help screen"
			echo "      -z Create a zip afterwards"
			echo "      -2 Prefer gtk2"
			echo "      -3 Prefer gtk3"
			echo "CACHEDIR Directory where to look for cached packages (default: /var/cache/pacman/pkg)"
			exit 1
			;;
		*)
			cachedir="$opt"
			;;
		esac
	done
}

initialize() {
	if [ -z "$cachedir" ]; then
		cachedir="/var/cache/pacman/pkg"
	fi

	if [ "$use_cache" = "yes" ] && ! [ -d "$cachedir" ]; then
		echo "Cache dir \"$cachedir\" not a directory"
		exit 1
	fi

	gtk="gtk$gtkv"
	eval "gtk_dependency_pkgs=\${${gtk}_dependency_pkgs}"

	pkgs="
${packages}
${gtk_dependency_pkgs}
"
}

_remember_package_source() {
	if [ "$use_cache" = "yes" ]; then
		package_url=`pacman -Sp mingw-w64-${ABI}-${2}`
	else
		package_url="${1}"
	fi
	package_urls="${package_urls}\n${package_url}"
}

_getpkg() {
	if [ "$use_cache" = "yes" ]; then
		package_info=`pacman -Qi mingw-w64-$ABI-$1`
		package_version=`echo "$package_info" | grep "^Version " | cut -d':' -f 2 | tr -d '[[:space:]]'`
		ls $cachedir/mingw-w64-${ABI}-${1}-${package_version}-* | sort -V | tail -n 1
	else
		pacman -Sp mingw-w64-${ABI}-${1}
	fi
}

extract_packages() {
	# hack for libxml2 postinstall script which expects "bin/mkdir"
	mkdir -p bin
	cp /bin/mkdir bin/
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
			curl -L "$pkg" | tar -x --xz
		fi
		if [ -f .INSTALL ]; then
			echo "Running post_install script for \"$i\""
			/bin/bash -c ". .INSTALL; post_install"
		fi
		rm -f .INSTALL .MTREE .PKGINFO .BUILDINFO
	done
}

move_extracted_files() {
	echo "Move extracted data to destination directory"
	if [ -d mingw32 ]; then
		for d in bin etc home include lib libexec locale sbin share ssl var; do
			if [ -d "mingw32/$d" ]; then
				rm -rf $d
				# prevent sporadic 'permission denied' errors on my system, not sure why they happen
				sleep 0.5
				mv mingw32/$d .
			fi
		done
		rmdir mingw32
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
	rm -rf lib/icu
	rm -rf lib/lua
	rm -rf lib/p11-kit
	rm -rf lib/python2.7
	rm -rf lib/python3.7
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
	rm -rf share/common-lisp
	rm -rf share/dbus-1
	rm -rf share/doc
	rm -rf share/emacs
	rm -rf share/GConf
	rm -rf share/geoclue-providers
	rm -rf share/gir-1.0
	rm -rf share/glib-2.0
	rm -rf share/gnupg
	rm -rf share/gst-plugins-base
	rm -rf share/gtk-doc
	rm -rf share/icu
	rm -rf share/info
	rm -rf share/lua
	rm -rf share/man
	rm -rf share/nghttp2
	rm -rf share/readline
	rm -rf share/zsh
	# ssl: cleanup ssl files
	rm -rf ssl
	# bin: cleanup binaries and libs (delete anything except *.dll and binaries we need)
	find bin \
		! -name '*.dll' \
		! -name ctags.exe \
		! -name gpg2.exe \
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
	unix2dos "${filename}"
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
cleanup_unnecessary_files
create_bundle_dependency_info_file
create_zip_archive
