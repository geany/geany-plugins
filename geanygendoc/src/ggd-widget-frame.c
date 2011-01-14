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

/*
 * A GtkFrame subclass that reproduce Glade's frames.
 */

#include <gtk/gtk.h>

#include "ggd-widget-frame.h"


struct _GgdFramePrivate
{
  PangoAttrList  *label_attr_list;
  GtkWidget      *alignment;
};


G_DEFINE_TYPE (GgdFrame, ggd_frame, GTK_TYPE_FRAME)


static void
ggd_frame_add (GtkContainer *container,
               GtkWidget    *child)
{
  GgdFrame *self = GGD_FRAME (container);
  
  /* chain additions to the alignment if we aren't adding it */
  if (child == self->priv->alignment) {
    GTK_CONTAINER_CLASS (ggd_frame_parent_class)->add (container, child);
  } else {
    container = GTK_CONTAINER (self->priv->alignment);
    GTK_CONTAINER_GET_CLASS (container)->add (container, child);
  }
}

static void
ggd_frame_finalize (GObject *object)
{
  GgdFrame *self = GGD_FRAME (object);
  
  pango_attr_list_unref (self->priv->label_attr_list);
  
  G_OBJECT_CLASS (ggd_frame_parent_class)->finalize (object);
}

static void
ggd_frame_class_init (GgdFrameClass *klass)
{
  GObjectClass       *object_class    = G_OBJECT_CLASS (klass);
  GtkContainerClass  *container_class = GTK_CONTAINER_CLASS (klass);
  
  object_class->finalize  = ggd_frame_finalize;
  container_class->add    = ggd_frame_add;
  
  g_type_class_add_private (klass, sizeof (GgdFramePrivate));
}

static void
ggd_frame_label_notify (GObject     *object,
                        GParamSpec  *pspec,
                        gpointer     data)
{
  GgdFrame   *self = GGD_FRAME (object);
  GtkWidget  *label;
  
  label = gtk_frame_get_label_widget (GTK_FRAME (self));
  if (label) {
    gtk_label_set_attributes (GTK_LABEL (label), self->priv->label_attr_list);
  }
}

static void
ggd_frame_init (GgdFrame *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GGD_TYPE_FRAME,
                                            GgdFramePrivate);
  
  /* set-up the frame attributes */
  gtk_frame_set_shadow_type (GTK_FRAME (self), GTK_SHADOW_NONE);
  /* set-up fake child */
  self->priv->alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT(self->priv->alignment), 0, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (self), self->priv->alignment);
  gtk_widget_show (self->priv->alignment);
  /* set-up the label's attributes */
  self->priv->label_attr_list = pango_attr_list_new ();
  pango_attr_list_insert (self->priv->label_attr_list,
                          pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  
  g_signal_connect (self, "notify::label",
                    G_CALLBACK (ggd_frame_label_notify), NULL);
}


GtkWidget *
ggd_frame_new (const gchar *label)
{
  return g_object_new (GGD_TYPE_FRAME, "label", label, NULL);
}
