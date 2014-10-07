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


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;


PLUGIN_VERSION_CHECK(219) /* for document IDs */

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Git Integration"),
  _("Integration of the Git version control system"),
  "0.1",
  "Colomban Wendling <ban@herbesfolles.org>"
)


/* g_async_queue_push() doesn't allow for NULL data, so use a non-NULL fake
 * data that we know cannot ever be a valid job */
#define QUIT_THREAD_JOB ((AsyncBlobJob *) (&G_queue))

#define MARKER_ALLOCATED_QTAG (g_quark_from_static_string ("ggu-git-marker-allocated"))


enum {
  MARKER_LINE_ADDED,
  MARKER_LINE_CHANGED,
  MARKER_COUNT
};

typedef void (*BlobReadyFunc) (const gchar *path,
                               git_blob    *blob,
                               gpointer     data);

typedef struct AsyncBlobJob AsyncBlobJob;
struct AsyncBlobJob {
  gchar        *path;
  git_blob     *blob;
  BlobReadyFunc callback;
  gpointer      user_data;
};


/* cache */
static git_blob        *G_file_blob = NULL;
/* global state */
static GAsyncQueue     *G_queue     = NULL;
static GThread         *G_thread    = NULL;
static gulong           G_source_id = 0;
static struct {
  gint    num;
  gint    style;
  guint32 color;
}                       G_markers[MARKER_COUNT] = {
  { -1, SC_MARK_LEFTRECT, 0x73d216 },
  { -1, SC_MARK_LEFTRECT, 0xf57900 }
};


static void on_git_dir_changed (GFileMonitor     *monitor,
                                GFile            *file,
                                GFile            *other_file,
                                GFileMonitorEvent event_type,
                                gpointer          user_data);


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

static gpointer
worker_thread (gpointer data)
{
  GAsyncQueue    *queue   = data;
  git_repository *repo    = NULL;
  GFileMonitor   *monitor = NULL;
  AsyncBlobJob   *job;
  
  while ((job = g_async_queue_pop (queue)) != QUIT_THREAD_JOB) {
    const gchar *path = job->path;
    
    if (repo && ! g_str_has_prefix (path, git_repository_workdir (repo))) {
      /* FIXME: this can fail with nested repositories */
      git_repository_free (repo);
      repo = NULL;
      if (monitor) {
        g_object_unref (monitor);
        monitor = NULL;
      }
    }
    if (! repo && git_repository_open_ext (&repo, path, 0, NULL) == 0) {
      if (git_repository_is_bare (repo)) {
        git_repository_free (repo);
        repo = NULL;
      } else {
        GFile  *file  = g_file_new_for_path (git_repository_path (repo));
        GError *err   = NULL;
        
        monitor = g_file_monitor_directory (file, 0, NULL, &err);
        if (err) {
          g_warning ("Failed to monitor Git directory: %s", err->message);
          g_error_free (err);
        } else {
          g_signal_connect (monitor, "changed",
                            G_CALLBACK (on_git_dir_changed), job->user_data);
        }
        g_object_unref (file);
      }
    }
    
    if (repo) {
      const gchar *relpath = path + strlen (git_repository_workdir (repo));
      
      job->blob = repo_get_file_blob (repo, relpath);
    } else {
      job->blob = NULL;
    }
    
    g_idle_add_full (G_PRIORITY_DEFAULT, report_work_in_idle, job, free_job);
  }
  
  if (monitor) {
    g_object_unref (monitor);
  }
  if (repo) {
    git_repository_free (repo);
  }
  
  return NULL;
}

