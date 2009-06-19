
EXTRA_DIST = COPYRIGHT.LUA keywords.list docs examples util \
             win32-install.vbs win32-uninstall.vbs

noinst_HEADERS = glspi.h glspi_keycmd.h glspi_sci.h glspi_ver.h gsdlg.h gsdlg.c


AUTOMAKE_OPTIONS = gnu


SNAPSHOT_NAME=date +$(PACKAGE)-%Y-%m-%d

AM_CFLAGS = \
 -DGEANY_DISABLE_DEPRECATED \
 -Wall -g \
 @OS_FLAGS@ \
 @GEANY_CFLAGS@ \
 @LUA_CFLAGS@


AM_LDFLAGS = -module -avoid-version @GEANY_LIBS@ @OS_FLAGS@


plugindir = $(PLUGIN_DEST_DIR)
plugin_LTLIBRARIES = geanylua.la
geanylua_la_LDFLAGS = $(AM_LDFLAGS)

geanylua_la_SOURCES = geanylua.c
geanylua_objects = geanylua.o

libgeanyluadir = $(DATA_DEST_DIR)
libgeanylua_LTLIBRARIES = libgeanylua.la
libgeanylua_la_LDFLAGS = $(AM_LDFLAGS) @LUA_LIBS@

libgeanylua_la_SOURCES = \
glspi_init.c \
glspi_app.c \
glspi_dlg.c \
glspi_doc.c \
glspi_kfile.c \
glspi_run.c \
glspi_sci.c \
gsdlg_lua.c

libgeanylua_objects = \
.libs/glspi_init.o \
.libs/glspi_app.o \
.libs/glspi_dlg.o \
.libs/glspi_doc.o \
.libs/glspi_kfile.o \
.libs/glspi_run.o \
.libs/glspi_sci.o \
.libs/gsdlg_lua.o





# Libtool complains about unresolved symbols
# and refuses to build the windows plugin -
# but manual linking seems to work just fine...
.libs/$(LIBRARY).dll: $(libgeanylua_objects)
	$(CC) $^ $(libgeanylua_la_LDFLAGS)  --shared -o $@

.libs/$(PACKAGE).dll: $(geanylua_objects)
	$(CC) $^ $(AM_LDFLAGS)  --shared -o $@


win32: all .libs/$(LIBRARY).dll .libs/$(PACKAGE).dll



.libs/$(PACKAGE).so: $(PACKAGE).la

.libs/$(LIBRARY).so: $(LIBRARY).la



install-library: .libs/$(LIBRARY_FILENAME)
	$(mkdir_p) $(DATA_DEST_DIR)
	$(INSTALL) $< $(DATA_DEST_DIR)


install-plugin: .libs/$(PLUGIN_FILENAME)
	$(mkdir_p) $(PLUGIN_DEST_DIR)
	$(INSTALL) $< $(PLUGIN_DEST_DIR)


install-scripts:
	$(mkdir_p) $(DATA_DEST_DIR)/examples
	DIRS=`find examples -maxdepth 1 -mindepth 1 -type d -not -name .svn`; \
	for DIR in $$DIRS; do \
		$(mkdir_p) $(DATA_DEST_DIR)/$$DIR; \
		cp $$DIR/*.lua $(DATA_DEST_DIR)/$$DIR; \
	done


install-docs:
	$(mkdir_p) $(DOC_DEST_DIR)
	cp ./docs/*.html $(DOC_DEST_DIR)/.


keywords.list: all
	cp .libs/$(LIBRARY_FILENAME) .
	./util/keywords.lua > keywords.list
	rm -f $(LIBRARY_FILENAME)


install: install-plugin install-library install-scripts install-docs


uninstall:
	rm -f $(DOC_DEST_DIR)/$(PACKAGE)-*.html
	rmdir $(DOC_DEST_DIR) || true	
	rm -f $(PLUGIN_DEST_DIR)/$(PLUGIN_FILENAME)
	rm -f $(DATA_DEST_DIR)/$(LIBRARY_FILENAME)
	rm -rf $(DATA_DEST_DIR)/examples
	rmdir $(DATA_DEST_DIR) || true


snapshot:
	./util/real-clean.sh
	ln -sf $${PWD} ../`$(SNAPSHOT_NAME)`
	rm -f ../`$(SNAPSHOT_NAME)`.tar.gz
	find ../`$(SNAPSHOT_NAME)`/ -type f > ../`$(SNAPSHOT_NAME)`.list
	tar --owner=0 --group=0 -zcf ../`$(SNAPSHOT_NAME)`.tar.gz -T ../`$(SNAPSHOT_NAME)`.list
	rm -f ../`$(SNAPSHOT_NAME)`{.list,}




RELEASE_DIR=../$(PACKAGE)-$(VERSION)-release
WIN32_ZIP_DIR=$(PACKAGE)-$(VERSION)-win32
WIN32_DLL_ZIP=$(RELEASE_DIR)/$(WIN32_ZIP_DIR).zip
LINUX_TAR_DIR=$(PACKAGE)-$(VERSION)-linux-x86
LINUX_X86_SO_TAR_GZ=$(RELEASE_DIR)/$(LINUX_TAR_DIR).tar.gz


PKG_NOTES=docs examples AUTHORS COPYING ChangeLog INSTALL THANKS keywords.list

win32-dll-zip:
	w32-config
	w32-make clean win32
	rm -f $(WIN32_DLL_ZIP)
	rm -rf $(WIN32_ZIP_DIR)
	mkdir $(WIN32_ZIP_DIR)
	w32-strip --strip-debug .libs/$(PACKAGE).dll
	w32-strip --strip-debug .libs/$(LIBRARY).dll
	cp .libs/$(PACKAGE).dll .libs/$(LIBRARY).dll   win32-*install.vbs $(WIN32_ZIP_DIR)
	cp -ai $(PKG_NOTES) $(WIN32_ZIP_DIR)
	zip -XKr -9 $(WIN32_DLL_ZIP) $(WIN32_ZIP_DIR)
	rm -rf $(WIN32_ZIP_DIR)
	w32-make clean




linux-x86-so-gz:
	./configure
	$(MAKE) clean all
	rm -f $(LINUX_X86_SO_TAR_GZ)
	rm -rf $(LINUX_TAR_DIR)
	mkdir $(LINUX_TAR_DIR)
	strip --strip-debug .libs/$(PACKAGE).so
	strip --strip-debug .libs/$(LIBRARY).so
	cp .libs/$(PACKAGE).so .libs/$(LIBRARY).so util/install.sh $(LINUX_TAR_DIR)
	cp -ai $(PKG_NOTES) $(LINUX_TAR_DIR)
	tar --owner=0 --group=0  -zvcf $(LINUX_X86_SO_TAR_GZ) $(LINUX_TAR_DIR)
	rm -rf $(LINUX_TAR_DIR)


release-dir:
	mkdir -p $(RELEASE_DIR)
	rm -f $(RELEASE_DIR)/$(distdir).tar.gz $(WIN32_DLL_ZIP) $(LINUX_X86_SO_TAR_GZ)
	rmdir $(RELEASE_DIR)
	mkdir $(RELEASE_DIR)


release: $(PKG_NOTES) release-dir win32-dll-zip linux-x86-so-gz dist
	mv -v $(PACKAGE)-$(VERSION).tar.gz $(RELEASE_DIR)/


pathcheck:
	@echo "Install plugin:         $(PLUGIN_DEST_DIR)"
	@echo "Install support files:  $(DATA_DEST_DIR)"
	@echo "Install documentation:  $(DOC_DEST_DIR)"

