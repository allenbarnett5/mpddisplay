/*
 * A button widget. This wil be specific to MPDisplay for sure.
 */
#include <stdio.h>
#include <stdlib.h>

#include "VG/openvg.h"
#include "VG/vgu.h"

#include "button_widget.h"

struct BUTTON_WIDGET_PRIVATE {
  float x_mm;
  float y_mm;
  float width_mm;
  float height_mm;
  float dpmm_x;
  float dpmm_y;
  VGPaint white;
  VGPaint surface;
  VGPaint edge;
};

static VGfloat BUTTON_SURFACE_COLOR[] = { 1.f, 1.f, 1.f, 0.5f };
static VGfloat BUTTON_EDGE_COLOR[] = { 0.f, 0.f, 0.f, 0.5f };
static VGfloat FILL_GRADIENT[] = { 0.f, 0.f, 0.f, 0.f, 50.f };

struct BUTTON_WIDGET_HANDLE button_widget_init ( float x_mm, float y_mm,
						 float width_mm,
						 float height_mm,
						 float dpmm_x,
						 float dpmm_y )
{
  struct BUTTON_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct BUTTON_WIDGET_PRIVATE ) );
  handle.d->x_mm = x_mm;
  handle.d->y_mm = y_mm;
  handle.d->width_mm = width_mm;
  handle.d->height_mm = height_mm;
  handle.d->dpmm_x = dpmm_x;
  handle.d->dpmm_y = dpmm_y;

  handle.d->white = vgCreatePaint();
  vgSetParameterfv( handle.d->white, VG_PAINT_COLOR, 4, BUTTON_SURFACE_COLOR );

  handle.d->surface = vgCreatePaint();
  vgSetParameteri( handle.d->surface, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT );
  vgSetParameterfv( handle.d->surface, VG_PAINT_RADIAL_GRADIENT, 5,
		    FILL_GRADIENT );

  handle.d->edge = vgCreatePaint();
  vgSetParameterfv( handle.d->edge, VG_PAINT_COLOR, 4, BUTTON_EDGE_COLOR );

  return handle;
}


void button_widget_draw_button ( struct BUTTON_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;

  vgSetPaint( handle.d->surface, VG_FILL_PATH );
  vgSetPaint( handle.d->edge, VG_STROKE_PATH );
  vgSetf( VG_STROKE_LINE_WIDTH, 0.125f );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE );
  vgLoadIdentity();

  // Offset in mm.
  vgScale( handle.d->dpmm_x, handle.d->dpmm_y );
  // Move to the center.
  vgTranslate( handle.d->x_mm + 0.5f * handle.d->width_mm,
	       handle.d->y_mm + 0.5f * handle.d->height_mm );
#if 0
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgTranslate( 15.f, -15.f );
  VGfloat well[9];
  vgGetMatrix( well );
  printf( "%f %f %f\n", well[0], well[1], well[2] );
  printf( "%f %f %f\n", well[3], well[4], well[5] );
  printf( "%f %f %f\n", well[6], well[7], well[8] );
#if 0
  vgLoadIdentity();
#endif
#endif
  
  VGPath path;

  path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		       1.f, 0.f, 0, 0, VG_PATH_CAPABILITY_ALL );
  vguEllipse( path, 0.f, 0.f, handle.d->width_mm, handle.d->height_mm );
  vgDrawPath( path, VG_FILL_PATH | VG_STROKE_PATH );
  vgDestroyPath( path );

  vgSetPaint( handle.d->white, VG_FILL_PATH );
  path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		       1.f, 0.f, 0, 0, VG_PATH_CAPABILITY_ALL );
  vguEllipse( path, 0.f, 0.f, 0.75f*handle.d->width_mm, 0.75f*handle.d->height_mm );
  vgDrawPath( path, VG_FILL_PATH );
  vgDestroyPath( path );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgTranslate( -40.f, 40.f );

  vgSetPaint( handle.d->surface, VG_FILL_PATH );
  path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		       1.f, 0.f, 0, 0, VG_PATH_CAPABILITY_ALL );
  vguEllipse( path, -.3f, .3f, 0.7f*handle.d->width_mm, 0.7f*handle.d->height_mm );
  vgDrawPath( path, VG_FILL_PATH );
  vgDestroyPath( path );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE );
}

void button_widget_free_handle ( struct BUTTON_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    vgDestroyPaint( handle.d->white );
    vgDestroyPaint( handle.d->surface );
    vgDestroyPaint( handle.d->edge );
    free( handle.d );
    handle.d = NULL;
  }
}
