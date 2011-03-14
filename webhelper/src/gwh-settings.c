/*
 *  
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

#include "gwh-settings.h"

#include <string.h>
#include <stdarg.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>


struct _GwhSettingsPrivate
{
  GPtrArray *prop_array;
};


G_DEFINE_TYPE (GwhSettings, gwh_settings, G_TYPE_OBJECT)


static void
gwh_settings_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GwhSettings *self = GWH_SETTINGS (object);
  
  if (G_LIKELY (prop_id > 0 && prop_id <= self->priv->prop_array->len)) {
    GValue *prop_value;
    
    prop_value = g_ptr_array_index (self->priv->prop_array, prop_id - 1);
    g_value_copy (prop_value, value);
  } else {
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
  
  if (G_LIKELY (prop_id > 0 && prop_id <= self->priv->prop_array->len)) {
    GValue *prop_value;
    
    prop_value = g_ptr_array_index (self->priv->prop_array, prop_id - 1);
    g_value_copy (value, prop_value);
  } else {
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
free_prop_item (gpointer data,
                gpointer user_data)
{
  g_value_unset (data);
  g_free (data);
}

static void
gwh_settings_finalize (GObject *object)
{
  GwhSettings *self = GWH_SETTINGS (object);
  
  g_ptr_array_foreach (self->priv->prop_array, free_prop_item, NULL);
  g_ptr_array_free (self->priv->prop_array, TRUE);
  self->priv->prop_array = NULL;
  
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
  
  g_type_class_add_private (klass, sizeof (GwhSettingsPrivate));
}

static void
gwh_settings_init (GwhSettings *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GWH_TYPE_SETTINGS,
                                            GwhSettingsPrivate);
  self->priv->prop_array = g_ptr_array_new ();
}


/*----------------------------- Begin public API -----------------------------*/

GwhSettings *
gwh_settings_get_default (void)
{
  return g_object_new (GWH_TYPE_SETTINGS, NULL);
}

static gboolean
is_pspec_installed (GObject          *obj,
                    const GParamSpec *pspec)
{
  GParamSpec  **pspecs;
  guint         n_props;
  guint         i;
  gboolean      installed = FALSE;
  
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (obj), &n_props);
  for (i = 0; ! installed && i < n_props; i++) {
    installed = (pspec->value_type == pspecs[i]->value_type &&
                 strcmp (pspec->name, pspecs[i]->name) == 0);
  }
  g_free (pspecs);
  
  return installed;
}

void
gwh_settings_install_property (GwhSettings *self,
                               GParamSpec  *pspec)
{
  GValue *value;
  
  g_return_if_fail (GWH_IS_SETTINGS (self));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  
  /* a bit hackish, but allows to install the same property twice because the
   * class will not be destroyed if the plugin gets reloaded. safe since the
   * object is a singleton that will never de destroyed, and the plugin is
   * resident, so the object is still valid after a reload. */
  if (is_pspec_installed (G_OBJECT (self), pspec)) {
    return;
  }
  
  value = g_value_init (g_malloc0 (sizeof *value), pspec->value_type);
  switch (G_TYPE_FUNDAMENTAL (pspec->value_type)) {
    #define HANDLE_BASIC_TYPE(NAME, Name, name)                                \
      case G_TYPE_##NAME:                                                      \
        g_value_set_##name (value, ((GParamSpec##Name*)pspec)->default_value); \
        break;
    
    HANDLE_BASIC_TYPE (BOOLEAN, Boolean, boolean)
    HANDLE_BASIC_TYPE (CHAR,    Char,    char)
    HANDLE_BASIC_TYPE (UCHAR,   UChar,   uchar)
    HANDLE_BASIC_TYPE (INT,     Int,     int)
    HANDLE_BASIC_TYPE (UINT,    UInt,    uint)
    HANDLE_BASIC_TYPE (LONG,    Long,    long)
    HANDLE_BASIC_TYPE (ULONG,   ULong,   ulong)
    HANDLE_BASIC_TYPE (INT64,   Int64,   int64)
    HANDLE_BASIC_TYPE (UINT64,  UInt64,  uint64)
    HANDLE_BASIC_TYPE (FLOAT,   Float,   float)
    HANDLE_BASIC_TYPE (DOUBLE,  Double,  double)
    HANDLE_BASIC_TYPE (ENUM,    Enum,    enum)
    HANDLE_BASIC_TYPE (FLAGS,   Flags,   flags)
    HANDLE_BASIC_TYPE (STRING,  String,  string)
    
    #undef HANDLE_BASIC_TYPE
    
    case G_TYPE_PARAM:
    case G_TYPE_BOXED:
    case G_TYPE_POINTER:
    case G_TYPE_OBJECT:
      /* nothing to do with these types, default GValue is fine since they have
       * no default value */
      break;
    
    default:
      g_critical ("Unsupported property type \"%s\" for property \"%s\"",
                  G_VALUE_TYPE_NAME (value), pspec->name);
      g_value_unset (value);
      g_free (value);
      return;
  }
  g_ptr_array_add (self->priv->prop_array, value);
  g_object_class_install_property (G_OBJECT_GET_CLASS (self),
                                   self->priv->prop_array->len, pspec);
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




/* display/edit widgets stuff */

/*
 * gwh_settings_widget_<type>_notify:
 *   calls user's callback with appropriate arguments
 * gwh_settings_widget_<type>_notify_callback:
 *   a callback connected to a signal on the widget.
 *   calls gwh_settings_widget_<type>_notify
 * gwh_settings_widget_<type>_new:
 *   creates the widget
 * gwh_settings_widget_<type>_sync:
 *   syncs widget's value with its setting
 */


typedef struct _GwhSettingsWidgetNotifyData
{
  GwhSettings  *settings;
  GCallback     callback;
  gpointer      user_data;
} GwhSettingsWidgetNotifyData;


static void
gwh_settings_widget_boolean_notify (GObject                      *tbutton,
                                    GwhSettingsWidgetNotifyData  *data)
{
  ((GwhSettingsWidgetBooleanNotify)data->callback) (data->settings,
                                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tbutton)),
                                                    data->user_data);
}

