/*
 *  
 *  Copyright (C) 2014  Colomban Wendling <ban@herbesfolles.org>
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <git2.h>

#include <geanyplugin.h>
#include <geany.h>
#include <document.h>

#if ! defined (LIBGIT2_SOVERSION) || LIBGIT2_SOVERSION < 22
# define git_libgit2_init     git_threads_init
# define git_libgit2_shutdown git_threads_shutdown
#endif
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 20
# define git_diff_hunk git_diff_range
#endif


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;


PLUGIN_VERSION_CHECK(219) /* for document IDs */

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Git Change Bar"),
  _("Highlights uncommitted changes in files tracked with Git"),
  "0.1",
  "Colomban Wendling <ban@herbesfolles.org>"
)


/* g_async_queue_push() doesn't allow for NULL data, so use a non-NULL fake
 * data that we know cannot ever be a valid job */
#define QUIT_THREAD_JOB ((AsyncBlobJob *) (&G_queue))

#define RESOURCES_ALLOCATED_QTAG \
  (g_quark_from_static_string (PLUGIN"/git-resources-allocated"))


enum {
  MARKER_LINE_ADDED,
  MARKER_LINE_CHANGED,
  MARKER_LINE_REMOVED,
  MARKER_COUNT
};

enum {
  KB_GOTO_PREV_HUNK,
  KB_GOTO_NEXT_HUNK,
  KB_COUNT
};

typedef void (*BlobReadyFunc) (const gchar *path,
                               git_blob    *blob,
                               gpointer     data);

typedef struct AsyncBlobJob AsyncBlobJob;
struct AsyncBlobJob {
  gboolean      force;
  gchar        *path;
  git_blob     *blob;
  BlobReadyFunc callback;
  gpointer      user_data;
};

typedef struct TooltipHunkData TooltipHunkData;
struct TooltipHunkData {
  gint            line;
  gboolean        found;
  GeanyDocument  *doc;
  const git_blob *file_blob;
  GtkTooltip     *tooltip;
};

#define TOOLTIP_HUNK_DATA_INIT(line, doc, blob, tooltip) \
  { line, FALSE, doc, blob, tooltip }

typedef struct GotoNextHunkData GotoNextHunkData;
struct GotoNextHunkData {
  guint kb;
  guint doc_id;
  gint  line;
  gint  next_line;
};


static void         on_git_repo_changed         (GFileMonitor     *monitor,
                                                 GFile            *file,
                                                 GFile            *other_file,
                                                 GFileMonitorEvent event_type,
                                                 gpointer          force);
static gboolean     on_sci_query_tooltip        (GtkWidget   *widget,
                                                 gint         x,
                                                 gint         y,
                                                 gboolean     keyboard_mode,
                                                 GtkTooltip  *tooltip,
                                                 gpointer     user_data);
static void         read_setting_color          (GKeyFile    *kf,
                                                 const gchar *group,
                                                 const gchar *key,
                                                 gpointer     value);
static void         write_setting_color         (GKeyFile      *kf,
                                                 const gchar   *group,
                                                 const gchar   *key,
                                                 gconstpointer  value);
static void         read_setting_boolean        (GKeyFile    *kf,
                                                 const gchar *group,
                                                 const gchar *key,
                                                 gpointer     value);
static void         write_setting_boolean       (GKeyFile      *kf,
                                                 const gchar   *group,
                                                 const gchar   *key,
                                                 gconstpointer  value);


/* cache */
static git_blob        *G_file_blob           = NULL;
/* global state */
static GAsyncQueue     *G_queue               = NULL;
static GThread         *G_thread              = NULL;
static gulong           G_source_id           = 0;
static gboolean         G_monitoring_enabled  = TRUE;
static struct {
  gint    num;
  gint    style;
  guint32 color;
}                       G_markers[MARKER_COUNT] = {
  { -1, SC_MARK_LEFTRECT, 0x73d216 },
  { -1, SC_MARK_LEFTRECT, 0xf57900 },
  { -1, SC_MARK_LEFTRECT, 0xcc0000 }
};
/* settings description */
static const struct {
  const gchar  *group;
  const gchar  *key;
  gpointer      value;
  void        (*read)   (GKeyFile    *kf,
                         const gchar *group,
                         const gchar *key,
                         gpointer     value);
  void        (*write)  (GKeyFile      *kf,
                         const gchar   *group,
                         const gchar   *key,
                         gconstpointer  value);
} G_settings_desc[] = {
  { "general", "monitor-repository", &G_monitoring_enabled,
    read_setting_boolean, write_setting_boolean },
  { "colors", "line-added", &G_markers[MARKER_LINE_ADDED].color,
    read_setting_color, write_setting_color },
  { "colors", "line-changed", &G_markers[MARKER_LINE_CHANGED].color,
    read_setting_color, write_setting_color },
  { "colors", "line-removed", &G_markers[MARKER_LINE_REMOVED].color,
    read_setting_color, write_setting_color }
};


static void
clear_cached_blob (void)
{
  if (G_file_blob) {
    git_blob_free (G_file_blob);
    G_file_blob = NULL;
  }
}

