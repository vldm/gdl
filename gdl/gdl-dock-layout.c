/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This file is part of the GNOME Devtools Libraries.
 *
 * Copyright (C) 2002 Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>

#include "gdl-dock-layout.h"


/**
 * SECTION:gdl-dock-layout
 * @title: GdlDockLayout
 * @short_description: save and restore dock widgets.
 * @stability: Unstable
 *
 * The layout of all docking widgets can be saved using this #GdlDockLayout
 * object. It automatically monitors the layout_changed signal of the
 * dock master and stores the position of all widgets in memory after each
 * change.
 *
 * The layout "dirty" property is set to %TRUE when the widget position in
 * memory is updated.
 * To keep an external file in sync with the dock, monitor this "dirty"
 * property and call gdl_dock_layout_save_to_file when
 * this changes to %TRUE. Informations are stored in XML format.
 */


/* ----- Private variables ----- */

enum {
    PROP_0,
    PROP_MASTER,
    PROP_DIRTY
};

#define ROOT_ELEMENT         "dock-layout"
#define DEFAULT_LAYOUT       "__default__"
#define LAYOUT_ELEMENT_NAME  "layout"
#define NAME_ATTRIBUTE_NAME  "name"

#define LAYOUT_UI_FILE    "layout.ui"

enum {
    COLUMN_NAME,
    COLUMN_SHOW,
    COLUMN_LOCKED,
    COLUMN_ITEM
};

#define COLUMN_EDITABLE COLUMN_SHOW

struct _GdlDockLayoutPrivate {
    gboolean              dirty;
    GdlDockMaster        *master;

    xmlDocPtr         doc;

    glong             layout_changed_id;

    /* idle control */
    gboolean          idle_save_pending;
};

/* ----- Private prototypes ----- */

static void     gdl_dock_layout_class_init      (GdlDockLayoutClass *klass);

static void     gdl_dock_layout_set_property    (GObject            *object,
                                                 guint               prop_id,
                                                 const GValue       *value,
                                                 GParamSpec         *pspec);

static void     gdl_dock_layout_get_property    (GObject            *object,
                                                 guint               prop_id,
                                                 GValue             *value,
                                                 GParamSpec         *pspec);

static void     gdl_dock_layout_dispose         (GObject            *object);

static void     gdl_dock_layout_build_doc       (GdlDockLayout      *layout);

static xmlNodePtr gdl_dock_layout_find_layout   (GdlDockLayout      *layout,
                                                 const gchar        *name);



/* ----- Private implementation ----- */

G_DEFINE_TYPE (GdlDockLayout, gdl_dock_layout, G_TYPE_OBJECT);

static void
gdl_dock_layout_class_init (GdlDockLayoutClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = gdl_dock_layout_set_property;
    object_class->get_property = gdl_dock_layout_get_property;
    object_class->dispose = gdl_dock_layout_dispose;

    g_object_class_install_property (
        object_class, PROP_MASTER,
        g_param_spec_object ("master", _("Master"),
                             _("GdlDockMaster or GdlDockObject object which the layout object "
                               "is attached to"),
                             G_TYPE_OBJECT,
                             G_PARAM_READWRITE));

    g_object_class_install_property (
        object_class, PROP_DIRTY,
        g_param_spec_boolean ("dirty", _("Dirty"),
                              _("True if the layouts have changed and need to be "
                                "saved to a file"),
                              FALSE,
                              G_PARAM_READABLE));

    g_type_class_add_private (object_class, sizeof (GdlDockLayoutPrivate));
}

static void
gdl_dock_layout_init (GdlDockLayout *layout)
{
    layout->priv = G_TYPE_INSTANCE_GET_PRIVATE (layout,
                                                GDL_TYPE_DOCK_LAYOUT,
                                                GdlDockLayoutPrivate);

    layout->priv->master = NULL;
    layout->priv->dirty = FALSE;
    layout->priv->idle_save_pending = FALSE;
#ifndef GDL_DISABLE_DEPRECATED
    layout->deprecated_master = NULL;
    layout->deprecated_dirty = FALSE;
#endif
}

static void
gdl_dock_layout_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
    GdlDockLayout *layout = GDL_DOCK_LAYOUT (object);

    switch (prop_id) {
        case PROP_MASTER:
            gdl_dock_layout_set_master (layout, g_value_get_object (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    };
}

static void
gdl_dock_layout_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
    GdlDockLayout *layout = GDL_DOCK_LAYOUT (object);

    switch (prop_id) {
        case PROP_MASTER:
            g_value_set_object (value, layout->priv->master);
            break;
        case PROP_DIRTY:
            g_value_set_boolean (value, layout->priv->dirty);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    };
}

