/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001      Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2004,2008 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_QUARTZ
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "ige-conf.h"
#include "dh-util.h"

static GList *views;

static GtkBuilder *
get_builder_file (const gchar *filename,
                  const gchar *root,
                  const gchar *domain,
                  const gchar *first_required_widget,
                  va_list args)
{
        GtkBuilder  *builder;
        const char  *name;
        GObject    **object_ptr;

        builder = gtk_builder_new ();
        if (!gtk_builder_add_from_file (builder, filename, NULL)) {
                g_warning ("Couldn't find necessary UI file '%s'", filename);
                g_object_unref (builder);
                return NULL;
        }

        for (name = first_required_widget; name; name = va_arg (args, char *)) {
                object_ptr = va_arg (args, void *);
                *object_ptr = gtk_builder_get_object (builder, name);

                if (!*object_ptr) {
                        g_warning ("UI file '%s' is missing widget '%s'.",
                                   filename, name);
                        continue;
                }
        }

        return builder;
}

GtkBuilder *
dh_util_builder_get_file (const gchar *filename,
                          const gchar *root,
                          const gchar *domain,
                          const gchar *first_required_widget,
                          ...)
{
        va_list     args;
        GtkBuilder *builder;

        va_start (args, first_required_widget);
        builder = get_builder_file (filename,
                                    root,
                                    domain,
                                    first_required_widget,
                                    args);
        va_end (args);

        return builder;
}

void
dh_util_builder_connect (GtkBuilder *builder,
                         gpointer    user_data,
                         gchar     *first_widget,
                         ...)
{
        va_list      args;
        const gchar *name;
        const gchar *signal;
        GObject     *object;
        gpointer    *callback;

        va_start (args, first_widget);

        for (name = first_widget; name; name = va_arg (args, char *)) {
                signal = va_arg (args, void *);
                callback = va_arg (args, void *);

                object = gtk_builder_get_object (builder, name);
                if (!object) {
                        g_warning ("UI file is missing widget '%s', aborting",
                                   name);
                        continue;
                }

                g_signal_connect (object,
                                  signal,
                                  G_CALLBACK (callback),
                                  user_data);
        }

        va_end (args);
}

#ifdef GDK_WINDOWING_QUARTZ
static gchar *
cf_string_to_utf8 (CFStringRef str)
{
  CFIndex  len;
  gchar   *ret;

  len = CFStringGetMaximumSizeForEncoding (CFStringGetLength (str),
                                           kCFStringEncodingUTF8) + 1;

  ret = g_malloc (len);
  ret[len] = '\0';

  if (CFStringGetCString (str, ret, len, kCFStringEncodingUTF8))
    return ret;

  g_free (ret);
  return NULL;
}

static gchar *
util_get_mac_data_dir (void)
{
        const gchar *env;
        CFBundleRef  cf_bundle;
        UInt32       type;
        UInt32       creator;
        CFURLRef     cf_url;
        CFStringRef  cf_string;
        gchar       *ret, *tmp;

        /* The environment variable overrides all. */
        env = g_getenv ("DEVHELP_DATADIR");
        if (env) {
                return g_strdup (env);
        }

        cf_bundle = CFBundleGetMainBundle ();
        if (!cf_bundle) {
                return NULL;
        }

        /* Only point into the bundle if it's an application. */
        CFBundleGetPackageInfo (cf_bundle, &type, &creator);
        if (type != 'APPL') {
                return NULL;
        }

        cf_url = CFBundleCopyBundleURL (cf_bundle);
        cf_string = CFURLCopyFileSystemPath (cf_url, kCFURLPOSIXPathStyle);
        ret = cf_string_to_utf8 (cf_string);
        CFRelease (cf_string);
        CFRelease (cf_url);

        tmp = g_build_filename (ret, "Contents", "Resources", NULL);
        g_free (ret);

        return tmp;
}
#endif

