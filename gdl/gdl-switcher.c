/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8 -*- */
/* gdl-switcher.c
 *
 * Copyright (C) 2003  Ettore Perazzoli,
 *               2007  Naba Kumar
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copied and adapted from ESidebar.[ch] from evolution
 *
 * Authors: Ettore Perazzoli <ettore@ximian.com>
 *          Naba Kumar  <naba@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "gdl-switcher.h"
#include "libgdlmarshal.h"
#include "libgdltypebuiltins.h"

#include <gtk/gtk.h>

/**
 * SECTION:gdl-switcher
 * @title: GdlSwitcher
 * @short_description: An improved notebook widget.
 * @stability: Unstable
 * @see_also: #GdlDockNotebook
 *
 * A #GdlSwitcher widget is an improved version of the #GtkNotebook. The
 * tab labels could have different style and could be layouted on several lines.
 *
 * It is used by the #GdlDockNotebook widget to dock other widgets.
 */


static void gdl_switcher_set_property  (GObject            *object,
                                        guint               prop_id,
                                        const GValue       *value,
                                        GParamSpec         *pspec);
static void gdl_switcher_get_property  (GObject            *object,
                                        guint               prop_id,
                                        GValue             *value,
                                        GParamSpec         *pspec);

static void gdl_switcher_add_button  (GdlSwitcher *switcher,
                                      const gchar *label,
                                      const gchar *tooltips,
                                      const gchar *stock_id,
                                      GdkPixbuf *pixbuf_icon,
                                      gint switcher_id,
                                      GtkWidget *page);
/* static void gdl_switcher_remove_button (GdlSwitcher *switcher, gint switcher_id); */
static void gdl_switcher_select_page (GdlSwitcher *switcher, gint switcher_id);
static void gdl_switcher_select_button (GdlSwitcher *switcher, gint switcher_id);
static void gdl_switcher_set_show_buttons (GdlSwitcher *switcher, gboolean show);
static void gdl_switcher_set_style (GdlSwitcher *switcher,
                                    GdlSwitcherStyle switcher_style);
static GdlSwitcherStyle gdl_switcher_get_style (GdlSwitcher *switcher);
static void gdl_switcher_set_tab_pos (GdlSwitcher *switcher, GtkPositionType pos);
static void gdl_switcher_set_tab_reorderable (GdlSwitcher *switcher, gboolean reorderable);
static void gdl_switcher_update_lone_button_visibility (GdlSwitcher *switcher);

enum {
    PROP_0,
    PROP_SWITCHER_STYLE,
    PROP_TAB_POS,
    PROP_TAB_REORDERABLE
};

typedef struct {
    GtkWidget *button_widget;
    GtkWidget *label;
    GtkWidget *icon;
    GtkWidget *arrow;
    GtkWidget *hbox;
    GtkWidget *page;
    int id;
} Button;

struct _GdlSwitcherPrivate {
    GdlSwitcherStyle switcher_style;
    GdlSwitcherStyle toolbar_style;
    GtkPositionType tab_pos;
    gboolean tab_reorderable;

    gboolean show;
    GSList *buttons;

    guint style_changed_id;
    gint buttons_height_request;
    gboolean in_toggle;
};

struct _GdlSwitcherClassPrivate {
    GtkCssProvider *css;
};

G_DEFINE_TYPE_WITH_CODE (GdlSwitcher, gdl_switcher, GTK_TYPE_NOTEBOOK,
                         g_type_add_class_private (g_define_type_id, sizeof (GdlSwitcherClassPrivate)))

#define INTERNAL_MODE(switcher)  (switcher->priv->switcher_style == \
            GDL_SWITCHER_STYLE_TOOLBAR ? switcher->priv->toolbar_style : \
            switcher->priv->switcher_style)

#define H_PADDING 0
#define V_PADDING 0
#define V_SPACING 2

/* Utility functions.  */

static void
gdl_switcher_long_name_changed (GObject* object,
                                GParamSpec* spec,
                                gpointer user_data)
{
    Button* button = user_data;
    gchar* label;

    g_object_get (object, "long-name", &label, NULL);
    gtk_label_set_text (GTK_LABEL (button->label), label);
    g_free (label);
}

static void
gdl_switcher_stock_id_changed (GObject* object,
                               GParamSpec* spec,
                               gpointer user_data)
{
    Button* button = user_data;
    gchar* id;

    g_object_get (object, "stock-id", &id, NULL);
    gtk_image_set_from_stock (GTK_IMAGE(button->icon), id, GTK_ICON_SIZE_MENU);
    g_free (id);
}

static void
gdl_switcher_visible_changed (GObject* object,
                               GParamSpec* spec,
                               gpointer user_data)
{
    Button* button = user_data;
    GdlSwitcher* switcher;

    if (gtk_widget_get_visible (button->page))
    {
        gtk_widget_show_all (button->button_widget);
    }
    else
    {
        gtk_widget_hide (button->button_widget);
    }
    switcher = GDL_SWITCHER (gtk_widget_get_parent (button->button_widget));
    gdl_switcher_update_lone_button_visibility (switcher);
}


