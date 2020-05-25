/*
 *      ao_colortip.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2017 LarsGit223
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <gtk/gtk.h>
#include <glib-object.h>

#include <geanyplugin.h>

#include "addons.h"
#include "ao_colortip.h"

typedef struct _AoColorTipPrivate			AoColorTipPrivate;

#define AO_COLORTIP_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_COLORTIP_TYPE, AoColorTipPrivate))

// This is helpful for making the color-tip larger on 4K screens or for people with less acute vision
#if (!defined(COLOR_TIP_TEMPLATE))
#   if defined(LARGE_COLOR_TIP)
#      define COLOR_TIP_TEMPLATE   "        \n        "
#   elif defined(XLARGE_COLOR_TIP)
#      define COLOR_TIP_TEMPLATE   "          \n          \n          \n          "
#   else
#      define COLOR_TIP_TEMPLATE   "    "
#   endif
#endif

struct _AoColorTip
{
	GObject parent;
};

struct _AoColorTipClass
{
	GObjectClass parent_class;
};

struct _AoColorTipPrivate
{
	gboolean enable_colortip;
	gboolean enable_double_click_color_chooser;
};

enum
{
	PROP_0,
	PROP_ENABLE_COLORTIP,
	PROP_ENABLE_DOUBLE_CLICK_COLOR_CHOOSER,
};

G_DEFINE_TYPE(AoColorTip, ao_color_tip, G_TYPE_OBJECT)

#  define SSM(s, m, w, l) scintilla_send_message (s, m, w, l)


/* Find a color value (short or long form, e.g. #fff or #ffffff) in the
   given string. position must be inside the found color or at maximum
   maxdist bytes in front of it (in front of the start of the color value)
   or behind it (behind the end of the color value). */
static gint contains_color_value(gchar *string, gint position, gint maxdist)
{
	gchar *start;
	gint end, value, offset, color = -1;
	guint length;

	start = strchr(string, '#');
	if (!start)
	{
		start = strstr(string, "0x");
		if (start)
		{
			start += 1; 
		}
	}
	if (start == NULL)
	{
		return color;
	}
	end = start - string + 1;
	while (g_ascii_isxdigit (string[end]))
	{
		end++;
	}
	end--;
	length = &(string[end]) - start + 1;

	if (maxdist != -1)
	{
		offset = start - string + 1;
		if (position < offset &&
		    (offset - position) > maxdist)
		{
			return color;
		}

		offset = end;
		if (position > offset &&
		    (position - offset) > maxdist)
		{
			return color;
		}
	}


	if (length == 4)
	{
		start++;
		color = (g_ascii_xdigit_value (*start) << 4)
				| g_ascii_xdigit_value (*start);

		start++;
		value = (g_ascii_xdigit_value (*start) << 4)
				| g_ascii_xdigit_value (*start);
		color += value << 8;

		start++;
		value = (g_ascii_xdigit_value (*start) << 4)
				| g_ascii_xdigit_value (*start);
		color += value << 16;
	}
	else if (length == 7)
	{
		start++;
		color = (g_ascii_xdigit_value (start[0]) << 4)
				| g_ascii_xdigit_value (start[1]);

		start += 2;
		value = (g_ascii_xdigit_value (start[0]) << 4)
				| g_ascii_xdigit_value (start[1]);
		color += value << 8;

		start += 2;
		value = (g_ascii_xdigit_value (start[0]) << 4)
				| g_ascii_xdigit_value (start[1]);
		color += value << 16;
	}

	return color;
}


static gint get_color_value_at_current_doc_position(void)
{
    gint color = -1;
    GeanyDocument *doc = document_get_current();
    gchar *word = editor_get_word_at_pos(doc->editor, -1, "0123456789abcdefABCDEF");

    if (word)
    {
        switch (strlen (word))
        {
            case 3:
                color = ((g_ascii_xdigit_value(word[0]) * 0x11) << 16 |
                         (g_ascii_xdigit_value(word[1]) * 0x11) << 8 |
                         (g_ascii_xdigit_value(word[2]) * 0x11) << 0);
                break;
            case 6:
                color = (g_ascii_xdigit_value(word[0]) << 20 |
                         g_ascii_xdigit_value(word[1]) << 16 |
                         g_ascii_xdigit_value(word[2]) << 12 |
                         g_ascii_xdigit_value(word[3]) << 8 |
                         g_ascii_xdigit_value(word[4]) << 4 |
                         g_ascii_xdigit_value(word[5]) << 0);
                break;
            default:
                /* invalid color or other format */
                break;
        }
    }

    return color;
}

static gboolean on_editor_button_press_event(GtkWidget *widget, GdkEventButton *event,
											 AoColorTip *colortip)
{
	if (event->button == 1)
	{
		if (event->type == GDK_2BUTTON_PRESS)
		{
			AoColorTipPrivate *priv = AO_COLORTIP_GET_PRIVATE(colortip);

			if (!priv->enable_double_click_color_chooser)
				return FALSE;

			if (get_color_value_at_current_doc_position() != -1)
			{
				keybindings_send_command
					(GEANY_KEY_GROUP_TOOLS, GEANY_KEYS_TOOLS_OPENCOLORCHOOSER);
			}
		}
	}
	return FALSE;
}