#define gwh_settings_widget_boolean_notify_callback gwh_settings_widget_boolean_notify

static GtkWidget *
gwh_settings_widget_boolean_new (GwhSettings   *self,
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
gwh_settings_widget_boolean_sync (GwhSettings  *self,
                                  GParamSpec   *pspec,
                                  const GValue *old_value,
                                  GtkWidget    *widget)
{
  gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  
  if (g_value_get_boolean (old_value) != val) {
    g_object_set (self, pspec->name, val, NULL);
  }
}

static void
gwh_settings_widget_enum_notify (GObject                     *object,
                                 GwhSettingsWidgetNotifyData *data)
{
  GtkTreeIter   iter;
  GtkComboBox  *cbox = GTK_COMBO_BOX (object);
  
  if (gtk_combo_box_get_active_iter (cbox, &iter)) {
    gint val;
    
    gtk_tree_model_get (gtk_combo_box_get_model (cbox), &iter, 0, &val, -1);
    ((GwhSettingsWidgetEnumNotify)data->callback) (data->settings, val,
                                                   data->user_data);
  }
}

#define gwh_settings_widget_enum_notify_callback gwh_settings_widget_enum_notify

static GtkWidget *
gwh_settings_widget_enum_new (GwhSettings  *self,
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
gwh_settings_widget_enum_sync (GwhSettings   *self,
                               GParamSpec    *pspec,
                               const GValue  *old_value,
                               GtkWidget     *widget)
{
  GtkTreeIter iter;
  
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
    GtkTreeModel *model;
    gint          val;
    
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
    gtk_tree_model_get (model, &iter, 0, &val, -1);
    if (g_value_get_enum (old_value) != val) {
      g_object_set (self, pspec->name, val, NULL);
    }
  }
}

static void
gwh_settings_widget_int_notify (GObject                     *spin,
                                GwhSettingsWidgetNotifyData *data)
{
  ((GwhSettingsWidgetIntNotify)data->callback) (data->settings,
                                                gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin)),
                                                data->user_data);
}

#define gwh_settings_widget_int_notify_callback gwh_settings_widget_int_notify

