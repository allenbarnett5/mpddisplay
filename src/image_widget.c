/*
 * Display an image in a box.
 * Slowly we're recapitulating Qt. Probably should have just used it.
 */
#include <stdlib.h>

#include "VG/openvg.h"

#include "image_widget.h"

struct IMAGE_WIDGET_PRIVATE {
  float x_mm;
  float y_mm;
  float width_mm;
  float height_mm;
  float dpmm_x;
  float dpmm_y;
  int image_width;
  int image_height;
  float scale_x;
  float scale_y;
  VGImage image;
};

struct IMAGE_WIDGET_HANDLE image_widget_init ( float x_mm, float y_mm,
					       float width_mm,
					       float height_mm,
					       float dpmm_x,
					       float dpmm_y )
{
  struct IMAGE_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_WIDGET_PRIVATE ) );
  handle.d->x_mm = x_mm;
  handle.d->y_mm = y_mm;
  handle.d->width_mm = width_mm;
  handle.d->height_mm = height_mm;
  handle.d->dpmm_x = dpmm_x;
  handle.d->dpmm_y = dpmm_y;
  return handle;
}

void image_widget_set_image ( struct IMAGE_WIDGET_HANDLE handle,
			      struct IMAGE_HANDLE image )
{
  if ( handle.d == NULL )
    return;
  int image_width = image_rgba_width( image );
  int image_height = image_rgba_height( image );
  unsigned char* image_data = image_rgba_image( image );

  handle.d->image_width = image_width;
  handle.d->image_height = image_height;

  vgDestroyImage( handle.d->image );

  if ( handle.d->image_width ==  0 ||
       handle.d->image_height == 0 ||
       image_data == NULL )
    return;

  handle.d->image = vgCreateImage( VG_sRGBA_8888, image_width, image_height,
				   VG_IMAGE_QUALITY_BETTER );

  VGImage base = vgCreateImage( VG_sRGBA_8888, image_width, image_height,
				   VG_IMAGE_QUALITY_BETTER );

  vgImageSubData( base, image_data, image_width * 4,
		  VG_sABGR_8888, 0, 0, image_width, image_height );

  // Make the image a bit transparent.

  float more_transparent[] = {
    1., 0., 0., 0., // R-src -> {RGBA}-dest
    0., 1., 0., 0., // G-src -> {RGBA}-dest
    0., 0., 1., 0., // B-src -> {RGBA}-dest
    0., 0., 0., 0.5, // A-src -> {RGBA}-dest
    0., 0., 0., 0.  // const -> {RGBA}-dest
  };

  vgColorMatrix( handle.d->image, base, more_transparent );

  vgDestroyImage( base );

  handle.d->scale_x = handle.d->width_mm  / handle.d->image_width;
  handle.d->scale_y = handle.d->height_mm / handle.d->image_height;
}

void image_widget_draw_image ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE );
  vgLoadIdentity();
  // Overscan.
  vgTranslate( 14.f, 8.f );
  // Values in mm all around.
  vgScale( handle.d->dpmm_x, handle.d->dpmm_y );
  // Move to the corner.
  vgTranslate( handle.d->x_mm, handle.d->y_mm );
  // And we bascially draw the image upside down, so we really start
  // drawing at the upper left corner.
  vgTranslate( 0.f, handle.d->height_mm );
  // Scale the image to mm.
  vgScale( handle.d->scale_x, -handle.d->scale_y );

  vgDrawImage( handle.d->image );
}

void image_widget_free_handle ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    vgDestroyImage( handle.d->image );
    free( handle.d );
    handle.d = NULL;
  }
}
