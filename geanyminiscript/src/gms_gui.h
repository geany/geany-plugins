/*
 * gms_gui.h: miniscript plugin for geany editor
 *            Geany, a fast and lightweight IDE
 *
 * Copyright 2008 Pascal BURLOT <prublot(at)users(dot)sourceforge(dot)net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#ifndef GMS_GUI_H
#define GMS_GUI_H

#define GMS_HANDLE(p) ((gms_handle_t *) p)
typedef void* gms_handle_t ;

typedef enum {
    IN_SELECTION    =0,
    IN_CURRENT_DOC  =1,
    IN_DOCS_SESSION =3
} gms_input_t ;

typedef enum {
    OUT_CURRENT_DOC    =0,
    OUT_NEW_DOC        =1
} gms_output_t ;

gms_handle_t gms_new(  GtkWidget *mw, gchar *font, gint tabs, gchar *config_dir);
void        gms_delete( gms_handle_t *hnd );
int         gms_dlg( gms_handle_t hnd ) ;
gchar       *gms_get_in_filename( gms_handle_t hnd ) ;
gchar       *gms_get_out_filename( gms_handle_t hnd ) ;
gchar       *gms_get_filter_filename( gms_handle_t hnd ) ;
gchar       *gms_get_error_filename( gms_handle_t hnd ) ;
void        gms_create_filter_file( gms_handle_t hnd ) ;
gchar       *gms_get_str_command( gms_handle_t hnd ) ;
gms_input_t  gms_get_input_mode( gms_handle_t hnd );
gms_output_t gms_get_output_mode( gms_handle_t hnd );

GtkWidget   *gms_configure_gui( gms_handle_t hnd );
void        on_gms_configure_response( GtkDialog *dialog, gint response, gpointer user_data) ;

#endif /* GMS_GUI_H */

