/*
 *      prjorg- wraplabel.h - renamed copy of geanywraplabel.h from Geany
 *
 *      Copyright 2009 The Geany contributors
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PRJORG_WRAP_LABEL_H
#define PRJORG_WRAP_LABEL_H 1

#include "gtkcompat.h"

G_BEGIN_DECLS


#define PRJORG_WRAP_LABEL_TYPE				(prjorg_wrap_label_get_type())
#define PRJORG_WRAP_LABEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), \
	PRJORG_WRAP_LABEL_TYPE, PrjorgWrapLabel))
#define PRJORG_WRAP_LABEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
	PRJORG_WRAP_LABEL_TYPE, PrjorgWrapLabelClass))
#define IS_PRJORG_WRAP_LABEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
	PRJORG_WRAP_LABEL_TYPE))
#define IS_PRJORG_WRAP_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
	PRJORG_WRAP_LABEL_TYPE))


typedef struct _PrjorgWrapLabel       PrjorgWrapLabel;
typedef struct _PrjorgWrapLabelClass  PrjorgWrapLabelClass;

GType			prjorg_wrap_label_get_type			(void);
GtkWidget*		prjorg_wrap_label_new				(const gchar *text);


G_END_DECLS

#endif /* PRJORG_WRAP_LABEL_H */