static void
gdl_dock_layout_dispose (GObject *object)
{
    GdlDockLayout *layout;

    layout = GDL_DOCK_LAYOUT (object);

    if (layout->priv->master)
        gdl_dock_layout_set_master (layout, NULL);

    if (layout->priv->idle_save_pending) {
        layout->priv->idle_save_pending = FALSE;
        g_idle_remove_by_data (layout);
    }

    if (layout->priv->doc) {
        xmlFreeDoc (layout->priv->doc);
        layout->priv->doc = NULL;
    }

    G_OBJECT_CLASS (gdl_dock_layout_parent_class)->dispose (object);
}

static void
gdl_dock_layout_build_doc (GdlDockLayout *layout)
{
    g_return_if_fail (layout->priv->doc == NULL);

    xmlIndentTreeOutput = TRUE;
    layout->priv->doc = xmlNewDoc (BAD_CAST "1.0");
    layout->priv->doc->children = xmlNewDocNode (layout->priv->doc, NULL,
                                                  BAD_CAST ROOT_ELEMENT, NULL);
}

static xmlNodePtr
gdl_dock_layout_find_layout (GdlDockLayout *layout,
                             const gchar   *name)
{
    xmlNodePtr node;
    gboolean   found = FALSE;

    g_return_val_if_fail (layout != NULL, NULL);

    if (!layout->priv->doc)
        return NULL;

    /* get document root */
    node = layout->priv->doc->children;
    for (node = node->children; node; node = node->next) {
        xmlChar *layout_name;

        if (strcmp ((char*)node->name, LAYOUT_ELEMENT_NAME))
            /* skip non-layout element */
            continue;

        /* we want the first layout */
        if (!name)
            break;

        layout_name = xmlGetProp (node, BAD_CAST NAME_ATTRIBUTE_NAME);
        if (!strcmp (name, (char*)layout_name))
            found = TRUE;
        xmlFree (layout_name);

        if (found)
            break;
    };
    return node;
}

/* ----- Save & Load layout functions --------- */

#define GDL_DOCK_PARAM_CONSTRUCTION(p) \
    (((p)->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) != 0)

