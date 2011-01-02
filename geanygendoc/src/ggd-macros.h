/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#ifndef H_GGD_MACROS
#define H_GGD_MACROS


#if defined (__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
# define __GGD_DO_PRAGMA(s) _Pragma (#s)
/**
 * GGD_PUSH_VISIBILITY:
 * @v: The visibility to push (one of default, hidden, internal or protected)
 * 
 * Pushes a visibility, applying to the following symbols until the next
 * GGD_POP_VISIBILITY().
 * You must have a corresponding GGD_POP_VISIBILITY() to each
 * GGD_PUSH_VISIBILITY().
 */
# define GGD_PUSH_VISIBILITY(v) __GGD_DO_PRAGMA (GCC visibility push(v))
/**
 * GGD_POP_VISIBILITY:
 * 
 * Pops the latest visibility pushed by GGD_PUSH_VISIBILITY().
 */
# define GGD_POP_VISIBILITY()   __GGD_DO_PRAGMA (GCC visibility pop)
#else /* ! GNUC >= 4.2 */
# define GGD_PUSH_VISIBILITY(v) /* nothing */
# define GGD_POP_VISIBILITY()   /* nothing */
#endif /* GNUC >= 4.2 */

/**
 * GGD_BEGIN_PLUGIN_API:
 * 
 * Marks the beginning of some plugin API definitions.
 * In practice, this makes the symbols have the internal visibility -- not to
 * pollute the symbol table of the built plugin for example.
 * You must add a corresponding %GGD_END_PLUGIN_API to end the declarations.
 * 
 * <warning><para>
 * You must not include anything that must be accessible from outside the plugin
 * between %GGD_BEGIN_PLUGIN_API and %GGD_END_PLUGIN_API. Doing so would lead to
 * strange crashes since the symbols are not exported at all. An example are the
 * Geany's variables such as geany_functions, since Geany will need to access it
 * at runtime.
 * </para></warning>
 * 
 * <example>
 *  <title>Defining some plugin API</title>
 *  <programlisting>
 * #include <glib.h>
 * 
 * #include "ggd-macros.h"
 * 
 * G_BEGIN_DECLS
 * GGD_BEGIN_PLUGIN_API
 * 
 * gint ggd_wonderful_function (void);
 * 
 * GGD_END_PLUGIN_API
 * G_END_DECLS
 *  </programlisting>
 * </example>
 */
#define GGD_BEGIN_PLUGIN_API  GGD_PUSH_VISIBILITY (internal)
/**
 * GGD_END_PLUGIN_API:
 * 
 * Marks the end of some plugin API definitions. See %GGD_BEGIN_PLUGIN_API.
 */
#define GGD_END_PLUGIN_API    GGD_POP_VISIBILITY ()


#endif /* guard */
