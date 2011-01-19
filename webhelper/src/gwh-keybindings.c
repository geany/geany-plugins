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

#include "gwh-keybindings.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <geanyplugin.h>
#include <geany.h>

#include "gwh-plugin.h"


static GeanyKeyGroup *G_key_group;


void
gwh_keybindings_init (void)
{
  G_key_group = plugin_set_key_group (geany_plugin, "webhelper", GWH_KB_COUNT,
                                      NULL);
}

void
gwh_keybindings_cleanup (void)
{
  G_key_group = NULL;
}

GeanyKeyGroup *
gwh_keybindings_get_group (void)
{
  return G_key_group;
}

/**
 * ghw_keybindings_handle_event:
 * @widget: Unused, may be %NULL
 * @event: A GdkEventKey to handle
 * @data: Unused, may be %NULL
 * 
 * Handles a GDK key event and calls corresponding keybindings callbacks.
 * This function is meant to be used as the callback for a
 * GtkWidget:key-press-event signal.
 * 
 * Returns: Whether the event was handled or not
 */
gboolean
gwh_keybindings_handle_event (GtkWidget    *widget,
                              GdkEventKey  *event,
                              gpointer      data)
{
  guint     mods = event->state & gtk_accelerator_get_default_mod_mask ();
  gboolean  handled = FALSE;
  guint     keyval = event->keyval;
  guint     i;
  
  if ((event->state & GDK_SHIFT_MASK) || (event->state & GDK_LOCK_MASK)) {
    keyval = gdk_keyval_to_lower (keyval);
  }
  for (i = 0; ! handled && i < GWH_KB_COUNT; i++) {
    GeanyKeyBinding *kb;
    
    kb = keybindings_get_item (G_key_group, i);
    if (kb->key == keyval && kb->mods == mods) {
      if (kb->callback) {
        kb->callback (i);
      /* We can't handle key group callback since we can't acces key group
       * fields. However, we don't use it so it's not a real problem. */
      /*} else if (G_key_group->callback) {
        G_key_group->callback (i);*/
      }
      handled = TRUE;
    }
  }
  
  return handled;
}
