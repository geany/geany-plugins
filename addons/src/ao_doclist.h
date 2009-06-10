/*
 *      ao_doclist.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef __AO_DOCLIST_H__
#define __AO_DOCLIST_H__


G_BEGIN_DECLS

#define AO_DOC_LIST_TYPE				(ao_doc_list_get_type())
#define AO_DOC_LIST(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			AO_DOC_LIST_TYPE, AoDocList))
#define AO_DOC_LIST_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			AO_DOC_LIST_TYPE, AoDocListClass))
#define IS_AO_DOC_LIST(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			AO_DOC_LIST_TYPE))
#define IS_AO_DOC_LIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			AO_DOC_LIST_TYPE))

typedef struct _AoDocList			AoDocList;
typedef struct _AoDocListClass		AoDocListClass;


GType		ao_doc_list_get_type		(void);
AoDocList*	ao_doc_list_new				(gboolean enable);

G_END_DECLS

#endif /* __AO_DOCLIST_H__ */
