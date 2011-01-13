/*
 *  
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
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

#include "gwh-settings.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h> /* for GtkOrientation */

#include "gwh-enum-types.h" /* for GwhUiBrowserPosition */


/* sets @ptr to @value, freeing @ptr before.
 * note that:
 *  * ptr can be used to compute @value, assignation is done after comuptation
 *  * ptr is used twice, so it must be side-effects-free */
#define setptr(ptr, value)       \
  G_STMT_START {                 \
    gpointer new_ptr = (value);  \
    g_free (ptr);                \
    ptr = new_ptr;               \
  } G_STMT_END


struct _GwhSettingsPrivate
{
  gboolean              browser_auto_reload;
  gchar                *browser_last_uri;
  GtkOrientation        browser_orientation;
  GwhBrowserPosition    browser_position;
  gchar                *browser_separate_window_geometry;
  gchar                *inspector_window_geometry;
};

enum
{
  PROP_0,
  PROP_BROWSER_AUTO_RELOAD,
  PROP_BROWSER_LAST_URI,
  PROP_BROWSER_ORIENTATION,
  PROP_BROWSER_POSITION,
  PROP_BROWSER_SEPARATE_WINDOW_GEOMETRY,
  PROP_INSPECTOR_WINDOW_GEOMETRY
};


G_DEFINE_TYPE (GwhSettings, gwh_settings, G_TYPE_OBJECT)


