/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#ifdef HAVE_CONFIG_H
# include "config.h" /* for the gettext domain */
#endif

#include "ggd-options.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>


/* This module is strongly inspired from Geany's Stash module with some design
 * changes and a complete reimplementation.
 * The major difference is the way proxies are managed. */


/* stolen from Geany */
#define foreach_array(array, type, item)                         \
  for ((item) = ((type*)(gpointer)(array)->data);                \
       (item) < &((type*)(gpointer)(array)->data)[(array)->len]; \
       (item)++)


/*
 * GgdOptEntry:
 * @type: The setting's type
 * @key: The setting's key (both its name and its key in the underlying
 *       configuration file)
 * @optvar: Pointer to the actual value
 * @value_destroy: Function use to destroy the value, or %NULL
 * @proxy: A #GObject to use as a proxy for the value
 * @proxy_prop: Name of the proxy's property to read and write the value
 * @destroy_hid: Signal handler identifier for the proxy's ::destroy signal, if
 *               @proxy is a #GtkObject.
 * 
 * The structure that represents an option.
 */
struct _GgdOptEntry
{
  GType           type;
  gchar          *key;
  gpointer        optvar;
  GDestroyNotify  value_destroy;
  GObject        *proxy;
  gchar          *proxy_prop;
  gulong          destroy_hid;
};

typedef struct _GgdOptEntry GgdOptEntry;

/* syncs an entry's proxy to the entry's value */
static void
ggd_opt_entry_sync_to_proxy (GgdOptEntry *entry)
{
  if (entry->proxy) {
    /*g_debug ("Syncing proxy for %s", entry->key);*/
    g_object_set (entry->proxy, entry->proxy_prop, *(gpointer *)entry->optvar,
                  NULL);
  }
}

/* syncs an entry's value to the entry's proxy value */
static void
ggd_opt_entry_sync_from_proxy (GgdOptEntry *entry)
{
  if (entry->proxy) {
    if (entry->value_destroy) entry->value_destroy (*(gpointer *)entry->optvar);
    g_object_get (entry->proxy, entry->proxy_prop, entry->optvar, NULL);
  }
}

/*
 * ggd_opt_entry_set_proxy:
 * @entry: A #GgdOptEntry
 * @proxy: The proxy object
 * @prop: The name of the proxy's property to sync with
 * 
 * Sets and syncs the proxy of a #GgdOptEntry.
 */
static void
ggd_opt_entry_set_proxy (GgdOptEntry *entry,
                         GObject     *proxy,
                         const gchar *prop)
{
  if (entry->proxy) {
    if (entry->destroy_hid > 0l) {
      g_signal_handler_disconnect (entry->proxy, entry->destroy_hid);
    }
    g_object_unref (entry->proxy);
  }
  g_free (entry->proxy_prop);
  entry->proxy = (proxy) ? g_object_ref (proxy) : proxy;
  entry->proxy_prop = g_strdup (prop);
  entry->destroy_hid = 0l;
  /* sync the proxy with the setting's current state */
  ggd_opt_entry_sync_to_proxy (entry);
}

/* Frees an entry's allocated data */
static void
ggd_opt_entry_free_data (GgdOptEntry *entry,
                         gboolean     free_opt)
{
  if (entry) {
    ggd_opt_entry_set_proxy (entry, NULL, NULL);
    /* don't free the value to let it in a usable state, and it is consistent
     * since the user allocated it */
    if (free_opt && entry->value_destroy) {
      entry->value_destroy (*(gpointer *)entry->optvar);
      *(gpointer *)entry->optvar = NULL;
    }
    g_free (entry->key);
  }
}


struct _GgdOptGroup
{
  gchar      *name;
  GArray     *prefs;
};

/**
 * ggd_opt_group_new:
 * @section: The name of the section for which this group is for
 * 
 * Creates a new #GgdOptGroup.
 * 
 * Returns: The newly created #GgdOptGroup. Free with ggd_opt_group_free().
 */
GgdOptGroup *
ggd_opt_group_new (const gchar *section)
{
  GgdOptGroup *group;
  
  group = g_slice_alloc (sizeof *group);
  group->name = g_strdup (section);
  group->prefs = g_array_new (FALSE, FALSE, sizeof (GgdOptEntry));
  
  return group;
}

/**
 * ggd_opt_group_free:
 * @group: A #GgdoptGroup
 * @free_opts: Whether to free the allocated options or not
 * 
 * Frees a #GgdOptGroup.
 */