/* get the file blob for @relpath at HEAD */
static git_blob *
repo_get_file_blob (git_repository *repo,
                    const gchar    *relpath)
{
  git_reference  *head = NULL;
  git_blob       *blob = NULL;
  
  if (git_repository_head (&head, repo) == 0) {
    git_commit *commit = NULL;
    
    if (git_commit_lookup (&commit, repo, git_reference_target (head)) == 0) {
      git_tree *tree = NULL;
      
      if (git_commit_tree (&tree, commit) == 0) {
        git_tree_entry *entry = NULL;
        
        if (git_tree_entry_bypath (&entry, tree, relpath) == 0) {
          git_blob_lookup (&blob, repo, git_tree_entry_id (entry));
          git_tree_entry_free (entry);
        }
        git_tree_free (tree);
      }
      git_commit_free (commit);
    }
    git_reference_free (head);
  }
  
  return blob;
}

static void
free_job (gpointer data)
{
  AsyncBlobJob *job = data;
  
  g_free (job->path);
  g_slice_free1 (sizeof *job, job);
}

static gboolean
report_work_in_idle (gpointer data)
{
  AsyncBlobJob *job = data;
  
  /* update cached blob */
  clear_cached_blob ();
  G_file_blob = job->blob;
  
  job->callback (job->path, job->blob, job->user_data);
  
  return FALSE;
}

static GFileMonitor *
monitor_repo_file (git_repository  *repo,
                   const gchar     *subpath,
                   GCallback        changed_callback,
                   gpointer         user_data)
{
  GFile        *file    = NULL;
  GError       *err     = NULL;
  GFileMonitor *monitor = NULL;
  gchar        *path    = g_build_filename (git_repository_path (repo),
                                            subpath, NULL);
  
  file = g_file_new_for_path (path);
  monitor = g_file_monitor (file, 0, NULL, &err);
  if (err) {
    g_warning ("Failed to monitor %s: %s", path, err->message);
    g_error_free (err);
  } else {
    g_signal_connect (monitor, "changed", changed_callback, user_data);
  }
  g_object_unref (file);
  g_free (path);
  
  return monitor;
}

/* monitors the current reference HEAD points to (probably a branch) */
static GFileMonitor *
monitor_head_ref (git_repository *repo,
                  GCallback       changed_callback,
                  gpointer        user_data)
{
  git_reference  *head    = NULL;
  GFileMonitor   *monitor = NULL;
  
  if (! git_repository_head_detached (repo) &&
      git_repository_head (&head, repo) == 0) {
    monitor = monitor_repo_file (repo, git_reference_name (head),
                                 changed_callback, user_data);
    git_reference_free (head);
  }
  
  return monitor;
}

static gpointer
worker_thread (gpointer data)
{
  GAsyncQueue    *queue       = data;
  git_repository *repo        = NULL;
  GFileMonitor   *monitors[2] = { NULL, NULL };
  AsyncBlobJob   *job;
  guint           i;
  
  while ((job = g_async_queue_pop (queue)) != QUIT_THREAD_JOB) {
    const gchar *path = job->path;
    
    if (repo && (job->force ||
                 ! g_str_has_prefix (path, git_repository_workdir (repo)))) {
      /* FIXME: this can fail with nested repositories */
      git_repository_free (repo);
      repo = NULL;
      for (i = 0; i < G_N_ELEMENTS (monitors); i++) {
        if (monitors[i]) {
          g_object_unref (monitors[i]);
          monitors[i] = NULL;
        }
      }
    }
    if (! repo && git_repository_open_ext (&repo, path, 0, NULL) == 0) {
      if (git_repository_is_bare (repo)) {
        git_repository_free (repo);
        repo = NULL;
      } else if (G_monitoring_enabled) {
        /* we need to monitor HEAD, in case of e.g. branch switch (e.g.
         * git checkout -b will switch the ref we need to watch) */
        monitors[0] = monitor_repo_file (repo, "HEAD",
                                         G_CALLBACK (on_git_repo_changed),
                                         GINT_TO_POINTER (TRUE));
        /* and of course the real ref (branch) for when changes get committed */
        monitors[1] = monitor_head_ref (repo, G_CALLBACK (on_git_repo_changed),
                                        GINT_TO_POINTER (FALSE));
      }
    }
    
    if (repo) {
      const gchar *relpath = path + strlen (git_repository_workdir (repo));
      
      job->blob = repo_get_file_blob (repo, relpath);
    } else {
      job->blob = NULL;
    }
    
    g_idle_add_full (G_PRIORITY_LOW, report_work_in_idle, job, free_job);
  }
  
  for (i = 0; i < G_N_ELEMENTS (monitors); i++) {
    if (monitors[i]) {
      g_object_unref (monitors[i]);
      monitors[i] = NULL;
    }
  }
  if (repo) {
    git_repository_free (repo);
  }
  
  return NULL;
}

