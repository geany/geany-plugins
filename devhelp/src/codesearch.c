#include <glib.h>
#include <webkit/webkitwebview.h>

#include "devhelpplugin.h"


struct LangMapEnt
{
	const gchar *geany_name;
	const gchar *google_name;
};


#define GOOGLE_CODE_SEARCH_URI "http://www.google.com/codesearch"

#define LANG_MAP_MAX 33 /* update this with lang_map[] size below */

/* maps Geany language names to Google Code language names */
static const struct LangMapEnt lang_map[LANG_MAP_MAX] = {
	{ "ActionScript", "actionscript" },
	{ "Ada", "ada" },
	{ "ASM", "assembly" },
	{ "FreeBasic", "basic" },
	{ "C", "c" },
	{ "C++", "c++" },
	{ "C#", "c#" },
	{ "COBOL", "cobol" },
	{ "CSS", "css" },
	{ "D", "d" },
	{ "Erlang", "erlang" },
	{ "Fortran", "fortran" },
	{ "Haskell", "haskell" },
	{ "Java", "java" },
	{ "Javascript", "javascript" },
	{ "Lisp", "lisp" },
	{ "Lua", "lua" },
	{ "Make", "makefile" },
	{ "Matlab/Octave", "matlab" },
	{ "CAML", "ocaml" },
	{ "Pascal", "pascal" },
	{ "Perl", "perl" },
	{ "PHP", "php" },
	{ "Python", "python" },
	{ "R", "r" },
	{ "Ruby", "ruby" },
	{ "Sh", "shell" },
	{ "SQL", "sql" },
	{ "Tcl", "tcl" },
	{ "LaTeX", "tex" },
	{ "Verilog", "verilog" },
	{ "VHDL", "vhdl" },
	{ "None", NULL }
};


void devhelp_plugin_search_code(DevhelpPlugin *self, const gchar *term, const gchar *lang)
{
	gint i;
	gchar *uri, *term_enc, *lang_enc;
	const gchar *google_lang = NULL;

	g_return_if_fail(self != NULL);
	g_return_if_fail(term != NULL);

	if (lang != NULL)
	{
		for (i = 0; i < LANG_MAP_MAX; i++)
		{
			if (g_strcmp0(lang, lang_map[i].geany_name) == 0)
			{
				google_lang = lang_map[i].google_name;
				break;
			}
		}
	}

	if (google_lang != NULL)
	{
		lang_enc = g_uri_escape_string(google_lang, NULL, TRUE);
		term_enc = g_uri_escape_string(term, NULL, TRUE);
		uri = g_strdup_printf("%s?as_q=%s&as_lang=%s", GOOGLE_CODE_SEARCH_URI, term_enc, lang_enc);
		g_free(lang_enc);
		g_free(term_enc);
	}
	else
	{
		term_enc = g_uri_escape_string(term, NULL, TRUE);
		uri = g_strdup_printf("%s?as_q=%s", GOOGLE_CODE_SEARCH_URI, term_enc);
		g_free(term_enc);
	}

	webkit_web_view_open(devhelp_plugin_get_webview(self), uri);
	g_free(uri);

	devhelp_plugin_activate_webview_tab(self);
}
