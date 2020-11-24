/*
 *      Copyright 2007-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007-2009 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *      Copyright 2007-2009 Yura Siamashka <yurand2@gmail.com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GEANYVC_HEADER__
#define __GEANYVC_HEADER__

enum
{
	VC_COMMAND_DIFF_FILE,
	VC_COMMAND_DIFF_DIR,
	VC_COMMAND_REVERT_FILE,
	VC_COMMAND_REVERT_DIR,
	VC_COMMAND_STATUS,
	VC_COMMAND_ADD,
	VC_COMMAND_REMOVE,
	VC_COMMAND_LOG_FILE,
	VC_COMMAND_LOG_DIR,
	VC_COMMAND_COMMIT,
	VC_COMMAND_BLAME,
	VC_COMMAND_SHOW,
	VC_COMMAND_UPDATE,
	VC_COMMAND_COUNT
};

/*
 * VC_COMMAND_STARTDIR_BASE - command strat dir will be VC base directory
 * VC_COMMAND_STARTDIR_FILE - command start dir will be taken as basename
 *                            of file, or directory itself if file is
 *                            directory
 */
enum
{
	VC_COMMAND_STARTDIR_BASE,
	VC_COMMAND_STARTDIR_FILE
};

#define COMMIT_DIFF_MAXLENGTH  16384

#define FLAG_RELOAD         (1<<0)
#define FLAG_FORCE_ASK      (1<<1)
#define FLAG_FILE           (1<<2)
#define FLAG_DIR            (1<<3)
#define FLAG_BASEDIR        (1<<4)

#define P_ABS_DIRNAME       "*<?geanyvcDIRNAME>*"
#define P_ABS_FILENAME      "*<?geanyvcFILENAME>*"
#define P_BASENAME          "*<?geanyvcBASE_FILENAME>*"

/* The addresses of these strings act as enums, their contents are not used. */


/* absolute path dirname of file */
extern const gchar ABS_DIRNAME[];
/* absolute path filename of file */
extern const gchar ABS_FILENAME[];

/* path to directory from base vc directory */
extern const gchar BASE_DIRNAME[];
/* path to file from base vc directory */
extern const gchar BASE_FILENAME[];

/* basename of file */
extern const gchar BASENAME[];
/* list with absolute file names*/
extern const gchar FILE_LIST[];
/* message */
extern const gchar MESSAGE[];




/* this string is used when action require to run several commands */
extern const gchar CMD_SEPARATOR[];

extern const gchar FILE_STATUS_MODIFIED[];
extern const gchar FILE_STATUS_ADDED[];
extern const gchar FILE_STATUS_DELETED[];
extern const gchar FILE_STATUS_UNKNOWN[];

gint
execute_custom_command(const gchar * dir, const gchar ** argv, const gchar ** env, gchar ** std_out,
		       gchar ** std_err, const gchar * filename, GSList * list,
		       const gchar * message);

gboolean find_dir(const gchar * filename, const char *find, gboolean recursive);
gchar *find_subdir_path(const gchar * filename, const gchar * subdir);

typedef struct _VC_COMMAND
{
	gint startdir;
	const gchar **command;
	const gchar **env;
	  gint(*function) (gchar **, gchar **, const gchar *, GSList *, const gchar *);
} VC_COMMAND;

typedef struct _VC_RECORD
{
	const VC_COMMAND *commands;
	const gchar *program;
	gchar *(*get_base_dir) (const gchar * path);
	/* check if file in VC */
	gboolean(*in_vc) (const gchar * path);
	GSList *(*get_commit_files) (const gchar * dir);
} VC_RECORD;

typedef struct _CommitItem
{
	gchar *path;
	const gchar *status;
} CommitItem;

#define REGISTER_VC(vc,enable) {extern VC_RECORD VC_##vc;if(enable){path = g_find_program_in_path(VC_##vc.program); \
							if (path) { g_free(path); VC = g_slist_append(VC, &VC_##vc);} }}

int commititem_compare_by_path(const CommitItem * a, const CommitItem * b);

/* Blank functions and values */
GSList *get_commit_files_null(const gchar * dir);
extern const gchar *NO_ENV[];

/* External diff viewer */
void external_diff_viewer_init(void);
void external_diff_viewer_deinit(void);
const gchar *get_external_diff_viewer(void);
void vc_external_diff(const gchar * src, const gchar * dest);

/* utils.c */
gchar *normpath(const gchar * filename);
gchar *get_full_path(const gchar * location, const gchar * path);
gchar *get_relative_path(const gchar * location, const gchar * path);


#endif /* __GEANYVC_HEADER__ */
