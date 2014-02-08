/*
 * Display an image in a box.
 */
#ifndef IMAGE_WIDGET_H
#define IMAGE_WIDGET_H

#include "magick_image.h"

struct IMAGE_WIDGET_PRIVATE;

struct IMAGE_WIDGET_HANDLE {
  struct IMAGE_WIDGET_PRIVATE* d;
};

/*!
 * Create an image widget.
 */
struct IMAGE_WIDGET_HANDLE image_widget_init ( float x, float y,
					       float width_mm,
					       float height_mm,
					       int width_pixels,
					       int height_pixels );

/*!
 * Replace the image.
 */
void image_widget_set_image ( struct IMAGE_WIDGET_HANDLE handle,
			      struct IMAGE_HANDLE image );

/*!
 * Draw the image widget.
 */
void image_widget_draw_image ( struct IMAGE_WIDGET_HANDLE handle );

/*!
 * Release any memory held by the handle.
 */
void image_widget_free_handle ( struct IMAGE_WIDGET_HANDLE handle );
#endif
