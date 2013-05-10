/*
 * config.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <geanyplugin.h>
#include "conf.h"
#include "markdown-gtk-compat.h"

#define FONT_NAME_MAX  256
#define COLOR_CODE_MAX 8
#define DEFAULT_CONF \
  "[general]\n" \
  "template=\n" \
  "\n" \
  "[view]\n" \
  "position=0\n" \
  "font_name=Serif\n" \
  "code_font_name=Mono\n" \
  "font_point_size=12\n" \
  "code_font_point_size=12\n" \
  "bg_color=#fff\n" \
  "fg_color=#000\n"

#define MARKDOWN_HTML_TEMPLATE \
  "<html>\n" \
  "  <head>\n" \
  "    <style type=\"text/css\">\n" \
  "      body {\n" \
  "        font-family: @@font_name@@;\n" \
  "        font-size: @@font_point_size@@pt;\n" \
  "        background-color: @@bg_color@@;\n" \
  "        color: @@fg_color@@;\n" \
  "      }\n" \
  "      code {\n" \
  "        font-family: @@code_font_name@@;\n" \
  "        font-size: @@code_font_point_size@@pt;\n" \
  "      }\n" \
  "    </style>\n" \
  "  </head>\n" \
  "  <body>\n" \
  "    @@markdown@@\n" \
  "  </body>\n" \
  "</html>\n"

enum
{
  PROP_0 = 0,
  PROP_TEMPLATE_FILE,
  PROP_FONT_NAME,
  PROP_CODE_FONT_NAME,
  PROP_FONT_POINT_SIZE,
  PROP_CODE_FONT_POINT_SIZE,
  PROP_BG_COLOR,
  PROP_FG_COLOR,
  PROP_VIEW_POS,
  PROP_LAST
};

static GParamSpec *md_props[PROP_LAST] = { NULL };

struct _MarkdownConfigPrivate
{
  gchar *filename;
  GKeyFile *kf;
  guint handle;
  gulong dlg_handle;
  gboolean initialized;
  gchar *tmpl_text;
  gsize tmpl_text_len;
  struct {
    GtkWidget *table;
    GtkWidget *pos_sb_radio;
    GtkWidget *pos_mw_radio;
    GtkWidget *font_button;
    GtkWidget *code_font_button;
    GtkWidget *bg_color_button;
    GtkWidget *fg_color_button;
    GtkWidget *tmpl_file_button;
  } widgets;
};

static void markdown_config_finalize(GObject *object);

G_DEFINE_TYPE(MarkdownConfig, markdown_config, G_TYPE_OBJECT)

static gboolean on_idle_timeout(MarkdownConfig *conf)
{
  markdown_config_save(conf);
  conf->priv->handle = 0;
  return FALSE;
}

static void
init_conf_file(MarkdownConfig *conf)
{
  GError *error = NULL;
  gchar *def_tmpl, *dirn;

  dirn = g_path_get_dirname(conf->priv->filename);
  if (!g_file_test(dirn, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dirn, 0755);
  }

  if (!g_file_test(conf->priv->filename, G_FILE_TEST_EXISTS)) {
    if (!g_file_set_contents(conf->priv->filename, DEFAULT_CONF, -1, &error)) {
      g_warning("Unable to write default configuration file: %s", error->message);
      g_error_free(error); error = NULL;
    }
  }

  def_tmpl = g_build_filename(dirn, "template.html", NULL);

  if (!g_file_test(def_tmpl, G_FILE_TEST_EXISTS)) {
    if (!g_file_set_contents(def_tmpl, MARKDOWN_HTML_TEMPLATE, -1, &error)) {
      g_warning("Unable to write default template file: %s", error->message);
      g_error_free(error); error = NULL;
    }
  }

  g_free(dirn);
  g_free(def_tmpl);
}

static void
markdown_config_load_template_text(MarkdownConfig *conf)
{
  GError *error = NULL;
  gchar *tmpl_file = NULL;

  g_object_get(conf, "template-file", &tmpl_file, NULL);

  g_free(conf->priv->tmpl_text);
  conf->priv->tmpl_text = NULL;
  conf->priv->tmpl_text_len = 0;

  if (!g_file_get_contents(tmpl_file, &(conf->priv->tmpl_text),
      &(conf->priv->tmpl_text_len), &error))
  {
    g_warning("Error reading template file: %s", error->message);
    g_error_free(error); error = NULL;
  }
}

static void
markdown_config_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  gboolean save_later = FALSE;
  MarkdownConfig *conf = MARKDOWN_CONFIG(obj);

  /* FIXME: Prevent writing to keyfile until it's ready */
  if (!conf->priv->initialized)
    return;

  switch (prop_id) {
    case PROP_TEMPLATE_FILE:
      g_key_file_set_string(conf->priv->kf, "general", "template",
        g_value_get_string(value));
      /*markdown_config_load_template_text(conf);*/
      save_later = TRUE;
      break;
    case PROP_FONT_NAME:
      g_key_file_set_string(conf->priv->kf, "view", "font_name",
        g_value_get_string(value));
      save_later = TRUE;
      break;
    case PROP_CODE_FONT_NAME:
      g_key_file_set_string(conf->priv->kf, "view", "code_font_name",
        g_value_get_string(value));
      save_later = TRUE;
      break;
    case PROP_FONT_POINT_SIZE:
      g_key_file_set_integer(conf->priv->kf, "view", "font_point_size",
        (gint) g_value_get_uint(value));
      save_later = TRUE;
      break;
    case PROP_CODE_FONT_POINT_SIZE:
      g_key_file_set_integer(conf->priv->kf, "view", "code_font_point_size",
        (gint) g_value_get_uint(value));
      save_later = TRUE;
      break;
    case PROP_BG_COLOR:
      g_key_file_set_string(conf->priv->kf, "view", "bg_color",
        g_value_get_string(value));
      save_later = TRUE;
      break;
    case PROP_FG_COLOR:
      g_key_file_set_string(conf->priv->kf, "view", "fg_color",
        g_value_get_string(value));
      save_later = TRUE;
      break;
    case PROP_VIEW_POS:
      g_key_file_set_integer(conf->priv->kf, "view", "position",
        (gint) g_value_get_uint(value));
      save_later = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }

  if (save_later && conf->priv->handle == 0) {
    conf->priv->handle = g_idle_add((GSourceFunc) on_idle_timeout, conf);
  }
}

