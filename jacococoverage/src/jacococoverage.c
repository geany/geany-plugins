/*
 * jacococoverage.c - JaCoCo Coverage Report viewer plugin for Geany
 *
 * Parses a JaCoCo CSV report and displays a package/class tree with
 * instruction, branch, line, and method coverage percentages.
 *
 * Copyright 2024  teknopaul
 * License: GPLv2 or later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "geany.h"
#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("JaCoCo Coverage"),
	_("Displays JaCoCo code coverage reports as a navigable package/class tree "
	  "with instruction, branch, line, and method coverage percentages."),
	"1.0",
	"teknopaul")

/* -------------------------------------------------------------------------
 * Tree store column indices
 * ---------------------------------------------------------------------- */

enum {
	COL_NAME = 0,
	COL_INSTR,
	COL_BRANCH,
	COL_LINE,
	COL_METHOD,
	COL_INSTR_PCT,    /* gdouble – used for colour-coding */
	COL_BRANCH_PCT,
	COL_LINE_PCT,
	COL_METHOD_PCT,
	COL_IS_PACKAGE,   /* gboolean */
	COL_COUNT
};

/* -------------------------------------------------------------------------
 * Global plugin state
 * ---------------------------------------------------------------------- */

static GtkWidget    *sidebar_vbox  = NULL;
static GtkWidget    *treeview      = NULL;
static GtkTreeStore *treestore     = NULL;
static GtkWidget    *path_entry    = NULL;
static gint          page_number   = 0;
static gchar        *current_csv   = NULL;

/* -------------------------------------------------------------------------
 * Data structures for parsed CSV
 * ---------------------------------------------------------------------- */

typedef struct {
	gchar *name;
	gint   instr_missed,  instr_covered;
	gint   branch_missed, branch_covered;
	gint   line_missed,   line_covered;
	gint   method_missed, method_covered;
} ClassData;

typedef struct {
	gchar *package;
	GList *classes;   /* GSList of ClassData* */
} PackageData;

/* -------------------------------------------------------------------------
 * Coverage math helpers
 * ---------------------------------------------------------------------- */

static gdouble
coverage_pct(gint missed, gint covered)
{
	gint total = missed + covered;
	if (total == 0)
		return 100.0;
	return (100.0 * covered) / (gdouble) total;
}

static gchar *
fmt_pct(gint missed, gint covered)
{
	return g_strdup_printf("%.0f%%", coverage_pct(missed, covered));
}

/* -------------------------------------------------------------------------
 * Cell renderer – colour-code coverage percentage
 * ---------------------------------------------------------------------- */

static void
render_pct_cell(GtkTreeViewColumn *col,
                GtkCellRenderer   *renderer,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           user_data)
{
	gint    pct_col = GPOINTER_TO_INT(user_data);
	gdouble val;

	(void) col;

	gtk_tree_model_get(model, iter, pct_col, &val, -1);

	if (val >= 80.0)
		g_object_set(renderer, "foreground", "#2e7d32", NULL);  /* green  */
	else if (val >= 50.0)
		g_object_set(renderer, "foreground", "#e65100", NULL);  /* orange */
	else
		g_object_set(renderer, "foreground", "#c62828", NULL);  /* red    */
}

/* -------------------------------------------------------------------------
 * Memory management
 * ---------------------------------------------------------------------- */

static void
class_data_free(gpointer data)
{
	ClassData *cd = data;
	if (cd) {
		g_free(cd->name);
		g_free(cd);
	}
}

static void
package_data_free(gpointer data)
{
	PackageData *pd = data;
	if (pd) {
		g_list_free_full(pd->classes, class_data_free);
		g_free(pd->package);
		g_free(pd);
	}
}

/* -------------------------------------------------------------------------
 * CSV parsing
 *
 * Expected header:
 *   GROUP,PACKAGE,CLASS,
 *   INSTRUCTION_MISSED,INSTRUCTION_COVERED,
 *   BRANCH_MISSED,BRANCH_COVERED,
 *   LINE_MISSED,LINE_COVERED,
 *   COMPLEXITY_MISSED,COMPLEXITY_COVERED,
 *   METHOD_MISSED,METHOD_COVERED
 * ---------------------------------------------------------------------- */

