/*
 * Draw on the screen. Really only going to work on my Raspberry Pi
 * with the tiny TV monitor. We're primarily working with OpenVG here,
 * but we may need to throw in some VideoCore Dispmanx stuff as well.
 */
#include <stdio.h>
#include <unistd.h>
#include "bcm_host.h"
#include "EGL/egl.h"
#include "VG/openvg.h"
#include "VG/vgu.h"

#include "glib.h"

#include "mpd_intf.h"
#include "text_widget.h"
#include "image_widget.h"
#include "cover_image.h"
#include "display_intf.h"
#include "image_intf.h"

static const char* egl_carp ( void );

static int window_width = 0;
static int window_height = 0;
// The basic decoration.
static VGPath frame_path;
// The background of the decoration.
static VGPath background_path;
// The basic decoration color.
static VGPaint frame_paint;
// The decoration brush image.
static VGImage fg_brush;
// The decoration background brush image.
static VGImage bg_brush;

static VGPath thermometer_path;
static VGPaint thermometer_paint;
// Background color and alpha
static const VGfloat thermometer_color[] = { 0.f, 0.75f, 1.f, 0.3f };

// The frame texture is compiled into the code.
extern const unsigned char _binary_pattern_png_start;
extern const unsigned char _binary_pattern_png_end;

// Rather than burying these in the code, here are some constants
// which we can play with to make the screen look better.

// Approximate # of horz blanking NTSC DISPMANX pixels on *my* TV
static const VGfloat vc_frame_x = 0.f;//14.f;
// Approximate # of vert blanking NTSC DISPMANX lines on *my* TV
static const VGfloat vc_frame_y =  0.f;//8.f;
// Approximate width of *my* TV in mm
static const VGfloat tv_width = 108.74f; //95.25f;
// Approximate height of *my* TV in mm
static const VGfloat tv_height = 65.89f; //53.975f;
// Approximate # of visible NTSC DISPMANX pixels on *my* TV
static const VGfloat vc_frame_width = 800.f; //698.f;
// Approximate # of visible NTSC DISPMANX lines on *my* TV
static const VGfloat vc_frame_height = 480.f; //461.f;
// Resolution in X d/mm. (converts a distance to pixels)
static const VGfloat dpmm_x = 800.f/108.74f; //698./95.25; // vc_frame_width / tv_width;
// Resolution in Y d/mm. (converts a distance to pixels)
static const VGfloat dpmm_y = 480.f/65.89f; //461./53.975; // vc_frame_height / tv_height;
// Background color and alpha
static const VGfloat background[] = { 0.f, 0.f, 0.f, 1.f };

// The size of border decoration in mm.
static const VGfloat border_thickness = 1.f;
// Round size (mm).
static const VGfloat round_radius = 4.f;
// Fraction of the screen to devote to the text box.
static const VGfloat text_scale = 0.5f;
// Space between the border and the text (units?)
static const VGfloat text_gutter = 1.f;
// Thermometer gap (mm).
static const VGfloat thermometer_gap = 0.5f;

// The basic height of the font in mm.
static const VGfloat font_size_mm = 3.f;

struct DISPLAY_PRIVATE {
  int status;
  struct IMAGE_DB_HANDLE image_db;
  struct MPD_HANDLE mpd;
  EGLDisplay egl_display;
  EGLSurface egl_surface;
  // This must persist or the process segfaults and/or the system hangs!
  EGL_DISPMANX_WINDOW_T native_window;
  // Our metadata widget.
  struct TEXT_WIDGET_HANDLE metadata_widget;
  // Our time widget.
  struct TEXT_WIDGET_HANDLE time_widget;
  // The album cover widget.
  struct IMAGE_WIDGET_HANDLE cover_widget;

};