void
ggd_opt_group_free (GgdOptGroup  *group,
                    gboolean      free_opts)
{
  if (group) {
    GgdOptEntry *entry;
    
    foreach_array (group->prefs, GgdOptEntry, entry) {
      ggd_opt_entry_free_data (entry, free_opts);
    }
    g_array_free (group->prefs, TRUE);
    g_free (group->name);
    g_slice_free1 (sizeof *group, group);
  }
}

/* adds an entry to a group */
static GgdOptEntry *
ggd_opt_group_add_entry (GgdOptGroup   *group,
                         GType          type,
                         const gchar   *key,
                         gpointer       optvar,
                         GDestroyNotify value_destroy)
{
  GgdOptEntry entry;
  
  entry.type          = type;
  entry.key           = g_strdup (key);
  entry.optvar        = optvar;
  entry.value_destroy = value_destroy;
  entry.proxy         = NULL;
  entry.proxy_prop    = NULL;
  
  g_array_append_val (group->prefs, entry);
  
  return &g_array_index (group->prefs, GgdOptEntry, group->prefs->len -1);
}

/* looks up for an entry in a group */
static GgdOptEntry *
ggd_opt_group_lookup_entry (GgdOptGroup  *group,
                            gpointer      optvar)
{
  GgdOptEntry *entry;
  
  foreach_array (group->prefs, GgdOptEntry, entry) {
    if (entry->optvar == optvar) {
      return entry;
    }
  }
  
  return NULL;
}

/* looks up an entry in a group from is proxy */
static GgdOptEntry *
ggd_opt_group_lookup_entry_from_proxy (GgdOptGroup *group,
                                       GObject     *proxy)
{
  GgdOptEntry *entry;
  
  foreach_array (group->prefs, GgdOptEntry, entry) {
    if (entry->proxy == proxy) {
      return entry;
    }
  }
  
  return NULL;
}

/**
 * ggd_opt_group_add_boolean:
 * @group: A #GgdOptGroup
 * @optvar: Pointer to setting's variable. It must be already set to a valid
 *          value.
 * @key: The key name for this setting
 * 
 * Adds a boolean setting to a #GgdOptGroup.
 */
void
ggd_opt_group_add_boolean (GgdOptGroup *group,
                           gboolean    *optvar,
                           const gchar *key)
{
  ggd_opt_group_add_entry (group, G_TYPE_BOOLEAN, key, optvar, NULL);
}

/**
 * ggd_opt_group_add_string:
 * @group: A #GgdOptGroup
 * @optvar: Pointer to setting's variable. It must be already set to a valid
 *          value allocated by the GLib's memory manager or to %NULL since it
 *          must can be freed.
 * @key: The key name for this setting
 * 
 * Adds a string setting to a #GgdOptGroup.
 */
void
ggd_opt_group_add_string (GgdOptGroup  *group,
                          gchar       **optvar,
                          const gchar  *key)
{
  if (*optvar == NULL) {
    *optvar = g_strdup ("");
  }
  ggd_opt_group_add_entry (group, G_TYPE_STRING, key, (gpointer)optvar, g_free);
}


/**
 * ggd_opt_group_sync_to_proxies:
 * @group: A #GgdOptGroup
 * 
 * Syncs proxies' values of a #GgdOptGroup to their setting' value.
 */
void
ggd_opt_group_sync_to_proxies (GgdOptGroup *group)
{
  GgdOptEntry *entry;
  
  foreach_array (group->prefs, GgdOptEntry, entry) {
    ggd_opt_entry_sync_to_proxy (entry);
  }
}

/**
 * ggd_opt_group_sync_from_proxies:
 * @group: A #GgdOptGroup
 * 
 * Syncs settings of a #GgdOptGroup to their proxies' values.
 */
void
ggd_opt_group_sync_from_proxies (GgdOptGroup *group)
{
  GgdOptEntry *entry;
  
  foreach_array (group->prefs, GgdOptEntry, entry) {
    ggd_opt_entry_sync_from_proxy (entry);
  }
}

/* set the proxy of a value
 * see the doc of ggd_opt_group_set_proxy_full() that does the same but returns
 * a boolean */
static GgdOptEntry *
ggd_opt_group_set_proxy_full_internal (GgdOptGroup  *group,
                                       gpointer      optvar,
                                       gboolean      check_type,
                                       GType         type_check,
                                       GObject      *proxy,
                                       const gchar  *prop)
{
  GgdOptEntry *entry;
  
  entry = ggd_opt_group_lookup_entry (group, optvar);
  if (! entry) {
    g_warning (_("Unknown option"));
  } else {
    if (check_type) {
      gboolean  success = TRUE;
      GValue    val     = {0};
      
      g_value_init (&val, type_check);
      g_object_get_property (proxy, prop, &val);
      if (! (G_VALUE_HOLDS (&val, type_check) && entry->type == type_check)) {
        g_critical (_("Invalid option or proxy: either the proxy's property or "
                      "the option type is incompatible."));
      }
      g_value_unset (&val);
      if (! success) {
        return FALSE;
      }
    }
    ggd_opt_entry_set_proxy (entry, proxy, prop);
  }
  
  return entry;
}

