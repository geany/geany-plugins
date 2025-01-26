/*
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright 2023 Jiri Techet <techet@gmail.com>
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

// stolen from glib (so it can be used under windows too), removed pollable
// and descriptor based interface implementations

#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "spawn/lspunixoutputstream.h"


/**
 * SECTION:gunixoutputstream
 * @short_description: Streaming output operations for UNIX file descriptors
 * @include: gio/gunixoutputstream.h
 * @see_also: #GOutputStream
 *
 * #LspUnixOutputStream implements #GOutputStream for writing to a UNIX
 * file descriptor, including asynchronous operations. (If the file
 * descriptor refers to a socket or pipe, this will use poll() to do
 * asynchronous I/O. If it refers to a regular file, it will fall back
 * to doing asynchronous I/O in another thread.)
 *
 * Note that `<gio/gunixoutputstream.h>` belongs to the UNIX-specific GIO
 * interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config file
 * when using it.
 */

enum {
  PROP_0,
  PROP_FD,
  PROP_CLOSE_FD
};

struct _LspUnixOutputStreamPrivate {
  int fd;
  guint close_fd : 1;
  guint can_poll : 1;
};


G_DEFINE_TYPE_WITH_CODE (LspUnixOutputStream, lsp_unix_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (LspUnixOutputStream)
			 )

static void     lsp_unix_output_stream_set_property (GObject              *object,
						   guint                 prop_id,
						   const GValue         *value,
						   GParamSpec           *pspec);
static void     lsp_unix_output_stream_get_property (GObject              *object,
						   guint                 prop_id,
						   GValue               *value,
						   GParamSpec           *pspec);
static gssize   lsp_unix_output_stream_write        (GOutputStream        *stream,
						   const void           *buffer,
						   gsize                 count,
						   GCancellable         *cancellable,
						   GError              **error);
static gboolean lsp_unix_output_stream_close        (GOutputStream        *stream,
						   GCancellable         *cancellable,
						   GError              **error);