/* @user_data will also be used to the file monitor callback */
static void
get_cached_blob_async (const gchar   *path,
                       BlobReadyFunc  callback,
                       gpointer       user_data)
{
  if (G_file_blob || ! path) {
    callback (path, G_file_blob, user_data);
  } else {
    AsyncBlobJob *job = g_slice_alloc (sizeof *job);
    
    job->path       = g_strdup (path);
    job->blob       = NULL;
    job->callback   = callback;
    job->user_data  = user_data;
    
    if (! G_thread) {
      G_queue = g_async_queue_new ();
#if GLIB_CHECK_VERSION (2, 32, 0)
      G_thread = g_thread_new ("ggu-git/blob-worker", worker_thread, G_queue);
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
allocate_markers (ScintillaObject *sci)
{
  guint i;
  
  if (g_object_get_qdata (G_OBJECT (sci), MARKER_ALLOCATED_QTAG)) {
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
  
  g_object_set_qdata (G_OBJECT (sci), MARKER_ALLOCATED_QTAG,
                      sci /* anything non-NULL */);
  
  return TRUE;
}

static void
release_markers (ScintillaObject *sci)
{
  if (g_object_get_qdata (G_OBJECT (sci), MARKER_ALLOCATED_QTAG)) {
    guint j;
    
    for (j = 0; j < MARKER_COUNT; j++) {
      if (G_markers[j].num >= 0) {
        scintilla_send_message (sci, SCI_MARKERDEFINE,
                                G_markers[j].num, SC_MARK_AVAILABLE);
      }
    }
    g_object_set_qdata (G_OBJECT (sci), MARKER_ALLOCATED_QTAG, NULL);
  }
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
  }
  
  return 0;
}

static void
update_diff (const gchar *path,
             git_blob    *blob,
             gpointer     data)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc && doc->id == GPOINTER_TO_UINT (data) &&
      blob && allocate_markers (doc->editor->sci)) {
    ScintillaObject  *sci = doc->editor->sci;
    git_diff_options  opts;
    const gchar      *buf;
    size_t            len;
    
    /* clear previous markers */
    scintilla_send_message (sci, SCI_MARKERDELETEALL,
                            G_markers[MARKER_LINE_ADDED].num, 0);
    scintilla_send_message (sci, SCI_MARKERDELETEALL,
                            G_markers[MARKER_LINE_CHANGED].num, 0);
    
    buf = (const gchar *) scintilla_send_message (sci, SCI_GETCHARACTERPOINTER,
                                                  0, 0);
    len = sci_get_length (sci);
    
    /* no context lines, and no need to bother about binary checks */
    git_diff_init_options (&opts, GIT_DIFF_OPTIONS_VERSION);
    opts.context_lines = 0;
    opts.flags = GIT_DIFF_FORCE_TEXT;
    
    git_diff_blob_to_buffer (blob, NULL, buf, len, NULL, &opts,
                             NULL, diff_hunk_cb, NULL, sci);
  }
}

static gboolean
update_diff_idle (gpointer id)
{
  GeanyDocument *doc = document_get_current ();
  
  G_source_id = 0;
  /* make sure the document is still valid and current */
  if (doc && doc->id == GPOINTER_TO_UINT (id))
    get_cached_blob_async (doc->real_path, update_diff, id);
  
  return FALSE;
}

static void
update_diff_push (GeanyDocument *doc)
{
  g_return_if_fail (DOC_VALID (doc));
  
  if (G_source_id) {
    g_source_remove (G_source_id);
  }
  if (doc->real_path) {
    G_source_id = g_timeout_add_full (G_PRIORITY_LOW, 100, update_diff_idle,
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
    update_diff_push (editor->document);
  }
  
  return FALSE;
}

static void
on_document_activate (GObject        *obj,
                      GeanyDocument  *doc,
                      gpointer        user_data)
{
  clear_cached_blob ();
  update_diff_push (doc);
}

static void
on_document_reload (GObject        *obj,
                    GeanyDocument  *doc,
                    gpointer        user_data)
{
  update_diff_push (doc);
}

static void
on_git_dir_changed (GFileMonitor     *monitor,
                    GFile            *file,
                    GFile            *other_file,
                    GFileMonitorEvent event_type,
                    gpointer          user_data)
{
  GeanyDocument *doc = document_find_by_id (GPOINTER_TO_UINT (user_data));
  
  if (doc) {
    clear_cached_blob ();
    update_diff_push (doc);
  }
}

void
plugin_init (GeanyData *data)
{
  GeanyDocument *doc;
  
  G_file_blob = NULL;
  G_source_id = 0;
  G_thread    = NULL;
  G_queue     = NULL;
  
  if (git_threads_init () != 0) {
    const git_error *err = giterr_last ();
    g_warning ("Failed to initialize libgit2: %s", err ? err->message : "?");
    return;
  }
  
  plugin_signal_connect (geany_plugin, NULL, "editor-notify", TRUE,
                         G_CALLBACK (on_editor_notify), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-activate", TRUE,
                         G_CALLBACK (on_document_activate), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-open", TRUE,
                         G_CALLBACK (on_document_activate), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-reload", TRUE,
                         G_CALLBACK (on_document_reload), NULL);
  
  /* update for the current document in case we are loaded in the middle
   * of a session */
  doc = document_get_current ();
  if (doc) {
    update_diff_push (doc);
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
    release_markers (documents[i]->editor->sci);
  }
  
  git_threads_shutdown ();
}