gchar *
dh_util_build_data_filename (const gchar *first_part,
                             ...)
{
        gchar        *datadir = NULL;
        va_list       args;
        const gchar  *part;
        gchar       **strv;
        gint          i;
        gchar        *ret;

        va_start (args, first_part);

#ifdef GDK_WINDOWING_QUARTZ
        datadir = util_get_mac_data_dir ();
#endif

        if (datadir == NULL) {
                datadir = g_strdup (DATADIR);
        }

        /* 2 = 1 initial component + terminating NULL element. */
        strv = g_malloc (sizeof (gchar *) * 2);
        strv[0] = (gchar *) datadir;

        i = 1;
        for (part = first_part; part; part = va_arg (args, char *), i++) {
                /* +2 = 1 new element + terminating NULL element. */
                strv = g_realloc (strv, sizeof (gchar*) * (i + 2));
                strv[i] = (gchar *) part;
        }

        strv[i] = NULL;
        ret = g_build_filenamev (strv);
        g_free (strv);

        g_free (datadir);

        va_end (args);

        return ret;
}

typedef struct {
        gchar *name;
        guint  timeout_id;
} DhUtilStateItem;

static void
util_state_item_free (DhUtilStateItem *item)
{
        g_free (item->name);
        if (item->timeout_id) {
                g_source_remove (item->timeout_id);
        }
        g_slice_free (DhUtilStateItem, item);
}

static void
util_state_setup_widget (GtkWidget   *widget,
                         const gchar *name)
{
        DhUtilStateItem *item;

        item = g_slice_new0 (DhUtilStateItem);
        item->name = g_strdup (name);

        g_object_set_data_full (G_OBJECT (widget),
                                "dh-util-state",
                                item,
                                (GDestroyNotify) util_state_item_free);
}

static gchar *
util_state_get_key (const gchar *name,
                    const gchar *key)
{
        return g_strdup_printf ("/apps/devhelp/state/%s/%s", name, key);
}

static void
util_state_schedule_save (GtkWidget   *widget,
                          GSourceFunc  func)

{
        DhUtilStateItem *item;

        item = g_object_get_data (G_OBJECT (widget), "dh-util-state");
        if (item->timeout_id) {
		g_source_remove (item->timeout_id);
	}

	item->timeout_id = g_timeout_add (500,
                                          func,
                                          widget);
}

static void
util_state_save_window (GtkWindow   *window,
                        const gchar *name)
{
        gchar          *key;
        GdkWindowState  state;
        gboolean        maximized;
        gint            width, height;
        gint            x, y;

#if GTK_CHECK_VERSION (2,14,0)
        state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));
#else
        state = gdk_window_get_state (GTK_WIDGET (window)->window);
#endif
        if (state & GDK_WINDOW_STATE_MAXIMIZED) {
                maximized = TRUE;
        } else {
                maximized = FALSE;
        }

        key = util_state_get_key (name, "maximized");
        ige_conf_set_bool (ige_conf_get (), key, maximized);
        g_free (key);

        /* If maximized don't save the size and position. */
        if (maximized) {
                return;
        }

        gtk_window_get_size (GTK_WINDOW (window), &width, &height);

        key = util_state_get_key (name, "width");
        ige_conf_set_int (ige_conf_get (), key, width);
        g_free (key);

        key = util_state_get_key (name, "height");
        ige_conf_set_int (ige_conf_get (), key, height);
        g_free (key);

        gtk_window_get_position (GTK_WINDOW (window), &x, &y);

        key = util_state_get_key (name, "x_position");
        ige_conf_set_int (ige_conf_get (), key, x);
        g_free (key);

        key = util_state_get_key (name, "y_position");
        ige_conf_set_int (ige_conf_get (), key, y);
        g_free (key);
}