static void
get_cached_blob_async (const gchar   *path,
                       gboolean       force,
                       BlobReadyFunc  callback,
                       gpointer       user_data)
{
  if ((! force && G_file_blob) || ! path) {
    callback (path, G_file_blob, user_data);
  } else {
    AsyncBlobJob *job = g_slice_alloc (sizeof *job);
    
    job->force      = force;
    job->path       = g_strdup (path);
    job->blob       = NULL;
    job->callback   = callback;
    job->user_data  = user_data;
    
    if (! G_thread) {
      G_queue = g_async_queue_new ();
#if GLIB_CHECK_VERSION (2, 32, 0)
      G_thread = g_thread_new (PLUGIN"/blob-worker", worker_thread, G_queue);
#else
      G_thread = g_thread_create (worker_thread, G_queue, NULL, NULL);
#endif
    }
    
    g_async_queue_push (G_queue, job);
  }
}

static gint
allocate_marker (ScintillaObject *sci,
                 guint            marker)
{
  g_return_val_if_fail (marker < MARKER_COUNT, -1);
  
  if (G_markers[marker].num == -1) {
    gint i;
    
    G_markers[marker].num = -2;
    /* markers 0-1 and 25-31 are reserved */
    for (i = 2; G_markers[marker].num < 0 && i < 25; i++) {
      gint sym = scintilla_send_message (sci, SCI_MARKERSYMBOLDEFINED, i, 0);
      
      if (sym == SC_MARK_AVAILABLE ||
          sym == 0 /* unfortunately it's also SC_MARK_CIRCLE */) {
        guint j;
        
        /* check if we already allocate it but not defined it yet */
        for (j = 0; j < MARKER_COUNT && G_markers[j].num != i; j++) {
          /* nothing */
        }
        if (j == MARKER_COUNT) {
          G_markers[marker].num = i;
        }
      }
    }
  }
  
  return G_markers[marker].num;
}

static gboolean
allocate_resources (ScintillaObject *sci)
{
  guint i;
  
  if (g_object_get_qdata (G_OBJECT (sci), RESOURCES_ALLOCATED_QTAG)) {
    return TRUE;
  }
  
  /* allocate all markers first so we have all or nothing */
  for (i = 0; i < MARKER_COUNT; i++) {
    if (allocate_marker (sci, i) < 0) {
      return FALSE;
    }
  }
  
  for (i = 0; i < MARKER_COUNT; i++) {
    scintilla_send_message (sci, SCI_MARKERDEFINE,
                            G_markers[i].num, G_markers[i].style);    
    scintilla_send_message (sci, SCI_MARKERSETBACK,
                            G_markers[i].num,
                            /* Scintilla uses BGR */
                            ((G_markers[i].color & 0xff0000) >> 16) |
                            ((G_markers[i].color & 0x00ff00)) |
                            ((G_markers[i].color & 0x0000ff) << 16));
  }
  
  /* setup tooltips */
  gtk_widget_set_has_tooltip (GTK_WIDGET (sci), TRUE);
  g_signal_connect (sci, "query-tooltip",
                    G_CALLBACK (on_sci_query_tooltip), NULL);
  
  g_object_set_qdata (G_OBJECT (sci), RESOURCES_ALLOCATED_QTAG,
                      sci /* anything non-NULL */);
  
  return TRUE;
}

static void
release_resources (ScintillaObject *sci)
{
  if (g_object_get_qdata (G_OBJECT (sci), RESOURCES_ALLOCATED_QTAG)) {
    guint j;
    
    for (j = 0; j < MARKER_COUNT; j++) {
      if (G_markers[j].num >= 0) {
        scintilla_send_message (sci, SCI_MARKERDEFINE,
                                G_markers[j].num, SC_MARK_AVAILABLE);
      }
    }
    g_signal_handlers_disconnect_by_func (sci, on_sci_query_tooltip, NULL);
    g_object_set_qdata (G_OBJECT (sci), RESOURCES_ALLOCATED_QTAG, NULL);
  }
}

/* checks whether @encoding needs to be converted to UTF-8 */
static gboolean
encoding_needs_coversion (const gchar *encoding)
{
  return (encoding &&
          ! utils_str_equal (encoding, "UTF-8") &&
          ! utils_str_equal (encoding, "None"));
}

/*
 * @brief Converts encoding
 * @param buffer Input and output buffer
 * @param length Input and output buffer length
 * @param to Target encoding
 * @param from Source encoding
 * @param error Return location for errors, or @c NULL
 * @returns @c TRUE if the conversion succeeded and the buffer has been
 *          replaced and should be freed, or @c FALSE otherwise.
 * 
 * @warning This function has a very weird API, but it is practical for
 *          how it's used.
 * 
 * Converts between encodings (using g_convert()) in-place.
 * 
 * The @p buffer is both an input and output parameter.  As an input
 * parameter, it is used as a constant buffer pointer and is never
 * modified nor freed.  As an output parameter, it is an allocated chunk
 * of memory that should be freed with @c g_free().
 * If the conversion succeeds, it replaces @p buffer and @p length with
 * the converted values and returns @c TRUE.  If it fails, it does not
 * modify the values and returns @c FALSE so that the passed-in
 * variables can be used as a fallback if the conversion was optional.
 */