static void
gwh_settings_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GwhSettings *self = GWH_SETTINGS (object);
  
  switch (prop_id) {
    case PROP_BROWSER_AUTO_RELOAD:
      g_value_set_boolean (value, self->priv->browser_auto_reload);
      break;
    case PROP_BROWSER_LAST_URI:
      g_value_set_string (value, self->priv->browser_last_uri);
      break;
    case PROP_BROWSER_ORIENTATION:
      g_value_set_enum (value, self->priv->browser_orientation);
      break;
    case PROP_BROWSER_POSITION:
      g_value_set_enum (value, self->priv->browser_position);
      break;
    case PROP_BROWSER_SEPARATE_WINDOW_GEOMETRY:
      g_value_set_string (value, self->priv->browser_separate_window_geometry);
      break;
    case PROP_INSPECTOR_WINDOW_GEOMETRY:
      g_value_set_string (value, self->priv->inspector_window_geometry);
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gwh_settings_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GwhSettings *self = GWH_SETTINGS (object);
  
  switch (prop_id) {
    case PROP_BROWSER_AUTO_RELOAD:
      self->priv->browser_auto_reload = g_value_get_boolean (value);
      break;
    case PROP_BROWSER_LAST_URI:
      setptr (self->priv->browser_last_uri, g_value_dup_string (value));
      break;
    case PROP_BROWSER_ORIENTATION:
      self->priv->browser_orientation = g_value_get_enum (value);
      break;
    case PROP_BROWSER_POSITION:
      self->priv->browser_position = g_value_get_enum (value);
      break;
    case PROP_BROWSER_SEPARATE_WINDOW_GEOMETRY:
      setptr (self->priv->browser_separate_window_geometry,
              g_value_dup_string (value));
      break;
    case PROP_INSPECTOR_WINDOW_GEOMETRY:
      setptr (self->priv->inspector_window_geometry, g_value_dup_string (value));
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GObject *
gwh_settings_constructor (GType                  gtype,
                          guint                  n_properties,
                          GObjectConstructParam *properties)
{
  static GObject *obj = NULL;

  if (G_UNLIKELY (! obj)) {
    obj = G_OBJECT_CLASS (gwh_settings_parent_class)->constructor (gtype,
                                                                   n_properties,
                                                                   properties);
  }
  
  /* reffing the object unconditionally will leak one ref (the first one), but
   * since we don't want the object to ever die, this is needed. the other
   * solution would be to force users to keep their reference all along, but
   * then if one drop its reference before somebody else get her one, the
   * settings would be implicitly "reset" to their default values */
  return g_object_ref (obj);
}

static void
gwh_settings_finalize (GObject *object)
{
  GwhSettings *self = GWH_SETTINGS (object);
  
  self->priv->browser_auto_reload = FALSE;
  setptr (self->priv->browser_last_uri, NULL);
  self->priv->browser_orientation = 0;
  self->priv->browser_position = 0;
  setptr (self->priv->inspector_window_geometry, NULL);
  
  G_OBJECT_CLASS (gwh_settings_parent_class)->finalize (object);
}

static void
gwh_settings_class_init (GwhSettingsClass *klass)
{
  GObjectClass       *object_class    = G_OBJECT_CLASS (klass);
  
  object_class->constructor   = gwh_settings_constructor;
  object_class->finalize      = gwh_settings_finalize;
  object_class->get_property  = gwh_settings_get_property;
  object_class->set_property  = gwh_settings_set_property;
  
  g_object_class_install_property (object_class, PROP_BROWSER_AUTO_RELOAD,
                                   g_param_spec_boolean ("browser-auto-reload",
                                                         "Browser auto reload",
                                                         "Whether the browser reloads itself upon document saving",
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_BROWSER_LAST_URI,
                                   g_param_spec_string ("browser-last-uri",
                                                        "Browser last URI",
                                                        "Last URI visited by the browser",
                                                        "about:blank",
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_BROWSER_ORIENTATION,
                                   g_param_spec_enum ("browser-orientation",
                                                      "Browser orientation",
                                                      "Orientation of the browser widget",
                                                      GTK_TYPE_ORIENTATION,
                                                      GTK_ORIENTATION_VERTICAL,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_BROWSER_POSITION,
                                   g_param_spec_enum ("browser-position",
                                                      "Browser position",
                                                      "Position of the browser widget in Geany's UI",
                                                      GWH_TYPE_BROWSER_POSITION,
                                                      GWH_BROWSER_POSITION_MESSAGE_WINDOW,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_BROWSER_SEPARATE_WINDOW_GEOMETRY,
                                   g_param_spec_string ("browser-separate-window-geometry",
                                                        "Browser separate window geometry",
                                                        "Last geometry of the separated browser's window",
                                                        "400x300",
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_INSPECTOR_WINDOW_GEOMETRY,
                                   g_param_spec_string ("inspector-window-geometry",
                                                        "Inspector window geometry",
                                                        "Last geometry of the inspector window",
                                                        "400x300",
                                                        G_PARAM_READWRITE));
  
  g_type_class_add_private (klass, sizeof (GwhSettingsPrivate));
}

static void
gwh_settings_init (GwhSettings *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GWH_TYPE_SETTINGS,
                                            GwhSettingsPrivate);
  self->priv->browser_auto_reload       = TRUE;
  self->priv->browser_last_uri          = g_strdup ("about:blank");
  self->priv->browser_orientation       = GTK_ORIENTATION_VERTICAL;
  self->priv->browser_position          = GWH_BROWSER_POSITION_MESSAGE_WINDOW;
  self->priv->browser_separate_window_geometry = g_strdup ("400x300");
  self->priv->inspector_window_geometry = g_strdup ("400x300");
}


/*----------------------------- Begin public API -----------------------------*/

GwhSettings *
gwh_settings_get_default (void)
{
  return g_object_new (GWH_TYPE_SETTINGS, NULL);
}

static void
get_key_and_group_from_property_name (const gchar *name,
                                      gchar      **group,
                                      gchar      **key)
{
  const gchar *sep;
  
  sep = strchr (name, '-');
  if (sep && sep[1] != 0) {
    *group = g_strndup (name, (gsize)(sep - name));
    *key = g_strdup (&sep[1]);
  } else {
    *group = g_strdup ("general");
    *key = g_strdup (name);
  }
}

static gboolean
key_file_set_value (GKeyFile     *kf,
                    const gchar  *name,
                    const GValue *value,
                    GError      **error)
{
  gboolean  success = TRUE;
  gchar    *group;
  gchar    *key;
  
  get_key_and_group_from_property_name (name, &group, &key);
  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value))) {
    case G_TYPE_BOOLEAN:
      g_key_file_set_boolean (kf, group, key, g_value_get_boolean (value));
      break;
    
    case G_TYPE_ENUM: {
      GEnumClass *enum_class;
      GEnumValue *enum_value;
      gint        val = g_value_get_enum (value);
      
      enum_class = g_type_class_ref (G_VALUE_TYPE (value));
      enum_value = g_enum_get_value (enum_class, val);
      if (! enum_value) {
        g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                     "Value \"%d\" is not valid for key \"%s::%s\"",
                     val, group, key);
      } else {
        g_key_file_set_string (kf, group, key, enum_value->value_nick);
      }
      g_type_class_unref (enum_class);
      break;
    }
    
    case G_TYPE_INT:
      g_key_file_set_integer (kf, group, key, g_value_get_int (value));
      break;
    
    case G_TYPE_STRING:
      g_key_file_set_string (kf, group, key, g_value_get_string (value));
      break;
    
    default:
      g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                   "Unsupported setting type \"%s\" for setting \"%s::%s\"",
                   G_VALUE_TYPE_NAME (value), group, key);
      success =  FALSE;
  }
  g_free (group);
  g_free (key);
  
  return success;
}

gboolean
gwh_settings_save_to_file (GwhSettings *self,
                           const gchar *filename,
                           GError     **error)
{
  GParamSpec  **pspecs;
  guint         n_props;
  guint         i;
  gboolean      success = TRUE;
  GKeyFile     *key_file;
  
  g_return_val_if_fail (GWH_IS_SETTINGS (self), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  
  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, filename, G_KEY_FILE_KEEP_COMMENTS |
                             G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (self), &n_props);
  for (i = 0; success && i < n_props; i++) {
    GValue  value = {0};
    
    g_value_init (&value, pspecs[i]->value_type);
    g_object_get_property (G_OBJECT (self), pspecs[i]->name, &value);
    success = key_file_set_value (key_file, pspecs[i]->name, &value, error);
    g_value_unset (&value);
  }
  g_free (pspecs);
  if (success) {
    gchar  *data;
    gsize   length;
    
    data = g_key_file_to_data (key_file, &length, error);
    if (! data) {
      success = FALSE;
    } else {
      success = g_file_set_contents (filename, data, length, error);
    }
  }
  g_key_file_free (key_file);
  
  return success;
}

static gboolean
key_file_get_value (GKeyFile     *kf,
                    const gchar  *name,
                    GValue       *value,
                    GError      **error)
{
  GError *err = NULL;
  gchar  *group;
  gchar  *key;
  
  get_key_and_group_from_property_name (name, &group, &key);
  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value))) {
    case G_TYPE_BOOLEAN: {
      gboolean val;
      
      val = g_key_file_get_boolean (kf, group, key, &err);
      if (! err) {
        g_value_set_boolean (value, val);
      }
      break;
    }
    
    case G_TYPE_ENUM: {
      gchar      *str;
      GEnumClass *enum_class;
      GEnumValue *enum_value;
      
      enum_class = g_type_class_ref (G_VALUE_TYPE (value));
      str = g_key_file_get_string (kf, group, key, &err);
      if (! err) {
        enum_value = g_enum_get_value_by_nick (enum_class, str);
        if (! enum_value) {
          g_set_error (&err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                       "Value \"%s\" is not valid for key \"%s::%s\"",
                       str, group, key);
        } else {
          g_value_set_enum (value, enum_value->value);
        }
        g_free (str);
      }
      g_type_class_unref (enum_class);
      break;
    }
    
    case G_TYPE_INT: {
      gint val;
      
      val = g_key_file_get_integer (kf, group, key, &err);
      if (! err) {
        g_value_set_int (value, val);
      }
      break;
    }
    
    case G_TYPE_STRING: {
      gchar *val;
      
      val = g_key_file_get_string (kf, group, key, &err);
      if (! err) {
        g_value_take_string (value, val);
      }
      break;
    }
    
    default:
      g_set_error (&err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE,
                   "Unsupported setting type \"%s\" for setting \"%s::%s\"",
                   G_VALUE_TYPE_NAME (value), group, key);
  }
  if (err) {
    g_warning ("%s::%s loading failed: %s", group, key, err->message);
    g_propagate_error (error, err);
  }
  g_free (group);
  g_free (key);
  
  return err == NULL;
}

