#ifndef __MOREMARKUP_H
#define __MOREMARKUP_H

extern GeanyPlugin		*geany_plugin;
extern GeanyData		*geany_data;
extern GeanyFunctions	*geany_functions;

#define MORE_MARKUP_TYPE				(more_markup_get_type())
#define MORE_MARKUP(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), MORE_MARKUP_TYPE, MoreMarkup))
#define MORE_MARKUP_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls),  MORE_MARKUP_TYPE, MoreMarkupClass))
#define IS_MORE_MARKUP(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), MORE_MARKUP_TYPE))
            
typedef struct _MoreMarkup       MoreMarkup;
typedef struct _MoreMarkupClass  MoreMarkupClass;

GType		more_markup_get_type		(void);
MoreMarkup  *more_markup_new			(gboolean plugin_active);

typedef struct {
    gchar * default_color; 
    gint default_indicator; 
    gint default_on_mark;
} MoreMarkupPrefs;

struct MarkupLabel {
    GtkWidget * label_box; 
    GeanyDocument * doc;
    gint indic_number;
    gint indic_style;
    gint sci_color;
    gchar *text;
};

#endif /* __MOREMARKUP_H */




