/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "level.h"

struct _Level
{
	GtkBox parent_instance;

	GtkLabel *sgn_snr;
	GtkProgressBar *bar_sgn;
	GtkProgressBar *bar_snr;
};

G_DEFINE_TYPE ( Level, level, GTK_TYPE_BOX )

void level_set_sgn_snr ( uint8_t qual, char *sgl, char *snr, float sgl_gd, float snr_gd, gboolean fe_lock, Level *level )
{
	gtk_progress_bar_set_fraction ( level->bar_sgn, sgl_gd / 100 );
	gtk_progress_bar_set_fraction ( level->bar_snr, snr_gd / 100 );

	const char *text_qul = "bfbfbf";
	if ( qual == 3 ) text_qul = "ff00ff"; // Good - Magenta
	if ( qual == 2 ) text_qul = "00ffff"; // Ok   - Cyan
	if ( qual == 1 ) text_qul = "ff9000"; // Poor - Orange ( Strong Orangish Yellow )

	char *markup = NULL;

	if ( fe_lock )
		markup = g_markup_printf_escaped ( "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#00ff00\">  ◉  </span>%s", text_qul, sgl, snr );
	else
		markup = g_markup_printf_escaped ( "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#ff0000\">  ◉  </span>%s", text_qul, sgl, snr );

	if ( sgl_gd == 0 && snr_gd == 0 )
		markup = g_markup_printf_escaped ( "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#bfbfbf\">  ◉  </span>%s", text_qul, sgl, snr );

	gtk_label_set_markup ( level->sgn_snr, markup );

	free ( markup );
}

static void level_init ( Level *level )
{
	GtkBox *box = GTK_BOX ( level );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 3 );

	level->sgn_snr = (GtkLabel *)gtk_label_new ( "Quality  ◉  Signal  ◉  Snr" );
	level->bar_sgn = (GtkProgressBar *)gtk_progress_bar_new ();
	level->bar_snr = (GtkProgressBar *)gtk_progress_bar_new ();

	gtk_box_pack_start ( box, GTK_WIDGET ( level->sgn_snr ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( box, GTK_WIDGET ( level->bar_sgn ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( box, GTK_WIDGET ( level->bar_snr ), FALSE, FALSE, 0 );
}

static void level_finalize ( GObject *object )
{
	G_OBJECT_CLASS (level_parent_class)->finalize (object);
}

static void level_class_init ( LevelClass *class )
{
	G_OBJECT_CLASS (class)->finalize = level_finalize;
}

Level * level_new ()
{
	return g_object_new ( LEVEL_TYPE_BOX, NULL );
}