/**
 * ggd_opt_group_set_proxy_full:
 * @group: A #GgdOptGroup
 * @optvar: The setting's pointer, as passed when adding the setting
 * @check_type: Whether to check the consistence of the setting's type and the
 *              proxy's property
 * @type_check: The type that the setting and the proxy's property should have
 *              (only used if @check_type is %TRUE)
 * @proxy: A #GObject to use as proxy for this setting
 * @prop: The property name of the proxy (must be readable and/or writable,
 *        depending on whether the prosy are sunk from or to settings)
 * 
 * This function sets the proxy object for a given setting.
 * Proxy objects can be used to e.g. display and edit a setting.
 * If you use a #GtkObject derivate as proxy (such as #GtkWidget<!-- -->s),
 * consider using ggd_opt_group_set_proxy_gtkobject_full() in place of this
 * function.
 * 
 * <note><para>
 *   Setting the proxy sets its value to the setting's value, no need to sync it
 *   again with ggd_opt_group_sync_to_proxies().
 * </para></note>
 * 
 * Returns: %TRUE if the proxy was correctly set, or %FALSE if the setting
 *          doesn't exists in @group.
 */
gboolean
ggd_opt_group_set_proxy_full (GgdOptGroup  *group,
                              gpointer      optvar,
                              gboolean      check_type,
                              GType         type_check,
                              GObject      *proxy,
                              const gchar  *prop)
{
  return ggd_opt_group_set_proxy_full_internal (group, optvar,
                                                check_type, type_check,
                                                proxy, prop) != NULL;
}

/**
 * ggd_opt_group_remove_proxy:
 * @group: A #GgdOptGroup
 * @optvar: The setting's pointer, as passed when adding the setting
 * 
 * Removes the proxy of a given setting.
 */
void
ggd_opt_group_remove_proxy (GgdOptGroup *group,
                            gpointer     optvar)
{
  ggd_opt_group_set_proxy_full_internal (group, optvar, FALSE, 0, NULL, NULL);
}

/* detaches a proxy */
static void
ggd_opt_group_remove_proxy_from_proxy (GgdOptGroup *group,
                                       GObject     *proxy)
{
  GgdOptEntry *entry;
  
  entry = ggd_opt_group_lookup_entry_from_proxy (group, proxy);
  if (entry) {
    ggd_opt_entry_set_proxy (entry, NULL, NULL);
  }
}

/**
 * ggd_opt_group_set_proxy_gtkobject_full:
 * @group: A #GgdOptGroup
 * @optvar: The setting's pointer, as passed when adding the setting
 * @check_type: Whether to check the consistence of the setting's type and the
 *              proxy's property
 * @type_check: The type that the setting and the proxy's property should have
 *              (only used if @check_type is %TRUE)
 * @proxy: A #GtkObject to use as proxy for this setting
 * @prop: The property name of the proxy (must be readable and/or writable,
 *        depending on whether the prosy are sunk from or to settings)
 * 
 * This is very similar to ggd_opt_group_set_proxy_full() but adds a signal
 * handler on object's ::destroy signal to release it when destroyed.
 * This should be used when @proxy is a GtkObject derivate, unless you handle
 * the case yourself (e.g. to sync when widgets gets destroyed).
 * 
 * Returns: %TRUE if the proxy was correctly set, or %FALSE if the setting
 *          doesn't exists in @group.
 */
gboolean
ggd_opt_group_set_proxy_gtkobject_full (GgdOptGroup  *group,
                                        gpointer      optvar,
                                        gboolean      check_type,
                                        GType         type_check,
                                        GtkObject    *proxy,
                                        const gchar  *prop)
{
  GgdOptEntry *entry;
  
  entry = ggd_opt_group_set_proxy_full_internal (group, optvar,
                                                 check_type, type_check,
                                                 G_OBJECT (proxy), prop);
  if (entry) {
    entry->destroy_hid = g_signal_connect_swapped (
      proxy, "destroy",
      G_CALLBACK (ggd_opt_group_remove_proxy_from_proxy), group
    );
  }
  
  return entry != NULL;
}

/*
 * ggd_opt_group_manage_key_file:
 * @group: A #GgdOptGroup
 * @load: Whether to read (%TRUE) or write (%FALSE) the group to or from the
 *        given key file
 * @key_file: A #GKeyFile
 * 
 * Reads or writes a #GgdOptGroup from or to a #GKeyFile
 */
