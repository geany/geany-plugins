/*
 * overviewscintilla.c - This file is part of the Geany Overview plugin
 *
 * Copyright (c) 2015 Matthew Brush <mbrush@codebrainz.ca>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "overviewscintilla.h"
#include "overviewplugin.h"
#include <string.h>

#define OVERVIEW_SCINTILLA_CURSOR        GDK_ARROW
#define OVERVIEW_SCINTILLA_CURSOR_CLICK  GDK_ARROW
#define OVERVIEW_SCINTILLA_CURSOR_SCROLL GDK_SB_V_DOUBLE_ARROW
#define OVERVIEW_SCINTILLA_ZOOM_MIN      -100
#define OVERVIEW_SCINTILLA_ZOOM_MAX      100
#define OVERVIEW_SCINTILLA_ZOOM_DEF      -20
#define OVERVIEW_SCINTILLA_WIDTH_MIN     16
#define OVERVIEW_SCINTILLA_WIDTH_MAX     512
#define OVERVIEW_SCINTILLA_WIDTH_DEF     120
#define OVERVIEW_SCINTILLA_SCROLL_LINES  1

#ifndef SC_MAX_MARGIN
# define SC_MAX_MARGIN 4
#endif

#define sci_send(sci, msg, wParam, lParam) \
  scintilla_send_message (SCINTILLA (sci), SCI_##msg, (uptr_t)(wParam), (sptr_t)(lParam))

static const OverviewColor def_overlay_color         = { 0.0, 0.0, 0.0, 0.25 };
static const OverviewColor def_overlay_outline_color = { 0.0, 0.0, 0.0, 0.0 };

enum
{
  PROP_0,
  PROP_SCINTILLA,
  PROP_CURSOR,
  PROP_VISIBLE_RECT,
  PROP_WIDTH,
  PROP_ZOOM,
  PROP_SHOW_TOOLTIP,
  PROP_OVERLAY_ENABLED,
  PROP_OVERLAY_COLOR,
  PROP_OVERLAY_OUTLINE_COLOR,
  PROP_OVERLAY_INVERTED,
  PROP_DOUBLE_BUFFERED,
  PROP_SCROLL_LINES,
  PROP_SHOW_SCROLLBAR,
  N_PROPERTIES,
};

struct OverviewScintilla_
{
  ScintillaObject  parent;
  ScintillaObject *sci;             // source scintilla
  GtkWidget       *canvas;          // internal GtkDrawingArea of scintilla
  GdkCursorType    cursor;          // the chosen mouse cursor to use over the scintilla
  GdkCursorType    active_cursor;   // the cursor to draw
  GdkRectangle     visible_rect;    // visible region rectangle
  guint            width;           // width of the overview
  gint             zoom;            // zoom level of scintilla
  gboolean         show_tooltip;    // whether tooltip shown on hover
  gboolean         overlay_enabled; // whether the visible overlay is drawn
  OverviewColor    overlay_color;   // the color of the visible overlay
  OverviewColor    overlay_outline_color; // the color of the outline of the overlay
  gboolean         overlay_inverted;// draw overlay over the visible area instead of around it
  gboolean         double_buffered; // whether to enable double-buffering on internal scintilla canvas
  gint             scroll_lines;    // number of lines to scroll each scroll-event
  gboolean         show_scrollbar;  // show the main scintilla's scrollbar
  gboolean         mouse_down;      // whether the mouse is down
  gulong           update_rect;     // signal id of idle rect handler
  gulong           conf_event;      // signal id of the configure event on scintilla internal drawing area
  GtkWidget       *src_canvas;      // internal drawing area of main scintilla
};

struct OverviewScintillaClass_
{
  ScintillaClass parent_class;
};

static GParamSpec *pspecs[N_PROPERTIES] = { NULL };

static void overview_scintilla_finalize     (GObject           *object);
static void overview_scintilla_set_property (GObject           *object,
                                             guint              prop_id,
                                             const GValue      *value,
                                             GParamSpec        *pspec);
static void overview_scintilla_get_property (GObject           *object,
                                             guint              prop_id,
                                             GValue            *value,
                                             GParamSpec        *pspec);
static void overview_scintilla_set_src_sci  (OverviewScintilla *self,
                                             ScintillaObject   *sci);

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean overview_scintilla_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);
#else
static gboolean overview_scintilla_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
#endif

G_DEFINE_TYPE (OverviewScintilla, overview_scintilla, scintilla_get_type())

static void
overview_scintilla_class_init (OverviewScintillaClass *klass)
{
  GObjectClass   *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize     = overview_scintilla_finalize;
  g_object_class->set_property = overview_scintilla_set_property;
  g_object_class->get_property = overview_scintilla_get_property;

  pspecs[PROP_SCINTILLA] =
    g_param_spec_object ("scintilla",
                         "Scintilla",
                         "The source ScintillaObject",
                         scintilla_get_type (),
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  pspecs[PROP_CURSOR] =
    g_param_spec_enum ("cursor",
                       "Cursor",
                       "The GdkCursorType to use for the mouse cursor",
                       GDK_TYPE_CURSOR_TYPE,
                       OVERVIEW_SCINTILLA_CURSOR,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_VISIBLE_RECT] =
    g_param_spec_boxed ("visible-rect",
                       "VisibleRect",
                       "The visible area indication rectangle to draw",
                       GDK_TYPE_RECTANGLE,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_WIDTH] =
    g_param_spec_uint ("width",
                       "Width",
                       "Width of the overview",
                       OVERVIEW_SCINTILLA_WIDTH_MIN,
                       OVERVIEW_SCINTILLA_WIDTH_MAX,
                       OVERVIEW_SCINTILLA_WIDTH_DEF,
                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_ZOOM] =
    g_param_spec_int ("zoom",
                      "Zoom",
                      "The zoom-level of the overview",
                      OVERVIEW_SCINTILLA_ZOOM_MIN,
                      OVERVIEW_SCINTILLA_ZOOM_MAX,
                      OVERVIEW_SCINTILLA_ZOOM_DEF,
                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_SHOW_TOOLTIP] =
    g_param_spec_boolean ("show-tooltip",
                          "ShowTooltip",
                          "Whether to show a tooltip with addition info on mouse over",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_OVERLAY_ENABLED] =
    g_param_spec_boolean ("overlay-enabled",
                          "OverlayEnabled",
                          "Whether an overlay is drawn ontop of the overview",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_OVERLAY_COLOR] =
    g_param_spec_boxed ("overlay-color",
                        "OverlayColor",
                        "The color of the overlay, if enabled",
                        OVERVIEW_TYPE_COLOR,
                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_OVERLAY_OUTLINE_COLOR] =
    g_param_spec_boxed ("overlay-outline-color",
                        "OverlayOutlineColor",
                        "The color of the overlay's outline, if enabled",
                        OVERVIEW_TYPE_COLOR,
                        G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_OVERLAY_INVERTED] =
    g_param_spec_boolean ("overlay-inverted",
                          "OverlayInverted",
                          "Whether to draw the overlay over the visible area",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_DOUBLE_BUFFERED] =
    g_param_spec_boolean ("double-buffered",
                          "DoubleBuffered",
                          "Whether the overview Scintilla's internal canvas is double-buffered",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_SCROLL_LINES] =
    g_param_spec_int ("scroll-lines",
                      "ScrollLines",
                      "The number of lines to move each scroll, -1 for default, 0 to disable.",
                      -1, 100, OVERVIEW_SCINTILLA_SCROLL_LINES,
                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  pspecs[PROP_SHOW_SCROLLBAR] =
    g_param_spec_boolean ("show-scrollbar",
                          "ShowScrollbar",
                          "Whether to show the scrollbar in the main Scintilla",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_properties (g_object_class, N_PROPERTIES, pspecs);
}

static void
overview_scintilla_finalize (GObject *object)
{
  OverviewScintilla *self;

  g_return_if_fail (OVERVIEW_IS_SCINTILLA (object));

  self = OVERVIEW_SCINTILLA (object);

  if (GTK_IS_WIDGET (self->src_canvas) && self->conf_event != 0)
    g_signal_handler_disconnect (self->src_canvas, self->conf_event);

  g_object_unref (self->sci);

  G_OBJECT_CLASS (overview_scintilla_parent_class)->finalize (object);
}

static inline void
cairo_set_source_overlay_color_ (cairo_t             *cr,
                                 const OverviewColor *color)
{
  cairo_set_source_rgba (cr, color->red, color->green, color->blue, color->alpha);
}

static inline void
overview_scintilla_draw_real (OverviewScintilla *self,
                              cairo_t           *cr)
{
  if (! self->overlay_enabled)
    return;

  GtkAllocation alloc;

  gtk_widget_get_allocation (GTK_WIDGET (self->canvas), &alloc);

  cairo_save (cr);

  cairo_set_line_width (cr, 1.0);

#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 12, 0)
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_GOOD);
#endif

  if (self->overlay_inverted)
    {
      cairo_set_source_overlay_color_ (cr, &self->overlay_color);
      cairo_rectangle (cr,
                       0,
                       self->visible_rect.y,
                       alloc.width,
                       self->visible_rect.height);
      cairo_fill (cr);
    }
  else
    {
      // draw a rectangle at the top and bottom to obscure the non-visible area
      cairo_set_source_overlay_color_ (cr, &self->overlay_color);
      cairo_rectangle (cr, 0, 0, alloc.width, self->visible_rect.y);
      cairo_rectangle (cr,
                       0,
                       self->visible_rect.y + self->visible_rect.height,
                       alloc.width,
                       alloc.height - (self->visible_rect.y + self->visible_rect.height));
      cairo_fill (cr);
    }

    // draw a highlighting border at top and bottom of visible rect
    cairo_set_source_overlay_color_ (cr, &self->overlay_outline_color);
    cairo_move_to (cr, self->visible_rect.x + 0.5, self->visible_rect.y + 0.5);
    cairo_line_to (cr, self->visible_rect.width, self->visible_rect.y + 0.5);
    cairo_move_to (cr, self->visible_rect.x + 0.5, self->visible_rect.y + 0.5 + self->visible_rect.height);
    cairo_line_to (cr, self->visible_rect.width, self->visible_rect.y + 0.5 + self->visible_rect.height);
    cairo_stroke (cr);

    // draw a left border if there's no scrollbar
    if (! overview_scintilla_get_show_scrollbar (self))
      {
        cairo_move_to (cr, 0.5, 0.5);
        cairo_line_to (cr, 0.5, alloc.height);
        cairo_stroke (cr);
      }

  cairo_restore (cr);
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean
overview_scintilla_draw (GtkWidget *widget,
                         cairo_t   *cr,
                         gpointer   user_data)
{
  overview_scintilla_draw_real (OVERVIEW_SCINTILLA (user_data), cr);
  return FALSE;
}
#else
static gboolean
overview_scintilla_expose_event (GtkWidget      *widget,
                                 GdkEventExpose *event,
                                 gpointer        user_data)
{
  cairo_t *cr;
  cr = gdk_cairo_create (gtk_widget_get_window (widget));
  overview_scintilla_draw_real (OVERVIEW_SCINTILLA (user_data), cr);
  cairo_destroy (cr);
  return FALSE;
}
#endif

static gboolean
on_focus_in_event (OverviewScintilla *self,
                   GdkEventFocus     *event,
                   ScintillaObject   *sci)
{
  sci_send (self, SETREADONLY, 1, 0);
  return TRUE;
}

static gboolean
on_focus_out_event (OverviewScintilla *self,
                    GdkEventFocus     *event,
                    ScintillaObject   *sci)
{
  GeanyDocument *doc = g_object_get_data (G_OBJECT (sci), "document");
  if (DOC_VALID (doc))
    sci_send (self, SETREADONLY, doc->readonly, 0);
  else
    sci_send (self, SETREADONLY, 0, 0);
  return TRUE;
}

static gboolean
on_enter_notify_event (OverviewScintilla *self,
                       GdkEventCrossing  *event,
                       ScintillaObject   *sci)
{
  return TRUE;
}

static gboolean
on_leave_notify_event (OverviewScintilla *self,
                       GdkEventCrossing  *event,
                       ScintillaObject   *sci)
{
  return TRUE;
}

static void
on_each_child (GtkWidget *child,
               GList    **list)
{
  *list = g_list_prepend (*list, child);
}

static GList *
gtk_container_get_internal_children_ (GtkContainer *cont)
{
  GList *list = NULL;
  gtk_container_forall (cont, (GtkCallback) on_each_child, &list);
  return g_list_reverse (list);
}

static GtkWidget *
overview_scintilla_find_drawing_area (GtkWidget *root)
{
  GtkWidget *da = NULL;
  if (GTK_IS_DRAWING_AREA (root))
    da = root;
  else if (GTK_IS_CONTAINER (root))
    {
      GList *children = gtk_container_get_internal_children_ (GTK_CONTAINER (root));
      for (GList *iter = children; iter != NULL; iter = g_list_next (iter))
        {
          GtkWidget *wid = overview_scintilla_find_drawing_area (iter->data);
          if (GTK_IS_DRAWING_AREA (wid))
            {
              da = wid;
              break;
            }
        }
      g_list_free (children);
    }
  return da;
}

static gboolean
on_scroll_event (OverviewScintilla *self,
                 GdkEventScroll    *event,
                 GtkWidget         *widget)
{
  gint delta = 0;

  if (self->scroll_lines == 0)
    return TRUE;

  if (event->direction == GDK_SCROLL_UP)
    delta = -(self->scroll_lines);
  else if (event->direction == GDK_SCROLL_DOWN)
    delta = self->scroll_lines;

  if (delta != 0)
    sci_send (self->sci, LINESCROLL, 0, delta);

  return TRUE;
}

static void
overview_scintilla_goto_point (OverviewScintilla *self,
                               gint               x,
                               gint               y)
{
  gint pos;
  pos = sci_send (self, POSITIONFROMPOINT, x, y);
  if (pos >= 0)
    sci_send (self->sci, GOTOPOS, pos, 0);
}

static void
overview_scintilla_update_cursor (OverviewScintilla *self)
{
  if (GTK_IS_WIDGET (self->canvas) && gtk_widget_get_mapped (self->canvas))
    {
      GdkCursor *cursor = gdk_cursor_new (self->active_cursor);
      gdk_window_set_cursor (gtk_widget_get_window (self->canvas), cursor);
      gdk_cursor_unref (cursor);
    }
}

static gboolean
on_button_press_event (OverviewScintilla *self,
                       GdkEventButton    *event,
                       GtkWidget         *widget)
{
  self->mouse_down = TRUE;
  self->active_cursor = OVERVIEW_SCINTILLA_CURSOR_CLICK;
  overview_scintilla_update_cursor (self);
  return TRUE;
}

static gboolean
on_button_release_event (OverviewScintilla *self,
                         GdkEventButton    *event,
                         GtkWidget         *widget)
{
  self->mouse_down = FALSE;
  self->active_cursor = OVERVIEW_SCINTILLA_CURSOR;
  overview_scintilla_update_cursor (self);
  overview_scintilla_goto_point (self, event->x, event->y);
  return TRUE;
}

static gboolean
on_motion_notify_event (OverviewScintilla *self,
                        GdkEventMotion    *event,
                        GtkWidget         *widget)
{
  if (self->mouse_down)
    {
      if (self->active_cursor != OVERVIEW_SCINTILLA_CURSOR_SCROLL)
        {
          self->active_cursor = OVERVIEW_SCINTILLA_CURSOR_SCROLL;
          overview_scintilla_update_cursor (self);
        }
      overview_scintilla_goto_point (self, event->x, event->y);
    }
  return TRUE;
}

static gboolean
overview_scintilla_setup_tooltip (OverviewScintilla *self,
                                  gint               x,
                                  gint               y,
                                  GtkTooltip        *tooltip)
{
  gint pos;

  if (!self->show_tooltip)
    return FALSE;

  pos = sci_send (self, POSITIONFROMPOINT, x, y);
  if (pos >= 0)
    {
      gint line = sci_send (self, LINEFROMPOSITION, pos, 0);
      gint column = sci_send (self, GETCOLUMN, pos, 0);
      gchar *text =
        g_strdup_printf (_("Line <b>%d</b>, Column <b>%d</b>, Position <b>%d</b>"),
                         line, column, pos);
      gtk_tooltip_set_markup (tooltip, text);
      g_free (text);
    }
  else
    gtk_tooltip_set_text (tooltip, NULL);

  return TRUE;
}

static gboolean
on_query_tooltip (OverviewScintilla *self,
                  gint               x,
                  gint               y,
                  gboolean           kbd_mode,
                  GtkTooltip        *tooltip,
                  GtkWidget         *widget)
{
  if (!kbd_mode)
    return overview_scintilla_setup_tooltip (self, x, y, tooltip);
  return FALSE;
}

static void
overview_scintilla_setup_canvas (OverviewScintilla *self)
{
  if (!GTK_IS_WIDGET (self->canvas))
    {
      self->canvas = overview_scintilla_find_drawing_area (GTK_WIDGET (self));

      gtk_widget_add_events (self->canvas,
                             GDK_EXPOSURE_MASK |
                              GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_SCROLL_MASK);

      g_signal_connect_swapped (self->canvas,
                                "scroll-event",
                                G_CALLBACK (on_scroll_event),
                                self);
      g_signal_connect_swapped (self->canvas,
                                "button-press-event",
                                G_CALLBACK (on_button_press_event),
                                self);
      g_signal_connect_swapped (self->canvas,
                                "button-release-event",
                                G_CALLBACK (on_button_release_event),
                                self);
      g_signal_connect_swapped (self->canvas,
                                "motion-notify-event",
                                G_CALLBACK (on_motion_notify_event),
                                self);
      g_signal_connect_swapped (self->canvas,
                                "query-tooltip",
                                G_CALLBACK (on_query_tooltip),
                                self);

      gtk_widget_set_has_tooltip (self->canvas, self->show_tooltip);

#if GTK_CHECK_VERSION (3, 0, 0)
      g_signal_connect_after (self->canvas,
                              "draw",
                              G_CALLBACK (overview_scintilla_draw),
                              self);
#else
      g_signal_connect_after (self->canvas,
                              "expose-event",
                              G_CALLBACK (overview_scintilla_expose_event),
                              self);
#endif

    }
}

static void
overview_scintilla_update_rect (OverviewScintilla *self)
{
  GtkAllocation alloc;
  GdkRectangle  rect;
  gint          first_line, n_lines, last_line;
  gint          pos_start, pos_end;
  gint          ystart, yend;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  first_line = sci_send (self->sci, GETFIRSTVISIBLELINE, 0, 0);
  n_lines = sci_send (self->sci, LINESONSCREEN, 0, 0);
  last_line = first_line + n_lines;

  pos_start = sci_send (self, POSITIONFROMLINE, first_line, 0);
  pos_end = sci_send (self, POSITIONFROMLINE, last_line, 0);

  ystart = sci_send (self, POINTYFROMPOSITION, 0, pos_start);
  yend = sci_send (self, POINTYFROMPOSITION, 0, pos_end);

  if (yend >= alloc.height || yend == 0)
    {
      n_lines = sci_send (self, GETLINECOUNT, 0, 0);
      last_line = n_lines - 1;
      pos_end = sci_send (self, POSITIONFROMLINE, last_line, 0);
      yend = sci_send (self, POINTYFROMPOSITION, 0, pos_end);
    }

  rect.x      = 0;
  rect.width  = alloc.width - 1;
  rect.y      = ystart;
  rect.height = yend - ystart;

  overview_scintilla_set_visible_rect (self, &rect);
}

static gint
sci_get_midline_ (gpointer sci)
{
  gint first, count;
  first = sci_send (sci, GETFIRSTVISIBLELINE, 0, 0);
  count = sci_send (sci, LINESONSCREEN, 0, 0);
  return first + (count / 2);
}

static void
overview_scintilla_sync_center (OverviewScintilla *self)
{
  gint mid_src = sci_get_midline_ (self->sci);
  gint mid_dst = sci_get_midline_ (self);
  gint delta = mid_src - mid_dst;
  sci_send (self, LINESCROLL, 0, delta);
  overview_scintilla_update_rect (self);
}

static gboolean
on_map_event (OverviewScintilla *self,
              GdkEventAny       *event,
              ScintillaObject   *sci)
{
  overview_scintilla_setup_canvas (self);

  if (GTK_IS_WIDGET (self->canvas) &&
      gtk_widget_get_double_buffered (self->canvas) != self->double_buffered)
    {
      gtk_widget_set_double_buffered (self->canvas, self->double_buffered);
      self->double_buffered = gtk_widget_get_double_buffered (self->canvas);
    }

  overview_scintilla_update_cursor (self);
  overview_scintilla_update_rect (self);

  return FALSE;
}

#define self_connect(name, cb) \
  g_signal_connect_swapped (self, name, G_CALLBACK (cb), self)

static void
overview_scintilla_init (OverviewScintilla *self)
{
  self->sci             = NULL;
  self->canvas          = NULL;
  self->cursor          = OVERVIEW_SCINTILLA_CURSOR;
  self->active_cursor   = OVERVIEW_SCINTILLA_CURSOR;
  self->update_rect     = 0;
  self->conf_event      = 0;
  self->src_canvas      = NULL;
  self->width           = OVERVIEW_SCINTILLA_WIDTH_DEF;
  self->zoom            = OVERVIEW_SCINTILLA_ZOOM_DEF;
  self->mouse_down      = FALSE;
  self->show_tooltip    = TRUE;
  self->double_buffered = TRUE;
  self->scroll_lines    = OVERVIEW_SCINTILLA_SCROLL_LINES;
  self->show_scrollbar  = TRUE;
  self->overlay_inverted = TRUE;

  memset (&self->visible_rect, 0, sizeof (GdkRectangle));
  memcpy (&self->overlay_color, &def_overlay_color, sizeof (OverviewColor));
  memcpy (&self->overlay_outline_color, &def_overlay_outline_color, sizeof (OverviewColor));

  gtk_widget_add_events (GTK_WIDGET (self),
                         GDK_EXPOSURE_MASK |
                         GDK_FOCUS_CHANGE_MASK |
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_STRUCTURE_MASK);

  self_connect ("focus-in-event", on_focus_in_event);
  self_connect ("focus-out-event", on_focus_out_event);
  self_connect ("enter-notify-event", on_enter_notify_event);
  self_connect ("leave-notify-event", on_leave_notify_event);
  self_connect ("map-event", on_map_event);
}

static void
overview_scintilla_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  OverviewScintilla *self = OVERVIEW_SCINTILLA (object);

  switch (prop_id)
    {
    case PROP_SCINTILLA:
      overview_scintilla_set_src_sci (self, g_value_get_object (value));
      break;
    case PROP_CURSOR:
      overview_scintilla_set_cursor (self, g_value_get_enum (value));
      break;
    case PROP_VISIBLE_RECT:
      overview_scintilla_set_visible_rect (self, g_value_get_boxed (value));
      break;
    case PROP_WIDTH:
      overview_scintilla_set_width (self, g_value_get_uint (value));
      break;
    case PROP_ZOOM:
      overview_scintilla_set_zoom (self, g_value_get_int (value));
      break;
    case PROP_SHOW_TOOLTIP:
      overview_scintilla_set_show_tooltip (self, g_value_get_boolean (value));
      break;
    case PROP_OVERLAY_ENABLED:
      overview_scintilla_set_overlay_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_OVERLAY_COLOR:
      overview_scintilla_set_overlay_color (self, g_value_get_boxed (value));
      break;
    case PROP_OVERLAY_OUTLINE_COLOR:
      overview_scintilla_set_overlay_outline_color (self, g_value_get_boxed (value));
      break;
    case PROP_OVERLAY_INVERTED:
      overview_scintilla_set_overlay_inverted (self, g_value_get_boolean (value));
      break;
    case PROP_DOUBLE_BUFFERED:
      overview_scintilla_set_double_buffered (self, g_value_get_boolean (value));
      break;
    case PROP_SCROLL_LINES:
      overview_scintilla_set_scroll_lines (self, g_value_get_int (value));
      break;
    case PROP_SHOW_SCROLLBAR:
      overview_scintilla_set_show_scrollbar (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
overview_scintilla_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  OverviewScintilla *self = OVERVIEW_SCINTILLA (object);

  switch (prop_id)
    {
    case PROP_SCINTILLA:
      g_value_set_object (value, self->sci);
      break;
    case PROP_CURSOR:
      g_value_set_enum (value, overview_scintilla_get_cursor (self));
      break;
    case PROP_VISIBLE_RECT:
      {
        GdkRectangle rect;
        overview_scintilla_get_visible_rect (self, &rect);
        g_value_set_boxed (value, &rect);
        break;
      }
    case PROP_WIDTH:
      g_value_set_uint (value, overview_scintilla_get_width (self));
      break;
    case PROP_ZOOM:
      g_value_set_int (value, overview_scintilla_get_zoom (self));
      break;
    case PROP_SHOW_TOOLTIP:
      g_value_set_boolean (value, overview_scintilla_get_show_tooltip (self));
      break;
    case PROP_OVERLAY_ENABLED:
      g_value_set_boolean (value, overview_scintilla_get_overlay_enabled (self));
      break;
    case PROP_OVERLAY_COLOR:
      {
        OverviewColor color;
        overview_scintilla_get_overlay_color (self, &color);
        g_value_set_boxed (value, &color);
        break;
      }
    case PROP_OVERLAY_OUTLINE_COLOR:
      {
        OverviewColor color;
        overview_scintilla_get_overlay_outline_color (self, &color);
        g_value_set_boxed (value, &color);
        break;
      }
    case PROP_OVERLAY_INVERTED:
      g_value_set_boolean (value, overview_scintilla_get_overlay_inverted (self));
      break;
    case PROP_DOUBLE_BUFFERED:
      g_value_set_boolean (value, overview_scintilla_get_double_buffered (self));
      break;
    case PROP_SCROLL_LINES:
      g_value_set_int (value, overview_scintilla_get_scroll_lines (self));
      break;
    case PROP_SHOW_SCROLLBAR:
      g_value_set_boolean (value, overview_scintilla_get_show_scrollbar (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget *
overview_scintilla_new (ScintillaObject *src_sci)
{
  return g_object_new (OVERVIEW_TYPE_SCINTILLA, "scintilla", src_sci, NULL);
}

static gchar *
sci_get_font (ScintillaObject *sci,
              gint             style)
{
  gsize  len   = sci_send (sci, STYLEGETFONT, style, 0);
  gchar *name  = g_malloc0 (len + 1);
  sci_send (sci, STYLEGETFONT, style, name);
  return name;
}

static gboolean
on_src_sci_configure_event (GtkWidget         *widget,
                            GdkEventConfigure *event,
                            OverviewScintilla *self)
{
  overview_scintilla_sync_center (self);
  return FALSE;
}

static gboolean
on_src_sci_map_event (ScintillaObject   *sci,
                      GdkEvent          *event,
                      OverviewScintilla *self)
{
  if (self->conf_event == 0)
    {
      GtkWidget *internal;
      internal = overview_scintilla_find_drawing_area (GTK_WIDGET (sci));
      if (GTK_IS_DRAWING_AREA (internal))
        {
          self->src_canvas = internal;
          self->conf_event = g_signal_connect (self->src_canvas,
                                               "configure-event",
                                               G_CALLBACK (on_src_sci_configure_event),
                                               self);
        }
    }
  return FALSE;
}

static void
on_src_sci_notify (ScintillaObject   *sci,
                   gpointer           unused,
                   SCNotification    *nt,
                   OverviewScintilla *self)
{
  if (nt->nmhdr.code == SCN_UPDATEUI && nt->updated & SC_UPDATE_V_SCROLL)
    {
      overview_scintilla_sync_center (self);
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_queue_draw (self->canvas);
    }
}

static void
overview_scintilla_clone_styles (OverviewScintilla *self)
{
  ScintillaObject *sci     = SCINTILLA (self);
  ScintillaObject *src_sci = self->sci;

  for (gint i = 0; i < STYLE_MAX; i++)
    {
      gchar   *font_name = sci_get_font (src_sci, i);
      gint     font_size = sci_send (src_sci, STYLEGETSIZE, i, 0);
      gint     weight    = sci_send (src_sci, STYLEGETWEIGHT, i, 0);
      gboolean italic    = sci_send (src_sci, STYLEGETITALIC, i, 0);
      gint     fg_color  = sci_send (src_sci, STYLEGETFORE, i, 0);
      gint     bg_color  = sci_send (src_sci, STYLEGETBACK, i, 0);

      sci_send (sci, STYLESETFONT, i, font_name);
      sci_send (sci, STYLESETSIZE, i, font_size);
      sci_send (sci, STYLESETWEIGHT, i, weight);
      sci_send (sci, STYLESETITALIC, i, italic);
      sci_send (sci, STYLESETFORE, i, fg_color);
      sci_send (sci, STYLESETBACK, i, bg_color);
      sci_send (sci, STYLESETCHANGEABLE, i, 0);

      g_free (font_name);
    }
}

static void
overview_scintilla_queue_draw (OverviewScintilla *self)
{
  if (GTK_IS_WIDGET (self->canvas))
    gtk_widget_queue_draw (self->canvas);
  else
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
overview_scintilla_sync (OverviewScintilla *self)
{
  sptr_t doc_ptr;

  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  doc_ptr = sci_send (self->sci, GETDOCPOINTER, 0, 0);
  sci_send (self, SETDOCPOINTER, 0, doc_ptr);

  overview_scintilla_clone_styles (self);

  for (gint i = 0; i < SC_MAX_MARGIN; i++)
    sci_send (self, SETMARGINWIDTHN, i, 0);

  sci_send (self, SETVIEWEOL, 0, 0);
  sci_send (self, SETVIEWWS, 0, 0);
  sci_send (self, SETHSCROLLBAR, 0, 0);
  sci_send (self, SETVSCROLLBAR, 0, 0);
  sci_send (self, SETZOOM, self->zoom, 0);
  sci_send (self, SETCURSOR, SC_CURSORARROW, 0);
  sci_send (self, SETENDATLASTLINE, sci_send (self->sci, GETENDATLASTLINE, 0, 0), 0);
  sci_send (self, SETMOUSEDOWNCAPTURES, 0, 0);
  sci_send (self, SETCARETPERIOD, 0, 0);
  sci_send (self, SETCARETWIDTH, 0, 0);
  sci_send (self, SETEXTRAASCENT, 0, 0);
  sci_send (self, SETEXTRADESCENT, 0, 0);

  sci_send (self->sci, SETVSCROLLBAR, self->show_scrollbar, 0);

  overview_scintilla_update_cursor (self);
  overview_scintilla_update_rect (self);
  overview_scintilla_sync_center (self);

  overview_scintilla_queue_draw (self);
}

static void
overview_scintilla_set_src_sci (OverviewScintilla *self,
                                ScintillaObject   *sci)
{

  g_assert (! IS_SCINTILLA (self->sci));

  self->sci = g_object_ref (sci);

  overview_scintilla_sync (self);
  sci_send (self->sci, SETVSCROLLBAR, self->show_scrollbar, 0);

  gtk_widget_add_events (GTK_WIDGET (self->sci), GDK_STRUCTURE_MASK);
  plugin_signal_connect (geany_plugin,
                         G_OBJECT (self->sci),
                         "map-event",
                         TRUE,
                         G_CALLBACK (on_src_sci_map_event),
                         self);

  plugin_signal_connect (geany_plugin,
                         G_OBJECT (self->sci),
                         "sci-notify",
                         TRUE,
                         G_CALLBACK (on_src_sci_notify),
                         self);

  g_object_notify (G_OBJECT (self), "scintilla");
}

GdkCursorType
overview_scintilla_get_cursor (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), GDK_ARROW);
  return self->cursor;
}

void
overview_scintilla_set_cursor (OverviewScintilla *self,
                               GdkCursorType      cursor_type)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  if (cursor_type != self->cursor)
    {
      self->cursor = cursor_type;
      self->active_cursor = cursor_type;
      overview_scintilla_update_cursor (self);
      g_object_notify (G_OBJECT (self), "cursor");
    }
}

void
overview_scintilla_get_visible_rect (OverviewScintilla *self,
                                     GdkRectangle      *rect)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  g_return_if_fail (rect != NULL);

  memcpy (rect, &self->visible_rect, sizeof (GdkRectangle));
}

void
overview_scintilla_set_visible_rect (OverviewScintilla  *self,
                                     const GdkRectangle *rect)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (rect == NULL)
    {
      memset (&self->visible_rect, 0, sizeof (GdkRectangle));
      g_object_notify (G_OBJECT (self), "visible-rect");
      return;
    }

  if (rect->x != self->visible_rect.x ||
      rect->y != self->visible_rect.y ||
      rect->width != self->visible_rect.width ||
      rect->height != self->visible_rect.height)
    {
      memcpy (&self->visible_rect, rect, sizeof (GdkRectangle));
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_queue_draw (self->canvas);
      g_object_notify (G_OBJECT (self), "visible-rect");
    }
}

guint
overview_scintilla_get_width (OverviewScintilla *self)
{
  GtkAllocation alloc;
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), 0);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  return alloc.width;
}

void
overview_scintilla_set_width (OverviewScintilla *self,
                              guint              width)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  gtk_widget_set_size_request (GTK_WIDGET (self), width, -1);
}

gint
overview_scintilla_get_zoom (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), 0);
  self->zoom = sci_send (self, GETZOOM, 0, 0);
  return self->zoom;
}

void
overview_scintilla_set_zoom (OverviewScintilla *self,
                             gint               zoom)
{
  gint old_zoom;

  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  g_return_if_fail (zoom >= OVERVIEW_SCINTILLA_ZOOM_MIN &&
                    zoom <= OVERVIEW_SCINTILLA_ZOOM_MAX);

  old_zoom = sci_send (self, GETZOOM, 0, 0);
  if (zoom != old_zoom)
    {
      sci_send (self, SETZOOM, zoom, 0);
      self->zoom = sci_send (self, GETZOOM, 0, 0);
      if (self->zoom != old_zoom)
        {
          overview_scintilla_sync_center (self);
          g_object_notify (G_OBJECT (self), "zoom");
        }
    }
}

gboolean
overview_scintilla_get_show_tooltip (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), FALSE);
  return self->show_tooltip;
}

void
overview_scintilla_set_show_tooltip (OverviewScintilla *self,
                                     gboolean           show)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (show != self->show_tooltip)
    {
      self->show_tooltip = show;
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_set_has_tooltip (self->canvas, self->show_tooltip);
      g_object_notify (G_OBJECT (self), "show-tooltip");
    }
}

gboolean
overview_scintilla_get_overlay_enabled (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), FALSE);
  return self->overlay_enabled;
}

void
overview_scintilla_set_overlay_enabled (OverviewScintilla *self,
                                        gboolean           enabled)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (enabled != self->overlay_enabled)
    {
      self->overlay_enabled = enabled;
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_queue_draw (self->canvas);
      g_object_notify (G_OBJECT (self), "overlay-enabled");
    }
}

void
overview_scintilla_get_overlay_color (OverviewScintilla *self,
                                      OverviewColor     *color)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  g_return_if_fail (color != NULL);
  memcpy (color, &self->overlay_color, sizeof (OverviewColor));
}

void
overview_scintilla_set_overlay_color (OverviewScintilla   *self,
                                      const OverviewColor *color)
{
  gboolean changed = FALSE;
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (color == NULL)
    {
      memcpy (&self->overlay_color, &def_overlay_color, sizeof (OverviewColor));
      changed = TRUE;
    }
  else if (!overview_color_equal (color, &self->overlay_color))
    {
      memcpy (&self->overlay_color, color, sizeof (OverviewColor));
      changed = TRUE;
    }

  if (changed)
    {
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_queue_draw (self->canvas);
      g_object_notify (G_OBJECT (self), "overlay-color");
    }
}

void
overview_scintilla_get_overlay_outline_color (OverviewScintilla *self,
                                              OverviewColor     *color)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));
  g_return_if_fail (color != NULL);
  memcpy (color, &self->overlay_outline_color, sizeof (OverviewColor));
}

void
overview_scintilla_set_overlay_outline_color (OverviewScintilla   *self,
                                              const OverviewColor *color)
{
  gboolean changed = FALSE;
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (color == NULL)
    {
      memcpy (&self->overlay_outline_color, &def_overlay_outline_color, sizeof (OverviewColor));
      changed = TRUE;
    }
  else if (!overview_color_equal (color, &self->overlay_outline_color))
    {
      memcpy (&self->overlay_outline_color, color, sizeof (OverviewColor));
      changed = TRUE;
    }

  if (changed)
    {
      if (GTK_IS_WIDGET (self->canvas))
        gtk_widget_queue_draw (self->canvas);
      g_object_notify (G_OBJECT (self), "overlay-outline-color");
    }
}

gboolean
overview_scintilla_get_overlay_inverted (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), FALSE);
  return self->overlay_inverted;
}

void
overview_scintilla_set_overlay_inverted (OverviewScintilla *self,
                                         gboolean           inverted)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (inverted != self->overlay_inverted)
    {
      self->overlay_inverted = inverted;
      overview_scintilla_queue_draw (self);
      g_object_notify (G_OBJECT (self), "overlay-inverted");
    }
}

gboolean
overview_scintilla_get_double_buffered (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), FALSE);
  if (GTK_IS_WIDGET (self->canvas))
    self->double_buffered = gtk_widget_get_double_buffered (self->canvas);
  return self->double_buffered;
}

void
overview_scintilla_set_double_buffered (OverviewScintilla *self,
                                        gboolean           enabled)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (enabled != self->double_buffered)
    {
      self->double_buffered = enabled;
      if (GTK_IS_WIDGET (self->canvas))
        {
          gtk_widget_set_double_buffered (self->canvas, self->double_buffered);
          self->double_buffered = gtk_widget_get_double_buffered (self->canvas);
        }
      if (self->double_buffered == enabled)
        g_object_notify (G_OBJECT (self), "double-buffered");
    }
}

gint
overview_scintilla_get_scroll_lines (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), -1);
  return self->scroll_lines;
}

void
overview_scintilla_set_scroll_lines (OverviewScintilla *self,
                                     gint               lines)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (lines < 0)
    lines = OVERVIEW_SCINTILLA_SCROLL_LINES;

  if (lines != self->scroll_lines)
    {
      self->scroll_lines = lines;
      g_object_notify (G_OBJECT (self), "scroll-lines");
    }
}

gboolean
overview_scintilla_get_show_scrollbar (OverviewScintilla *self)
{
  g_return_val_if_fail (OVERVIEW_IS_SCINTILLA (self), FALSE);
  return self->show_scrollbar;
}

void
overview_scintilla_set_show_scrollbar (OverviewScintilla *self,
                                       gboolean           show)
{
  g_return_if_fail (OVERVIEW_IS_SCINTILLA (self));

  if (show != self->show_scrollbar)
    {
      self->show_scrollbar = show;
      sci_send (self->sci, SETVSCROLLBAR, self->show_scrollbar, 0);
      gtk_widget_queue_draw (GTK_WIDGET (self->sci));
      g_object_notify (G_OBJECT (self), "show-scrollbar");
    }
}