struct DISPLAY_HANDLE display_init ( struct IMAGE_DB_HANDLE image_db,
				     struct MPD_HANDLE mpd )
{
  struct DISPLAY_HANDLE handle;
  handle.d = malloc( sizeof( struct DISPLAY_PRIVATE ) );
  handle.d->status      = 0;
  handle.d->image_db    = image_db;
  handle.d->mpd         = mpd;
  handle.d->egl_display = EGL_NO_DISPLAY;
  handle.d->egl_surface = EGL_NO_SURFACE;

  // There is a lot which can go wrong here. But evidently this can't
  // fail!
  bcm_host_init();

  // Acquire the native VideoCore (DISPMANX) buffer.

  uint32_t dspmx_width;
  uint32_t dspmx_height;
  int32_t dspmx_ret = graphics_get_display_size( 0,
						 &dspmx_width,
						 &dspmx_height );

  if ( dspmx_ret < 0 ) {
    printf( "Error: (VC) Could not the display size for display 0: code %d\n",
	    dspmx_ret );
    handle.d->status = -1;
    return handle;
  }

  DISPMANX_DISPLAY_HANDLE_T dspmx_display;
  dspmx_display = vc_dispmanx_display_open( 0 );

  if ( dspmx_display == DISPMANX_NO_HANDLE ) {
    printf( "Error: (VC) Could not open display 0\n" );
    handle.d->status = -1;
    return handle;
  }

  DISPMANX_UPDATE_HANDLE_T dspmx_update;
  dspmx_update = vc_dispmanx_update_start( 0 );

  if ( dspmx_update == DISPMANX_NO_HANDLE ) {
    printf( "Error: (VC) Could not get update handle\n" );
    handle.d->status = -1;
    return handle;
  }

  // After a lot of experimentation, I decided to just render into a
  // buffer the same size as the TV buffer (720x480). This is really
  // distorted onto the TV, though, partly because of the conversion
  // to wide screen (by the TV) and partly due to the non-unity
  // aspect of the pixels on the screen.

  VC_RECT_T dst_rect;
  VC_RECT_T src_rect;

  int window_x = 0;
  int window_y = 0;

  dst_rect.x = window_x;
  dst_rect.y = window_y;
  dst_rect.width = dspmx_width;
  dst_rect.height = dspmx_height;

  window_width = dspmx_width;
  window_height = dspmx_height;

  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.width = window_width << 16;
  src_rect.height = window_height << 16;

  DISPMANX_ELEMENT_HANDLE_T dspmx_element;
  dspmx_element = vc_dispmanx_element_add( dspmx_update,
					   dspmx_display,
					   0, /* layer */
					   &dst_rect,
					   0, /* src */
					   &src_rect,
					   DISPMANX_PROTECTION_NONE,
					   0, /* alpha */
					   0, /* clamp */
					   0  /* transform */ );
  if ( dspmx_element == DISPMANX_NO_HANDLE ) {
    printf( "Error: (VC) Could not create element (aka window)\n" );
    handle.d->status = -1;
    return handle;
  }

  dspmx_ret = vc_dispmanx_update_submit_sync( dspmx_update );

  if ( dspmx_ret < 0 ) {
    printf( "Error: (VC) Could not complete element update: code %d\n",
	    dspmx_ret );
    handle.d->status = -1;
    return handle;
  }

  handle.d->egl_display = eglGetDisplay( EGL_DEFAULT_DISPLAY );

  if ( handle.d->egl_display == EGL_NO_DISPLAY ) {
    printf( "Error: Could not open the EGL display\n" );
    handle.d->status = -1;
    return handle;
  }

  EGLBoolean egl_inited = eglInitialize( handle.d->egl_display, NULL, NULL );