static GdlDockObject *
gdl_dock_layout_setup_object (GdlDockMaster *master,
                              xmlNodePtr     node,
                              gint          *n_after_params,
                              GParameter   **after_params)
{
    GdlDockObject *object = NULL;
    GType          object_type;
    xmlChar       *object_name;
    GObjectClass  *object_class = NULL;

    GParamSpec   **props;
    guint          n_props, i;
    GParameter    *params = NULL;
    gint           n_params = 0;
    GValue         serialized = { 0, };

    object_name = xmlGetProp (node, BAD_CAST GDL_DOCK_NAME_PROPERTY);
    if (object_name && strlen ((char*)object_name) > 0) {
        /* the object can be already be bound to the master or a
         * placeholder object is created */
        object = gdl_dock_master_get_object (master, (char*)object_name);

        object_type = object ? G_TYPE_FROM_INSTANCE (object) : gdl_dock_object_type_from_nick ((char*)node->name);
    }
    else {
        /* the object should be automatic, so create it by
           retrieving the object type from the dock registry */
        object_type = gdl_dock_object_type_from_nick ((char*)node->name);
        if (object_type == G_TYPE_NONE) {
            g_warning (_("While loading layout: don't know how to create "
                         "a dock object whose nick is '%s'"), node->name);
        }
    }

    if (object_type == G_TYPE_NONE || !G_TYPE_IS_CLASSED (object_type))
        return NULL;

    object_class = g_type_class_ref (object_type);
    props = g_object_class_list_properties (object_class, &n_props);

    /* create parameter slots */
    /* extra parameter is the master */
    params = g_new0 (GParameter, n_props + 1);
    *after_params = g_new0 (GParameter, n_props);
    *n_after_params = 0;

    /* initialize value used for transformations */
    g_value_init (&serialized, GDL_TYPE_DOCK_PARAM);

    for (i = 0; i < n_props; i++) {
        xmlChar *xml_prop;

        /* process all exported properties */
        /* keep GDL_DOCK_NAME_PROPERTY because we can create
           placeholder object for object not already in the
           master */
        if (!(props [i]->flags & GDL_DOCK_PARAM_EXPORT))
            continue;

        /* get the property from xml if there is one */
        xml_prop = xmlGetProp (node, BAD_CAST props [i]->name);
        if (xml_prop) {
            g_value_set_static_string (&serialized, (char*)xml_prop);

            if (!GDL_DOCK_PARAM_CONSTRUCTION (props [i]) &&
                (props [i]->flags & GDL_DOCK_PARAM_AFTER)) {
                (*after_params) [*n_after_params].name = props [i]->name;
                g_value_init (&((* after_params) [*n_after_params].value),
                              props [i]->value_type);
                g_value_transform (&serialized,
                                   &((* after_params) [*n_after_params].value));
                (*n_after_params)++;
            }
            else if (!object || (!GDL_DOCK_PARAM_CONSTRUCTION (props [i]) && object)) {
                params [n_params].name = props [i]->name;
                g_value_init (&(params [n_params].value), props [i]->value_type);
                g_value_transform (&serialized, &(params [n_params].value));
                n_params++;
            }
            xmlFree (xml_prop);
        }
    }
    g_value_unset (&serialized);
    g_free (props);

    if (!object) {
        params [n_params].name = GDL_DOCK_MASTER_PROPERTY;
        g_value_init (&params [n_params].value, GDL_TYPE_DOCK_MASTER);
        g_value_set_object (&params [n_params].value, master);
        n_params++;

        /* construct the object if we have to */
        /* set the master, so toplevels are created correctly and
           other objects are bound */
        object = g_object_newv (object_type, n_params, params);
        if (object_name) {
            gdl_dock_object_set_manual (GDL_DOCK_OBJECT (object));
            gdl_dock_master_add (master, object);
        }
    }
    else {
        /* set the parameters to the existing object */
        for (i = 0; i < n_params; i++)
            g_object_set_property (G_OBJECT (object),
                                   params [i].name,
                                   &params [i].value);
    }
    if (object_name) xmlFree (object_name);

    /* free the parameters (names are static/const strings) */
    for (i = 0; i < n_params; i++)
        g_value_unset (&params [i].value);
    g_free (params);

    /* finally unref object class */
    g_type_class_unref (object_class);


    return object;
}

static void
gdl_dock_layout_recursive_build (GdlDockMaster *master,
                                 xmlNodePtr     parent_node,
                                 GdlDockObject *parent)
{
    GdlDockObject *object;
    xmlNodePtr     node;

    g_return_if_fail (master != NULL && parent_node != NULL);

    /* if parent is NULL we should build toplevels */
    for (node = parent_node->children; node; node = node->next) {
        GParameter *after_params = NULL;
        gint        n_after_params = 0, i;

        object = gdl_dock_layout_setup_object (master, node,
                                               &n_after_params,
                                               &after_params);

        if (object) {
            gdl_dock_object_freeze (object);

            /* detach children */
            if (gdl_dock_object_is_compound (object)) {
                gtk_container_foreach (GTK_CONTAINER (object),
                               (GtkCallback) gdl_dock_object_detach,
                               GINT_TO_POINTER (TRUE));
            }

            /* add the object to the parent */
            if (parent) {
                if (gdl_dock_object_is_compound (parent)) {
                    gtk_container_add (GTK_CONTAINER (parent), GTK_WIDGET (object));
                }
            }

            /* build children */
            gdl_dock_layout_recursive_build (master, node, object);

            /* apply "after" parameters */
            for (i = 0; i < n_after_params; i++) {
                g_object_set_property (G_OBJECT (object),
                                       after_params [i].name,
                                       &after_params [i].value);
                /* unset and free the value */
                g_value_unset (&after_params [i].value);
            }
            g_free (after_params);

            gdl_dock_object_thaw (object);
        }
    }
}

static void
_gdl_dock_layout_foreach_detach (GdlDockObject *object)
{
    gdl_dock_object_detach (object, TRUE);
}

static void
gdl_dock_layout_foreach_toplevel_detach (GdlDockObject *object)
{
    gtk_container_foreach (GTK_CONTAINER (object),
                           (GtkCallback) _gdl_dock_layout_foreach_detach,
                           NULL);
}

static void
gdl_dock_layout_load (GdlDockMaster *master, xmlNodePtr node)
{
    g_return_if_fail (master != NULL && node != NULL);

    /* start by detaching all items from the toplevels */
    gdl_dock_master_foreach_toplevel (master, TRUE,
                                      (GFunc) gdl_dock_layout_foreach_toplevel_detach,
                                      NULL);

    gdl_dock_layout_recursive_build (master, node, NULL);
}

