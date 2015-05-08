#ifndef __MARKER_H_
#define __MARKER_H_ 

G_BEGIN_DECLS

/* SCFIND FLAGS
 * 
 * SCFIND_WHOLEWORD 0x2
 * SCFIND_MATCHCASE 0x4
 * SCFIND_WORDSTART 0x00100000
 * SCFIND_REGEXP 0x00200000
 * SCFIND_POSIX 0x00400000
 * */
 /*
  * INDIC_PLAIN 	0 	Underlined with a single, straight line.
    INDIC_SQUIGGLE 	1 	A squiggly underline. Requires 3 pixels of descender space.
    INDIC_TT 	2 	A line of small T shapes.
    INDIC_DIAGONAL 	3 	Diagonal hatching.
    INDIC_STRIKE 	4 	Strike out.
    INDIC_HIDDEN 	5 	An indicator with no visual effect.
    INDIC_BOX 	6 	A rectangle around the text.
    INDIC_ROUNDBOX 	7
  * */
/* 
 * struct Sci_TextToFind {
	struct Sci_CharacterRange chrg;      //Range to search in - set chrg.cpMin and chrg.cpMax with the range of positions in the document to search 
	const char *lpstrText;              //Pointer of null-terminated string of text to search for
	struct Sci_CharacterRange chrgText; //Filled in with positions of found text
 *  */
#ifndef SSM 
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)
#endif

typedef struct {
    struct Sci_TextToFind; 
    gint indic_number;
    gint indic_style; /* Determins which Scintilla-Indicator type is set */
    gint sci_color; /* Determins which Scintilla Color is set for the Indicator */
    gint sci_alpha; /* Determines Scintilla foreground alpha for box type markers */
    guint flags;  /* Search flags -- currently unimplemented */
    GeanyDocument *doc; /* Tracks which doc was marked up */
} MarkerMatchInfo;

typedef struct  {
	gchar	*text;
	guint	flags;
	gboolean		backwards;
	/* set to TRUE when text was set by a search bar callback to keep track of
	 * search bar background colour */
	gboolean		search_bar;
	/* text as it was entered by user */
	gchar			*original_text;
    gint indic_number;
    gint indic_style;
    gint sci_alpha;
    guint cleared_signal;
    GdkColor *color;
    MarkerMatchInfo *last_mark_info;
} MarkerData;


void marker_data_init(void) ;
void marker_data_finalize(void); 
inline gint _GDK_COLOR_TO_SCI_COLOR(GdkColor *color);
gint on_marker_set(gchar *entry_text, gint indic_number, gint indic_style, GdkColor *color, gint alpha);
MarkerMatchInfo *get_last_marker_info(void);
MarkerMatchInfo *match_info_duplicate(MarkerMatchInfo *info);

void marker_indicator_clear(ScintillaObject *sci, gint indic_number); 
G_END_DECLS

#endif /* __MARKER_H_ */
