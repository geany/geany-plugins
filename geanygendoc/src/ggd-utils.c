/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2009-2010  Colomban Wendling <ban@herbesfolles.org>
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


#include "ggd-utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h> /* for BUFSIZ */
#include <glib.h>
#include <gio/gio.h> /* for G_FILE_ERROR and friends */
#include <geanyplugin.h>

#include "ggd-plugin.h" /* to access Geany data/funcs */


/*
 * set_file_error_from_errno:
 * @error: A #GError
 * @errnum: An errno value
 * @filename: The file name for which the error applies
 * 
 * Sets a #GError from an errno value, prefixed with a file's name.
 */
#define set_file_error_from_errno(error, errnum, filename)                     \
  G_STMT_START {                                                               \
    gint s_e_f_e_errum = errnum; /* need if @errnum is errno */                \
                                                                               \
    g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (s_e_f_e_errum), \
                 "%s: %s", filename, g_strerror (s_e_f_e_errum));              \
  } G_STMT_END

/*
 * ggd_copy_file:
 * @input: Path of the file to copy
 * @output: Path of the destination
 * @exclusive: %FALSE to override the destination if it already exist, %TRUE
 *             otherwise
 * @mode: Mode to use for creating the file
 * @error: Return location for errors, or %NULL to ignore them
 * 
 * Copies a file to a destination. If @exlusive is %TRUE, the destination is not
 * overwritten if it exists in a safe way. Otherwise, the destination file is
 * simply truncated before the copy, with all the problems that can happen (such
 * as partially overwritten file and so).
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static gboolean
ggd_copy_file (const gchar *input,
               const gchar *output,
               gboolean     exclusive,
               mode_t       mode,
               GError     **error)
{
  gboolean  success = FALSE;
  gint      fd_in;
  
  fd_in = open (input, O_RDONLY);
  if (fd_in < 0) {
    set_file_error_from_errno (error, errno, input);
  } else {
    gint fd_out;
    gint flags_out;
    
    flags_out = O_WRONLY | O_CREAT | O_TRUNC;
    if (exclusive) flags_out |= O_EXCL;
    fd_out = open (output, flags_out, mode);
    if (fd_out < 0) {
      set_file_error_from_errno (error, errno, output);
    } else {
      char    buf[BUFSIZ];
      size_t  buf_size = sizeof buf;
      ssize_t size_in;
      ssize_t size_out;
      
      success = TRUE;
      do {
        size_in = read (fd_in, buf, buf_size);
        if (size_in < 0) {
          set_file_error_from_errno (error, errno, input);
          success = FALSE;
        } else {
          size_out = write (fd_out, buf, (size_t)size_in);
          if (size_out < 0) {
            set_file_error_from_errno (error, errno, output);
            success = FALSE;
          } else if (size_out < size_in) {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "%s: failed to write %"G_GSIZE_FORMAT" bytes "
                         "(read %"G_GSIZE_FORMAT", wrote %"G_GSIZE_FORMAT")",
                         input, size_in - size_out, size_in, size_out);
            success = FALSE;
          }
        }
      } while (success && (size_t)size_in >= buf_size);
      close (fd_out);
    }
    close (fd_in);
  }
  
  return success;
}

/**
 * ggd_get_config_file:
 * @name: The name of the configuration file to get
 * @section: The name of the configuration section of the file, or %NULL for the
 *           default one
 * @perms_req: Requested permissions on the configuration file
 * @error: Return location for errors, or %NULL to ignore them
 * 
 * Gets the configuration file path from its name.
 * Configuration files may be either the system-wide or the user-specific ones,
 * depending on their existence and on the requested permissions.
 * 
 * If @GGD_PERM_NOCREAT is not given in @perms_req and @GGD_PERM_W is, the file
 * at the returned path will be copied from the system configuration directory,
 * or created empty if the system file doesn't exist.
 * 
 * Returns: The path for the requested configuration file or %NULL if not found.
 */
gchar *
ggd_get_config_file (const gchar *name,
                     const gchar *section,
                     GgdPerms     perms_req,
                     GError     **error)
{
  gchar  *path = NULL;
  gchar  *user_dir;
  gchar  *user_path;
  gchar  *system_dir;
  gchar  *system_path;
  
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
  user_dir = g_build_filename (geany->app->configdir, "plugins",
                               GGD_PLUGIN_CNAME, section, NULL);
  system_dir = g_build_filename (PKGDATADIR, GGD_PLUGIN_CNAME, section, NULL);
  user_path = g_build_filename (user_dir, name, NULL);
  system_path = g_build_filename (system_dir, name, NULL);
  if (perms_req & GGD_PERM_R) {
    if (g_file_test (user_path, G_FILE_TEST_EXISTS)) {
      if (! g_file_test (user_path, G_FILE_TEST_IS_REGULAR)) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("File \"%s\" exists but is not a regular file"),
                     user_path);
      } else {
        path = user_path;
      }
    }
    if (! path) {
      if (g_file_test (system_path, G_FILE_TEST_EXISTS)) {
        if (! g_file_test (system_path, G_FILE_TEST_IS_REGULAR)) {
          g_clear_error (error);
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("File \"%s\" exists but is not a regular file"),
                       system_path);
        } else {
          path = system_path;
        }
      }
    }
    if (! path && error && ! *error) {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   _("%s: no such file or directory"), user_path);
    }
  }
  if (perms_req & GGD_PERM_W) {
    if (path == user_path) {
      /* nothing to do, write should succeed on user's path */
    } else {
      path = NULL;
      if (g_mkdir_with_parents (user_dir, 0750) < 0) {
        gint errnum = errno;
        
        g_clear_error (error);
        set_file_error_from_errno (error, errnum, user_dir);
      } else if (perms_req & GGD_PERM_NOCREAT) {
        /* just give the user path if user don't want the copy to be done */
        path = user_path;
      } else {
        GError *gerr = NULL;
        
        /* try to copy the system file to the user's configuration directory */
        if (ggd_copy_file (system_path, user_path, TRUE, 0640, &gerr) ||
            /* the file already exists (unlikely if GGD_PERMS_R is set) */
            gerr->code == G_FILE_ERROR_EXIST) {
          path = user_path;
          if (gerr) g_clear_error (&gerr);
          g_clear_error (error);
        } else if (gerr->code == G_FILE_ERROR_NOENT) {
          /* the system file doesn't exist. No problem, just try to create the
           * file (if it does not already exist) */
          gint fd;
          
          g_clear_error (&gerr);
          fd = open (user_path, O_CREAT | O_WRONLY, 0640);
          if (fd < 0) {
            set_file_error_from_errno (&gerr, errno, user_path);
          } else {
            close (fd);
            path = user_path;
            g_clear_error (error);
          }
        }
        if (gerr) {
          g_clear_error (error);
          g_propagate_error (error, gerr);
        }
      }
    }
  }
  if (path != user_path) g_free (user_path);
  if (path != system_path) g_free (system_path);
  g_free (user_dir);
  g_free (system_dir);
  
  return path;
}