static Button *
button_new (GtkWidget *button_widget, GtkWidget *label, GtkWidget *icon,
            GtkWidget *arrow, GtkWidget *hbox, int id, GtkWidget *page)
{
    Button *button = g_new (Button, 1);

    button->button_widget = button_widget;
    button->label = label;
    button->icon = icon;
    button->arrow = arrow;
    button->hbox = hbox;
    button->id = id;
    button->page = page;

    g_signal_connect (page, "notify::long-name", G_CALLBACK (gdl_switcher_long_name_changed),
                      button);
    g_signal_connect (page, "notify::stock-id", G_CALLBACK (gdl_switcher_stock_id_changed),
                      button);
    g_signal_connect (page, "notify::visible", G_CALLBACK (gdl_switcher_visible_changed),
                      button);

    g_object_ref (button_widget);
    g_object_ref (label);
    g_object_ref (icon);
    g_object_ref (arrow);
    g_object_ref (hbox);

    return button;
}

static void
button_free (Button *button)
{
    g_signal_handlers_disconnect_by_func (button->page,
                                          gdl_switcher_long_name_changed,
                                          button);
    g_signal_handlers_disconnect_by_func (button->page,
                                          gdl_switcher_stock_id_changed,
                                          button);
    g_signal_handlers_disconnect_by_func (button->page,
                                          gdl_switcher_visible_changed,
                                          button);

    g_object_unref (button->button_widget);
    g_object_unref (button->label);
    g_object_unref (button->icon);
    g_object_unref (button->hbox);
    g_free (button);
}

static gint
gdl_switcher_get_page_id (GtkWidget *widget)
{
    static gint switcher_id_count = 0;
    gint switcher_id;
    switcher_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                      "__switcher_id"));
    if (switcher_id <= 0) {
        switcher_id = ++switcher_id_count;
        g_object_set_data (G_OBJECT (widget), "__switcher_id",
                           GINT_TO_POINTER (switcher_id));
    }
    return switcher_id;
}

/* Hide switcher button if they are not needed (only one visible page)
 * or show switcher button if there are two visible pages */
static void
gdl_switcher_update_lone_button_visibility (GdlSwitcher *switcher)
{
    GSList *p;
    GtkWidget *alone = NULL;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (gtk_widget_get_visible (button->page))
        {
            if (alone == NULL)
            {
                alone = button->button_widget;
            }
            else
            {
                gtk_widget_show (alone);
                gtk_widget_show (button->button_widget);
                alone = NULL;
                break;
            }
        }
    }

    if (alone) gtk_widget_hide (alone);
}

static void
update_buttons (GdlSwitcher *switcher, int new_selected_id)
{
    GSList *p;

    switcher->priv->in_toggle = TRUE;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (button->id == new_selected_id) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (button->button_widget), TRUE);
            gtk_widget_set_sensitive (button->arrow, TRUE);
        } else {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (button->button_widget), FALSE);
            gtk_widget_set_sensitive (button->arrow, FALSE);
        }
    }

    switcher->priv->in_toggle = FALSE;
}

/* Callbacks.  */

static void
button_toggled_callback (GtkToggleButton *toggle_button,
                         GdlSwitcher *switcher)
{
    int id = 0;
    gboolean is_active = FALSE;
    GSList *p;

    if (switcher->priv->in_toggle)
        return;

    switcher->priv->in_toggle = TRUE;

    if (gtk_toggle_button_get_active (toggle_button))
        is_active = TRUE;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (button->button_widget != GTK_WIDGET (toggle_button)) {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (button->button_widget), FALSE);
            gtk_widget_set_sensitive (button->arrow, FALSE);
        } else {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (button->button_widget), TRUE);
            gtk_widget_set_sensitive (button->arrow, TRUE);
            id = button->id;
        }
    }

    switcher->priv->in_toggle = FALSE;

    if (is_active)
    {
        gdl_switcher_select_page (switcher, id);
    }
}

static gboolean
delayed_resize_switcher (gpointer data)
{
    gtk_widget_queue_resize (GTK_WIDGET (data));
    return FALSE;
}