static GHashTable *
parse_csv(const gchar *filename)
{
	GError    *error   = NULL;
	gchar     *content = NULL;
	gchar    **lines;
	GHashTable *packages;

	if (!g_file_get_contents(filename, &content, NULL, &error)) {
		ui_set_statusbar(FALSE, _("JaCoCo: Cannot open %s: %s"),
		                 filename, error->message);
		g_error_free(error);
		return NULL;
	}

	packages = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                 NULL,
	                                 package_data_free);

	lines = g_strsplit(content, "\n", -1);
	g_free(content);

	/* line 0 is the header – skip it */
	for (gint i = 1; lines[i] != NULL; i++) {
		gchar  *line = g_strstrip(lines[i]);
		gchar **f;

		if (*line == '\0')
			continue;

		f = g_strsplit(line, ",", -1);
		if (g_strv_length(f) < 13) {
			g_strfreev(f);
			continue;
		}

		ClassData *cd      = g_new0(ClassData, 1);
		cd->name           = g_strdup(f[2]);
		cd->instr_missed   = atoi(f[3]);
		cd->instr_covered  = atoi(f[4]);
		cd->branch_missed  = atoi(f[5]);
		cd->branch_covered = atoi(f[6]);
		cd->line_missed    = atoi(f[7]);
		cd->line_covered   = atoi(f[8]);
		/* f[9], f[10] = COMPLEXITY (not shown) */
		cd->method_missed  = atoi(f[11]);
		cd->method_covered = atoi(f[12]);

		/* Intern the package name so we can use it as hash key */
		const gchar *pkg_name = f[1];
		PackageData *pd = g_hash_table_lookup(packages, pkg_name);
		if (!pd) {
			pd          = g_new0(PackageData, 1);
			pd->package = g_strdup(pkg_name);
			pd->classes = NULL;
			g_hash_table_insert(packages, pd->package, pd);
		}
		pd->classes = g_list_append(pd->classes, cd);

		g_strfreev(f);
	}

	g_strfreev(lines);
	return packages;
}

/* -------------------------------------------------------------------------
 * Populate the GtkTreeStore from parsed package/class data
 * ---------------------------------------------------------------------- */

static gint
cmp_class_name(gconstpointer a, gconstpointer b)
{
	return g_strcmp0(((const ClassData *) a)->name,
	                 ((const ClassData *) b)->name);
}