static void
gdl_dock_layout_foreach_object_save (GdlDockObject *object,
                                     gpointer       user_data)
{
    xmlNodePtr where = (xmlNodePtr)user_data;
    xmlNodePtr   node;
    guint        n_props, i;
    GParamSpec **props;
    GValue       attr = { 0, };

    g_return_if_fail (object != NULL && GDL_IS_DOCK_OBJECT (object));
    g_return_if_fail (where != NULL);

    node = xmlNewChild (where,
                        NULL,               /* ns */
                        BAD_CAST gdl_dock_object_nick_from_type (G_TYPE_FROM_INSTANCE (object)),
                        BAD_CAST NULL);     /* contents */

    /* get object exported attributes */
    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object),
                                            &n_props);
    g_value_init (&attr, GDL_TYPE_DOCK_PARAM);
    for (i = 0; i < n_props; i++) {
        GParamSpec *p = props [i];

        if (p->flags & GDL_DOCK_PARAM_EXPORT) {
            GValue v = { 0, };

            /* export this parameter */
            /* get the parameter value */
            g_value_init (&v, p->value_type);
            g_object_get_property (G_OBJECT (object),
                                   p->name,
                                   &v);

            /* only save the object "name" if it is set
               (i.e. don't save the empty string) */
            if (strcmp (p->name, GDL_DOCK_NAME_PROPERTY) ||
                g_value_get_string (&v)) {
                if (g_value_transform (&v, &attr))
                    xmlSetProp (node, BAD_CAST p->name, BAD_CAST g_value_get_string (&attr));
            }

            /* free the parameter value */
            g_value_unset (&v);
        }
    }
    g_value_unset (&attr);
    g_free (props);

    /* recurse the object if appropiate */
    if (gdl_dock_object_is_compound (object)) {
        gtk_container_foreach (GTK_CONTAINER (object),
                               (GtkCallback) gdl_dock_layout_foreach_object_save,
                               (gpointer) node);
    }
}

static void
gdl_dock_layout_save (GdlDockMaster *master,
                      xmlNodePtr     where)
{

	g_return_if_fail (master != NULL && where != NULL);

    /* save the layout recursively */
    gdl_dock_master_foreach_toplevel (master, TRUE,
                                      (GFunc) gdl_dock_layout_foreach_object_save,
                                      (gpointer) where);
}


/* ----- Public interface ----- */

/**
 * gdl_dock_layout_new:
 * @master: A master or a dock object to which the layout will be attached.
 *
 * Creates a new #GdlDockLayout. Instead of setting @master
 * directly with a master object, it is possible to use a #GdlDockObject, in
 * this case the layout will be attached to the same master than the dock
 * object.
 *
 * Returns: New #GdlDockLayout item.
 */
GdlDockLayout *
gdl_dock_layout_new (GObject *master)
{
    g_return_val_if_fail (master == NULL || GDL_IS_DOCK_MASTER (master) || GDL_IS_DOCK_OBJECT (master), NULL);

    return g_object_new (GDL_TYPE_DOCK_LAYOUT,
                         "master", master,
                         NULL);
}

static gboolean
gdl_dock_layout_idle_save (GdlDockLayout *layout)
{
    /* save default layout */
    gdl_dock_layout_save_layout (layout, NULL);

    layout->priv->idle_save_pending = FALSE;

    return FALSE;
}

static void
gdl_dock_layout_layout_changed_cb (GdlDockMaster *master,
                                   GdlDockLayout *layout)
{
    if (!layout->priv->idle_save_pending) {
        g_idle_add ((GSourceFunc) gdl_dock_layout_idle_save, layout);
        layout->priv->idle_save_pending = TRUE;
    }
}


/**
 * gdl_dock_layout_set_master:
 * @layout: The layout object
 * @master: The master object to which the layout will be attached
 *
 * Attach the @layout to the @master and delete the reference to
 * the master that the layout attached previously. Instead of setting @master
 * directly with the master object, it is possible to use a #GdlDockObject, in
 * this case the layout will be attached to the same master than the dock
 * object.
 */