gboolean
gwh_settings_load_from_file (GwhSettings *self,
                             const gchar *filename,
                             GError     **error)
{
  gboolean  success = TRUE;
  GKeyFile *key_file;
  
  g_return_val_if_fail (GWH_IS_SETTINGS (self), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  
  key_file = g_key_file_new ();
  success = g_key_file_load_from_file (key_file, filename, 0, error);
  if (success) {
    GParamSpec  **pspecs;
    guint         n_props;
    guint         i;
    
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (self), &n_props);
    for (i = 0; i < n_props; i++) {
      GValue  value = {0};
      
      g_value_init (&value, pspecs[i]->value_type);
      /* ignore the error since it's likely this one is simply missing and other
       * will be loaded without any problem */
      if (key_file_get_value (key_file, pspecs[i]->name, &value, NULL)) {
        g_object_set_property (G_OBJECT (self), pspecs[i]->name, &value);
      }
      g_value_unset (&value);
    }
    g_free (pspecs);
  }
  g_key_file_free (key_file);
  
  return success;
}







static GtkWidget *
gwh_settings_widget_new_boolean (GwhSettings   *self,
                                 const GValue  *value,
                                 GParamSpec    *pspec,
                                 gboolean      *needs_label)
{
  GtkWidget *widget;
  
  widget = gtk_check_button_new_with_label (g_param_spec_get_nick (pspec));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                g_value_get_boolean (value));
  *needs_label = FALSE;
  
  return widget;
}