static gboolean
convert_encoding_inplace (gchar       **buffer,
                          gsize        *length,
                          const gchar  *to,
                          const gchar  *from,
                          GError      **error)
{
  gsize   tmp_len;
  gchar  *tmp_buf = g_convert (*buffer, (gssize) *length, to, from,
                               NULL, &tmp_len, error);
  
  if (tmp_buf) {
    *buffer = tmp_buf;
    *length = tmp_len;
  }
  
  return tmp_buf != NULL;
}

static int
diff_blob_to_doc (const git_blob   *old_blob,
                  GeanyDocument    *doc,
                  git_diff_hunk_cb  hunk_cb,
                  void             *payload)
{
  ScintillaObject  *sci = doc->editor->sci;
  git_diff_options  opts = GIT_DIFF_OPTIONS_INIT;
  gchar            *buf;
  size_t            len;
  gboolean          free_buf = FALSE;
  int               ret;
  
  buf = (gchar *) scintilla_send_message (sci, SCI_GETCHARACTERPOINTER, 0, 0);
  len = sci_get_length (sci);
  
  /* convert the buffer back to in-file encoding if necessary */
  if (encoding_needs_coversion (doc->encoding)) {
    free_buf = convert_encoding_inplace (&buf, &len, doc->encoding, "UTF-8",
                                         NULL);
  }
  
  /* no context lines, and no need to bother about binary checks */
  opts.context_lines = 0;
  opts.flags = GIT_DIFF_FORCE_TEXT;
  
  ret = git_diff_blob_to_buffer (old_blob, NULL, buf, len, NULL, &opts,
                                 NULL, hunk_cb, NULL, payload);
  
  if (free_buf) {
    g_free (buf);
  }
  
  return ret;
}

static int
diff_hunk_cb (const git_diff_delta *delta,
              const git_diff_hunk  *hunk,
              void                 *data)
{
  ScintillaObject *sci = data;
  
  if (hunk->new_lines > 0) {
    gint  line;
    guint marker = hunk->old_lines > 0 ? MARKER_LINE_CHANGED : MARKER_LINE_ADDED;
    
    for (line = hunk->new_start; line < hunk->new_start + hunk->new_lines; line++) {
      scintilla_send_message (sci, SCI_MARKERADD, line - 1, G_markers[marker].num);
    }
  } else {
    scintilla_send_message (sci, SCI_MARKERADD, hunk->new_start - 1,
                            G_markers[MARKER_LINE_REMOVED].num);
  }
  
  return 0;
}
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 20
static int
diff_hunk_cb_wrapper (const git_diff_delta *delta,
                      const git_diff_hunk  *hunk,
                      const char           *header,
                      size_t                header_len,
                      void                 *data)
{
  return diff_hunk_cb (delta, hunk, data);
}
# define diff_hunk_cb diff_hunk_cb_wrapper
#endif

static GtkWidget *
get_widget_for_blob_range (GeanyDocument   *doc,
                           const git_blob  *blob,
                           gint             line_start,
                           gint             n_lines)
{
  ScintillaObject        *sci     = editor_create_widget (doc->editor);
  const GeanyIndentPrefs *iprefs  = editor_get_indent_prefs (doc->editor);
  gint                    width   = 0;
  gint                    height  = 0;
  gint                    zoom;
  gint                    i;
  GtkAllocation           alloc;
  gchar                  *buf;
  gsize                   buf_len;
  gboolean                free_buf = FALSE;
  
  gtk_widget_get_allocation (GTK_WIDGET (doc->editor->sci), &alloc);
  
  highlighting_set_styles (sci, doc->file_type);
  if (iprefs->type == GEANY_INDENT_TYPE_BOTH) {
    scintilla_send_message (sci, SCI_SETTABWIDTH, iprefs->hard_tab_width, 0);
  } else {
    scintilla_send_message (sci, SCI_SETTABWIDTH, iprefs->width, 0);
  }
  scintilla_send_message (sci, SCI_SETINDENT, iprefs->width, 0);
  zoom = scintilla_send_message (doc->editor->sci, SCI_GETZOOM, 0, 0);
  scintilla_send_message (sci, SCI_SETZOOM, zoom, 0);
  
  /* hide stuff we don't wanna see */
  scintilla_send_message (sci, SCI_SETHSCROLLBAR, 0, 0);
  scintilla_send_message (sci, SCI_SETVSCROLLBAR, 0, 0);
  for (i = 0; i < SC_MAX_MARGIN; i++) {
    scintilla_send_message (sci, SCI_SETMARGINWIDTHN, i, 0);
  }
  
  buf_len = git_blob_rawsize (blob);
  buf = (gchar *) git_blob_rawcontent (blob);
  
  /* convert the buffer to UTF-8 if necessary */
  if (encoding_needs_coversion (doc->encoding)) {
    free_buf = convert_encoding_inplace (&buf, &buf_len, "UTF-8", doc->encoding,
                                         NULL);
  }
  
  scintilla_send_message (sci, SCI_ADDTEXT, buf_len, (glong) buf);
  
  if (free_buf) {
    g_free (buf);
  }
  
  /* we need to enable extra scroll after last line so that SETFIRSTVISIBLELINE
   * really places the line we want on top of the view, even if the line is
   * close to the end and wouldn't possibly end on top otherwise */
  scintilla_send_message (sci, SCI_SETENDATLASTLINE, 0, 0);
  scintilla_send_message (sci, SCI_SETFIRSTVISIBLELINE, line_start, 0);
  
  /* compute the size of the area we want to see */
  for (i = line_start; i < line_start + n_lines; i++) {
    gint pos    = sci_get_line_end_position (sci, i);
    gint end_x  = scintilla_send_message (sci, SCI_POINTXFROMPOSITION, 0, pos);
    
    height += scintilla_send_message (sci, SCI_TEXTHEIGHT, i, 0);
    width = MAX (width, end_x);
    
    if (height > alloc.height) {
      break;
    }
  }
  gtk_widget_set_size_request (GTK_WIDGET (sci),
                               MIN (width, alloc.width),
                               MIN (height + 1, alloc.height));
  
  return GTK_WIDGET (sci);
}

