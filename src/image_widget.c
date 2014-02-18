/*
 * Display an image in a box.
 * Slowly we're recapitulating Qt. Probably should have just used it.
 */
#include <stdlib.h>

#include "VG/openvg.h"

#include "image_widget.h"

struct IMAGE_WIDGET_PRIVATE {
  float x;
  float y;
  float width_mm;
  float height_mm;
  int width_pixels;
  int height_pixels;
  int image_width;
  int image_height;
  float scale_x;
  float scale_y;
  VGImage image;
};

struct IMAGE_WIDGET_HANDLE image_widget_init ( float x, float y,
					       float width_mm,
					       float height_mm,
					       int width_pixels,
					       int height_pixels )
{
  struct IMAGE_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_WIDGET_PRIVATE ) );
  handle.d->x = x;
  handle.d->y = y;
  handle.d->width_mm = width_mm;
  handle.d->height_mm = height_mm;
  handle.d->width_pixels = width_pixels;
  handle.d->height_pixels = height_pixels;
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
#if 0
  handle.d->image = vgCreateImage( VG_sRGBA_8888, image_width, image_height,
				   VG_IMAGE_QUALITY_BETTER );

  vgImageSubData( handle.d->image, image_data, image_width * 4,
		  VG_sABGR_8888, 0, 0, image_width, image_height );

  handle.d->scale_x = (float)handle.d->width_pixels/handle.d->image_width;
  handle.d->scale_y = -(float)handle.d->height_pixels/handle.d->image_height;
#else
  handle.d->image = vgCreateImage( VG_sRGBA_8888, image_width, image_height,
				   VG_IMAGE_QUALITY_BETTER );

  VGImage base = vgCreateImage( VG_sRGBA_8888, image_width, image_height,
				   VG_IMAGE_QUALITY_BETTER );

  vgImageSubData( base, image_data, image_width * 4,
		  VG_sABGR_8888, 0, 0, image_width, image_height );

  float more_transparent[] = {
    1., 0., 0., 0., // R-src -> {RGBA}-dest
    0., 1., 0., 0., // G-src -> {RGBA}-dest
    0., 0., 1., 0., // B-src -> {RGBA}-dest
    0., 0., 0., 0.5, // A-src -> {RGBA}-dest
    0., 0., 0., 0.  // const -> {RGBA}-dest
  };

  vgColorMatrix( handle.d->image, base, more_transparent );

  vgDestroyImage( base );

  handle.d->scale_x = (float)handle.d->width_pixels/handle.d->image_width;
  handle.d->scale_y = -(float)handle.d->height_pixels/handle.d->image_height;
#endif
}

void image_widget_draw_image ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE );
  vgLoadIdentity();
  vgTranslate( handle.d->x, handle.d->y );
  vgScale( handle.d->scale_x, handle.d->scale_y );
  vgDrawImage( handle.d->image );
}