static GtkWidget *
gwh_settings_widget_int_new (GwhSettings  *self,
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
gwh_settings_widget_int_sync (GwhSettings  *self,
                              GParamSpec   *pspec,
                              const GValue *old_value,
                              GtkWidget    *widget)
{
  gint val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
  
  if (g_value_get_int (old_value) != val) {
    g_object_set (self, pspec->name, val, NULL);
  }
}

static void
gwh_settings_widget_string_notify (GObject                     *entry,
                                   GwhSettingsWidgetNotifyData *data)
{
  ((GwhSettingsWidgetStringNotify)data->callback) (data->settings,
                                                   gtk_entry_get_text (GTK_ENTRY (entry)),
                                                   data->user_data);
}

static void
gwh_settings_widget_string_notify_callback (GObject                     *object,
                                            GParamSpec                  *pspec,
                                            GwhSettingsWidgetNotifyData *data)
{
  gwh_settings_widget_string_notify (object, data);
}

static GtkWidget *
gwh_settings_widget_string_new (GwhSettings  *self,
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
gwh_settings_widget_string_sync (GwhSettings   *self,
                                 GParamSpec    *pspec,
                                 const GValue  *old_value,
                                 GtkWidget     *widget)
{
  const gchar *val      = gtk_entry_get_text (GTK_ENTRY (widget));
  const gchar *old_val  = g_value_get_string (old_value);
  
  if (g_strcmp0 (old_val, val) != 0) {
    g_object_set (self, pspec->name, val, NULL);
  }
}


#define KEY_PSPEC   "gwh-settings-configure-pspec"
#define KEY_WIDGET  "gwh-settings-configure-widget"

/**
 * gwh_settings_widget_new_full:
 * @self: A GwhSettings object
 * @prop_name: the name of the setting for which create a widget
 * @setting_changed_callback: a callback to be called when the widget's value
 *                            changes, or %NULL. The type of the callback
 *                            is GwhSettingsWidget*Notify, depending of the
 *                            setting type
 * @user_data: user data to pass to @callback
 * @notify_flags: notification flags
 * 
 * Creates a widgets to configure the value of a setting @prop_name.
 * This setting will not be changed automatically, you'll need to call
 * gwh_settings_widget_sync() when you want to synchronize the widget's value to
 * the setting.
 * 
 * Returns: A #GtkWidget that displays and edit @prop_name.
 */
GtkWidget *
gwh_settings_widget_new_full (GwhSettings            *self,
                              const gchar            *prop_name,
                              GCallback               setting_changed_callback,
                              gpointer                user_data,
                              GwhSettingsNotifyFlags  notify_flags)
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
    #define HANDLE_TYPE(T, t, signal)                                          \
      case G_TYPE_##T:                                                         \
        widget = gwh_settings_widget_##t##_new (self, &value, pspec,           \
                                                &needs_label);                 \
        if (setting_changed_callback) {                                        \
          GwhSettingsWidgetNotifyData *data = g_malloc (sizeof *data);         \
                                                                               \
          data->settings  = self;                                              \
          data->callback  = setting_changed_callback;                          \
          data->user_data = user_data;                                         \
          g_signal_connect_data (widget, signal,                               \
                                 G_CALLBACK (gwh_settings_widget_##t##_notify_callback),\
                                 data, (GClosureNotify)g_free, 0);             \
          if (notify_flags & GWH_SETTINGS_NOTIFY_ON_CONNEXION) {               \
            gwh_settings_widget_##t##_notify (G_OBJECT (widget), data);        \
          }                                                                    \
        }                                                                      \
        break;
    
    HANDLE_TYPE (BOOLEAN, boolean,  "toggled")
    HANDLE_TYPE (ENUM,    enum,     "changed")
    HANDLE_TYPE (INT,     int,      "value-changed")
    HANDLE_TYPE (STRING,  string,   "notify::text")
    
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

GtkWidget *
gwh_settings_widget_new (GwhSettings *self,
                         const gchar *prop_name)
{
  return gwh_settings_widget_new_full (self, prop_name, NULL, NULL, 0);
}

static gboolean
gwh_settings_widget_sync_internal (GwhSettings *self,
                                   GtkWidget   *widget)
{
  GParamSpec *pspec;
  GValue      value = {0};
  
  g_return_val_if_fail (G_IS_OBJECT (widget), FALSE);
  
  widget = g_object_get_data (G_OBJECT (widget), KEY_WIDGET);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  pspec = g_object_get_data (G_OBJECT (widget), KEY_PSPEC);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (self), pspec->name, &value);
  switch (G_TYPE_FUNDAMENTAL (pspec->value_type)) {
    #define HANDLE_TYPE(T, t)                                                  \
      case G_TYPE_##T:                                                         \
        gwh_settings_widget_##t##_sync (self, pspec, &value, widget);          \
        break;
    
    HANDLE_TYPE (BOOLEAN,  boolean)
    HANDLE_TYPE (ENUM,     enum)
    HANDLE_TYPE (INT,      int)
    HANDLE_TYPE (STRING,   string)
    
    #undef HANDLE_TYPE
    
    default:
      g_critical ("Unsupported property type \"%s\"",
                  g_type_name (pspec->value_type));
  }
  g_value_unset (&value);
  
  return TRUE;
}

/**
 * gwh_settings_widget_sync_v:
 * @self: A #GwhSettings object
 * @...: a %NULL-terminated list of widgets to sync
 * 
 * Same as gwh_settings_widget_sync() but emits notifications only after all
 * widgets got synchronized.
 */
void
gwh_settings_widget_sync_v (GwhSettings *self,
                            ...)
{
  GtkWidget  *widget;
  va_list     ap;
  
  g_return_if_fail (GWH_IS_SETTINGS (self));
  
  g_object_freeze_notify (G_OBJECT (self));
  va_start (ap, self);
  while ((widget = va_arg (ap, GtkWidget*))) {
    if (! gwh_settings_widget_sync_internal (self, widget)) {
      break;
    }
  }
  va_end (ap);
  g_object_thaw_notify (G_OBJECT (self));
}

void
gwh_settings_widget_sync (GwhSettings *self,
                          GtkWidget   *widget)
{
  g_return_if_fail (GWH_IS_SETTINGS (self));
  
  gwh_settings_widget_sync_internal (self, widget);
}
