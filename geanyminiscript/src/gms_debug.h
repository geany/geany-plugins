/*
 * gms_debug.h: miniscript plugin for geany editor
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

/*******************************************************************************
 * \file  gms_debug.h
 * \brief macros used for debugging
 */

#ifndef GMS_DEBUG_H
#define GMS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

//! \brief Macro to glib allocation (glib)
#define GMS_G_MALLOC(t,n)       ((t *) g_malloc(sizeof(t)*n))
//! \brief Macro to glib allocation (glib) with zero filling
#define GMS_G_MALLOC0(t,n)      ((t *) g_malloc0(sizeof(t)*n))

//! \brief Macro to free (glib)
#define GMS_G_FREE(p)   if ( p != NULL ) { g_free(p) ; p=NULL ; }

//! \brief Macro pour la liberation des PangoFontDescription
#define GMS_FREE_FONTDESC(p) if ( p != NULL ) { pango_font_description_free (p) ;p=NULL ; }

//! \brief Macro for freeing a GtkWidget structure
#define GMS_FREE_WIDGET(p)   if ( p != NULL ) { gtk_widget_destroy(GTK_WIDGET(p)) ;p=NULL ; }


#ifdef GMS_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define GMS_CALL(f) \
                                                                    \
  if (((int)(f)) == -1)                                             \
  {                                                                 \
    int err = errno;                                                \
    char *strerr = strerror(err);                                   \
                                                                    \
    fprintf(stderr,                                                 \
            "GMS_CALL(%s): Error at line %u, file %s: %s (errno=%d)\n", \
            #f, __LINE__, __FILE__,                                 \
            strerr == NULL ? "Bad error number" : strerr,           \
            err);                                                   \
    abort();                                                        \
  }

#define GMS_PNULL(pp) \
                                                                    \
  if ( (pp) == NULL)                                                \
  {                                                                 \
    fprintf(stderr,                                                 \
            "GMS_PNULL(%s): null pointer => %s at line %u, file %s: \n",\
            #pp, __FUNCTION__, __LINE__, __FILE__ );                \
    abort();                                                        \
  }

#define GMS_FOPEN(pp,...)                                            \
  pp=fopen(__VA_ARGS__) ;                                           \
  if ( (pp) == NULL)                                                \
  {                                                                 \
    int err = errno;                                                \
    char *strerr = strerror(err);                                   \
                                                                    \
    fprintf(stderr,                                                 \
            "%s=fopen%s: \n\tError at line %u, file %s: %s (errno=%d)\n",\
            #pp, #__VA_ARGS__, __LINE__, __FILE__ ,                         \
            strerr == NULL ? "Bad error number" : strerr,           \
            err);                                                   \
    abort();                                                        \
  }

#define GMS_ASSERT(cc)                                    \
                                                         \
  if (!(cc))                                             \
  {                                                      \
    fprintf(stderr,                                      \
            "GMS_ASSERT(%s): %s at line %u, file %s\n",   \
            #cc,  __FUNCTION__, __LINE__, __FILE__);     \
    abort();                                             \
  }

#define GMS_WARNING(cc)                                   \
                                                         \
  if ((cc))                                              \
  {                                                      \
    fprintf(stderr,                                      \
            "GMS_WARNING(%s): %s at line %u, file %s\n",  \
            #cc, __FUNCTION__, __LINE__, __FILE__);      \
  }

#define GMS_FINFO(fd,...) fprintf(fd, "INFO:"__VA_ARGS__)
#define GMS_INFO(...)     printf("INFO:"__VA_ARGS__)

#else

#define GMS_CALL(f)  (f)
#define GMS_ASSERT(cc)
#define GMS_WARNING(cc)
#define GMS_PNULL(pp)
#define GMS_FOPEN(pp,...)   pp=fopen(__VA_ARGS__)
#define GMS_FINFO(fd,...)
#define GMS_INFO(...)

#endif


#ifdef __cplusplus
}
#endif

#endif /* GMS_DEBUG_H */