static void
populate_tree(GHashTable *packages)
{
	GList *pkg_names, *pn;

	gtk_tree_store_clear(treestore);

	if (!packages)
		return;

	/* Iterate packages in sorted order */
	pkg_names = g_list_sort(g_hash_table_get_keys(packages),
	                        (GCompareFunc) g_strcmp0);

	for (pn = pkg_names; pn != NULL; pn = pn->next) {
		const gchar *pkg_name = pn->data;
		PackageData *pd       = g_hash_table_lookup(packages, pkg_name);
		GtkTreeIter  pkg_iter;
		GList       *cl;

		/* Aggregate package-level counters */
		gint p_im = 0, p_ic = 0;
		gint p_bm = 0, p_bc = 0;
		gint p_lm = 0, p_lc = 0;
		gint p_mm = 0, p_mc = 0;

		for (cl = pd->classes; cl != NULL; cl = cl->next) {
			ClassData *cd = cl->data;
			p_im += cd->instr_missed;    p_ic += cd->instr_covered;
			p_bm += cd->branch_missed;   p_bc += cd->branch_covered;
			p_lm += cd->line_missed;     p_lc += cd->line_covered;
			p_mm += cd->method_missed;   p_mc += cd->method_covered;
		}

		gchar *pi = fmt_pct(p_im, p_ic);
		gchar *pb = fmt_pct(p_bm, p_bc);
		gchar *pl = fmt_pct(p_lm, p_lc);
		gchar *pm = fmt_pct(p_mm, p_mc);

		gtk_tree_store_append(treestore, &pkg_iter, NULL);
		gtk_tree_store_set(treestore, &pkg_iter,
			COL_NAME,       pkg_name,
			COL_INSTR,      pi,
			COL_BRANCH,     pb,
			COL_LINE,       pl,
			COL_METHOD,     pm,
			COL_INSTR_PCT,  coverage_pct(p_im, p_ic),
			COL_BRANCH_PCT, coverage_pct(p_bm, p_bc),
			COL_LINE_PCT,   coverage_pct(p_lm, p_lc),
			COL_METHOD_PCT, coverage_pct(p_mm, p_mc),
			COL_IS_PACKAGE, TRUE,
			-1);

		g_free(pi); g_free(pb); g_free(pl); g_free(pm);

		/* Sort classes alphabetically */
		pd->classes = g_list_sort(pd->classes, cmp_class_name);

		for (cl = pd->classes; cl != NULL; cl = cl->next) {
			ClassData   *cd = cl->data;
			GtkTreeIter  cls_iter;

			gchar *ci = fmt_pct(cd->instr_missed,  cd->instr_covered);
			gchar *cb = fmt_pct(cd->branch_missed,  cd->branch_covered);
			gchar *cl2 = fmt_pct(cd->line_missed,   cd->line_covered);
			gchar *cm = fmt_pct(cd->method_missed,  cd->method_covered);

			gtk_tree_store_append(treestore, &cls_iter, &pkg_iter);
			gtk_tree_store_set(treestore, &cls_iter,
				COL_NAME,       cd->name,
				COL_INSTR,      ci,
				COL_BRANCH,     cb,
				COL_LINE,       cl2,
				COL_METHOD,     cm,
				COL_INSTR_PCT,  coverage_pct(cd->instr_missed,  cd->instr_covered),
				COL_BRANCH_PCT, coverage_pct(cd->branch_missed, cd->branch_covered),
				COL_LINE_PCT,   coverage_pct(cd->line_missed,   cd->line_covered),
				COL_METHOD_PCT, coverage_pct(cd->method_missed, cd->method_covered),
				COL_IS_PACKAGE, FALSE,
				-1);

			g_free(ci); g_free(cb); g_free(cl2); g_free(cm);
		}
	}

	g_list_free(pkg_names);

	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}

/* -------------------------------------------------------------------------
 * Load a CSV file and update the tree
 * ---------------------------------------------------------------------- */

static void
load_csv_file(const gchar *filename)
{
	GHashTable *packages = parse_csv(filename);
	if (!packages)
		return;

	populate_tree(packages);
	g_hash_table_destroy(packages);

	g_free(current_csv);
	current_csv = g_strdup(filename);
	gtk_entry_set_text(GTK_ENTRY(path_entry), filename);
	ui_set_statusbar(FALSE, _("JaCoCo: Loaded %s"), filename);
}

/* -------------------------------------------------------------------------
 * Toolbar callbacks
 * ---------------------------------------------------------------------- */

static void
on_open_csv(GtkToolButton *btn, gpointer user_data)
{
	GtkWidget     *dialog;
	GtkFileFilter *ff;

	(void) btn;
	(void) user_data;

	dialog = gtk_file_chooser_dialog_new(
		_("Open JaCoCo CSV Report"),
		GTK_WINDOW(geany->main_widgets->window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"),   GTK_RESPONSE_ACCEPT,
		NULL);

	ff = gtk_file_filter_new();
	gtk_file_filter_set_name(ff, _("CSV files (*.csv)"));
	gtk_file_filter_add_pattern(ff, "*.csv");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ff);

	/* Try to default to project directory */
	if (geany->app->project && geany->app->project->base_path)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
		                                    geany->app->project->base_path);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		load_csv_file(fn);
		g_free(fn);
	}

	gtk_widget_destroy(dialog);
}

static void
on_refresh(GtkToolButton *btn, gpointer user_data)
{
	(void) btn;
	(void) user_data;

	if (current_csv)
		load_csv_file(current_csv);
	else
		ui_set_statusbar(FALSE,
		    _("JaCoCo: No file loaded – use the Open button to select a CSV."));
}

/* -------------------------------------------------------------------------
 * Row-activated: double-click / Enter on a class row → locate the source file
 * ---------------------------------------------------------------------- */