static void
gwh_settings_widget_sync_boolean (GwhSettings *self,
                                  GParamSpec  *pspec,
                                  GtkWidget   *widget)
{
  g_object_set (self, pspec->name,
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
                NULL);
}

static GtkWidget *
gwh_settings_widget_new_enum (GwhSettings  *self,
                              const GValue *value,
                              GParamSpec   *pspec,
                              gboolean     *needs_label)
{
  GtkWidget        *widget;
  GEnumClass       *enum_class;
  guint             i;
  GtkListStore     *store;
  GtkCellRenderer  *renderer;
  gint              active = 0;
  
  store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  for (i = 0; i < enum_class->n_values; i++) {
    GtkTreeIter iter;
    
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        0, enum_class->values[i].value,
                        1, enum_class->values[i].value_nick,
                        -1);
    if (g_value_get_enum (value) == enum_class->values[i].value) {
      active = i;
    }
  }
  g_type_class_unref (enum_class);
  
  widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
                                  "text", 1, NULL);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);
  
  *needs_label = TRUE;
  
  return widget;
}

static void
gwh_settings_widget_sync_enum (GwhSettings *self,
                               GParamSpec  *pspec,
                               GtkWidget   *widget)
{
  GtkTreeIter iter;
  
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
    GtkTreeModel *model;
    gint          val;
    
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
    gtk_tree_model_get (model, &iter, 0, &val, -1);
    g_object_set (self, pspec->name, val, NULL);
  }
}

static GtkWidget *
gwh_settings_widget_new_int (GwhSettings  *self,
                             const GValue *value,
                             GParamSpec   *pspec,
                             gboolean     *needs_label)
{
  GtkWidget      *button;
  GtkObject      *adj;
  GParamSpecInt  *pspec_int = G_PARAM_SPEC_INT (pspec);
  
  adj = gtk_adjustment_new ((gdouble)g_value_get_int (value),
                            (gdouble)pspec_int->minimum,
                            (gdouble)pspec_int->maximum,
                            1.0, 10.0, 0.0);
  button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0.0, 0);
  *needs_label = TRUE;
  
  return button;
}

static void
gwh_settings_widget_sync_int (GwhSettings *self,
                              GParamSpec  *pspec,
                              GtkWidget   *widget)
{
  g_object_set (self, pspec->name,
                gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget)),
                NULL);
}

