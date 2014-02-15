/*
 * Display the text in the given box. Text may use the Pango markup
 * to achieve tasteful effects.
 */
#ifndef TEXT_WIDGET_H
#define TEXT_WIDGET_H

#include "glib.h"

struct TEXT_WIDGET_PRIVATE;

struct TEXT_WIDGET_HANDLE {
  struct TEXT_WIDGET_PRIVATE* d;
};

enum TEXT_WIDGET_ALIGNMENT {
  TEXT_WIDGET_ALIGN_LEFT,
  TEXT_WIDGET_ALIGN_CENTER,
  TEXT_WIDGET_ALIGN_RIGHT
};

/*!
 * Create a text widget.  What attributes are important? Really the
 * width and height in mm and and pixels. Do we want to have a default
 * font and character size? Well, they're just going to be overriden
 * by the markup. Furthermore, it appears that Pango/FreeType has
 * some kind of default, probably provided by fontconfig.
 * \param[in] width_mm
 * \param[in] height_mm
 * \param[in] width_pixels
 * \param[in] height_pixels
 */
struct TEXT_WIDGET_HANDLE text_widget_init ( int x, int y,
					     float width_mm,
					     float height_mm,
					     int width_pixels,
					     int height_pixels );

/*!
 * Set the text alignment.
 * \param[inout] handle the text widget to update.
 * \param[in] alignment the new text alignment.
 */
void text_widget_set_alignment ( struct TEXT_WIDGET_HANDLE handle,
				 enum TEXT_WIDGET_ALIGNMENT alignment );

/*!
 * Set the default foreground color.
 * \param[inout] handle the text widget to update.
 * \param[in] color a four float, RGBA, color specification.
 */
void text_widget_set_foreground( struct TEXT_WIDGET_HANDLE handle,
				 float color[4] );

/*!
 * Layout this text. Could have Pango markup to make it attractive.
 * \param[inout] handle the text widget.
 * \param[in] text the new string to display.
 */
void text_widget_set_text ( struct TEXT_WIDGET_HANDLE handle,
			    const GString* text );

/*!
 * Draw the text. The OpenVG context should be all set up to
 * draw the text in the right place, namely the text transform
 * should be properly set.
 * \param handle the text widget.
 */
void text_widget_draw_text ( struct TEXT_WIDGET_HANDLE handle );
			   
/*!
 * Release any memory held by the handle.
 */
void text_widget_free_handle ( struct TEXT_WIDGET_HANDLE handle );
#endif