static void
lsp_unix_output_stream_class_init (LspUnixOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

  gobject_class->get_property = lsp_unix_output_stream_get_property;
  gobject_class->set_property = lsp_unix_output_stream_set_property;

  stream_class->write_fn = lsp_unix_output_stream_write;
  stream_class->writev_fn = NULL;
  stream_class->close_fn = lsp_unix_output_stream_close;

   /**
   * LspUnixOutputStream:fd:
   *
   * The file descriptor that the stream writes to.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_FD,
				   g_param_spec_int ("fd",
						     "File descriptor",
						     "The file descriptor to write to",
						     G_MININT, G_MAXINT, -1,
						     G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  /**
   * LspUnixOutputStream:close-fd:
   *
   * Whether to close the file descriptor when the stream is closed.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_CLOSE_FD,
				   g_param_spec_boolean ("close-fd",
							 "Close file descriptor",
							 "Whether to close the file descriptor when the stream is closed",
							 TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}


static void
lsp_unix_output_stream_set_property (GObject         *object,
				   guint            prop_id,
				   const GValue    *value,
				   GParamSpec      *pspec)
{
  LspUnixOutputStream *unix_stream;

  unix_stream = LSP_UNIX_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      unix_stream->priv->fd = g_value_get_int (value);
      unix_stream->priv->can_poll = FALSE; //_g_fd_is_pollable (unix_stream->priv->fd);
      break;
    case PROP_CLOSE_FD:
      unix_stream->priv->close_fd = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
lsp_unix_output_stream_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  LspUnixOutputStream *unix_stream;

  unix_stream = LSP_UNIX_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, unix_stream->priv->fd);
      break;
    case PROP_CLOSE_FD:
      g_value_set_boolean (value, unix_stream->priv->close_fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
lsp_unix_output_stream_init (LspUnixOutputStream *unix_stream)
{
  unix_stream->priv = lsp_unix_output_stream_get_instance_private (unix_stream);
  unix_stream->priv->fd = -1;
  unix_stream->priv->close_fd = TRUE;
}

/**
 * lsp_unix_output_stream_new:
 * @fd: a UNIX file descriptor
 * @close_fd: %TRUE to close the file descriptor when done
 * 
 * Creates a new #LspUnixOutputStream for the given @fd. 
 * 
 * If @close_fd, is %TRUE, the file descriptor will be closed when 
 * the output stream is destroyed.
 * 
 * Returns: a new #GOutputStream
 **/
GOutputStream *
lsp_unix_output_stream_new (gint     fd,
			  gboolean close_fd)
{
  LspUnixOutputStream *stream;

  g_return_val_if_fail (fd != -1, NULL);

  stream = g_object_new (LSP_TYPE_UNIX_OUTPUT_STREAM,
			 "fd", fd,
			 "close-fd", close_fd,
			 NULL);
  
  return G_OUTPUT_STREAM (stream);
}

/**
 * lsp_unix_output_stream_set_close_fd:
 * @stream: a #LspUnixOutputStream
 * @close_fd: %TRUE to close the file descriptor when done
 *
 * Sets whether the file descriptor of @stream shall be closed
 * when the stream is closed.
 *
 * Since: 2.20
 */
void
lsp_unix_output_stream_set_close_fd (LspUnixOutputStream *stream,
                                   gboolean           close_fd)
{
  g_return_if_fail (LSP_IS_UNIX_OUTPUT_STREAM (stream));

  close_fd = close_fd != FALSE;
  if (stream->priv->close_fd != close_fd)
    {
      stream->priv->close_fd = close_fd;
      g_object_notify (G_OBJECT (stream), "close-fd");
    }
}

/**
 * lsp_unix_output_stream_get_close_fd:
 * @stream: a #LspUnixOutputStream
 *
 * Returns whether the file descriptor of @stream will be
 * closed when the stream is closed.
 *
 * Returns: %TRUE if the file descriptor is closed when done
 *
 * Since: 2.20
 */
gboolean
lsp_unix_output_stream_get_close_fd (LspUnixOutputStream *stream)
{
  g_return_val_if_fail (LSP_IS_UNIX_OUTPUT_STREAM (stream), FALSE);

  return stream->priv->close_fd;
}

/**
 * lsp_unix_output_stream_get_fd:
 * @stream: a #LspUnixOutputStream
 *
 * Return the UNIX file descriptor that the stream writes to.
 *
 * Returns: The file descriptor of @stream
 *
 * Since: 2.20
 */
gint
lsp_unix_output_stream_get_fd (LspUnixOutputStream *stream)
{
  g_return_val_if_fail (LSP_IS_UNIX_OUTPUT_STREAM (stream), -1);

  return stream->priv->fd;
}

static gssize
lsp_unix_output_stream_write (GOutputStream  *stream,
			    const void     *buffer,
			    gsize           count,
			    GCancellable   *cancellable,
			    GError        **error)
{
  LspUnixOutputStream *unix_stream;
  gssize res = -1;

  unix_stream = LSP_UNIX_OUTPUT_STREAM (stream);

  while (1)
    {
      int errsv;

      res = write (unix_stream->priv->fd, buffer, count);
      errsv = errno;
      if (res == -1)
	{
	  if (errsv == EINTR || errsv == EAGAIN)
	    continue;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       "Error writing to file descriptor: %s",
		       g_strerror (errsv));
	}

      break;
    }

  return res;
}


static gboolean
lsp_unix_output_stream_close (GOutputStream  *stream,
			    GCancellable   *cancellable,
			    GError        **error)
{
  LspUnixOutputStream *unix_stream;
  int res;

  unix_stream = LSP_UNIX_OUTPUT_STREAM (stream);

  if (!unix_stream->priv->close_fd)
    return TRUE;
  
  /* This might block during the close. Doesn't seem to be a way to avoid it though. */
  res = close (unix_stream->priv->fd);
  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   "Error closing file descriptor: %s",
		   g_strerror (errsv));
    }

  return res != -1;
}