static void
util_state_restore_window (GtkWindow   *window,
                           const gchar *name)
{
        gchar     *key;
        gboolean   maximized;
        gint       width, height;
        gint       x, y;
        GdkScreen *screen;
        gint       max_width, max_height;

        key = util_state_get_key (name, "width");
        ige_conf_get_int (ige_conf_get (), key, &width);
        g_free (key);

        key = util_state_get_key (name, "height");
        ige_conf_get_int (ige_conf_get (), key, &height);
        g_free (key);

        key = util_state_get_key (name, "x_position");
        ige_conf_get_int (ige_conf_get (), key, &x);
        g_free (key);

        key = util_state_get_key (name, "y_position");
        ige_conf_get_int (ige_conf_get (), key, &y);
        g_free (key);

        if (width > 1 && height > 1) {
                screen = gtk_widget_get_screen (GTK_WIDGET (window));
                max_width = gdk_screen_get_width (screen);
                max_height = gdk_screen_get_height (screen);

                width = CLAMP (width, 0, max_width);
                height = CLAMP (height, 0, max_height);

                x = CLAMP (x, 0, max_width - width);
                y = CLAMP (y, 0, max_height - height);

                gtk_window_set_default_size (window, width, height);
        }

        gtk_window_move (window, x, y);

        key = util_state_get_key (name, "maximized");
        ige_conf_get_bool (ige_conf_get (), key, &maximized);
        g_free (key);

        if (maximized) {
                gtk_window_maximize (window);
        }
}

static gboolean
util_state_window_timeout_cb (gpointer window)
{
        DhUtilStateItem *item;

        item = g_object_get_data (window, "dh-util-state");
        if (item) {
                item->timeout_id = 0;
                util_state_save_window (window, item->name);
        }

	return FALSE;
}

static gboolean
util_state_window_configure_event_cb (GtkWidget         *window,
                                      GdkEventConfigure *event,
                                      gpointer           user_data)
{
	util_state_schedule_save (window, util_state_window_timeout_cb);
	return FALSE;
}

static gboolean
util_state_paned_timeout_cb (gpointer paned)
{
        DhUtilStateItem *item;

        item = g_object_get_data (paned, "dh-util-state");
        if (item) {
                gchar *key;

                item->timeout_id = 0;

                key = util_state_get_key (item->name, "position");
                ige_conf_set_int (ige_conf_get (),
                                  key,
                                  gtk_paned_get_position (paned));
                g_free (key);
        }

	return FALSE;
}

static gboolean
util_state_paned_changed_cb (GtkWidget *paned,
                             gpointer   user_data)
{
	util_state_schedule_save (paned, util_state_paned_timeout_cb);
	return FALSE;
}

void
dh_util_state_manage_window (GtkWindow   *window,
                             const gchar *name)
{
        util_state_setup_widget (GTK_WIDGET (window), name);

        g_signal_connect (window, "configure-event",
                          G_CALLBACK (util_state_window_configure_event_cb),
                          NULL);

        util_state_restore_window (window, name);
}

void
dh_util_state_manage_paned (GtkPaned    *paned,
                            const gchar *name)
{
        gchar *key;
        gint   position;

        util_state_setup_widget (GTK_WIDGET (paned), name);

        key = util_state_get_key (name, "position");
        if (ige_conf_get_int (ige_conf_get (), key, &position)) {
                gtk_paned_set_position (paned, position);
        }
        g_free (key);

        g_signal_connect (paned, "notify::position",
                          G_CALLBACK (util_state_paned_changed_cb),
                          NULL);
}

GSList *
dh_util_state_load_books_disabled (void)
{
        gchar *key;
        GSList *books_disabled = NULL;

        key = util_state_get_key ("main/contents", "books_disabled");
        ige_conf_get_string_list (ige_conf_get (), key, &books_disabled);
        g_free(key);

        return books_disabled;
}

void
dh_util_state_store_books_disabled (GSList *books_disabled)
{
        gchar *key;

        key = util_state_get_key ("main/contents", "books_disabled");
        ige_conf_set_string_list (ige_conf_get (), key, books_disabled);
        g_free(key);
}

static gboolean
util_state_notebook_timeout_cb (gpointer notebook)
{
        DhUtilStateItem *item;

        item = g_object_get_data (notebook, "dh-util-state");
        if (item) {
                GtkWidget   *page;
                const gchar *page_name;

                item->timeout_id = 0;

                page = gtk_notebook_get_nth_page (
                        notebook,
                        gtk_notebook_get_current_page (notebook));
                page_name = dh_util_state_get_notebook_page_name (page);
                if (page_name) {
                        gchar *key;

                        key = util_state_get_key (item->name, "selected_tab");
                        ige_conf_set_string (ige_conf_get (), key, page_name);
                        g_free (key);
                }
        }

	return FALSE;
}

