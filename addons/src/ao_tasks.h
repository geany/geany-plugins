/*
 *      ao_tasks.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef __AO_TASKS_H__
#define __AO_TASKS_H__

G_BEGIN_DECLS

#define AO_TASKS_TYPE				(ao_tasks_get_type())
#define AO_TASKS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), AO_TASKS_TYPE, AoTasks))
#define AO_TASKS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), AO_TASKS_TYPE, AoTasksClass))
#define IS_AO_TASKS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), AO_TASKS_TYPE))
#define IS_AO_TASKS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), AO_TASKS_TYPE))

typedef struct _AoTasks			AoTasks;
typedef struct _AoTasksClass	AoTasksClass;

GType			ao_tasks_get_type		(void);
AoTasks*		ao_tasks_new			(gboolean enable,
										 const gchar *tokens,
										 gboolean scan_all_documents);
void			ao_tasks_update			(AoTasks *t, GeanyDocument *cur_doc);
void			ao_tasks_update_single	(AoTasks *t, GeanyDocument *cur_doc);
void			ao_tasks_remove			(AoTasks *t, GeanyDocument *cur_doc);
void			ao_tasks_activate		(AoTasks *t);
void			ao_tasks_set_active		(AoTasks *t);

G_END_DECLS

#endif /* __AO_TASKS_H__ */
