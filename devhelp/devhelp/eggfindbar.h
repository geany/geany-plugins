/* Copyright (C) 2004 Red Hat, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the Gnome Library; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#ifndef __EGG_FIND_BAR_H__
#define __EGG_FIND_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_FIND_BAR            (egg_find_bar_get_type ())
#define EGG_FIND_BAR(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), EGG_TYPE_FIND_BAR, EggFindBar))
#define EGG_FIND_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_FIND_BAR, EggFindBarClass))
#define EGG_IS_FIND_BAR(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), EGG_TYPE_FIND_BAR))
#define EGG_IS_FIND_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_FIND_BAR))
#define EGG_FIND_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_FIND_BAR, EggFindBarClass))

typedef struct _EggFindBar        EggFindBar;
typedef struct _EggFindBarClass   EggFindBarClass;
typedef struct _EggFindBarPrivate EggFindBarPrivate;

struct _EggFindBar
{
  GtkToolbar parent;

  /*< private >*/
  EggFindBarPrivate *priv;
};

struct _EggFindBarClass
{
  GtkToolbarClass parent_class;

  void (* next)	    (EggFindBar *find_bar);
  void (* previous) (EggFindBar *find_bar);
  void (* close)    (EggFindBar *find_bar);
  void (* scroll)   (EggFindBar *find_bar, GtkScrollType* scroll);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType                  egg_find_bar_get_type               (void) G_GNUC_CONST;
GtkWidget             *egg_find_bar_new                    (void);

void        egg_find_bar_set_search_string       (EggFindBar *find_bar,
                                                  const char *search_string);
const char* egg_find_bar_get_search_string       (EggFindBar *find_bar);
void        egg_find_bar_set_case_sensitive      (EggFindBar *find_bar,
                                                  gboolean    case_sensitive);
gboolean    egg_find_bar_get_case_sensitive      (EggFindBar *find_bar);
void        egg_find_bar_get_all_matches_color   (EggFindBar *find_bar,
                                                  GdkColor   *color);
void        egg_find_bar_get_current_match_color (EggFindBar *find_bar,
                                                  GdkColor   *color);
void        egg_find_bar_set_status_text         (EggFindBar *find_bar,
                                                  const char *text);

G_END_DECLS

#endif /* __EGG_FIND_BAR_H__ */