  if ( egl_inited == EGL_FALSE ) {
    printf( "Error: Could not initialize EGL display: %s\n", egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  EGLBoolean egl_got_api = eglBindAPI( EGL_OPENVG_API );

  if ( egl_got_api == EGL_FALSE ) {
    printf( "Error: Could not bind OpenVG API: %s (OpenVG is probably not suppoerted)\n", egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  EGLint    egl_attrs[] = {
    // These are all defaults.
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    // If this is not set to something, then the DISPMANX window
    // is opaque. (So, there's not much reason for it here.)
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLConfig egl_config;
  EGLint    egl_n_configs = 0;

  EGLBoolean got_config  = eglChooseConfig( handle.d->egl_display,
					    egl_attrs, &egl_config, 1,
					    &egl_n_configs );

  if ( got_config == EGL_FALSE ) {
    printf( "Error: Could not find a usable config: %s\n", egl_carp() );
    handle.d->status = -1;
    return handle;
  }
  else if ( egl_n_configs == 0 ) {
    printf( "Error: No matching EGL configs\n" );
    handle.d->status = -1;
    return handle;
  }

  // Now connect the DISPMANX window to the EGL context. Note again
  // that the native_window object has to hang around until the
  // program is done.
  handle.d->native_window.element = dspmx_element;
  handle.d->native_window.width   = window_width;
  handle.d->native_window.height  = window_height;

  handle.d->egl_surface = eglCreateWindowSurface( handle.d->egl_display,
						  egl_config,
						  &handle.d->native_window,
						  NULL );

  if ( handle.d->egl_surface == EGL_NO_SURFACE ) {
    printf( "Error: Could not bind VC window to EGL surface: %s\n",
	    egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  EGLContext egl_context = eglCreateContext( handle.d->egl_display, egl_config,
					     EGL_NO_CONTEXT, // no sharing
					     NULL );

  if ( egl_context == EGL_NO_CONTEXT ) {
    printf( "Error: Failed to create a rendering context: %s\n",
	    egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  EGLBoolean egl_current = eglMakeCurrent( handle.d->egl_display,
					   handle.d->egl_surface,
					   handle.d->egl_surface,
					   egl_context );

  if ( egl_current == EGL_FALSE ) {
    printf( "Error: Failed to make context current: %s\n", egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: Hmm. something happened which wasn't reported: 0x%x\n",
	    eglGetError() );
    handle.d->status = -1;
    return handle;
  }

  vgSetfv( VG_CLEAR_COLOR, 4, background );
  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: EGL Couldn't set background color 0x%x\n", eglGetError() );
    handle.d->status = -1;
    return handle;
  }
  if ( vgGetError() != VG_NO_ERROR ) {
    printf( "Error: VG Couldn't set background color 0x%x\n", vgGetError() );
  }
  vgClear( 0, 0, window_width, window_height );
  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: EGL Couldn't clear window\n" );
    handle.d->status = -1;
    return handle;
  }
  if ( vgGetError() != VG_NO_ERROR ) {
    printf( "Error: VG Couldn't clear window 0x%x\n", vgGetError() );
    handle.d->status = -1;
    return handle;
  }

  size_t pattern_size =
    &_binary_pattern_png_end - &_binary_pattern_png_start;

  struct IMAGE_HANDLE pattern = image_rgba_create( &_binary_pattern_png_start,
						   pattern_size );
  int pattern_width = image_rgba_width( pattern );
  int pattern_height = image_rgba_height( pattern );
  unsigned char* image = image_rgba_image( pattern );

  fg_brush = vgCreateImage( VG_sRGBA_8888, pattern_width, pattern_height,
			    VG_IMAGE_QUALITY_NONANTIALIASED );
  vgImageSubData( fg_brush, image, pattern_width * 4,
		  VG_sABGR_8888, 0, 0, pattern_width, pattern_height );

  bg_brush = vgCreateImage( VG_sRGBA_8888, pattern_width, pattern_height,
			    VG_IMAGE_QUALITY_NONANTIALIASED );
  float darken[] = {
    0.5, 0., 0., 0., // R-src -> {RGBA}-dest
    0., 0.5, 0., 0., // G-src -> {RGBA}-dest
    0., 0., 0.5, 0., // B-src -> {RGBA}-dest
    0., 0., 0., 1., // A-src -> {RGBA}-dest
    0., 0., 0., 0.  // const -> {RGBA}-dest
  };

  vgColorMatrix( bg_brush, fg_brush, darken );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();
  vgScale( 0.1, 0.1 );

  frame_paint = vgCreatePaint();
  vgSetParameteri( frame_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_PATTERN );
  vgSetParameteri( frame_paint, VG_PAINT_PATTERN_TILING_MODE,
		   VG_TILE_REPEAT );

  image_rgba_free( pattern );

  // Prepare to draw.
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE );
  vgLoadIdentity();
  vgTranslate( vc_frame_x, vc_frame_y );
  // We should be able to draw in mm and squares should be square.
  vgScale( dpmm_x, dpmm_y );

  // The background is a dark brushed metal.
  background_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
				  VG_PATH_DATATYPE_F,
				  1.0f, 0.0f,
				  0, 0,
				  VG_PATH_CAPABILITY_ALL );
  vguRect( background_path, 0.f, 0.f, tv_width, tv_height );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgPaintPattern( frame_paint, bg_brush );

  vgDrawPath( background_path, VG_FILL_PATH );

  // Define the main frame.
  frame_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
			     VG_PATH_DATATYPE_F,
			     1.0f, 0.0f,
			     0, 0,
			     VG_PATH_CAPABILITY_ALL );

  vguRect( frame_path, 0.f, 0.f, tv_width, tv_height );

  // Metadata box.
  vguRoundRect( frame_path,
	   border_thickness,
	   border_thickness,
	   tv_width / 2.f - 1.5f * border_thickness,
	   tv_height - 2.f * border_thickness,
	   round_radius, round_radius );

  // Image box.
  float image_edge_length = tv_width / 2.f - 1.5 * border_thickness;
  vguRoundRect( frame_path,
	   tv_width / 2.f + border_thickness / 2.f,
	   tv_height - border_thickness - image_edge_length,
	   image_edge_length,
	   image_edge_length,
	   round_radius, round_radius
	   );

  // Thermometer box.

  // This should really be a function of the font height.
  float therm_height = 2.f * font_size_mm;
  float therm_width  = image_edge_length;
  float therm_x      = tv_width / 2.f + border_thickness / 2.f;
  float therm_y      = border_thickness;

  vguRoundRect( frame_path,
		therm_x,
		therm_y,
		therm_width,
		therm_height,
		round_radius, round_radius );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgPaintPattern( frame_paint, fg_brush );

  vgDrawPath( frame_path, VG_FILL_PATH );

  thermometer_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
				   VG_PATH_DATATYPE_F,
				   1.0f, 0.0f,
				   0, 0,
				   VG_PATH_CAPABILITY_ALL );

  thermometer_paint = vgCreatePaint();

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();

  vgSetParameteri( thermometer_paint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT );
  VGfloat gradient_points[] = { 0.f, border_thickness,
				0.f, border_thickness + therm_height };
  vgSetParameterfv( thermometer_paint, VG_PAINT_LINEAR_GRADIENT,
		    4, gradient_points );
  float fill_stops[] = {
    0., 0., 0.75, 1., 0.01,
    0.25, 0., 0.75, 1., 0.7,
    0.5, 0., 0.75, 1., 0.3,
    1., 0., 0.75, 1., 0.0,
  };
  vgSetParameterfv( thermometer_paint, VG_PAINT_COLOR_RAMP_STOPS,
		    4 * 5, fill_stops );

  handle.d->metadata_widget =
    text_widget_init( border_thickness, border_thickness,
		      tv_width / 2.f - 1.5f * border_thickness,
		      tv_height - 2.f * border_thickness,
		      dpmm_x, dpmm_y );

  // \bug widget height is a judgement text_widget should really
  // have a vertical centering option.
  handle.d->time_widget =
    text_widget_init( tv_width / 2.f + border_thickness / 2.f,
		      border_thickness,
		      image_edge_length, therm_height - border_thickness,
		      dpmm_x, dpmm_y );

  text_widget_set_alignment( handle.d->time_widget, TEXT_WIDGET_ALIGN_CENTER );

  // The cover widget. Where is it going to go? Need to specify
  // the width and height carefully so that the aspect ratio is
  // correct.
  float iw_x_mm = tv_width / 2.f + border_thickness / 2.f;
  float iw_y_mm = tv_height - border_thickness - image_edge_length;
  float iw_width_mm = image_edge_length;
  float iw_height_mm = iw_width_mm;

  handle.d->cover_widget = image_widget_init( iw_x_mm, iw_y_mm,
					      iw_width_mm, iw_height_mm,
					      dpmm_x, dpmm_y );

  EGLBoolean swapped = eglSwapBuffers( handle.d->egl_display,
				       handle.d->egl_surface );

  if ( swapped == EGL_FALSE ) {
    printf( "Error: Could not swap EGL buffers: %s\n", egl_carp() );
    handle.d->status = -1;
    return handle;
  }

  return handle;
}

void display_update ( struct DISPLAY_HANDLE handle )
{
  // For now, this is very simple. Redraw the whole screen.

  vgClear( 0, 0, window_width, window_height );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();
  vgScale( 0.1, 0.1 );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgPaintPattern( frame_paint, bg_brush );

  vgDrawPath( background_path, VG_FILL_PATH );

  vgPaintPattern( frame_paint, fg_brush );

  if ( mpd_changed( handle.d->mpd,
		    MPD_CHANGED_ARTIST | MPD_CHANGED_ALBUM | MPD_CHANGED_TITLE ) ) {
    char* buffer =
      g_markup_printf_escaped( "<span font=\"Droid Sans 24px\">%s\n<i>%s</i>\n<b>%s</b></span>",
			       mpd_artist( handle.d->mpd ),
			       mpd_album( handle.d->mpd ),
			       mpd_title( handle.d->mpd ) );

    size_t len = strlen( buffer );

    text_widget_set_text( handle.d->metadata_widget, buffer, len );

    g_free( buffer );
  }

  struct MPD_TIMES times = mpd_times( handle.d->mpd );

  if ( mpd_changed( handle.d->mpd,
		    MPD_CHANGED_ELAPSED | MPD_CHANGED_TOTAL ) ) {
    char* buffer =
      g_markup_printf_escaped( "<span font=\"Droid Sans 24px\">%02ld:%02ld / %02ld:%02ld</span>",
			       times.elapsed / 60,
			       times.elapsed % 60,
			       times.total / 60,
			       times.total % 60 );

    size_t len = strlen( buffer );

    text_widget_set_text( handle.d->time_widget, buffer, len );

    g_free( buffer );

    vgClearPath( thermometer_path, VG_PATH_CAPABILITY_ALL );

    if ( times.total > 0 ) {
      float image_edge = tv_width / 2.f - 1.5 * border_thickness;
      float time_height = tv_height - border_thickness - image_edge - border_thickness - border_thickness - 2.f * thermometer_gap;

      VGfloat thermometer_width =
	(float)times.elapsed / (float)times.total *
	( tv_width / 2.f - 1.5f * border_thickness - 2.f * thermometer_gap );


      vguRoundRect( thermometer_path, 
		    tv_width / 2.f + border_thickness / 2.f + thermometer_gap,
		    border_thickness + thermometer_gap,
		    thermometer_width,
		    time_height,
		    round_radius, round_radius );
    }
  }

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();

  vgSetPaint( thermometer_paint, VG_FILL_PATH );
  vgDrawPath( thermometer_path, VG_FILL_PATH );

  text_widget_draw_text( handle.d->metadata_widget );

  text_widget_draw_text( handle.d->time_widget );

  if ( mpd_changed( handle.d->mpd, MPD_CHANGED_ALBUM ) ) {

    struct IMAGE_HANDLE cover_image_handle =
      cover_image( handle.d->image_db,
		   mpd_artist( handle.d->mpd ),
		   mpd_album( handle.d->mpd ) );

    image_widget_set_image( handle.d->cover_widget, cover_image_handle );

    // I guess we're responsible for this and can assume that the
    // widget doesn't need it anymore.
    image_rgba_free( cover_image_handle );
  }

  image_widget_draw_image( handle.d->cover_widget );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();
  vgScale( 0.1, 0.1 );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgDrawPath( frame_path, VG_FILL_PATH );

  EGLBoolean swapped = eglSwapBuffers( handle.d->egl_display,
				       handle.d->egl_surface );

  if ( swapped == EGL_FALSE ) {
    printf( "Error: Could not swap EGL buffers: %s\n", egl_carp() );
    handle.d->status = -1;
  }
}

int display_status ( struct DISPLAY_HANDLE handle )
{
  if ( handle.d != 0 ) {
    return handle.d->status;
  }
  return -1;
}

void display_close ( struct DISPLAY_HANDLE handle )
{
  if ( handle.d != NULL ) {
    text_widget_free_handle( handle.d->metadata_widget );
    text_widget_free_handle( handle.d->time_widget );
    image_widget_free_handle( handle.d->cover_widget );

    eglTerminate( handle.d->egl_display );
    // \bug what about the native window?
    image_db_free( handle.d->image_db );

    free( handle.d );
    handle.d = NULL;
  }
}

static const char* egl_carp ( void )
{
  EGLint err = eglGetError();
  switch ( err ) {
  case EGL_SUCCESS:
    return "success";
  case EGL_NOT_INITIALIZED:
    return "(EGL_NOT_INITIALIZED) EGL is not initialized, or could not be initialized, for the specified EGL display connection";
  case EGL_BAD_DISPLAY:
    return "(EGL_BAD_DISPLAY) An EGLDisplay argument does not name a valid EGL display connection";
  case EGL_BAD_PARAMETER:
    return "(EGL_BAD_PARAMETER) One or more argument values are invalid";
  case EGL_BAD_ATTRIBUTE:
    return "(EGL_BAD_ATTRIBUTE) An unrecognized attribute or attribute value was passed in the attribute list";
  case EGL_BAD_MATCH:
    return "(EGL_BAD_MATCH) Arguments are inconsistent";
  case EGL_BAD_CONFIG:
    return "(EGL_BAD_CONFIG) An EGLConfig argument does not name a valid EGL frame buffer configuration";
  case EGL_BAD_CONTEXT:
    return "(EGL_BAD_CONTEXT) An EGLContext argument does not name a valid EGL rendering context";
  case EGL_BAD_ALLOC:
    return "(EGL_BAD_ALLOC) EGL failed to allocate resources for the requested operation";
  case EGL_BAD_NATIVE_WINDOW:
    return "(EGL_BAD_NATIVE_WINDOW) A NativeWindowType argument does not refer to a valid native window";
  case EGL_BAD_SURFACE:
    return "(EGL_BAD_SURFACE) An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering";
  case EGL_BAD_ACCESS:
    return "(EGL_BAD_ACCESS) EGL cannot access a requested resource (for example a context is bound in another thread)";
  case EGL_BAD_NATIVE_PIXMAP:
    return "(EGL_BAD_NATIVE_PIXMAP) A NativePixmapType argument does not refer to a valid native pixmap";
  case EGL_BAD_CURRENT_SURFACE:
    return "(EGL_BAD_CURRENT_SURFACE) The current surface of the callling thread is a window, pixel buffer or pixmap that is no longer valid";
  case EGL_CONTEXT_LOST:
    return "(EGL_CONTEXT_LOST) A power management event has occurred. The application must destroy all contexts and reinitialize OpenGL ES state and objects to continue rendering";
  }
  return "(OpenVG) unknown error";
}