static int
tooltip_diff_hunk_cb (const git_diff_delta *delta,
                      const git_diff_hunk  *hunk,
                      void                 *data)
{
  TooltipHunkData *thd = data;
  
  if (thd->found) {
    return 1;
  }
  
  if (hunk->old_lines > 0 &&
      thd->line >= hunk->new_start &&
      thd->line < hunk->new_start + MAX (1, hunk->new_lines)) {
    GtkWidget *old = get_widget_for_blob_range (thd->doc, thd->file_blob,
                                                hunk->old_start - 1,
                                                hunk->old_lines);
    
    gtk_tooltip_set_custom (thd->tooltip, old);
    thd->found = old != NULL;
  }
  
  return thd->found;
}
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 20
static int
tooltip_diff_hunk_cb_wrapper (const git_diff_delta *delta,
                              const git_diff_hunk  *hunk,
                              const char           *header,
                              size_t                header_len,
                              void                 *data)
{
  return tooltip_diff_hunk_cb (delta, hunk, data);
}
# define tooltip_diff_hunk_cb tooltip_diff_hunk_cb_wrapper
#endif

static gboolean
on_sci_query_tooltip (GtkWidget  *widget,
                      gint        x,
                      gint        y,
                      gboolean    keyboard_mode,
                      GtkTooltip *tooltip,
                      gpointer    user_data)
{
  gint              min_x;
  gint              max_x;
  ScintillaObject  *sci         = (ScintillaObject *) widget;
  GeanyDocument    *doc         = document_get_current ();
  gboolean          has_tooltip = FALSE;
  
  /* for some reason the widget isn't the current one during tab switch, so
   * give up silently when we receive a query for a non-current widget */
  if (! doc || doc->editor->sci != sci) {
    return FALSE;
  }
  
  min_x = scintilla_send_message (sci, SCI_GETMARGINWIDTHN, 0, 0);
  max_x = min_x + scintilla_send_message (sci, SCI_GETMARGINWIDTHN, 1, 0);
  
  if (x >= min_x && x <= max_x && G_file_blob) {
    gint pos  = scintilla_send_message (sci, SCI_POSITIONFROMPOINT, x, y);
    gint line = sci_get_line_from_position (sci, pos);
    gint mask = scintilla_send_message (sci, SCI_MARKERGET, line, 0);
    
    if (mask & ((1 << G_markers[MARKER_LINE_CHANGED].num) |
                (1 << G_markers[MARKER_LINE_REMOVED].num))) {
      TooltipHunkData thd = TOOLTIP_HUNK_DATA_INIT (line + 1, doc, G_file_blob,
                                                    tooltip);
      
      diff_blob_to_doc (G_file_blob, doc, tooltip_diff_hunk_cb, &thd);
      has_tooltip = thd.found;
    }
  }
  
  return has_tooltip;
}

static void
update_diff (const gchar *path,
             git_blob    *blob,
             gpointer     data)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc && doc->id == GPOINTER_TO_UINT (data) &&
      blob && allocate_resources (doc->editor->sci)) {
    ScintillaObject  *sci = doc->editor->sci;
    guint             i;
    
    /* clear previous markers */
    for (i = 0; i < MARKER_COUNT; i++) {
      scintilla_send_message (sci, SCI_MARKERDELETEALL, G_markers[i].num, 0);
    }
    
    diff_blob_to_doc (blob, doc, diff_hunk_cb, sci);
  }
}

static gboolean
do_update_diff_idle (guint    doc_id,
                     gboolean force)
{
  GeanyDocument *doc = document_get_current ();
  
  G_source_id = 0;
  /* make sure the document is still valid and current */
  if (doc && doc->id == doc_id) {
    get_cached_blob_async (doc->real_path, force, update_diff,
                           GUINT_TO_POINTER (doc->id));
  }
  
  return FALSE;
}

