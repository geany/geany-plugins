/*
 *      callbacks.h
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data);
void on_document_activate(GObject *obj, GeanyDocument *doc, gpointer user_data);
void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data);
void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data);
void on_document_new(GObject *obj, GeanyDocument *doc, gpointer user_data);
gboolean on_editor_notify(GObject *object, GeanyEditor *editor, SCNotification *nt, gpointer data);
gboolean keys_callback(guint key_id);