/* Returns -1 if layout didn't happen because a resize request was queued */
static int
layout_buttons (GdlSwitcher *switcher, GtkAllocation* allocation)
{
    gint min_height, nat_height;
    GdlSwitcherStyle switcher_style;
    gboolean icons_only;
    int num_btns;
    int btns_per_row;
    GSList **rows, *p;
    Button *button;
    int row_number;
    int max_btn_width = 0, max_btn_height = 0;
    int optimal_layout_width = 0;
    int row_last;
    int x, y;
    int i;
    int rows_count;
    int last_buttons_height;

    last_buttons_height = switcher->priv->buttons_height_request;

    GTK_WIDGET_CLASS (gdl_switcher_parent_class)->get_preferred_height (GTK_WIDGET (switcher), &min_height, &nat_height);

    y = allocation->y + allocation->height - V_PADDING - 1;

    /* Return bottom of the area if there isn't any visible button */
    for (p = switcher->priv->buttons; p != NULL; p = p->next) if (gtk_widget_get_visible (((Button *)p->data)->button_widget)) break;
    if (p == NULL)
        return y;

    switcher_style = INTERNAL_MODE (switcher);
    icons_only = (switcher_style == GDL_SWITCHER_STYLE_ICON);

    /* Figure out the max width and height taking into account visible buttons */
    optimal_layout_width = H_PADDING;
    num_btns = 0;
    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        GtkRequisition requisition;

        button = p->data;
        if (gtk_widget_get_visible (button->button_widget)) {
            gtk_widget_get_preferred_size (GTK_WIDGET (button->button_widget),
                                           &requisition, NULL);
            optimal_layout_width += requisition.width + H_PADDING;
            max_btn_height = MAX (max_btn_height, requisition.height);
            max_btn_width = MAX (max_btn_width, requisition.width);
            num_btns++;
        }
    }

    /* Figure out how many rows and columns we'll use. */
    btns_per_row = allocation->width / (max_btn_width + H_PADDING);
    /* Use at least one column */
    if (btns_per_row == 0) btns_per_row = 1;

    /* If all the buttons could fit in the single row, have it so */
    if (allocation->width >= optimal_layout_width)
    {
        btns_per_row = num_btns;
    }
    if (!icons_only) {
        /* If using text buttons, we want to try to have a
         * completely filled-in grid, but if we can't, we want
         * the odd row to have just a single button.
         */
        while (num_btns % btns_per_row > 1)
            btns_per_row--;
    }

    rows_count = num_btns / btns_per_row;
    if (num_btns % btns_per_row != 0)
	rows_count++;

    /* Assign visible buttons to rows */
    rows = g_new0 (GSList *, rows_count);

    if (!icons_only && num_btns % btns_per_row != 0) {
        p = switcher->priv->buttons;
        for (; p != NULL; p = p->next) if (gtk_widget_get_visible (((Button *)p->data)->button_widget)) break;
        button = p->data;
        rows [0] = g_slist_append (rows [0], button->button_widget);

        p = p->next;
        row_number = p ? 1 : 0;
    } else {
        p = switcher->priv->buttons;
        row_number = 0;
    }

    for (; p != NULL; p = p->next) {
        button = p->data;

        if (gtk_widget_get_visible (button->button_widget)) {
            if (g_slist_length (rows [row_number]) == btns_per_row)
                row_number ++;

            rows [row_number] = g_slist_append (rows [row_number],
                                                button->button_widget);
        }
    }

    row_last = row_number;

    /* If there are more than 1 row of buttons, save the current height
     * requirement for subsequent size requests.
     */
    if (row_last > 0)
    {
        switcher->priv->buttons_height_request =
            (row_last + 1) * (max_btn_height + V_PADDING) + 1;
    } else { /* Otherwize clear it */
        if (last_buttons_height >= 0) {

            switcher->priv->buttons_height_request = -1;
        }
    }

    /* If it turns out that we now require smaller height for the buttons
     * than it was last time, make a resize request to ensure our
     * size requisition is properly communicated to the parent (otherwise
     * parent tend to keep assuming the older size).
     */
    if (last_buttons_height > switcher->priv->buttons_height_request)
    {
        g_idle_add (delayed_resize_switcher, switcher);
        return -1;
    }

    /* Layout the buttons. */
    for (i = row_last; i >= 0; i --) {
        int len, extra_width;

        y -= max_btn_height;

        /* Check for possible size over flow (taking into account client
         * requisition
         */
        if (y < (allocation->y + min_height)) {
            /* We have an overflow: Insufficient allocation */
            if (last_buttons_height < switcher->priv->buttons_height_request) {
                /* Request for a new resize */
                g_idle_add (delayed_resize_switcher, switcher);

                return -1;
            }
        }
        x = H_PADDING + allocation->x;
        len = g_slist_length (rows[i]);
        if (switcher_style == GDL_SWITCHER_STYLE_TEXT ||
            switcher_style == GDL_SWITCHER_STYLE_BOTH)
            extra_width = (allocation->width - (len * max_btn_width )
                           - (len * H_PADDING)) / len;
        else
            extra_width = 0;
        for (p = rows [i]; p != NULL; p = p->next) {
            GtkAllocation child_allocation;
            GtkStyleContext *style_context;
            GtkJunctionSides junction = 0;

            child_allocation.x = x;
            child_allocation.y = y;
            if (rows_count == 1 && row_number == 0)
            {
                GtkRequisition child_requisition;
                gtk_widget_get_preferred_size (GTK_WIDGET (p->data),
                                               &child_requisition, NULL);
                child_allocation.width = child_requisition.width;
            }
            else
            {
                child_allocation.width = max_btn_width + extra_width;
            }
            child_allocation.height = max_btn_height;

            style_context = gtk_widget_get_style_context (GTK_WIDGET (p->data));

            if (row_last) {
                if (i) {
                    junction |= GTK_JUNCTION_TOP;
                }
                if (i != row_last) {
                    junction |= GTK_JUNCTION_BOTTOM;
                }
            }

            if (p->next) {
                junction |= GTK_JUNCTION_RIGHT;
            }

            if (p != rows [i]) {
                junction |= GTK_JUNCTION_LEFT;
            }

            gtk_style_context_set_junction_sides (style_context, junction);

            gtk_widget_size_allocate (GTK_WIDGET (p->data), &child_allocation);

            x += child_allocation.width + H_PADDING;
        }
    }

    y -= V_SPACING;

    for (i = 0; i <= row_last; i ++)
        g_slist_free (rows [i]);
    g_free (rows);

    return y;
}

