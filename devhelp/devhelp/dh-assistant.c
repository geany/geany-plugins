/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "dh-window.h"
#include "dh-util.h"
#include "dh-assistant-view.h"
#include "dh-assistant.h"

typedef struct {
        GtkWidget *main_box;
        GtkWidget *view;
} DhAssistantPriv;

static void dh_assistant_class_init (DhAssistantClass *klass);
static void dh_assistant_init       (DhAssistant      *assistant);

G_DEFINE_TYPE (DhAssistant, dh_assistant, GTK_TYPE_WINDOW);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE \
  (instance, DH_TYPE_ASSISTANT, DhAssistantPriv)

static gboolean
assistant_key_press_event_cb (GtkWidget   *widget,
                              GdkEventKey *event,
                              DhAssistant *assistant)
{
        if (event->keyval == GDK_Escape) {
                gtk_widget_destroy (GTK_WIDGET (assistant));
                return TRUE;
        }

        return FALSE;
}

static void
dh_assistant_class_init (DhAssistantClass *klass)
{
        g_type_class_add_private (klass, sizeof (DhAssistantPriv));
}

static void
dh_assistant_init (DhAssistant *assistant)
{
        DhAssistantPriv *priv = GET_PRIVATE (assistant);
        GtkWidget       *scrolled_window;

        priv->main_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (priv->main_box);
        gtk_container_add (GTK_CONTAINER (assistant), priv->main_box);

        /* i18n: Please don't translate "Devhelp". */
        gtk_window_set_title (GTK_WINDOW (assistant), _("Devhelp — Assistant"));
        gtk_window_set_icon_name (GTK_WINDOW (assistant), "devhelp");

        priv->view = dh_assistant_view_new ();

        g_signal_connect (assistant, "key-press-event",
                          G_CALLBACK (assistant_key_press_event_cb),
                          assistant);

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        gtk_container_add (GTK_CONTAINER (scrolled_window), priv->view);

        gtk_widget_show_all (scrolled_window);

        gtk_box_pack_start (GTK_BOX (priv->main_box),
                            scrolled_window, TRUE, TRUE, 0);

        dh_util_state_manage_window (GTK_WINDOW (assistant),
                                     "assistant/window");
}

GtkWidget *
dh_assistant_new (DhBase *base)
{
        GtkWidget       *assistant;
        DhAssistantPriv *priv;

        assistant = g_object_new (DH_TYPE_ASSISTANT, NULL);

        priv = GET_PRIVATE (assistant);

        dh_assistant_view_set_base (DH_ASSISTANT_VIEW (priv->view), base);

        return assistant;
}

gboolean
dh_assistant_search (DhAssistant *assistant,
                     const gchar *str)
{
        DhAssistantPriv *priv;

        g_return_val_if_fail (DH_IS_ASSISTANT (assistant), FALSE);
        g_return_val_if_fail (str != NULL, FALSE);

        priv = GET_PRIVATE (assistant);

        if (dh_assistant_view_search (DH_ASSISTANT_VIEW (priv->view), str)) {
                gtk_widget_show (GTK_WIDGET (assistant));
                return TRUE;
        }

        return FALSE;
}