static void
on_row_activated(GtkTreeView *tv, GtkTreePath *path,
                 G_GNUC_UNUSED GtkTreeViewColumn *col,
                 G_GNUC_UNUSED gpointer data)
{
	GtkTreeIter   iter, parent;
	GtkTreeModel *model   = gtk_tree_view_get_model(tv);
	gboolean      is_pkg  = FALSE;
	gchar        *name    = NULL;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	gtk_tree_model_get(model, &iter,
	                   COL_IS_PACKAGE, &is_pkg,
	                   COL_NAME,       &name,
	                   -1);

	if (is_pkg || !name) {
		g_free(name);
		return;
	}

	gchar *pkg = NULL;
	if (gtk_tree_model_iter_parent(model, &parent, &iter))
		gtk_tree_model_get(model, &parent, COL_NAME, &pkg, -1);

	/* JaCoCo CSV uses '.' as package separators; locate needs '/' */
	gchar *pkg_path = pkg ? g_strdelimit(g_strdup(pkg), ".", '/') : NULL;
	gchar *query = (pkg_path && *pkg_path)
	    ? g_strdup_printf("%s/%s.java", pkg_path, name)
	    : g_strdup_printf("%s.java", name);
	g_free(pkg_path);

	g_free(name);
	g_free(pkg);

	GType obj_type = G_OBJECT_TYPE(geany->object);
	guint sig_id   = g_signal_lookup("locate-find-file", obj_type);
	if (sig_id && g_signal_has_handler_pending(geany->object, sig_id, 0, FALSE))
		g_signal_emit_by_name(geany->object, "locate-find-file", query);
	else
		ui_set_statusbar(FALSE, _("JaCoCo: locate plugin not loaded – path: %s"), query);

	g_free(query);
}

/* -------------------------------------------------------------------------
 * Build the GtkTreeView widget
 * ---------------------------------------------------------------------- */

static GtkWidget *
create_treeview(void)
{
	GtkWidget         *tv;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *renderer;

	treestore = gtk_tree_store_new(COL_COUNT,
		G_TYPE_STRING,   /* COL_NAME       */
		G_TYPE_STRING,   /* COL_INSTR      */
		G_TYPE_STRING,   /* COL_BRANCH     */
		G_TYPE_STRING,   /* COL_LINE       */
		G_TYPE_STRING,   /* COL_METHOD     */
		G_TYPE_DOUBLE,   /* COL_INSTR_PCT  */
		G_TYPE_DOUBLE,   /* COL_BRANCH_PCT */
		G_TYPE_DOUBLE,   /* COL_LINE_PCT   */
		G_TYPE_DOUBLE,   /* COL_METHOD_PCT */
		G_TYPE_BOOLEAN   /* COL_IS_PACKAGE */
	);

	tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(treestore));
	g_object_unref(treestore);   /* tv holds the reference */

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv), TRUE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tv), TRUE);
	gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(tv), TRUE);

	/* Package / Class name column */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
		_("Package / Class"), renderer,
		"text", COL_NAME,
		NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tv), col);

	/* Coverage percentage columns – right-aligned with colour coding */
#define ADD_PCT_COL(label, text_col, pct_col) \
	do { \
		renderer = gtk_cell_renderer_text_new(); \
		g_object_set(renderer, "xalign", 1.0f, NULL); \
		col = gtk_tree_view_column_new_with_attributes((label), renderer, \
			"text", (text_col), NULL); \
		gtk_tree_view_column_set_cell_data_func(col, renderer, \
			render_pct_cell, GINT_TO_POINTER(pct_col), NULL); \
		gtk_tree_view_column_set_resizable(col, TRUE); \
		gtk_tree_view_append_column(GTK_TREE_VIEW(tv), col); \
	} while (0)

	ADD_PCT_COL(_("Instr%"),  COL_INSTR,  COL_INSTR_PCT);
	ADD_PCT_COL(_("Branch%"), COL_BRANCH, COL_BRANCH_PCT);
	ADD_PCT_COL(_("Line%"),   COL_LINE,   COL_LINE_PCT);
	ADD_PCT_COL(_("Method%"), COL_METHOD, COL_METHOD_PCT);

#undef ADD_PCT_COL

	return tv;
}

/* -------------------------------------------------------------------------
 * Project-open signal: auto-load JaCoCo report if one exists
 * ---------------------------------------------------------------------- */