static void
util_state_notebook_switch_page_cb (GtkWidget       *notebook,
                                    gpointer         page,
                                    guint            page_num,
                                    gpointer         user_data)
{
	util_state_schedule_save (notebook, util_state_notebook_timeout_cb);
}

void
dh_util_state_set_notebook_page_name (GtkWidget   *page,
                                      const gchar *page_name)
{
        g_object_set_data_full (G_OBJECT (page),
                                "dh-util-state-tab-name",
                                g_strdup (page_name),
                                g_free);
}

const gchar *
dh_util_state_get_notebook_page_name (GtkWidget *page)
{
        return g_object_get_data (G_OBJECT (page),
                                  "dh-util-state-tab-name");
}

void
dh_util_state_manage_notebook (GtkNotebook *notebook,
                               const gchar *name,
                               const gchar *default_tab)
{
        gchar     *key;
        gchar     *tab;
        gint       i;

        util_state_setup_widget (GTK_WIDGET (notebook), name);

        key = util_state_get_key (name, "selected_tab");
        if (!ige_conf_get_string (ige_conf_get (), key, &tab)) {
                tab = g_strdup (default_tab);
        }
        g_free (key);

        for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++) {
                GtkWidget   *page;
                const gchar *page_name;

                page = gtk_notebook_get_nth_page (notebook, i);
                page_name = dh_util_state_get_notebook_page_name (page);
                if (page_name && strcmp (page_name, tab) == 0) {
                        gtk_notebook_set_current_page (notebook, i);
                        gtk_widget_grab_focus (page);
                        break;
                }
        }

        g_free (tab);

        g_signal_connect (notebook, "switch-page",
                          G_CALLBACK (util_state_notebook_switch_page_cb),
                          NULL);
}

static gboolean
split_font_string (const gchar  *name_and_size,
                   gchar       **name,
                   gdouble      *size)
{
	PangoFontDescription *desc;
	PangoFontMask         mask;
	gboolean              retval = FALSE;

	desc = pango_font_description_from_string (name_and_size);
	if (!desc) {
		return FALSE;
	}

	mask = (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_SIZE);
        if ((pango_font_description_get_set_fields (desc) & mask) == mask) {
		*size = PANGO_PIXELS (pango_font_description_get_size (desc));
		*name = g_strdup (pango_font_description_get_family (desc));
		retval = TRUE;
	}

	pango_font_description_free (desc);

	return retval;
}

#define DH_CONF_PATH                  "/apps/devhelp"
#define DH_CONF_USE_SYSTEM_FONTS      DH_CONF_PATH "/ui/use_system_fonts"
#define DH_CONF_VARIABLE_FONT         DH_CONF_PATH "/ui/variable_font"
#define DH_CONF_FIXED_FONT            DH_CONF_PATH "/ui/fixed_font"
#define DH_CONF_SYSTEM_VARIABLE_FONT  "/desktop/gnome/interface/font_name"
#define DH_CONF_SYSTEM_FIXED_FONT     "/desktop/gnome/interface/monospace_font_name"

void
dh_util_font_get_variable (gchar    **name,
                           gdouble   *size,
                           gboolean   use_system_fonts)
{
	IgeConf *conf;
	gchar   *name_and_size;

	conf = ige_conf_get ();

	if (use_system_fonts) {
#ifdef GDK_WINDOWING_QUARTZ
                name_and_size = g_strdup ("Lucida Grande 14");
#else
		ige_conf_get_string (conf,
                                     DH_CONF_SYSTEM_VARIABLE_FONT,
                                     &name_and_size);
#endif
	} else {
		ige_conf_get_string (conf,
                                     DH_CONF_VARIABLE_FONT,
                                     &name_and_size);
	}

        if (!split_font_string (name_and_size, name, size)) {
                *name = g_strdup ("sans");
                *size = 12;
        }

        g_free (name_and_size);
}

