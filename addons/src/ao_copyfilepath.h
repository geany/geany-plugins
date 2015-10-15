/*
 *      ao_copyfilepath.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2015 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 */


#ifndef __AO_COPYFILEPATH_H__
#define __AO_COPYFILEPATH_H__

G_BEGIN_DECLS

#define AO_COPY_FILE_PATH_TYPE				(ao_copy_file_path_get_type())
#define AO_COPY_FILE_PATH(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			AO_COPY_FILE_PATH_TYPE, AoCopyFilePath))
#define AO_COPY_FILE_PATH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			AO_COPY_FILE_PATH_TYPE, AoCopyFilePathClass))
#define IS_AO_COPY_FILE_PATH(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			AO_COPY_FILE_PATH_TYPE))
#define IS_AO_COPY_FILE_PATH_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			AO_COPY_FILE_PATH_TYPE))

typedef struct _AoCopyFilePath			AoCopyFilePath;
typedef struct _AoCopyFilePathClass		AoCopyFilePathClass;


GType			ao_copy_file_path_get_type		(void);
AoCopyFilePath*	ao_copy_file_path_new			(void);
void 			ao_copy_file_path_copy			(AoCopyFilePath *self);
GtkWidget* 		ao_copy_file_path_get_menu_item	(AoCopyFilePath *self);

G_END_DECLS

#endif /* __AO_COPYFILEPATH_H__ */