static gchar *
markdown_config_get_string_key(MarkdownConfig *conf, const gchar *group,
  const gchar *key, const gchar *default_value)
{
  gchar *out_str;
  GError *error = NULL;

  out_str = g_key_file_get_string(conf->priv->kf, group, key, &error);
  if (error) {
    g_debug("Config read failed: %s", error->message);
    g_error_free(error); error = NULL;
    out_str = g_strdup(default_value);
  }

  return out_str;
}

static guint
markdown_config_get_uint_key(MarkdownConfig *conf, const gchar *group,
  const gchar *key, guint default_value)
{
  guint out_uint;
  GError *error = NULL;

  out_uint = (guint) g_key_file_get_integer(conf->priv->kf, group, key, &error);
  if (error) {
    g_debug("Config read failed: %s", error->message);
    g_error_free(error); error = NULL;
    out_uint = default_value;
  }

  return out_uint;
}

static void
markdown_config_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
  MarkdownConfig *conf = MARKDOWN_CONFIG(obj);

  switch (prop_id) {
    case PROP_TEMPLATE_FILE:
    {
      gchar *tmpl_file;
      tmpl_file = markdown_config_get_string_key(conf, "general", "template", "");
      /* If empty, use default template.html file. */
      if (!tmpl_file || !tmpl_file[0]) {
        gchar *dn;
        g_free(tmpl_file);
        dn = g_path_get_dirname(conf->priv->filename);
        tmpl_file = g_build_filename(dn, "template.html", NULL);
        g_free(dn);
      }
      g_value_set_string(value, tmpl_file);
      g_free(tmpl_file);
      break;
    }
    case PROP_FONT_NAME:
    {
      gchar *font_name;
      font_name = markdown_config_get_string_key(conf, "view", "font_name", "Serif");
      g_value_set_string(value, font_name);
      g_free(font_name);
      break;
    }
    case PROP_CODE_FONT_NAME:
    {
      gchar *font_name;
      font_name = markdown_config_get_string_key(conf, "view", "code_font_name", "Monospace");
      g_value_set_string(value, font_name);
      g_free(font_name);
      break;
    }
    case PROP_FONT_POINT_SIZE:
    {
      guint font_size;
      font_size = markdown_config_get_uint_key(conf, "view", "font_point_size", 12);
      g_value_set_uint(value, font_size);
      break;
    }
    case PROP_CODE_FONT_POINT_SIZE:
    {
      guint font_size;
      font_size = markdown_config_get_uint_key(conf, "view", "code_font_point_size", 12);
      g_value_set_uint(value, font_size);
      break;
    }
    case PROP_BG_COLOR:
    {
      gchar *bg_color;
      bg_color = markdown_config_get_string_key(conf, "view", "bg_color", "#ffffff");
      g_value_set_string(value, bg_color);
      g_free(bg_color);
      break;
    }
    case PROP_FG_COLOR:
    {
      gchar *fg_color;
      fg_color = markdown_config_get_string_key(conf, "view", "fg_color", "#000000");
      g_value_set_string(value, fg_color);
      g_free(fg_color);
      break;
    }
    case PROP_VIEW_POS:
    {
      guint view_pos;
      view_pos = markdown_config_get_uint_key(conf, "view", "position", 0);
      g_value_set_uint(value, view_pos);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void
markdown_install_class_properties(GObjectClass *gclass, guint n_pspecs,
  GParamSpec **pspecs)
{
#if GLIB_CHECK_VERSION(2, 26, 0)
  g_object_class_install_properties(gclass, n_pspecs, pspecs);
#else
  guint i;
  for (i = 1; i < n_pspecs; i++)
    g_object_class_install_property(gclass, i, pspecs[i]);
#endif
}

static void
markdown_config_class_init(MarkdownConfigClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);
  g_object_class->finalize = markdown_config_finalize;
  g_object_class->set_property = markdown_config_set_property;
  g_object_class->get_property = markdown_config_get_property;
  g_type_class_add_private((gpointer)klass, sizeof(MarkdownConfigPrivate));

  md_props[PROP_TEMPLATE_FILE] = g_param_spec_string("template-file", "TemplateFile",
    "HTML template file for preview", "", G_PARAM_READWRITE);
  md_props[PROP_FONT_NAME] = g_param_spec_string("font-name", "FontName",
    "Font family name", "Serif", G_PARAM_READWRITE);
  md_props[PROP_CODE_FONT_NAME] = g_param_spec_string("code-font-name", "CodeFontName",
    "Font family for code blocks", "Monospace", G_PARAM_READWRITE);
  md_props[PROP_FONT_POINT_SIZE] = g_param_spec_uint("font-point-size", "FontPointSize",
    "Size in points of the font", 2, 100, 12, G_PARAM_READWRITE);
  md_props[PROP_CODE_FONT_POINT_SIZE] = g_param_spec_uint("code-font-point-size",
    "CodeFontPointSize", "Size in points of the font for code blocks", 2, 100, 12,
    G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  md_props[PROP_BG_COLOR] = g_param_spec_string("bg-color", "BackgroundColor",
    "Background colour of the page", "#ffffff", G_PARAM_READWRITE);
  md_props[PROP_FG_COLOR] = g_param_spec_string("fg-color", "ForegroundColor",
    "Foreground colour of the page", "#000000", G_PARAM_READWRITE);
  md_props[PROP_VIEW_POS] = g_param_spec_uint("view-pos", "ViewPosition",
    "Notebook where the view will be positioned", 0,
    MARKDOWN_CONFIG_VIEW_POS_MAX-1, (guint) MARKDOWN_CONFIG_VIEW_POS_SIDEBAR,
    G_PARAM_READWRITE);

  markdown_install_class_properties(g_object_class, PROP_LAST, md_props);
}


static void
markdown_config_finalize(GObject *object)
{
  MarkdownConfig *self;

  g_return_if_fail(MARKDOWN_IS_CONFIG(object));

  self = MARKDOWN_CONFIG(object);

  if (self->priv->handle != 0) {
    g_source_remove(self->priv->handle);
    markdown_config_save(self);
  }

  g_free(self->priv->filename);
  g_key_file_free(self->priv->kf);

  G_OBJECT_CLASS(markdown_config_parent_class)->finalize (object);
}


static void
markdown_config_init(MarkdownConfig *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MARKDOWN_TYPE_CONFIG, MarkdownConfigPrivate);
}


MarkdownConfig *
markdown_config_new(const gchar *filename)
{
  MarkdownConfig *conf = g_object_new(MARKDOWN_TYPE_CONFIG, NULL);
  GError *error = NULL;

  g_return_val_if_fail(filename, conf);

  conf->priv->filename = g_strdup(filename);
  init_conf_file(conf);
  conf->priv->kf = g_key_file_new();
  if (!g_key_file_load_from_file(conf->priv->kf, conf->priv->filename,
    G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
  {
    g_warning("Error loading configuration file: %s", error->message);
    g_error_free(error); error = NULL;
  }

  conf->priv->initialized = TRUE;

  return conf;
}

gboolean
markdown_config_save(MarkdownConfig *conf)
{
  gchar *contents;
  gsize len;
  gboolean success = FALSE;
  GError *error = NULL;

  contents = g_key_file_to_data(conf->priv->kf, &len, &error);

  /*g_debug("Saving: %s\n%s", conf->priv->filename, contents);*/

  if (error) {
    g_warning("Error getting config data as string: %s", error->message);
    g_error_free(error); error = NULL;
    return success;
  }

  success = g_file_set_contents(conf->priv->filename, contents, len, &error);
  g_free(contents);

  if (!success) {
    g_warning("Error writing config data to disk: %s", error->message);
    g_error_free(error); error = NULL;
  }

  return success;
}

static gchar *
color_button_get_color(GtkColorButton *color_button)
{
  MarkdownColor color;

  markdown_gtk_color_button_get_color(color_button, &color);

  return g_strdup_printf("#%02x%02x%02x", color.red, color.green, color.blue);
}

static gboolean
get_font_info(const gchar *font_desc, gchar **font_name, guint *font_size)
{
  gboolean success = FALSE;
  PangoFontDescription *pfd;

  pfd = pango_font_description_from_string(font_desc);
  if (pfd) {
    *font_name = g_strdup(pango_font_description_get_family(pfd));
    *font_size = (guint) (pango_font_description_get_size(pfd) / PANGO_SCALE);
    success = TRUE;
    pango_font_description_free(pfd);
  }

  return success;
}

static void
on_dialog_response(MarkdownConfig *conf, gint response_id, GtkDialog *dialog)
{
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK) {
    GtkWidget *wid = conf->priv->widgets.pos_sb_radio;
    gboolean pos_sidebar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wid));
    gchar *bg_color, *fg_color;
    gchar *tmpl_file = NULL, *fnt = NULL, *code_fnt = NULL;
    guint fnt_size = 0, code_fnt_size = 0;
    const gchar *font_desc;
    MarkdownConfigViewPos view_pos;

    view_pos = pos_sidebar ? MARKDOWN_CONFIG_VIEW_POS_SIDEBAR : MARKDOWN_CONFIG_VIEW_POS_MSGWIN;

    bg_color = color_button_get_color(GTK_COLOR_BUTTON(conf->priv->widgets.bg_color_button));
    fg_color = color_button_get_color(GTK_COLOR_BUTTON(conf->priv->widgets.fg_color_button));

    font_desc = gtk_font_button_get_font_name(GTK_FONT_BUTTON(conf->priv->widgets.font_button));
    get_font_info(font_desc, &fnt, &fnt_size);

    font_desc = gtk_font_button_get_font_name(GTK_FONT_BUTTON(conf->priv->widgets.code_font_button));
    get_font_info(font_desc, &code_fnt, &code_fnt_size);

    tmpl_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(conf->priv->widgets.tmpl_file_button));

    g_object_set(conf,
                 "font-name", fnt,
                 "font-point-size", fnt_size,
                 "code-font-name", code_fnt,
                 "code-font-point-size", code_fnt_size,
                 "view-pos", view_pos,
                 "bg-color", bg_color,
                 "fg-color", fg_color,
                 "template-file", tmpl_file,
                 NULL);

    g_free(fnt);
    g_free(code_fnt);
    g_free(bg_color);
    g_free(fg_color);
    g_free(tmpl_file);
  }
}

