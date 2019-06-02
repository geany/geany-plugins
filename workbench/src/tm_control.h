/*
 * Copyright 2017 LarsGit223
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __WB_TM_CONTROL_H__
#define __WB_TM_CONTROL_H__

void wb_tm_control_init (void);
void wb_tm_control_cleanup (void);
void wb_tm_control_source_file_add(const gchar *filename);
void wb_tm_control_source_file_remove(const gchar *filename);
void wb_tm_control_source_files_remove(GPtrArray *files);
void wb_tm_control_source_files_new(GPtrArray *files);

void wb_tm_control_source_file_free(TMSourceFile *source_file);

#endif
