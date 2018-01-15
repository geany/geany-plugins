/*
 *      ao_colortip.h - this file is part of Addons, a Geany plugin
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


#ifndef __AO_COLORTIP_H__
#define __AO_COLORTIP_H__

G_BEGIN_DECLS

#define AO_COLORTIP_TYPE				(ao_color_tip_get_type())
#define AO_COLORTIP(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			AO_COLORTIP_TYPE, AoColorTip))
#define AO_COLORTIP_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			AO_COLORTIP_TYPE, AoColorTipClass))
#define IS_AO_COLORTIP(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			AO_COLORTIP_TYPE))
#define IS_AO_COLORTIP_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
			AO_COLORTIP_TYPE))

typedef struct _AoColorTip				AoColorTip;
typedef struct _AoColorTipClass			AoColorTipClass;

GType		ao_color_tip_get_type		(void);
AoColorTip*	ao_color_tip_new			(gboolean enable_tip, gboolean double_click_color_chooser);
void		ao_color_tip_document_new		(AoColorTip *colortip, GeanyDocument *document);
void		ao_color_tip_document_open		(AoColorTip *colortip, GeanyDocument *document);
void		ao_color_tip_document_close		(AoColorTip *colortip, GeanyDocument *document);
void		ao_color_tip_editor_notify	(AoColorTip *colortip, GeanyEditor *editor, SCNotification *nt);

G_END_DECLS

#endif /* __AO_COLORTIP_H__ */
