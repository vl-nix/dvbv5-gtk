/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-2
* file:///usr/share/common-licenses/GPL-2
* http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

#include "level.h"

void level_set_sgn_snr ( uint8_t qual, char *sgl, char *snr, float sgl_gd, float snr_gd, bool fe_lock, Level *level )
{
	gtk_progress_bar_set_fraction ( level->bar_sgn, sgl_gd / 100 );
	gtk_progress_bar_set_fraction ( level->bar_snr, snr_gd / 100 );

	const char *text_qul = "bfbfbf";
	if ( qual == 3 ) text_qul = "ff00ff"; // Good - Magenta
	if ( qual == 2 ) text_qul = "00ffff"; // Ok   - Cyan
	if ( qual == 1 ) text_qul = "ff9000"; // Poor - Orange ( Strong Orangish Yellow )

	const char *format = NULL;

	if ( fe_lock )
		format = "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#00ff00\">  ◉  </span>%s";
	else
		format = "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#ff0000\">  ◉  </span>%s";

	if ( sgl_gd == 0 && snr_gd == 0 )
		format = "Quality<span foreground=\"#%s\">  ◉  </span>%s<span foreground=\"#bfbfbf\">  ◉  </span>%s";

	char *markup = g_markup_printf_escaped ( format, text_qul, sgl, snr );

		gtk_label_set_markup ( level->sgn_snr, markup );

	free ( markup );
}

static void level_init ( Level *level )
{
	level->v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( level->v_box, 3 );

	level->sgn_snr = (GtkLabel *)gtk_label_new ( "Quality  ◉  Signal  ◉  Snr" );
	level->bar_sgn = (GtkProgressBar *)gtk_progress_bar_new ();
	level->bar_snr = (GtkProgressBar *)gtk_progress_bar_new ();

	gtk_box_pack_start ( level->v_box, GTK_WIDGET ( level->sgn_snr ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( level->v_box, GTK_WIDGET ( level->bar_sgn ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( level->v_box, GTK_WIDGET ( level->bar_snr ), FALSE, FALSE, 0 );
}

void level_destroy ( Level *level )
{
	free ( level );
}

Level * level_new (void)
{
	Level *level = g_new0 ( Level, 1 );

	level_init ( level );

	return level;
}

