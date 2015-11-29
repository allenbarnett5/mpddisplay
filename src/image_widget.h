/*
 * Display an image in a box. Possibly with an emblem of some type in
 * the corner.
 */
#ifndef IMAGE_WIDGET_H
#define IMAGE_WIDGET_H

#include "image_intf.h"

struct IMAGE_WIDGET_PRIVATE;

struct IMAGE_WIDGET_HANDLE {
  struct IMAGE_WIDGET_PRIVATE* d;
};

enum IMAGE_WIDGET_EMBLEM {
  IMAGE_WIDGET_EMBLEM_STOPPED,
  IMAGE_WIDGET_EMBLEM_PLAYING,
  IMAGE_WIDGET_EMBLEM_PAUSED,
  IMAGE_WIDGET_EMBLEM_NOEMBLEM
};

/*!
 * Create an image widget.
 * \param[in] x_mm position of the left edge in mm.
 * \param[in] y_mm position of the bottom edge in mm.
 * \param[in] width_mm width of the window in mm.
 * \param[in] height_mm height of the window in mm.
 * \param[in] dpmm_x dots per mm in x direction.
 * \param[in] dpmm_y dots per mm in y direction.
 */
struct IMAGE_WIDGET_HANDLE image_widget_init ( float x_mm, float y_mm,
					       float width_mm,
					       float height_mm,
					       float dpmm_x,
					       float dpmm_y );

/*!
 * Replace the image.
 */
void image_widget_set_image ( struct IMAGE_WIDGET_HANDLE handle,
			      struct IMAGE_HANDLE image );
/*!
 * Draw an emblem of some kind in the corner of the image.
 */
void image_widget_set_emblem ( struct IMAGE_WIDGET_HANDLE handle,
			       enum IMAGE_WIDGET_EMBLEM emblem );
/*!
 * Draw the image widget.
 */
void image_widget_draw_image ( struct IMAGE_WIDGET_HANDLE handle );

/*!
 * Release any memory held by the handle.
 */
void image_widget_free_handle ( struct IMAGE_WIDGET_HANDLE handle );


#endif