static void
do_layout (GdlSwitcher *switcher, GtkAllocation* allocation)
{

    GtkAllocation child_allocation;
    int y;

    if (switcher->priv->show) {
        y = layout_buttons (switcher, allocation);
        if (y < 0) /* Layout did not happen and a resize was requested */
            return;
    }
    else
        y = allocation->y + allocation->height;

    /* Place the parent widget.  */
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y;
    child_allocation.width = allocation->width;
    child_allocation.height = y - allocation->y;

    GTK_WIDGET_CLASS (gdl_switcher_parent_class)->size_allocate (GTK_WIDGET (switcher), &child_allocation);
}

/* GtkContainer methods.  */

static void
gdl_switcher_forall (GtkContainer *container, gboolean include_internals,
                     GtkCallback callback, void *callback_data)
{
    GdlSwitcher *switcher =
        GDL_SWITCHER (container);
    GSList *p;

    GTK_CONTAINER_CLASS (gdl_switcher_parent_class)->forall (GTK_CONTAINER (switcher),
                                                             include_internals,
                                                             callback, callback_data);
    if (include_internals) {
        for (p = switcher->priv->buttons; p != NULL; p = p->next) {
            GtkWidget *widget = ((Button *) p->data)->button_widget;
            (* callback) (widget, callback_data);
        }
    }
}

static void
gdl_switcher_remove (GtkContainer *container, GtkWidget *widget)
{
    gint switcher_id;
    GdlSwitcher *switcher =
        GDL_SWITCHER (container);
    GSList *p;

    switcher_id = gdl_switcher_get_page_id (widget);
    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *b = (Button *) p->data;

        if (b->id == switcher_id) {
            gtk_widget_unparent (b->button_widget);
            switcher->priv->buttons =
                g_slist_remove_link (switcher->priv->buttons, p);
            button_free (b);
            gtk_widget_queue_resize (GTK_WIDGET (switcher));
            break;
        }
    }
    gdl_switcher_update_lone_button_visibility (switcher);
    GTK_CONTAINER_CLASS (gdl_switcher_parent_class)->remove (GTK_CONTAINER (switcher), widget);
}

/* GtkWidget methods.  */

static void
gdl_switcher_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural)
{
    GdlSwitcher *switcher = GDL_SWITCHER (widget);
    GSList *p;
    gint button_height = 0;

    GTK_WIDGET_CLASS (gdl_switcher_parent_class)->get_preferred_width (GTK_WIDGET (switcher), minimum, natural);

    if (!switcher->priv->show)
        return;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (gtk_widget_get_visible (button->button_widget)) {
            gint min, nat;

            gtk_widget_get_preferred_width(button->button_widget, &min, &nat);
            *minimum = MAX (*minimum, min + 2 * H_PADDING);
            *natural = MAX (*natural, nat + 2 * H_PADDING);
		}
    }
}

static void
gdl_switcher_get_preferred_height (GtkWidget *widget, gint *minimum, gint *natural)
{
    GdlSwitcher *switcher = GDL_SWITCHER (widget);
    GSList *p;
    gint button_min = 0;
    gint button_nat = 0;

    GTK_WIDGET_CLASS (gdl_switcher_parent_class)->get_preferred_height (GTK_WIDGET (switcher), minimum, natural);

    if (!switcher->priv->show)
        return;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (gtk_widget_get_visible (button->button_widget)) {
            gint min, nat;

            gtk_widget_get_preferred_height (button->button_widget, &min, &nat);
            button_min = MAX (button_min, min + 2 * V_PADDING);
            button_nat = MAX (button_nat, nat + 2 * V_PADDING);
        }
    }

    if (switcher->priv->buttons_height_request > 0) {
        *minimum += switcher->priv->buttons_height_request;
        *natural += switcher->priv->buttons_height_request;
    } else {
        *minimum += button_min + V_PADDING;
        *natural += button_nat + V_PADDING;
    }
}

