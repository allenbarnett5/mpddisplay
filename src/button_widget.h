/*
 * Draw a button. React to a click.
 */
#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

struct BUTTON_WIDGET_PRIVATE;

struct BUTTON_WIDGET_HANDLE {
  struct BUTTON_WIDGET_PRIVATE* d;
};

/*!
 * Create a button widget.  What attributes are important? Really the
 * width and height in mm and and pixels.
 * \param[in] x_mm x position in mm.
 * \param[in] y_mm y position in mm.
 * \param[in] width_mm width in mm.
 * \param[in] height_mm height in mm.
 * \param[in] dpmm_x dots per mm in the x direction.
 * \param[in] dpmm_y dots per mm in the y direction.
 */
struct BUTTON_WIDGET_HANDLE button_widget_init ( float x_mm, float y_mm,
					     float width_mm,
					     float height_mm,
					     float dpmm_x,
					     float dpmm_y );

/*!
 * Draw the button. The OpenVG conbutton should be all set up to
 * draw the button in the right place, namely the button transform
 * should be properly set.
 * \param handle the button widget.
 */
void button_widget_draw_button ( struct BUTTON_WIDGET_HANDLE handle );
			   
/*!
 * Release any memory held by the handle.
 */
void button_widget_free_handle ( struct BUTTON_WIDGET_HANDLE handle );
#endif
