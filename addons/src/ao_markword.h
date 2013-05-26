/*
 *      ao_markword.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *
 * $Id$
 */


#ifndef __AO_MARKWORD_H__
#define __AO_MARKWORD_H__

G_BEGIN_DECLS

#define AO_MARKWORD_TYPE				(ao_mark_word_get_type())
#define AO_MARKWORD(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			AO_MARKWORD_TYPE, AoMarkWord))
#define AO_MARKWORD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			AO_MARKWORD_TYPE, AoMarkWordClass))
#define IS_AO_MARKWORD(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			AO_MARKWORD_TYPE))
#define IS_AO_MARKWORD_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
			AO_MARKWORD_TYPE))

typedef struct _AoMarkWord				AoMarkWord;
typedef struct _AoMarkWordClass			AoMarkWordClass;

typedef enum
{
	MARKWORD_BY_DBLCLICK = 1,
	MARKWORD_BY_SELECTION = 2
} MarkWordMode;

GType			ao_mark_word_get_type		(void);
AoMarkWord*		ao_mark_word_new			(gboolean enable, MarkWordMode markword_mode);
void			ao_mark_word_check			(AoMarkWord *bm, GeanyEditor *editor,
											 SCNotification *nt);

G_END_DECLS

#endif /* __AO_MARKWORD_H__ */