void
dh_util_font_get_fixed (gchar    **name,
                        gdouble   *size,
                        gboolean   use_system_fonts)
{
	IgeConf *conf;
	gchar   *name_and_size;

	conf = ige_conf_get ();

	if (use_system_fonts) {
#ifdef GDK_WINDOWING_QUARTZ
                name_and_size = g_strdup ("Monaco 14");
#else
		ige_conf_get_string (conf,
                                     DH_CONF_SYSTEM_FIXED_FONT,
                                     &name_and_size);
#endif
	} else {
		ige_conf_get_string (conf,
                                     DH_CONF_FIXED_FONT,
                                     &name_and_size);
	}

        if (!split_font_string (name_and_size, name, size)) {
                *name = g_strdup ("monospace");
                *size = 12;
        }

        g_free (name_and_size);
}

static void
view_destroy_cb (GtkWidget *view,
                 gpointer   user_data)
{
        views = g_list_remove (views, view);
}

static void
view_setup_fonts (WebKitWebView *view)
{
        IgeConf           *conf;
        WebKitWebSettings *settings;
        gboolean           use_system_fonts;
	gchar             *variable_name;
	gdouble            variable_size;
	gchar             *fixed_name;
	gdouble            fixed_size;

        conf = ige_conf_get ();

        settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (view));

	ige_conf_get_bool (conf,
                           DH_CONF_USE_SYSTEM_FONTS,
                           &use_system_fonts);

        dh_util_font_get_variable (&variable_name, &variable_size,
                                   use_system_fonts);
        dh_util_font_get_fixed (&fixed_name, &fixed_size,
                                   use_system_fonts);

        g_object_set (settings,
                      "monospace-font-family", fixed_name,
                      "default-monospace-font-size", (guint) fixed_size,
                      "sans-serif-font-family", variable_name,
                      "serif-font-family", variable_name,
                      "default-font-size", (guint) variable_size,
                      NULL);

        g_free (variable_name);
        g_free (fixed_name);
}

static void
font_notify_cb (IgeConf     *conf,
                const gchar *path,
                gpointer     user_data)
{
        GList *l;

        for (l = views; l; l = l->next) {
                view_setup_fonts (l->data);
        }
}

void
dh_util_font_add_web_view (WebKitWebView *view)
{
        static gboolean setup;

        if (!setup) {
                IgeConf *conf;

                conf = ige_conf_get ();

		ige_conf_notify_add (conf,
                                     DH_CONF_USE_SYSTEM_FONTS,
                                     font_notify_cb,
                                     NULL);
		ige_conf_notify_add (conf,
                                     DH_CONF_SYSTEM_VARIABLE_FONT,
                                     font_notify_cb,
                                     NULL);
		ige_conf_notify_add (conf,
                                     DH_CONF_SYSTEM_FIXED_FONT,
                                     font_notify_cb,
                                     NULL);
		ige_conf_notify_add (conf,
                                     DH_CONF_VARIABLE_FONT,
                                     font_notify_cb,
                                     NULL);
		ige_conf_notify_add (conf,
                                     DH_CONF_FIXED_FONT,
                                     font_notify_cb,
                                     NULL);

                setup = TRUE;
        }

        views = g_list_prepend (views, view);

        g_signal_connect (view, "destroy",
                          G_CALLBACK (view_destroy_cb),
                          NULL);

        view_setup_fonts (view);
}

gint
dh_util_cmp_book (DhLink *a, DhLink *b)
{
        const gchar *name_a;
        const gchar *name_b;
        gchar       *name_a_casefold;
        gchar       *name_b_casefold;
        int          rc;

        name_a = dh_link_get_name (a);
        if (!name_a) {
                name_a = "";
        }

        name_b = dh_link_get_name (b);
        if (!name_b) {
                name_b = "";
        }

        if (g_ascii_strncasecmp (name_a, "the ", 4) == 0) {
                name_a += 4;
        }
        if (g_ascii_strncasecmp (name_b, "the ", 4) == 0) {
                name_b += 4;
        }

        name_a_casefold = g_utf8_casefold (name_a, -1);
        name_b_casefold = g_utf8_casefold (name_b, -1);

        rc = strcmp (name_a_casefold, name_b_casefold);

        g_free (name_a_casefold);
        g_free (name_b_casefold);

        return rc;
}