void
gdl_dock_layout_set_master (GdlDockLayout *layout,
                            GObject *master)
{
    g_return_if_fail (layout != NULL);
    g_return_if_fail (master == NULL || GDL_IS_DOCK_OBJECT (master) || GDL_IS_DOCK_MASTER (master));

    if (layout->priv->master) {
        g_signal_handler_disconnect (layout->priv->master,
                                     layout->priv->layout_changed_id);
        g_object_unref (layout->priv->master);
    }

    if (master != NULL)
    {
        /* Accept a GdlDockObject instead of a GdlDockMaster */
        if (GDL_IS_DOCK_OBJECT (master)) {
            master = gdl_dock_object_get_master (GDL_DOCK_OBJECT (master));
        }
        layout->priv->master = g_object_ref (master);
        layout->priv->layout_changed_id =
            g_signal_connect (layout->priv->master, "layout-changed",
                              (GCallback) gdl_dock_layout_layout_changed_cb,
                              layout);

    } else {
        layout->priv->master = NULL;
    }
#ifndef GDL_DISABLE_DEPRECATED
    layout->deprecated_master = layout->priv->master;
#endif
}

/**
 * gdl_dock_layout_get_master:
 * @layout: a #GdlDockLayout
 *
 * Retrieves the master of the object.
 *
 * Return value: (transfer none): a #GdlDockMaster object
 *
 * Since: 3.6
 */
GObject *
gdl_dock_layout_get_master (GdlDockLayout *layout)
{
    g_return_val_if_fail (GDL_IS_DOCK_LAYOUT (layout), NULL);

    return G_OBJECT (layout->priv->master);
}


/**
* gdl_dock_layout_load_layout:
* @layout: The dock item.
* @name: (allow-none): The name of the layout to load or %NULL for a default layout name.
*
* Loads the layout with the given name from the memory.
* This will set #GdlDockLayout:dirty to %TRUE.
*
* See also gdl_dock_layout_load_from_file()
*
* Returns: %TRUE if layout successfully loaded else %FALSE
*/
gboolean
gdl_dock_layout_load_layout (GdlDockLayout *layout,
                             const gchar   *name)
{
    xmlNodePtr  node;
    gchar      *layout_name;

    g_return_val_if_fail (layout != NULL, FALSE);

    if (!layout->priv->doc || !layout->priv->master)
        return FALSE;

    if (!name)
        layout_name = DEFAULT_LAYOUT;
    else
        layout_name = (gchar *) name;

    node = gdl_dock_layout_find_layout (layout, layout_name);
    if (!node && !name)
        /* return the first layout if the default name failed to load */
        node = gdl_dock_layout_find_layout (layout, NULL);

    if (node) {
        gdl_dock_layout_load (layout->priv->master, node);
        return TRUE;
    } else
        return FALSE;
}

/**
* gdl_dock_layout_save_layout:
* @layout: The dock item.
* @name: (allow-none): The name of the layout to save or %NULL for a default layout name.
*
* Saves the @layout with the given name to the memory.
* This will set #GdlDockLayout:dirty to %TRUE.
*
* See also gdl_dock_layout_save_to_file().
*/

void
gdl_dock_layout_save_layout (GdlDockLayout *layout,
                             const gchar   *name)
{
    xmlNodePtr  node;
    gchar      *layout_name;

    g_return_if_fail (layout != NULL);
    g_return_if_fail (layout->priv->master != NULL);

    if (!layout->priv->doc)
        gdl_dock_layout_build_doc (layout);

    if (!name)
        layout_name = DEFAULT_LAYOUT;
    else
        layout_name = (gchar *) name;

    /* delete any previously node with the same name */
    node = gdl_dock_layout_find_layout (layout, layout_name);
    if (node) {
        xmlUnlinkNode (node);
        xmlFreeNode (node);
    };

    /* create the new node */
    node = xmlNewChild (layout->priv->doc->children, NULL,
                        BAD_CAST LAYOUT_ELEMENT_NAME, NULL);
    xmlSetProp (node, BAD_CAST NAME_ATTRIBUTE_NAME, BAD_CAST layout_name);

    /* save the layout */
    gdl_dock_layout_save (layout->priv->master, node);
    layout->priv->dirty = TRUE;
    g_object_notify (G_OBJECT (layout), "dirty");
}