static GtkWidget *
gwh_settings_widget_new_string (GwhSettings  *self,
                                const GValue *value,
                                GParamSpec   *pspec,
                                gboolean     *needs_label)
{
  GtkWidget *widget;
  
  widget = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (widget), g_value_get_string (value));
  *needs_label = TRUE;
  
  return widget;
}

static void
gwh_settings_widget_sync_string (GwhSettings *self,
                                 GParamSpec  *pspec,
                                 GtkWidget   *widget)
{
  g_object_set (self, pspec->name, gtk_entry_get_text (GTK_ENTRY (widget)),
                NULL);
}


#define KEY_PSPEC   "gwh-settings-configure-pspec"
#define KEY_WIDGET  "gwh-settings-configure-widget"

GtkWidget *
gwh_settings_widget_new (GwhSettings *self,
                         const gchar *prop_name)
{
  GtkWidget  *widget      = NULL;
  GParamSpec *pspec;
  GValue      value       = {0};
  gboolean    needs_label = FALSE;
  
  g_return_val_if_fail (GWH_IS_SETTINGS (self), NULL);
  
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), prop_name);
  g_return_val_if_fail (pspec != NULL, NULL);
  
  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (self), prop_name, &value);
  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (&value))) {
    #define HANDLE_TYPE(T, h) \
      case T: widget = ((h) (self, &value, pspec, &needs_label)); break;
    
    HANDLE_TYPE (G_TYPE_BOOLEAN,  gwh_settings_widget_new_boolean)
    HANDLE_TYPE (G_TYPE_ENUM,     gwh_settings_widget_new_enum)
    HANDLE_TYPE (G_TYPE_INT,      gwh_settings_widget_new_int)
    HANDLE_TYPE (G_TYPE_STRING,   gwh_settings_widget_new_string)
    
    #undef HANDLE_TYPE
    
    default:
      g_critical ("Unsupported property type \"%s\"",
                  G_VALUE_TYPE_NAME (&value));
  }
  if (widget) {
    g_object_set_data_full (G_OBJECT (widget), KEY_PSPEC,
                            g_param_spec_ref (pspec),
                            (GDestroyNotify)g_param_spec_unref);
    if (needs_label) {
      GtkWidget *box;
      gchar     *label;
      
      box = gtk_hbox_new (FALSE, 6);
      label = g_strdup_printf ("%s:", g_param_spec_get_nick (pspec));
      gtk_box_pack_start (GTK_BOX (box), gtk_label_new (label), FALSE, TRUE, 0);
      g_free (label);
      gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
      g_object_set_data_full (G_OBJECT (box), KEY_WIDGET,
                              g_object_ref (widget), g_object_unref);
      widget = box;
    } else {
      g_object_set_data_full (G_OBJECT (widget), KEY_WIDGET,
                              g_object_ref (widget), g_object_unref);
    }
    gtk_widget_set_tooltip_text (widget, g_param_spec_get_blurb (pspec));
  }
  
  return widget;
}

void
gwh_settings_widget_sync (GwhSettings *self,
                          GtkWidget   *widget)
{
  GParamSpec *pspec;
  
  g_return_if_fail (GWH_IS_SETTINGS (self));
  g_return_if_fail (G_IS_OBJECT (widget));
  
  widget = g_object_get_data (G_OBJECT (widget), KEY_WIDGET);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  pspec = g_object_get_data (G_OBJECT (widget), KEY_PSPEC);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  switch (G_TYPE_FUNDAMENTAL (pspec->value_type)) {
    #define HANDLE_TYPE(T, h) case T: (h) (self, pspec, widget); break;
    
    HANDLE_TYPE (G_TYPE_BOOLEAN,  gwh_settings_widget_sync_boolean)
    HANDLE_TYPE (G_TYPE_ENUM,     gwh_settings_widget_sync_enum)
    HANDLE_TYPE (G_TYPE_INT,      gwh_settings_widget_sync_int)
    HANDLE_TYPE (G_TYPE_STRING,   gwh_settings_widget_sync_string)
    
    #undef HANDLE_TYPE
    
    default:
      g_critical ("Unsupported property type \"%s\"",
                  g_type_name (pspec->value_type));
  }
}