static gboolean
update_diff_idle (gpointer id)
{
  return do_update_diff_idle (GPOINTER_TO_UINT (id), FALSE);
}

static gboolean
update_diff_force_idle (gpointer id)
{
  return do_update_diff_idle (GPOINTER_TO_UINT (id), TRUE);
}

/*
 * @brief Pushes a request for updating the diff
 * @param doc The document for which update the diff
 * @param force Whether to force reloading the repository information.  This
 *              is used to e.g. force re-setting up monitors after a repository
 *              change.
 * 
 * Pushes a request for updating the diff.  Typically this should be called
 * after the user modified the buffer to keep the diff in sync.
 * 
 * Pass @c TRUE to @p force if the repository might have changed in a way that
 * requires reloading it.  Note that generally you don't need to do so when the
 * file might have changed in the repository (e.g. when the user checked out
 * another ref) because then the diff is either still valid or the buffer needs
 * reloading from disk anyway.
 */
static void
update_diff_push (GeanyDocument  *doc,
                  gboolean        force)
{
  g_return_if_fail (DOC_VALID (doc));
  
  if (G_source_id) {
    g_source_remove (G_source_id);
    G_source_id = 0;
  }
  if (doc->real_path) {
    G_source_id = g_timeout_add_full (G_PRIORITY_LOW, 100,
                                      force ? update_diff_force_idle
                                            : update_diff_idle,
                                      GUINT_TO_POINTER (doc->id), NULL);
  }
}

static gboolean
on_editor_notify (GObject        *obj,
                  GeanyEditor    *editor,
                  SCNotification *nt,
                  gpointer        user_data)
{
  if (nt->nmhdr.code == SCN_CHARADDED ||
      (nt->nmhdr.code == SCN_MODIFIED &&
       nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) {
    update_diff_push (editor->document, FALSE);
  }
  
  return FALSE;
}

static void
on_document_activate (GObject        *obj,
                      GeanyDocument  *doc,
                      gpointer        user_data)
{
  clear_cached_blob ();
  update_diff_push (doc, FALSE);
}

static void
on_startup_complete (GObject *obj,
                     gpointer user_data)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc) {
    update_diff_push (doc, FALSE);
  }
}

static void
on_git_repo_changed (GFileMonitor     *monitor,
                     GFile            *file,
                     GFile            *other_file,
                     GFileMonitorEvent event_type,
                     gpointer          force)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc) {
    clear_cached_blob ();
    update_diff_push (doc, GPOINTER_TO_INT (force));
  }
}

static int
goto_next_hunk_diff_hunk_cb (const git_diff_delta *delta,
                             const git_diff_hunk  *hunk,
                             void                 *udata)
{
  GotoNextHunkData *data = udata;
  
  switch (data->kb) {
    case KB_GOTO_NEXT_HUNK:
      if (data->next_line >= 0) {
        return 1;
      } else if (data->line < hunk->new_start - 1) {
        data->next_line = hunk->new_start - 1;
      }
      break;
    
    case KB_GOTO_PREV_HUNK:
      if (data->line > hunk->new_start - 1 + MAX (hunk->new_lines - 1, 0)) {
        data->next_line = hunk->new_start - 1;
      }
      break;
  }
  
  return 0;
}
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 20
static int
goto_next_hunk_diff_hunk_cb_wrapper (const git_diff_delta *delta,
                                     const git_diff_hunk  *hunk,
                                     const char           *header,
                                     size_t                header_len,
                                     void                 *data)
{
  return goto_next_hunk_diff_hunk_cb (delta, hunk, data);
}
# define goto_next_hunk_diff_hunk_cb goto_next_hunk_diff_hunk_cb_wrapper
#endif

static void
goto_next_hunk_cb (const gchar *path,
                   git_blob    *blob,
                   gpointer     udata)
{
  GotoNextHunkData *data  = udata;
  GeanyDocument    *doc   = document_get_current ();
  
  if (doc && doc->id == data->doc_id && blob) {
    diff_blob_to_doc (blob, doc, goto_next_hunk_diff_hunk_cb, data);
    
    if (data->next_line >= 0) {
      gint pos = sci_get_position_from_line (doc->editor->sci, data->next_line);
      
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
  
  g_slice_free1 (sizeof *data, data);
}

static void
on_kb_goto_next_hunk (guint kb)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc) {
    GotoNextHunkData *data = g_slice_alloc (sizeof *data);
  
    data->kb        = kb;
    data->doc_id    = doc->id;
    data->line      = sci_get_current_line (doc->editor->sci);
    data->next_line = -1;
    
    get_cached_blob_async (doc->real_path, FALSE, goto_next_hunk_cb, data);
  }
}

/* --- configuration loading and saving --- */

static void
read_setting_color (GKeyFile     *kf,
                    const gchar  *group,
                    const gchar  *key,
                    gpointer      value)
{
  guint32  *color = value;
  gchar    *kfval = g_key_file_get_value (kf, group, key, NULL);
  
  if (kfval) {
    const gchar  *nptr = kfval;
    gchar        *endptr;
    glong         val;
    
    if (*nptr == '#') {
      nptr++;
    }
    
    val = strtol (nptr, &endptr, 16);
    if (! *endptr && val >= 0 && val <= 0xffffff) {
      *color = (guint32) val;
    }
    g_free (kfval);
  }
}

