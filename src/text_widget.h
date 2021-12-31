/*
 * Display the text in the given box. Text may use the Pango markup
 * to achieve tasteful effects.
 */
#pragma once

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
 * \param[in] x_mm x position in mm.
 * \param[in] y_mm y position in mm.
 * \param[in] width_mm width in mm.
 * \param[in] height_mm height in mm.
 * \param[in] dpmm_x dots per mm in the x direction.
 * \param[in] dpmm_y dots per mm in the y direction.
 * \param[in] screen_width_mm width of screen in mm.
 * \param[in] screen_height_mm height of screen in mm.
 */
struct TEXT_WIDGET_HANDLE text_widget_init ( float x_mm, float y_mm,
					     float width_mm,
					     float height_mm,
					     float dpmm_x,
					     float dpmm_y,
                                             float screen_width_mm,
                                             float screen_height_mm );

/*!
 * Set the text alignment.
 * \param[inout] handle the text widget to update.
 * \param[in] alignment the new text alignment.
 */
void text_widget_set_alignment ( struct TEXT_WIDGET_HANDLE handle,
				 enum TEXT_WIDGET_ALIGNMENT alignment );

/*!
 * Layout this text. We expect it to have GTK markup.
 * \param[inout] handle the text widget.
 * \param[in] text the new string to display.
 * \param[in] length the number of bytes in text.
 */
void text_widget_set_text ( struct TEXT_WIDGET_HANDLE handle,
			    const char* text, int length );

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
