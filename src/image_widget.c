/*
 * Display an image in a box.
 * Slowly we're recapitulating Qt. Probably should have just used it.
 */
#include <stdlib.h>

#include "VG/openvg.h"
#include "VG/vgu.h"

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
  enum IMAGE_WIDGET_EMBLEM emblem;
  VGImage image;
};

static void image_widget_stopped ( VGPaint color, VGPath path );
static void image_widget_playing ( VGPaint color, VGPath path );
static void image_widget_paused  ( VGPaint color, VGPath path );

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
  handle.d->emblem = IMAGE_WIDGET_EMBLEM_NOEMBLEM;
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
    0., 0., 0., 0.75, // A-src -> {RGBA}-dest
    0., 0., 0., 0.  // const -> {RGBA}-dest
  };

  vgColorMatrix( handle.d->image, base, more_transparent );

  vgDestroyImage( base );

  handle.d->scale_x = handle.d->width_mm  / handle.d->image_width;
  handle.d->scale_y = handle.d->height_mm / handle.d->image_height;
}

void image_widget_set_emblem ( struct IMAGE_WIDGET_HANDLE handle,
			       enum IMAGE_WIDGET_EMBLEM emblem )
{
  if ( handle.d == NULL )
    return;

  handle.d->emblem = emblem;
}

void image_widget_draw_image ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE );
  vgLoadIdentity();
#if 0
  // Overscan.
  vgTranslate( 14.f, 8.f );
#endif
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

  if ( handle.d->emblem != IMAGE_WIDGET_EMBLEM_NOEMBLEM ) {
    VGPath path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
				VG_PATH_DATATYPE_F,
				1.0f, 0.0f,
				0, 0,
				VG_PATH_CAPABILITY_ALL );
    VGPaint color = vgCreatePaint();

    vgSeti( VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE );
    vgLoadIdentity();
    // Draw in mm as usual.
    vgScale( handle.d->dpmm_x, handle.d->dpmm_y );
    // Move to the corner of the window.
    vgTranslate( handle.d->x_mm, handle.d->y_mm );
    // Drawing is generally in the lower right corner inside a 1 cm x 1 cm
    // box.
    vgTranslate( handle.d->width_mm - 10.f, 0.f );

    switch ( handle.d->emblem ) {
    case IMAGE_WIDGET_EMBLEM_STOPPED:
      image_widget_stopped( color, path );
      break;
    case IMAGE_WIDGET_EMBLEM_PLAYING:
      image_widget_playing( color, path );
      break;
    case IMAGE_WIDGET_EMBLEM_PAUSED:
      image_widget_paused( color, path );
      break;
    default:
      break;
    }

    vgSetf( VG_STROKE_LINE_WIDTH, 0.5f );
    vgSetPaint( color, VG_STROKE_PATH );
    vgDrawPath( path, VG_STROKE_PATH );

    vgDestroyPaint( color );
    vgDestroyPath( path );
  }
}

void image_widget_free_handle ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    vgDestroyImage( handle.d->image );
    free( handle.d );
    handle.d = NULL;
  }
}

void image_widget_stopped ( VGPaint color, VGPath path )
{
  VGfloat octagon[] = { 5.f - 2.071f, 0.f,
			5.f + 2.071f, 0.f,
			10.f, 5.f - 2.071f,
			10.f, 5.f + 2.071f,
			5.f + 2.071f, 10.f,
			5.f - 2.071f, 10.f,
			0.f, 5.f + 2.071f,
			0.f, 5.f - 2.071f, };
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguPolygon( path, octagon, 8, VG_TRUE );
}

void image_widget_playing ( VGPaint color, VGPath path )
{
  VGfloat triangle[] = {  0.f,  0.f,
			  0.f, 10.f,
			 10.f,  5.f };
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguPolygon( path, triangle, 3, VG_TRUE );
}

void image_widget_paused ( VGPaint color, VGPath path )
{
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguRect( path, 0.f, 0.f, 3.f, 10.f );
  vguRect( path, 7.f, 0.f, 3.f, 10.f );
}