static void
project_open_cb(G_GNUC_UNUSED GObject *obj,
                G_GNUC_UNUSED GKeyFile *config,
                G_GNUC_UNUSED gpointer data)
{
	GeanyProject *project = geany->app->project;
	const gchar  *candidates[] = {
		"target/jacoco-report.csv",
		"target/site/jacoco/jacoco.csv",
		NULL
	};

	if (!project || EMPTY(project->base_path))
		return;

	for (gint i = 0; candidates[i] != NULL; i++) {
		gchar *path = g_build_filename(project->base_path, candidates[i], NULL);
		if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
			load_csv_file(path);
			g_free(path);
			return;
		}
		g_free(path);
	}
}

PluginCallback plugin_callbacks[] =
{
	{ "project-open", (GCallback) &project_open_cb, TRUE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

/* -------------------------------------------------------------------------
 * Toolbar callbacks – expand / collapse
 * ---------------------------------------------------------------------- */

static void
on_expand_all(GtkToolButton *btn, gpointer user_data)
{
	(void) btn;
	(void) user_data;
	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}

static void
on_collapse_all(GtkToolButton *btn, gpointer user_data)
{
	(void) btn;
	(void) user_data;
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeview));
}

/* -------------------------------------------------------------------------
 * Build and add the sidebar panel
 * ---------------------------------------------------------------------- */

static void
create_sidebar(void)
{
	GtkWidget *scrollwin;
	GtkWidget *toolbar;
	GtkWidget *wid;

	sidebar_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	treeview     = create_treeview();
	g_signal_connect(treeview, "row-activated", G_CALLBACK(on_row_activated), NULL);
	scrollwin    = gtk_scrolled_window_new(NULL, NULL);
	path_entry   = gtk_entry_new();

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), treeview);

	/* Read-only path display */
	gtk_editable_set_editable(GTK_EDITABLE(path_entry), FALSE);
	gtk_entry_set_placeholder_text(GTK_ENTRY(path_entry),
		_("No report loaded – click Open"));

	/* Toolbar */
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_OPEN));
#endif
	gtk_widget_set_tooltip_text(wid, _("Open JaCoCo CSV report"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_open_csv), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH));
#endif
	gtk_widget_set_tooltip_text(wid, _("Reload current report"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	/* Separator */
	gtk_container_add(GTK_CONTAINER(toolbar),
		GTK_WIDGET(gtk_separator_tool_item_new()));

	/* Expand all */
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_ADD));
#endif
	gtk_widget_set_tooltip_text(wid, _("Expand all packages"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_expand_all), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	/* Collapse all */
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REMOVE));
#endif
	gtk_widget_set_tooltip_text(wid, _("Collapse all packages"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_collapse_all), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	gtk_box_pack_start(GTK_BOX(sidebar_vbox), toolbar,    FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox), path_entry, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox), scrollwin,  TRUE,  TRUE, 0);

	gtk_widget_show_all(sidebar_vbox);

	page_number = gtk_notebook_append_page(
		GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		sidebar_vbox,
		gtk_label_new(_("JaCoCo")));
}

/* -------------------------------------------------------------------------
 * Keybindings
 * ---------------------------------------------------------------------- */

enum {
	KB_OPEN_CSV = 0,
	KB_REFRESH,
	KB_COUNT
};

static void
kb_activate(guint key_id)
{
	switch (key_id) {
		case KB_OPEN_CSV:
			on_open_csv(NULL, NULL);
			break;
		case KB_REFRESH:
			on_refresh(NULL, NULL);
			break;
		default:
			break;
	}
}

/* -------------------------------------------------------------------------
 * Plugin lifecycle
 * ---------------------------------------------------------------------- */

void
plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	(void) data;
	create_sidebar();

	key_group = plugin_set_key_group(geany_plugin, "jacoco_coverage", KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_OPEN_CSV, kb_activate,
		0, 0, "jacoco_open_csv", _("Open JaCoCo CSV report"), NULL);
	keybindings_set_item(key_group, KB_REFRESH, kb_activate,
		0, 0, "jacoco_refresh", _("Refresh JaCoCo report"), NULL);
}

void
plugin_cleanup(void)
{
	g_free(current_csv);
	current_csv = NULL;
	gtk_widget_destroy(sidebar_vbox);
}
