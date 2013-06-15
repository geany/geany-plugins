/*
 * geanypy.h
 *
 * Copyright 2011 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA.
 *
 */

#ifndef GEANYPY_H__
#define GEANYPY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* For plain make file build using Mingw on Windows */
#if defined(__MINGW32__) && defined(GEANYPY_WINDOWS_BUILD)
#  define GEANYPY_WINDOWS 1
#endif

#include <Python.h>
#ifndef PyMODINIT_FUNC
#  define PyMODINIT_FUNC void
#endif
#include <structmember.h>


/* Defines a setter that throws an attribute exception when called. */
#define GEANYPY_PROPS_READONLY(cls) \
	static int \
	cls ## _set_property(cls *self, PyObject *value, const gchar *prop_name) { \
		PyErr_SetString(PyExc_AttributeError, "can't set attribute"); \
		return -1; }

/* Defines a getter/setter for use in the tp_getset table. */
#define GEANYPY_GETSETDEF(cls, prop_name, doc) \
	{ prop_name, \
		(getter) cls ## _get_property, \
		(setter) cls ## _set_property, \
		doc, \
		prop_name }


/* Initializes a new `cls` type object. */
#define GEANYPY_NEW(cls) \
	(cls *) PyObject_CallObject((PyObject *) &(cls ## Type), NULL);


/* Returns a new py string or py none if string is NULL. */
#define GEANYPY_RETURN_STRING(memb) \
	{ \
		if (memb != NULL) \
			return PyString_FromString(memb); \
		else \
			Py_RETURN_NONE; \
	}


#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>
#include <pygobject.h>

#ifndef GEANYPY_WINDOWS
/* Used with the results of `pkg-config --cflags pygtk-2.0` */
#  include <pygtk/pygtk.h>
#else
/* On windows the path of pygtk.h is directly an include dir */
#  include <pygtk.h>
#endif

#ifndef GTK
#  define GTK
#endif
#include <Scintilla.h>
#include <ScintillaWidget.h>

#include <geanyplugin.h>

#ifndef G_LOG_DOMAIN
#  define G_LOG_DOMAIN "GeanyPy"
#endif

#ifndef GEANYPY_WINDOWS
#  include "config.h"
#endif

#include "geanypy-document.h"
#include "geanypy-editor.h"
#include "geanypy-encoding.h"
#include "geanypy-filetypes.h"
#include "geanypy-plugin.h"
#include "geanypy-project.h"
#include "geanypy-scintilla.h"
#include "geanypy-signalmanager.h"
#include "geanypy-uiutils.h"


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GEANYPY_H__ */