/**
* gdl_dock_layout_delete_layout:
* @layout: The dock item.
* @name: The name of the layout to delete.
*
* Deletes the layout with the given name from the memory.
* This will set #GdlDockLayout:dirty to %TRUE.
*/
void
gdl_dock_layout_delete_layout (GdlDockLayout *layout,
                               const gchar   *name)
{
    xmlNodePtr node;

    g_return_if_fail (layout != NULL);

    /* don't allow the deletion of the default layout */
    if (!name || !strcmp (DEFAULT_LAYOUT, name))
        return;

    node = gdl_dock_layout_find_layout (layout, name);
    if (node) {
        xmlUnlinkNode (node);
        xmlFreeNode (node);
        layout->priv->dirty = TRUE;
        g_object_notify (G_OBJECT (layout), "dirty");
    }
}

/**
* gdl_dock_layout_load_from_file:
* @layout: The layout item.
* @filename: The name of the file to load.
*
* Loads the layout from file with the given @filename.
* This will set #GdlDockLayout:dirty to %FALSE.
*
* Returns: %TRUE if @layout successfully loaded else %FALSE
*/
gboolean
gdl_dock_layout_load_from_file (GdlDockLayout *layout,
                                const gchar   *filename)
{
    gboolean retval = FALSE;

    if (layout->priv->doc) {
        xmlFreeDoc (layout->priv->doc);
        layout->priv->doc = NULL;
        layout->priv->dirty = FALSE;
        g_object_notify (G_OBJECT (layout), "dirty");
    }

    /* FIXME: cannot open symlinks */
    if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
        layout->priv->doc = xmlParseFile (filename);
        if (layout->priv->doc) {
            xmlNodePtr root = layout->priv->doc->children;
            /* minimum validation: test the root element */
            if (root && !strcmp ((char*)root->name, ROOT_ELEMENT)) {
                retval = TRUE;
            } else {
                xmlFreeDoc (layout->priv->doc);
                layout->priv->doc = NULL;
            }
        }
    }

    return retval;
}

/**
 * gdl_dock_layout_save_to_file:
 * @layout: The layout item.
 * @filename: Name of the file we want to save in layout
 *
 * This function saves the current layout in XML format to
 * the file with the given @filename.
 *
 * Returns: %TRUE if @layout successfuly save to the file, otherwise %FALSE.
 */
gboolean
gdl_dock_layout_save_to_file (GdlDockLayout *layout,
                              const gchar   *filename)
{
    FILE     *file_handle;
    int       bytes;
    gboolean  retval = FALSE;

    g_return_val_if_fail (layout != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    /* if there is still no xml doc, create an empty one */
    if (!layout->priv->doc)
        gdl_dock_layout_build_doc (layout);

    file_handle = fopen (filename, "w");
    if (file_handle) {
        bytes = xmlDocFormatDump (file_handle, layout->priv->doc, 1);
        if (bytes >= 0) {
            layout->priv->dirty = FALSE;
            g_object_notify (G_OBJECT (layout), "dirty");
            retval = TRUE;
        };
        fclose (file_handle);
    };

    return retval;
}

/**
 * gdl_dock_layout_is_dirty:
 * @layout: The layout item.
 *
 * Checks whether the XML tree in memory is different from the file where the layout was saved.
 * Returns: %TRUE is the layout in the memory is different from the file, else %FALSE.
 */
gboolean
gdl_dock_layout_is_dirty (GdlDockLayout *layout)
{
    g_return_val_if_fail (layout != NULL, FALSE);

    return layout->priv->dirty;
};

/**
 * gdl_dock_layout_get_layouts:
 * @layout: The layout item.
 * @include_default: %TRUE to include the default layout.
 *
 * Get the list of layout names including or not the default layout.
 *
 * Returns: (element-type utf8) (transfer full): a #GList list
 *  holding the layout names. You must first free each element in the list
 *  with g_free(), then free the list itself with g_list_free().
 */
GList *
gdl_dock_layout_get_layouts (GdlDockLayout *layout,
                             gboolean       include_default)
{
    GList      *retval = NULL;
    xmlNodePtr  node;

    g_return_val_if_fail (layout != NULL, NULL);

    if (!layout->priv->doc)
        return NULL;

    node = layout->priv->doc->children;
    for (node = node->children; node; node = node->next) {
        xmlChar *name;

        if (strcmp ((char*)node->name, LAYOUT_ELEMENT_NAME))
            continue;

        name = xmlGetProp (node, BAD_CAST NAME_ATTRIBUTE_NAME);
        if (include_default || strcmp ((char*)name, DEFAULT_LAYOUT))
            retval = g_list_prepend (retval, g_strdup ((char*)name));
        xmlFree (name);
    };
    retval = g_list_reverse (retval);

    return retval;
}