static void
ggd_opt_group_manage_key_file (GgdOptGroup  *group,
                               gboolean      load,
                               GKeyFile     *key_file)
{
  GgdOptEntry *entry;
  
  foreach_array (group->prefs, GgdOptEntry, entry) {
    GError *err = NULL;
    
    switch (entry->type) {
      case G_TYPE_BOOLEAN: {
        gboolean *setting = (gboolean *)entry->optvar;
        
        if (load) {
          gboolean v;
          
          v = g_key_file_get_boolean (key_file, group->name, entry->key, &err);
          if (! err) {
            *setting = v;
          }
        } else {
          g_key_file_set_boolean (key_file, group->name, entry->key, *setting);
        }
        break;
      }
      
      case G_TYPE_STRING: {
        gchar **setting = (gchar **)entry->optvar;
        
        if (load) {
          gchar *str;
          
          str = g_key_file_get_string (key_file, group->name, entry->key, &err);
          if (! err) {
            if (entry->value_destroy) entry->value_destroy (*setting);
            *setting = str;
          }
        } else {
          g_key_file_set_string (key_file, group->name, entry->key, *setting);
        }
        break;
      }
      
      default:
        g_warning (_("Unknown value type for keyfile entry %s::%s"),
                   group->name, entry->key);
    }
    if (err) {
      g_warning (_("Error retrieving keyfile entry %s::%s: %s"),
                 group->name, entry->key, err->message);
      g_error_free (err);
    } else {
      if (load) {
        ggd_opt_entry_sync_to_proxy (entry);
      }
    }
  }
}

/**
 * ggd_opt_group_load_from_key_file:
 * @group: A #GgdOptGroup
 * @key_file: A #GKeyFile from where load values
 * 
 * Loads values of a #GgdOptGroup from a #GKeyFile.
 */
void
ggd_opt_group_load_from_key_file (GgdOptGroup  *group,
                                  GKeyFile     *key_file)
{
  ggd_opt_group_manage_key_file (group, TRUE, key_file);
}

/**
 * ggd_opt_group_load_from_file:
 * @group: A #GgdOptGroup
 * @filename: Name of the file from which load values in the GLib file names
 *            encoding
 * @error: return location for or %NULL to ignore them
 * 
 * Loads values of a #GgdOptGroup from a file.
 * 
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean
ggd_opt_group_load_from_file (GgdOptGroup  *group,
                              const gchar  *filename,
                              GError      **error)
{
  gboolean  success = FALSE;
  GKeyFile *key_file;
  
  key_file = g_key_file_new ();
  if (g_key_file_load_from_file (key_file, filename, 0, error)) {
    ggd_opt_group_load_from_key_file (group, key_file);
    success = TRUE;
  }
  g_key_file_free (key_file);
  
  return success;
}

/**
 * ggd_opt_group_write_to_key_file:
 * @group: A #GgdOptGroup
 * @key_file: A #GKeyFile
 * 
 * Writes the values of a #GgdOptGroup to a #GkeyFile.
 */
void
ggd_opt_group_write_to_key_file (GgdOptGroup *group,
                                 GKeyFile    *key_file)
{
  ggd_opt_group_manage_key_file (group, FALSE, key_file);
}

/**
 * ggd_opt_group_write_to_file:
 * @group: A #GgdOptGroup
 * @filename: Name of the file in which save the values, in the GLib file names
 *            encoding
 * @error: Return location for errors or %NULL to ignore them
 * 
 * Writes a #GgdOptGroup to a file.
 * It keeps everything in the file, overwriting only the group managed by the
 * given #GgdOptGroup. This means that it is perfectly safe to use this function
 * to save a group that is not alone in a key file.
 * 
 * <note><para>The file must be both readable and writable.</para></note>
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
gboolean
ggd_opt_group_write_to_file (GgdOptGroup *group,
                             const gchar *filename,
                             GError     **error)
{
  gboolean  success = FALSE;
  GKeyFile *key_file;
  gchar    *data;
  gsize     data_length;
  
  key_file = g_key_file_new ();
  /* try to load the original file but blindly ignore errors because they are
   * unlikely to be interesting (the file doesn't already exist, a syntax error
   * because the file exists but is empty (yes, this throws a parse error),
   * etc.) */
  g_key_file_load_from_file (key_file, filename,
                             G_KEY_FILE_KEEP_COMMENTS |
                             G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
  ggd_opt_group_write_to_key_file (group, key_file);
  data = g_key_file_to_data (key_file, &data_length, error);
  if (data) {
    success = g_file_set_contents (filename, data, data_length, error);
  }
  g_key_file_free (key_file);
  
  return success;
}