static void
write_setting_color (GKeyFile      *kf,
                     const gchar   *group,
                     const gchar   *key,
                     gconstpointer  value)
{
  const guint32  *color     = value;
  gchar           kfval[8]  = {0};
  
  g_return_if_fail (*color <= 0xffffff);
  
  g_snprintf (kfval, sizeof kfval, "#%.6x", *color);
  g_key_file_set_value (kf, group, key, kfval);
}

static void
read_setting_boolean (GKeyFile     *kf,
                      const gchar  *group,
                      const gchar  *key,
                      gpointer      value)
{
  gboolean *bool = value;
  
  *bool = utils_get_setting_boolean (kf, group, key, *bool);
}

static void
write_setting_boolean (GKeyFile      *kf,
                       const gchar   *group,
                       const gchar   *key,
                       gconstpointer  value)
{
  const gboolean *bool = value;
  
  g_key_file_set_boolean (kf, group, key, *bool);
}

/* loads @filename in @kf and return %FALSE if failed, emitting a warning
 * unless the file was simply missing */
static gboolean
read_keyfile (GKeyFile     *kf,
              const gchar  *filename,
              GKeyFileFlags flags)
{
  GError *error = NULL;
  
  if (! g_key_file_load_from_file (kf, filename, flags, &error)) {
    if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOENT) {
      g_warning (_("Failed to load configuration file: %s"), error->message);
    }
    g_error_free (error);
    
    return FALSE;
  }
  
  return TRUE;
}

/* writes @kf in @filename, possibly creating directories to be able to write
 * in @filename */
static gboolean
write_keyfile (GKeyFile    *kf,
               const gchar *filename)
{
  gchar    *dirname = g_path_get_dirname (filename);
  GError   *error   = NULL;
  gint      err;
  gchar    *data;
  gsize     length;
  gboolean  success = FALSE;
  
  data = g_key_file_to_data (kf, &length, NULL);
  if ((err = utils_mkdir (dirname, TRUE)) != 0) {
    g_warning (_("Failed to create configuration directory \"%s\": %s"),
               dirname, g_strerror (err));
  } else if (! g_file_set_contents (filename, data, (gssize) length, &error)) {
    g_warning (_("Failed to save configuration file: %s"), error->message);
    g_error_free (error);
  } else {
    success = TRUE;
  }
  g_free (data);
  g_free (dirname);
  
  return success;
}

static gchar *
get_config_filename (void)
{
  return g_build_filename (geany_data->app->configdir, "plugins",
                           PLUGIN, PLUGIN".conf", NULL);
}

static void
load_config (void)
{
  gchar    *filename  = get_config_filename ();
  GKeyFile *kf        = g_key_file_new ();
  
  if (read_keyfile (kf, filename, G_KEY_FILE_NONE)) {
    guint i;
    
    for (i = 0; i < G_N_ELEMENTS (G_settings_desc); i++) {
      G_settings_desc[i].read (kf, G_settings_desc[i].group,
                               G_settings_desc[i].key,
                               G_settings_desc[i].value);
    }
  }
  g_key_file_free (kf);
  g_free (filename);
}

static void
save_config (void)
{
  gchar    *filename  = get_config_filename ();
  GKeyFile *kf        = g_key_file_new ();
  guint     i;
  
  read_keyfile (kf, filename, G_KEY_FILE_KEEP_COMMENTS);
  for (i = 0; i < G_N_ELEMENTS (G_settings_desc); i++) {
    G_settings_desc[i].write (kf, G_settings_desc[i].group,
                              G_settings_desc[i].key,
                              G_settings_desc[i].value);
  }
  write_keyfile (kf, filename);
  
  g_key_file_free (kf);
  g_free (filename);
}

/* --- plugin initialization and cleanup --- */

void
plugin_init (GeanyData *data)
{
  GeanyKeyGroup *kb_group;
  
  G_file_blob = NULL;
  G_source_id = 0;
  G_thread    = NULL;
  G_queue     = NULL;
  
  if (git_libgit2_init () < 0) {
    const git_error *err = giterr_last ();
    g_warning ("Failed to initialize libgit2: %s", err ? err->message : "?");
    return;
  }
  
  load_config ();
  
  kb_group = plugin_set_key_group (geany_plugin, PLUGIN, KB_COUNT, NULL);
  keybindings_set_item (kb_group, KB_GOTO_PREV_HUNK, on_kb_goto_next_hunk, 0, 0,
                        "goto-prev-hunk", _("Go to the previous hunk"), NULL);
  keybindings_set_item (kb_group, KB_GOTO_NEXT_HUNK, on_kb_goto_next_hunk, 0, 0,
                        "goto-next-hunk", _("Go to the next hunk"), NULL);
  
  plugin_signal_connect (geany_plugin, NULL, "editor-notify", TRUE,
                         G_CALLBACK (on_editor_notify), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-activate", TRUE,
                         G_CALLBACK (on_document_activate), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-reload", TRUE,
                         G_CALLBACK (on_document_activate), NULL);
  plugin_signal_connect (geany_plugin, NULL, "geany-startup-complete", TRUE,
                         G_CALLBACK (on_startup_complete), NULL);
  
  if (main_is_realized ()) {
    /* update for the current document as we are loaded in the middle of a
     * session and so won't receive the :geany-startup-complete signal */
    on_startup_complete (NULL, NULL);
  }
}