static void
gdl_switcher_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    do_layout (GDL_SWITCHER (widget), allocation);
    gtk_widget_set_allocation (widget, allocation);
}

static gint
gdl_switcher_draw (GtkWidget *widget, cairo_t *cr)
{
    GSList *p;
    GdlSwitcher *switcher = GDL_SWITCHER (widget);
    if (switcher->priv->show) {
        for (p = switcher->priv->buttons; p != NULL; p = p->next) {
            GtkWidget *button = ((Button *) p->data)->button_widget;
            gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                          button, cr);
        }
    }
    return GTK_WIDGET_CLASS (gdl_switcher_parent_class)->draw (widget, cr);
}

static void
gdl_switcher_map (GtkWidget *widget)
{
    GSList *p;
    GdlSwitcher *switcher = GDL_SWITCHER (widget);

    if (switcher->priv->show) {
        for (p = switcher->priv->buttons; p != NULL; p = p->next) {
            GtkWidget *button = ((Button *) p->data)->button_widget;
            if (gtk_widget_get_visible (button) &&
                !gtk_widget_get_mapped (button))
                gtk_widget_map (button);
        }
    }
    GTK_WIDGET_CLASS (gdl_switcher_parent_class)->map (widget);
}

/* GObject methods.  */

static void
gdl_switcher_set_property  (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GdlSwitcher *switcher = GDL_SWITCHER (object);

    switch (prop_id) {
        case PROP_SWITCHER_STYLE:
            gdl_switcher_set_style (switcher, g_value_get_enum (value));
            break;
        case PROP_TAB_POS:
            gdl_switcher_set_tab_pos (switcher, g_value_get_enum (value));
            break;
        case PROP_TAB_REORDERABLE:
            gdl_switcher_set_tab_reorderable (switcher, g_value_get_boolean (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gdl_switcher_get_property  (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GdlSwitcher *switcher = GDL_SWITCHER (object);

    switch (prop_id) {
        case PROP_SWITCHER_STYLE:
            g_value_set_enum (value, gdl_switcher_get_style (switcher));
            break;
        case PROP_TAB_POS:
            g_value_set_enum (value, switcher->priv->tab_pos);
            break;
        case PROP_TAB_REORDERABLE:
            g_value_set_enum (value, switcher->priv->tab_reorderable);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gdl_switcher_dispose (GObject *object)
{
    GdlSwitcherPrivate *priv = GDL_SWITCHER (object)->priv;

#if HAVE_GNOME
    GConfClient *gconf_client = gconf_client_get_default ();

    if (priv->style_changed_id) {
        gconf_client_notify_remove (gconf_client, priv->style_changed_id);
        priv->style_changed_id = 0;
    }
    g_object_unref (gconf_client);
#endif

    g_slist_free_full (priv->buttons, (GDestroyNotify)button_free);
    priv->buttons = NULL;

    G_OBJECT_CLASS (gdl_switcher_parent_class)->dispose (object);
}

static void
gdl_switcher_finalize (GObject *object)
{
    G_OBJECT_CLASS (gdl_switcher_parent_class)->finalize (object);
}

/* Signal handlers */

static void
gdl_switcher_notify_cb (GObject *g_object, GParamSpec *pspec,
                        GdlSwitcher *switcher)
{
}

static void
gdl_switcher_switch_page_cb (GtkNotebook *nb, GtkWidget *page_widget,
                             gint page_num, GdlSwitcher *switcher)
{
    gint             switcher_id;

    /* Change switcher button */
    switcher_id = gdl_switcher_get_page_id (page_widget);
    gdl_switcher_select_button (GDL_SWITCHER (switcher), switcher_id);
}

static void
gdl_switcher_page_added_cb (GtkNotebook *nb, GtkWidget *page,
                            gint page_num, GdlSwitcher *switcher)
{
    gint         switcher_id;

    (void)nb;
    (void)page_num;
    switcher_id = gdl_switcher_get_page_id (page);

    gdl_switcher_add_button (GDL_SWITCHER (switcher), NULL, NULL, NULL, NULL,
                             switcher_id, page);
    gdl_switcher_select_button (GDL_SWITCHER (switcher), switcher_id);
}

static void
gdl_switcher_select_page (GdlSwitcher *switcher, gint id)
{
    GList *children, *node;
    children = gtk_container_get_children (GTK_CONTAINER (switcher));
    node = children;
    while (node)
    {
        gint switcher_id;
        switcher_id = gdl_switcher_get_page_id (GTK_WIDGET (node->data));
        if (switcher_id == id)
        {
            gint page_num;
            page_num = gtk_notebook_page_num (GTK_NOTEBOOK (switcher),
                                              GTK_WIDGET (node->data));
            g_signal_handlers_block_by_func (switcher,
                                             gdl_switcher_switch_page_cb,
                                             switcher);
            gtk_notebook_set_current_page (GTK_NOTEBOOK (switcher), page_num);
            g_signal_handlers_unblock_by_func (switcher,
                                               gdl_switcher_switch_page_cb,
                                               switcher);
            break;
        }
        node = g_list_next (node);
    }
    g_list_free (children);
}

/* Initialization.  */

static void
gdl_switcher_class_init (GdlSwitcherClass *klass)
{
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    static const gchar button_style[] =
       "* {\n"
           "-GtkWidget-focus-line-width : 1;\n"
           "-GtkWidget-focus-padding : 1;\n"
           "padding: 0;\n"
       "}";

    container_class->forall = gdl_switcher_forall;
    container_class->remove = gdl_switcher_remove;

    widget_class->get_preferred_width = gdl_switcher_get_preferred_width;
    widget_class->get_preferred_height = gdl_switcher_get_preferred_height;
    widget_class->size_allocate = gdl_switcher_size_allocate;
    widget_class->draw = gdl_switcher_draw;
    widget_class->map = gdl_switcher_map;

    object_class->dispose  = gdl_switcher_dispose;
    object_class->finalize = gdl_switcher_finalize;
    object_class->set_property = gdl_switcher_set_property;
    object_class->get_property = gdl_switcher_get_property;

    g_object_class_install_property (
        object_class, PROP_SWITCHER_STYLE,
        g_param_spec_enum ("switcher-style", _("Switcher Style"),
                           _("Switcher buttons style"),
                           GDL_TYPE_SWITCHER_STYLE,
                           GDL_SWITCHER_STYLE_BOTH,
                           G_PARAM_READWRITE));

    g_object_class_install_property (
        object_class, PROP_TAB_POS,
        g_param_spec_enum ("tab-pos", _("Tab Position"),
                           _("Which side of the notebook holds the tabs"),
                           GTK_TYPE_POSITION_TYPE,
                           GTK_POS_BOTTOM,
                           G_PARAM_READWRITE));

    g_object_class_install_property (
        object_class, PROP_TAB_REORDERABLE,
        g_param_spec_boolean ("tab-reorderable", _("Tab reorderable"),
                              _("Whether the tab is reorderable by user action"),
                              FALSE,
                              G_PARAM_READWRITE));

    g_type_class_add_private (object_class, sizeof (GdlSwitcherPrivate));

    /* set the style */
    klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, GDL_TYPE_SWITCHER, GdlSwitcherClassPrivate);

    klass->priv->css = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (klass->priv->css, button_style, -1, NULL);
}

static void
gdl_switcher_init (GdlSwitcher *switcher)
{
    GdlSwitcherPrivate *priv;

    gtk_widget_set_has_window (GTK_WIDGET (switcher), FALSE);

    switcher->priv = G_TYPE_INSTANCE_GET_PRIVATE (switcher,
                                                  GDL_TYPE_SWITCHER,
                                                  GdlSwitcherPrivate);
    priv = switcher->priv;

    priv->show = TRUE;
    priv->buttons_height_request = -1;
    priv->tab_pos = GTK_POS_BOTTOM;
    priv->tab_reorderable = FALSE;

    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (switcher), GTK_POS_BOTTOM);
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (switcher), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (switcher), FALSE);
    gdl_switcher_set_style (switcher, GDL_SWITCHER_STYLE_BOTH);

    /* notebook signals */
    g_signal_connect (switcher, "switch-page",
                      G_CALLBACK (gdl_switcher_switch_page_cb), switcher);
    g_signal_connect (switcher, "page-added",
                      G_CALLBACK (gdl_switcher_page_added_cb), switcher);
    g_signal_connect (switcher, "notify::show-tabs",
                      G_CALLBACK (gdl_switcher_notify_cb), switcher);
}

/**
 * gdl_switcher_new:
 *
 * Creates a new notebook widget with no pages.
 *
 * Returns: The newly created #GdlSwitcher
 */
GtkWidget *
gdl_switcher_new (void)
{
    return g_object_new (gdl_switcher_get_type (), NULL);
}

/**
 * gdl_switcher_add_button:
 * @switcher: The #GdlSwitcher to which a button will be added
 * @label: The label for the button
 * @tooltips: The tooltip for the button
 * @stock_id: The stock ID for the button
 * @pixbuf_icon: The pixbuf to use for the button icon
 * @switcher_id: The ID of the switcher
 * @page: The notebook page
 *
 * Adds a button to a #GdlSwitcher.  The button icon is taken preferentially
 * from the @stock_id parameter.  If this is %NULL, then the @pixbuf_icon
 * parameter is used.  Failing that, the %GTK_STOCK_NEW stock icon is used.
 * The text label for the button is specified using the @label parameter.  If
 * it is %NULL then a default incrementally numbered label is used instead.
 */
static void
gdl_switcher_add_button (GdlSwitcher *switcher, const gchar *label,
                         const gchar *tooltips, const gchar *stock_id,
			 GdkPixbuf *pixbuf_icon,
                         gint switcher_id, GtkWidget* page)
{
    GtkWidget *event_box;
    GtkWidget *button_widget;
    GtkWidget *hbox;
    GtkWidget *icon_widget;
    GtkWidget *label_widget;
    GtkWidget *arrow;

    button_widget = gtk_toggle_button_new ();
    gtk_button_set_relief (GTK_BUTTON(button_widget), GTK_RELIEF_HALF);
    if (switcher->priv->show && gtk_widget_get_visible (page))
        gtk_widget_show (button_widget);
    g_signal_connect (button_widget, "toggled",
                      G_CALLBACK (button_toggled_callback),
                      switcher);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
    gtk_container_add (GTK_CONTAINER (button_widget), hbox);
    gtk_widget_show (hbox);

    if (stock_id) {
        icon_widget = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
    } else if (pixbuf_icon) {
        icon_widget = gtk_image_new_from_pixbuf (pixbuf_icon);
    } else {
        icon_widget = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
    }

    gtk_widget_show (icon_widget);

    if (!label) {
        gchar *text = g_strdup_printf ("Item %d", switcher_id);
        label_widget = gtk_label_new (text);
        g_free (text);
    } else {
        label_widget = gtk_label_new (label);
    }
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
    gtk_widget_show (label_widget);


    gtk_widget_set_tooltip_text (button_widget,
                                 tooltips);

    switch (INTERNAL_MODE (switcher)) {
    case GDL_SWITCHER_STYLE_TEXT:
        gtk_box_pack_start (GTK_BOX (hbox), label_widget, TRUE, TRUE, 0);
        break;
    case GDL_SWITCHER_STYLE_ICON:
        gtk_box_pack_start (GTK_BOX (hbox), icon_widget, TRUE, TRUE, 0);
        break;
    case GDL_SWITCHER_STYLE_BOTH:
    default:
        gtk_box_pack_start (GTK_BOX (hbox), icon_widget, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), label_widget, TRUE, TRUE, 0);
        break;
    }
    arrow = gtk_arrow_new (GTK_ARROW_UP, GTK_SHADOW_NONE);
    gtk_widget_show (arrow);
    gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);

    switcher->priv->buttons =
        g_slist_append (switcher->priv->buttons,
                        button_new (button_widget, label_widget,
                                    icon_widget,
                                    arrow, hbox, switcher_id, page));

    gtk_widget_set_parent (button_widget, GTK_WIDGET (switcher));
    gdl_switcher_update_lone_button_visibility (switcher);
    gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

#if 0
static void
gdl_switcher_remove_button (GdlSwitcher *switcher, gint switcher_id)
{
    GSList *p;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (button->id == switcher_id)
        {
            gtk_container_remove (GTK_CONTAINER (switcher),
                                  button->button_widget);
            break;
        }
    }
    gtk_widget_queue_resize (GTK_WIDGET (switcher));
}
#endif

static void
gdl_switcher_select_button (GdlSwitcher *switcher, gint switcher_id)
{
    update_buttons (switcher, switcher_id);

    /* Select the notebook page associated with this button */
    gdl_switcher_select_page (switcher, switcher_id);
}


/**
 * gdl_switcher_insert_page:
 * @switcher: The switcher to which a page will be added
 * @page: The page to add to the switcher
 * @tab_widget: The  to add to the switcher
 * @label: The label text for the button
 * @tooltips: The tooltip for the button
 * @stock_id: The stock ID for the button icon
 * @pixbuf_icon: The pixbuf to use for the button icon
 * @position: The position at which to create the page
 *
 * Adds a page to a #GdlSwitcher.  A button is created in the switcher, with its
 * icon taken preferentially from the @stock_id parameter.  If this parameter is
 * %NULL, then the @pixbuf_icon parameter is used.  Failing that, the
 * %GTK_STOCK_NEW stock icon is used.  The text label for the button is specified
 * using the @label parameter.  If it is %NULL then a default incrementally
 * numbered label is used instead.
 *
 * Returns: The index (starting from 0) of the appended page in the notebook, or -1 if function fails
 */
gint
gdl_switcher_insert_page (GdlSwitcher *switcher, GtkWidget *page,
                          GtkWidget *tab_widget, const gchar *label,
                          const gchar *tooltips, const gchar *stock_id,
                          GdkPixbuf *pixbuf_icon, gint position)
{
    GtkNotebook *notebook = GTK_NOTEBOOK (switcher);
    gint ret_position;
    gint switcher_id;
    g_signal_handlers_block_by_func (switcher,
                                     gdl_switcher_page_added_cb,
                                     switcher);

    if (!tab_widget) {
        tab_widget = gtk_label_new (label);
        if (gtk_widget_get_visible (page)) gtk_widget_show (tab_widget);
    }
    switcher_id = gdl_switcher_get_page_id (page);
    gdl_switcher_add_button (switcher, label, tooltips, stock_id, pixbuf_icon, switcher_id, page);

    ret_position = gtk_notebook_insert_page (notebook, page,
                                             tab_widget, position);
    gtk_notebook_set_tab_reorderable (notebook, page,
                                      switcher->priv->tab_reorderable);
    g_signal_handlers_unblock_by_func (switcher,
                                       gdl_switcher_page_added_cb,
                                       switcher);

    return ret_position;
}

static void
set_switcher_style_toolbar (GdlSwitcher *switcher,
                            GdlSwitcherStyle switcher_style)
{
    GSList *p;

    if (switcher_style == GDL_SWITCHER_STYLE_NONE
        || switcher_style == GDL_SWITCHER_STYLE_TABS)
        return;

    if (switcher_style == GDL_SWITCHER_STYLE_TOOLBAR)
        switcher_style = GDL_SWITCHER_STYLE_BOTH;

    if (switcher_style == INTERNAL_MODE (switcher))
        return;

    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (switcher), FALSE);

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        gtk_container_remove (GTK_CONTAINER (button->hbox), button->arrow);

        if (gtk_widget_get_parent (button->icon))
            gtk_container_remove (GTK_CONTAINER (button->hbox), button->icon);
        if (gtk_widget_get_parent (button->label))
            gtk_container_remove (GTK_CONTAINER (button->hbox), button->label);

        switch (switcher_style) {
        case GDL_SWITCHER_STYLE_TEXT:
            gtk_box_pack_start (GTK_BOX (button->hbox), button->label,
                                TRUE, TRUE, 0);
            gtk_widget_show (button->label);
            break;

        case GDL_SWITCHER_STYLE_ICON:
            gtk_box_pack_start (GTK_BOX (button->hbox), button->icon,
                                TRUE, TRUE, 0);
            gtk_widget_show (button->icon);
            break;

        case GDL_SWITCHER_STYLE_BOTH:
            gtk_box_pack_start (GTK_BOX (button->hbox), button->icon,
                                FALSE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX (button->hbox), button->label,
                                TRUE, TRUE, 0);
            gtk_widget_show (button->icon);
            gtk_widget_show (button->label);
            break;

        default:
            break;
        }

        gtk_box_pack_start (GTK_BOX (button->hbox), button->arrow,
                            FALSE, FALSE, 0);
    }

    gdl_switcher_set_show_buttons (switcher, TRUE);
}

