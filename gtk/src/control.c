/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "control.h"

struct _Control
{
	GtkBox parent_instance;

	GtkButton *button[NUM_BUTTONS];
};

G_DEFINE_TYPE ( Control, control, GTK_TYPE_BOX )

static const char *b_n[NUM_BUTTONS][3] = 
{
	{ "start", "dvb-start", "⏵" }, { "stop", "dvb-stop", "⏹" }, { "mini", "dvb-mini", "🗕" },
	/*{ "dark",  "dvb-dark",  "⏾" }, { "info", "dvb-info", "🛈" },*/ { "quit", "dvb-quit", "⏻" }
};

void control_button_set_sensitive ( const char *name, gboolean set, Control *control )
{
	u_int8_t c = 0; for ( c = 0; c < NUM_BUTTONS; c++ )
	{
		if ( g_str_has_prefix ( b_n[c][0], name ) ) { gtk_widget_set_sensitive ( GTK_WIDGET ( control->button[c] ), set ); break; }
	}
}

gboolean control_check_icon_theme ( const char *name_icon )
{
	gboolean ret = FALSE;

	char **icons = g_resources_enumerate_children ( "/dvbv5", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL );

	if ( icons == NULL ) return ret;

	uint j = 0, numfields = g_strv_length ( icons );

	for ( j = 0; j < numfields; j++ )
	{
		if ( g_strrstr ( icons[j], name_icon ) ) { ret = TRUE; break; }
	}

	g_strfreev ( icons );

	return ret;
}

static GtkImage * control_create_image ( const char *icon, u_int8_t size )
{
	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), icon, size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	GtkImage *image   = (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf );
	gtk_image_set_pixel_size ( image, size );

	if ( pixbuf ) g_object_unref ( pixbuf );

	return image;
}

static GtkButton * control_set_image_button ( const char *icon, u_int8_t size )
{
	GtkButton *button = (GtkButton *)gtk_button_new ();

	GtkImage *image   = control_create_image ( icon, size );

	gtk_button_set_image ( button, GTK_WIDGET ( image ) );

	return button;
}

void control_resize_icon ( u_int8_t icon_size, Control *control )
{
	u_int8_t c = 0; for ( c = 0; c < NUM_BUTTONS; c++ )
	{
		if ( !control_check_icon_theme ( b_n[c][1] ) ) continue;

		GtkImage *image = control_create_image ( b_n[c][1], icon_size );
		gtk_button_set_image ( control->button[c], GTK_WIDGET ( image ) );
	}
}

GtkButton * control_create_button ( GtkBox *h_box, const char *name, const char *icon_u, u_int8_t icon_size )
{
	GtkButton *button;

	if ( control_check_icon_theme ( name ) )
		button = control_set_image_button ( name, icon_size );
	else
		button = (GtkButton *)gtk_button_new_with_label ( icon_u );

	if ( h_box ) gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static void control_signal_handler ( GtkButton *button, Control *control )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	g_signal_emit_by_name ( control, "button-clicked", name );
}

static void control_init ( Control *control )
{
	GtkBox *box = GTK_BOX ( control );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_HORIZONTAL );
	gtk_box_set_spacing ( box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	u_int8_t c = 0; for ( c = 0; c < NUM_BUTTONS; c++ )
	{
		control->button[c] = control_create_button ( box, b_n[c][1], b_n[c][2], SIZE_ICONS );
		gtk_widget_set_name ( GTK_WIDGET ( control->button[c] ), b_n[c][0] );
		g_signal_connect ( control->button[c], "clicked", G_CALLBACK ( control_signal_handler ), control );
	}
}

static void control_finalize ( GObject *object )
{
	G_OBJECT_CLASS (control_parent_class)->finalize (object);
}

static void control_class_init ( ControlClass *class )
{
	G_OBJECT_CLASS (class)->finalize = control_finalize;

	g_signal_new ( "button-clicked", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
}

Control * control_new ( void )
{
	return g_object_new ( CONTROL_TYPE_BOX, NULL );
}