void ao_color_tip_editor_notify(AoColorTip *colortip, GeanyEditor *editor, SCNotification *nt)
{
	ScintillaObject *sci = editor->sci;
	AoColorTipPrivate *priv = AO_COLORTIP_GET_PRIVATE(colortip);

	if (!priv->enable_colortip)
	{
		/* Ignore all events if color tips are disabled in preferences */
		return;
	}

	switch (nt->nmhdr.code)
	{
		case SCN_DWELLSTART:
		{
			gchar *subtext;
			gint start, end, pos, max;

			/* Is position valid? */
			if (nt->position < 0)
				break;

			/* Calculate range */
			start = nt->position;
			if (start >= 7)
			{
				start -= 7;
			}
			else
			{
				start = 0;
			}
			pos = nt->position - start;
			end = nt->position + 7;
			max = SSM(sci, SCI_GETTEXTLENGTH, 0, 0);
			if (end > max)
			{
				end = max;
			}

			/* Get text in range and examine it */
			subtext = sci_get_contents_range(sci, start, end);
			if (subtext != NULL)
			{
				gint color;
				
				color = contains_color_value (subtext, pos, 2);
				if (color != -1)
				{
					SSM(sci, SCI_CALLTIPSETBACK, color, 0);
					SSM(sci, SCI_CALLTIPSHOW, nt->position, (sptr_t)COLOR_TIP_TEMPLATE);
				}
				g_free(subtext);
			}
		}
			break;

		case SCN_DWELLEND:
			SSM(sci, SCI_CALLTIPCANCEL, 0, 0);
			break;
	}
}


static void connect_document_button_press_signal_handler(AoColorTip *colortip, GeanyDocument *document)
{
	g_return_if_fail(DOC_VALID(document));

	plugin_signal_connect(
		geany_plugin,
		G_OBJECT(document->editor->sci),
		"button-press-event",
		FALSE,
		G_CALLBACK(on_editor_button_press_event),
		colortip);
}


static void connect_documents_button_press_signal_handler(AoColorTip *colortip)
{
	guint i = 0;
	/* connect the button-press event for all open documents */
	foreach_document(i)
	{
		connect_document_button_press_signal_handler(colortip, documents[i]);
	}
}


static void ao_color_tip_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_COLORTIP(object));

	G_OBJECT_CLASS(ao_color_tip_parent_class)->finalize(object);
}


static void ao_color_tip_set_property(GObject *object, guint prop_id,
									  const GValue *value, GParamSpec *pspec)
{
	AoColorTipPrivate *priv = AO_COLORTIP_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_COLORTIP:
			priv->enable_colortip = g_value_get_boolean(value);
			break;
		case PROP_ENABLE_DOUBLE_CLICK_COLOR_CHOOSER:
			priv->enable_double_click_color_chooser = g_value_get_boolean(value);

			/* If the plugin is loaded while Geany is already running, we need to connect the
			 * button press signal for open documents, if Geany is just booting,
			 * it happens automatically */
			if (priv->enable_double_click_color_chooser && main_is_realized())
			{
				connect_documents_button_press_signal_handler(AO_COLORTIP(object));
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


void ao_color_tip_document_new(AoColorTip *colortip, GeanyDocument *document)
{
	connect_document_button_press_signal_handler(colortip, document);
	SSM(document->editor->sci, SCI_SETMOUSEDWELLTIME, 300, 0);
}


void ao_color_tip_document_open(AoColorTip *colortip, GeanyDocument *document)
{
	connect_document_button_press_signal_handler(colortip, document);
	SSM(document->editor->sci, SCI_SETMOUSEDWELLTIME, 300, 0);
}


void ao_color_tip_document_close(AoColorTip *colortip, GeanyDocument *document)
{
	g_return_if_fail(DOC_VALID(document));

	g_signal_handlers_disconnect_by_func(document->editor->sci, on_editor_button_press_event, colortip);
}


static void ao_color_tip_class_init(AoColorTipClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_color_tip_finalize;
	g_object_class->set_property = ao_color_tip_set_property;
	g_type_class_add_private(klass, sizeof(AoColorTipPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_COLORTIP,
									g_param_spec_boolean(
									"enable-colortip",
									"enable-colortip",
									"Whether to show a calltip when hovering over a color value",
									TRUE,
									G_PARAM_WRITABLE));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_DOUBLE_CLICK_COLOR_CHOOSER,
									g_param_spec_boolean(
									"enable-double-click-color-chooser",
									"enable-double-click-color-chooser",
									"Enable starting the Color Chooser when double clicking on a color value",
									TRUE,
									G_PARAM_WRITABLE));
}

static void ao_color_tip_init(AoColorTip *self)
{
	AoColorTipPrivate *priv = AO_COLORTIP_GET_PRIVATE(self);
	memset(priv, 0, sizeof(*priv));
}

AoColorTip *ao_color_tip_new(gboolean enable_tip, gboolean double_click_color_chooser)
{
	return g_object_new(
		AO_COLORTIP_TYPE,
		"enable-colortip", enable_tip,
		"enable-double-click-color-chooser", double_click_color_chooser,
		NULL);
}