static void
gdl_switcher_set_style (GdlSwitcher *switcher, GdlSwitcherStyle switcher_style)
{
    if (switcher->priv->switcher_style == switcher_style)
        return;

    if (switcher_style == GDL_SWITCHER_STYLE_NONE) {
        gdl_switcher_set_show_buttons (switcher, FALSE);
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (switcher), FALSE);
    }
    else if (switcher_style == GDL_SWITCHER_STYLE_TABS) {
        gdl_switcher_set_show_buttons (switcher, FALSE);
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (switcher), TRUE);
    }
    else
        set_switcher_style_toolbar (switcher, switcher_style);

    gtk_widget_queue_resize (GTK_WIDGET (switcher));
    switcher->priv->switcher_style = switcher_style;
}

static void
gdl_switcher_set_show_buttons (GdlSwitcher *switcher, gboolean show)
{
    GSList *p;

    if (switcher->priv->show == show)
        return;

    for (p = switcher->priv->buttons; p != NULL; p = p->next) {
        Button *button = p->data;

        if (show && gtk_widget_get_visible (button->page))
            gtk_widget_show (button->button_widget);
        else
            gtk_widget_hide (button->button_widget);
    }
    gdl_switcher_update_lone_button_visibility (switcher);
    switcher->priv->show = show;

    gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

static GdlSwitcherStyle
gdl_switcher_get_style (GdlSwitcher *switcher)
{
    if (!switcher->priv->show)
        return GDL_SWITCHER_STYLE_TABS;
    return switcher->priv->switcher_style;
}

static void
gdl_switcher_set_tab_pos (GdlSwitcher *switcher, GtkPositionType pos)
{
    if (switcher->priv->tab_pos == pos)
        return;

    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (switcher), pos);

    switcher->priv->tab_pos = pos;
}

static void
gdl_switcher_set_tab_reorderable (GdlSwitcher *switcher, gboolean reorderable)
{
    GList *children, *l;

    if (switcher->priv->tab_reorderable == reorderable)
        return;

    children = gtk_container_get_children (GTK_CONTAINER (switcher));
    for (l = children; l != NULL; l->next) {
        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (switcher),
                                          GTK_WIDGET (l->data),
                                          reorderable);
    }
    g_list_free (children);

    switcher->priv->tab_reorderable = reorderable;
}