GtkWidget *markdown_config_gui(MarkdownConfig *conf, GtkDialog *dialog)
{
  GSList *grp = NULL;
  GtkWidget *table, *label, *hbox, *wid;
  gchar *tmpl_file=NULL, *fnt=NULL, *code_fnt=NULL, *bg=NULL, *fg=NULL;
  guint view_pos=0, fnt_sz=0, code_fnt_sz=0;

  g_object_get(conf,
               "view-pos", &view_pos,
               "font-name", &fnt,
               "code-font-name", &code_fnt,
               "font-point-size", &fnt_sz,
               "code-font-point-size", &code_fnt_sz,
               "bg-color", &bg,
               "fg-color", &fg,
               "template-file", &tmpl_file,
               NULL);

  table = markdown_gtk_table_new(6, 2, FALSE);
  markdown_gtk_table_set_col_spacing(MARKDOWN_GTK_TABLE(table), 6);
  markdown_gtk_table_set_row_spacing(MARKDOWN_GTK_TABLE(table), 6);

  conf->priv->widgets.table = table;

  { /* POSITION OF VIEW */
    label = gtk_label_new(_("Position:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL);

    hbox = gtk_hbox_new(FALSE, 6);

    wid = gtk_radio_button_new_with_label(grp, _("Sidebar"));
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(wid));
    gtk_box_pack_start(GTK_BOX(hbox), wid, FALSE, TRUE, 0);
    conf->priv->widgets.pos_sb_radio = wid;
    if (((MarkdownConfigViewPos) view_pos) == MARKDOWN_CONFIG_VIEW_POS_SIDEBAR) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), TRUE);
    }

    wid = gtk_radio_button_new_with_label(grp, _("Message Window"));
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(wid));
    gtk_box_pack_start(GTK_BOX(hbox), wid, FALSE, TRUE, 0);
    conf->priv->widgets.pos_mw_radio = wid;
    if (((MarkdownConfigViewPos) view_pos) == MARKDOWN_CONFIG_VIEW_POS_MSGWIN) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), TRUE);
    }

    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), hbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL);
  }

  { /* FONT BUTTON */
    gchar *font_desc;

    label = gtk_label_new(_("Font:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL);

    font_desc = g_strdup_printf("%s %d", fnt, fnt_sz);
    wid = gtk_font_button_new_with_font(font_desc);
    conf->priv->widgets.font_button = wid;
    g_free(font_desc);

    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), wid, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL);

    g_free(fnt);
  }

  { /* CODE FONT BUTTON */
    gchar *font_desc;

    label = gtk_label_new(_("Code Font:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL);

    font_desc = g_strdup_printf("%s %d", code_fnt, code_fnt_sz);
    wid = gtk_font_button_new_with_font(font_desc);
    conf->priv->widgets.code_font_button = wid;
    g_free(font_desc);

    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), wid, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL);

    g_free(code_fnt);
  }

  { /* BG COLOR */
    MarkdownColor bgclr;

    label = gtk_label_new(_("BG Color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL);

    markdown_color_parse(bg, &bgclr);

    wid = markdown_gtk_color_button_new_with_color(&bgclr);
    conf->priv->widgets.bg_color_button = wid;
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), wid, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL);

    g_free(bg);
  }

  { /* FG COLOR */
    MarkdownColor fgclr;

    label = gtk_label_new(_("FG Color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL, GTK_FILL);

    markdown_color_parse(fg, &fgclr);

    wid = markdown_gtk_color_button_new_with_color(&fgclr);
    conf->priv->widgets.fg_color_button = wid;
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), wid, 1, 2, 4, 5, GTK_FILL | GTK_EXPAND, GTK_FILL);

    g_free(fg);
  }

  { /* TEMPLATE FILE */
    label = gtk_label_new(_("Template:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL, GTK_FILL);

    wid = gtk_file_chooser_button_new(_("Select Template File"),
      GTK_FILE_CHOOSER_ACTION_OPEN);
    conf->priv->widgets.tmpl_file_button = wid;
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(wid), g_get_home_dir());
    if (tmpl_file && tmpl_file[0]) {
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(wid), tmpl_file);
    }
    markdown_gtk_table_attach(MARKDOWN_GTK_TABLE(table), wid, 1, 2, 5, 6, GTK_FILL | GTK_EXPAND, GTK_FILL);

    g_free(tmpl_file);
  }

  conf->priv->dlg_handle = g_signal_connect_swapped(dialog, "response",
    G_CALLBACK(on_dialog_response), conf);

  gtk_widget_show_all(table);

  return table;
}

const gchar *
markdown_config_get_template_text(MarkdownConfig *conf)
{
  g_return_val_if_fail(conf, NULL);
  if (!conf->priv->tmpl_text) {
    markdown_config_load_template_text(conf);
  }
  return (const gchar *) conf->priv->tmpl_text;
}

gchar *
markdown_config_get_dirname(MarkdownConfig *conf)
{
  g_return_val_if_fail(conf, NULL);
  return g_path_get_dirname(conf->priv->filename);
}

MarkdownConfigViewPos markdown_config_get_view_pos(MarkdownConfig *conf)
{
  guint view_pos;
  g_return_val_if_fail(MARKDOWN_IS_CONFIG(conf), MARKDOWN_CONFIG_VIEW_POS_SIDEBAR);
  g_object_get(conf, "view-pos", &view_pos, NULL);
  return (MarkdownConfigViewPos) view_pos;
}

void markdown_config_set_view_pos(MarkdownConfig *conf, MarkdownConfigViewPos view_pos)
{
  g_return_if_fail(MARKDOWN_IS_CONFIG(conf));
  g_object_set(conf, "view-pos", view_pos, NULL);
}
