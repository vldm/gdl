/*
 *  Symbol browser component for GNOME Development Tools
 *
 *  Copyright (c) 2001 Naba Kumar <kh_naba@yahoo.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>
#include <gdl/gdl.h>
#include "symbol-browser.h"
#include "symbol-browser-cobject.h"

/*#define USE_SHARED_LIBRARY_COMPONENT*/

static void on_format_update (GtkWidget     *widget,
			      BonoboControl *control)
{
	/*
	GnomeSymbolBrowser* symbol_browser = GNOME_SYMBOL_BROWSER(
		gtk_object_get_data (GTK_OBJECT(control), "SymbolBrowser"));
	gnome_symbol_browser_update (symbol_browser);
	*/
}

static BonoboUIVerb verbs[] =  {
	BONOBO_UI_UNSAFE_VERB ("FormatUpdate", on_format_update),
	BONOBO_UI_VERB_END
};

static void
on_control_set_frame (BonoboControl *control,
		      gpointer       data)
{
	Bonobo_UIContainer uic;
	BonoboUIComponent *component;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	bonobo_control_get_control_frame (control, &ev);
	g_return_if_fail (!BONOBO_EX (&ev));

	uic = bonobo_control_get_remote_ui_container (control, &ev);
	g_return_if_fail (!BONOBO_EX (&ev));
	component = bonobo_control_get_ui_component (control);
	bonobo_ui_component_set_container (component, uic, &ev);
	g_return_if_fail (!BONOBO_EX (&ev));

	/* FIXME: Merge UI */
	bonobo_ui_component_add_verb_list_with_data (component, verbs, control);

	bonobo_ui_util_set_ui (component, GNOME_DATADIR,
			       "gnome-symbol-browser.xml",
			       "Gnome Symbol Browser", &ev);
	g_return_if_fail (!BONOBO_EX (&ev));

	CORBA_exception_free (&ev);
}

static void
on_control_destroy (GtkObject          *control,
		    GnomeSymbolBrowser *symbol_browser)
{
	g_object_unref (symbol_browser);
}

static BonoboObject *
control_factory (BonoboGenericFactory *factory,
		 const char           *component_id,
		 gpointer              closure)
{
	BonoboControl *control;
	GtkWidget *symbol_browser;
	GnomeSymbolBrowserCObject *cobj;
	BonoboEventSource *event_source;

	/* Create the control. */
	symbol_browser = gnome_symbol_browser_new ();
	gtk_widget_show_all (symbol_browser);

	control = bonobo_control_new (symbol_browser);

	cobj = gnome_symbol_browser_cobject_new (GNOME_SYMBOL_BROWSER (symbol_browser));

	g_object_set_data (G_OBJECT (control), "SymbolBrowser", symbol_browser);
	g_object_set_data (G_OBJECT (control), "SymbolBrowserCObject", cobj);

	bonobo_object_add_interface (BONOBO_OBJECT (control), 
				     BONOBO_OBJECT (cobj));

	/* register EventSource interface for go-to event notification. */
	event_source = gnome_symbol_browser_get_event_source (GNOME_SYMBOL_BROWSER (symbol_browser));
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (event_source));

	/* UI initialization takes place when the control frame is set */
	g_signal_connect (control,
			  "set_frame",
			  G_CALLBACK (on_control_set_frame),
			  symbol_browser);

	g_signal_connect (control,
			  "destroy",
			  G_CALLBACK (on_control_destroy),
			  symbol_browser);

	return BONOBO_OBJECT (control);
}

#ifndef USE_SHARED_LIBRARY_COMPONENT
BONOBO_ACTIVATION_FACTORY ("OAFIID:GNOME_Development_SymbolBrowser_ControlFactory",
			   "Factory for the gnome symbol-browser", "0.1",
			   control_factory, NULL);
#else
BONOBO_ACTIVATION_SHLIB_FACTORY ("OAFIID:GNOME_Development_SymbolBrowser_ControlFactory",
				 "Factory for the gnome symbol-browser",
				 control_factory, NULL);
#endif /* USE_SHARED_LIBRARY_COMPONENT */