void
plugin_cleanup (void)
{
  guint i;
  
  if (G_source_id) {
    g_source_remove (G_source_id);
    G_source_id = 0;
  }
  if (G_thread) {
    g_async_queue_push (G_queue, QUIT_THREAD_JOB); /* notify the thread */
    g_thread_join (G_thread);
    G_thread = NULL;
    g_async_queue_unref (G_queue);
    G_queue = NULL;
  }
  if (G_file_blob) {
    git_blob_free (G_file_blob);
    G_file_blob = NULL;
  }
  
  foreach_document (i) {
    release_resources (documents[i]->editor->sci);
  }
  
  save_config ();
  
  git_libgit2_shutdown ();
}

/* --- configuration dialog --- */

typedef struct ConfigureWidgets ConfigureWidgets;
struct ConfigureWidgets {
  GtkWidget  *base;
  GtkWidget  *monitoring_check;
  GtkWidget  *added_color_button;
  GtkWidget  *changed_color_button;
  GtkWidget  *removed_color_button;
};

static void configure_widgets_free (ConfigureWidgets *cw)
{
  g_object_unref (cw->base);
  g_free (cw);
}

static void
color_from_int (GdkColor *color,
                guint32   val)
{
  color->red    = ((val & 0xff0000) >> 16) * 0x101;
  color->green  = ((val & 0x00ff00) >>  8) * 0x101;
  color->blue   = ((val & 0x0000ff) >>  0) * 0x101;
}

static guint32
color_to_int (const GdkColor *color)
{
  return (((color->red   / 0x101) << 16) |
          ((color->green / 0x101) <<  8) |
          ((color->blue  / 0x101) <<  0));
}

static void
on_plugin_configure_response (GtkDialog        *dialog,
                              gint              response,
                              ConfigureWidgets *cw)
{
  switch (response) {
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK: {
      guint           i;
      GdkColor        color;
      GeanyDocument  *doc = document_get_current ();
      
      G_monitoring_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cw->monitoring_check));
      gtk_color_button_get_color (GTK_COLOR_BUTTON (cw->added_color_button),
                                  &color);
      G_markers[MARKER_LINE_ADDED].color = color_to_int (&color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (cw->changed_color_button),
                                  &color);
      G_markers[MARKER_LINE_CHANGED].color = color_to_int (&color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (cw->removed_color_button),
                                  &color);
      G_markers[MARKER_LINE_REMOVED].color = color_to_int (&color);
      
      /* update everything */
      foreach_document (i) {
        release_resources (documents[i]->editor->sci);
      }
      if (doc) {
        update_diff_push (doc, TRUE);
      }
    }
  }
}

GtkWidget *
plugin_configure (GtkDialog *dialog)
{
  GError     *error   = NULL;
  GtkWidget  *base    = NULL;
  GtkBuilder *builder = gtk_builder_new ();
  
  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
  if (! gtk_builder_add_from_file (builder, PKGDATADIR"/"PLUGIN"/prefs.ui",
                                   &error)) {
    g_critical (_("Failed to load UI definition, please check your "
                  "installation. The error was: %s"), error->message);
    g_error_free (error);
  } else {
    GdkColor          color;
    ConfigureWidgets *cw = g_malloc (sizeof *cw);
    struct {
      const gchar  *name;
      GtkWidget   **ptr;
    } map[] = {
      { "base",                 &cw->base },
      { "monitoring-check",     &cw->monitoring_check },
      { "added-color-button",   &cw->added_color_button },
      { "changed-color-button", &cw->changed_color_button },
      { "removed-color-button", &cw->removed_color_button },
    };
    guint i;
    
    for (i = 0; i < G_N_ELEMENTS (map); i++) {
      *map[i].ptr = GTK_WIDGET (gtk_builder_get_object (builder, map[i].name));
    }
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw->monitoring_check),
                                  G_monitoring_enabled);
    color_from_int (&color, G_markers[MARKER_LINE_ADDED].color);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cw->added_color_button),
                                &color);
    color_from_int (&color, G_markers[MARKER_LINE_CHANGED].color);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cw->changed_color_button),
                                &color);
    color_from_int (&color, G_markers[MARKER_LINE_REMOVED].color);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (cw->removed_color_button),
                                &color);
    
    base = g_object_ref_sink (cw->base);
    
    g_signal_connect_data (dialog, "response",
                           G_CALLBACK (on_plugin_configure_response),
                           cw, (GClosureNotify) configure_widgets_free, 0);
  }
  
  g_object_unref (builder);
  
  return base;
}
