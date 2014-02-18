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

#include "mpd_intf.h"
#include "text_widget.h"
#include "image_widget.h"
#include "cover_image.h"
#include "display_intf.h"
#include "magick_image.h"

static const char* egl_carp ( void );

// \bug better if the main program is handed some object to hold
// this state. Although technically we can ask EGL about these
// objects through the API.
static EGLDisplay egl_display = EGL_NO_DISPLAY;
static EGLSurface egl_surface = EGL_NO_SURFACE;
// This must persist.
static EGL_DISPMANX_WINDOW_T native_window;
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

// The frame texture is compiled into the code.
extern const unsigned char _binary_pattern_png_start;
extern const unsigned char _binary_pattern_png_end;

// Rather than burying these in the code, here are some constants
// which we can play with to make the screen look better.

// Approximate # of horz blanking NTSC DISPMANX pixels on *my* TV
static const VGfloat vc_frame_x = 14.f;
// Approximate # of vert blanking NTSC DISPMANX lines on *my* TV
static const VGfloat vc_frame_y =  8.f;
// Approximate width of *my* TV in mm
static const VGfloat tv_width = 95.25f;
// Approximate height of *my* TV in mm
static const VGfloat tv_height = 53.975f;
// Approximate # of visible NTSC DISPMANX pixels on *my* TV
static const VGfloat vc_frame_width = 698.f;
// Approximate # of visible NTSC DISPMANX lines on *my* TV
static const VGfloat vc_frame_height = 461.f;
// Resolution in X d/mm. (converts a distance to pixels)
static const VGfloat dpmm_x = 698./95.25; // vc_frame_width / tv_width;
// Resolution in Y d/mm. (converts a distance to pixels)
static const VGfloat dpmm_y = 461./53.975; // vc_frame_height / tv_height;
// Background color and alpha
static const VGfloat background[] = { 0.f, 0.f, 0.f, 1.f };

// The size of border decoration in mm.
static const VGfloat border_thickness = 2.f;
// Fraction of the screen to devote to the text box.
static const VGfloat text_scale = 0.5f;
// Space between the border and the text (units?)
static const VGfloat text_gutter = 1.f;

// The basic height of the font in mm.
static const VGfloat font_size_mm = 3.f;

// Our metadata widget.
static struct TEXT_WIDGET_HANDLE metadata_widget;
// Our time widget.
static struct TEXT_WIDGET_HANDLE time_widget;
// The album cover widget.
static struct IMAGE_WIDGET_HANDLE cover_widget;

int display_init ( void )
{
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
    return -1;
  }

  DISPMANX_DISPLAY_HANDLE_T dspmx_display;
  dspmx_display = vc_dispmanx_display_open( 0 );

  if ( dspmx_display == DISPMANX_NO_HANDLE ) {
    printf( "Error: (VC) Could not open display 0\n" );
    return -1;
  }

  DISPMANX_UPDATE_HANDLE_T dspmx_update;
  dspmx_update = vc_dispmanx_update_start( 0 );

  if ( dspmx_update == DISPMANX_NO_HANDLE ) {
    printf( "Error: (VC) Could not get update handle\n" );
    return -1;
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
    return -1;
  }

  dspmx_ret = vc_dispmanx_update_submit_sync( dspmx_update );

  if ( dspmx_ret < 0 ) {
    printf( "Error: (VC) Could not complete element update: code %d\n",
	    dspmx_ret );
    return -1;
  }

  egl_display = eglGetDisplay( EGL_DEFAULT_DISPLAY );

  if ( egl_display == EGL_NO_DISPLAY ) {
    printf( "Error: Could not open the EGL display\n" );
    return -1;
  }

  EGLBoolean egl_inited = eglInitialize( egl_display, NULL, NULL );

  if ( egl_inited == EGL_FALSE ) {
    printf( "Error: Could not initialize EGL display: %s\n", egl_carp() );
    return -1;
  }

  EGLBoolean egl_got_api = eglBindAPI( EGL_OPENVG_API );

  if ( egl_got_api == EGL_FALSE ) {
    printf( "Error: Could not bind OpenVG API: %s (OpenVG is probably not suppoerted)\n", egl_carp() );
    return -1;
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

  EGLBoolean got_config  = eglChooseConfig( egl_display,
					    egl_attrs, &egl_config, 1,
					    &egl_n_configs );

  if ( got_config == EGL_FALSE ) {
    printf( "Error: Could not find a usable config: %s\n", egl_carp() );
    return -1;
  }
  else if ( egl_n_configs == 0 ) {
    printf( "Error: No matching EGL configs\n" );
    return -1;
  }

  // Now connect the DISPMANX window to the EGL context. Note again
  // that the native_window object has to hang around until the
  // program is done.
  native_window.element = dspmx_element;
  native_window.width   = window_width;
  native_window.height  = window_height;

  egl_surface = eglCreateWindowSurface( egl_display, egl_config,
					&native_window, NULL );

  if ( egl_surface == EGL_NO_SURFACE ) {
    printf( "Error: Could not bind VC window to EGL surface: %s\n",
	    egl_carp() );
    return -1;
  }

  EGLContext egl_context = eglCreateContext( egl_display, egl_config,
					     EGL_NO_CONTEXT, // no sharing
					     NULL );

  if ( egl_context == EGL_NO_CONTEXT ) {
    printf( "Error: Failed to create a rendering context: %s\n",
	    egl_carp() );
    return -1;
  }

  EGLBoolean egl_current = eglMakeCurrent( egl_display, egl_surface,
					   egl_surface, egl_context );

  if ( egl_current == EGL_FALSE ) {
    printf( "Error: Failed to make context current: %s\n", egl_carp() );
    return -1;
  }

  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: Hmm. something happened which wasn't reported: 0x%x\n",
	    eglGetError() );
    return -1;
  }

  vgSetfv( VG_CLEAR_COLOR, 4, background );
  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: EGL Couldn't set background color 0x%x\n", eglGetError() );
    return -1;
  }
  if ( vgGetError() != VG_NO_ERROR ) {
    printf( "Error: VG Couldn't set background color 0x%x\n", vgGetError() );
  }
  vgClear( 0, 0, window_width, window_height );
  if ( eglGetError() != EGL_SUCCESS ) {
    printf( "Error: EGL Couldn't clear window\n" );
    return -1;
  }
  if ( vgGetError() != VG_NO_ERROR ) {
    printf( "Error: VG Couldn't clear window 0x%x\n", vgGetError() );
    return -1;
  }

  size_t pattern_size =
    &_binary_pattern_png_end - &_binary_pattern_png_start;

  struct IMAGE_HANDLE pattern = image_rgba_create( "PNG",
						   &_binary_pattern_png_start,
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

  // Define the main frame.
  frame_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
			     VG_PATH_DATATYPE_F,
			     1.0f, 0.0f,
			     0, 0,
			     VG_PATH_CAPABILITY_ALL );

  vguRect( frame_path, 0.f, 0.f, tv_width, tv_height );

  // Text box.
  vguRect( frame_path, border_thickness, border_thickness,
	   tv_width / 2.f - border_thickness / 2.f,
	   tv_height - 2.f * border_thickness );

  // Time box.
  vguRect( frame_path,
	   tv_width / 2.f + border_thickness,
	   border_thickness,
	   tv_width / 2.f - 2.f * border_thickness,
	   2.f * border_thickness );

  vgDrawPath( frame_path, VG_FILL_PATH );

  background_path = vgCreatePath( VG_PATH_FORMAT_STANDARD,
				  VG_PATH_DATATYPE_F,
				  1.0f, 0.0f,
				  0, 0,
				  VG_PATH_CAPABILITY_ALL );
  vguRect( background_path, 0.f, 0.f, tv_width, tv_height );

  EGLBoolean swapped = eglSwapBuffers( egl_display, egl_surface );

  if ( swapped == EGL_FALSE ) {
    printf( "Error: Could not swap EGL buffers: %s\n", egl_carp() );
    return -1;
  }

  // \bug need to think more deeply about what the right description
  // of the text box should be passed to the widget. The font cares
  // about the screen DPI, but (I think) the layout is done on the
  // basis of pixels.
  metadata_widget = text_widget_init( vc_frame_x + border_thickness * dpmm_x
				      + text_gutter,
				      -border_thickness * dpmm_y,
				      tv_width/2.f, tv_height,
				      vc_frame_width/2.f, vc_frame_height );

  // Not sure what the parameters of this should be. A height of 35.f
  // looks ok for now, but really depends on the font.
  time_widget = text_widget_init( vc_frame_x + border_thickness * dpmm_x
				  + text_gutter + vc_frame_width/2.,
				  vc_frame_y + border_thickness * dpmm_y,
				  tv_width/2.f, tv_height,
				  vc_frame_width/2.f - 2.f*border_thickness * dpmm_y, 30.f );

  text_widget_set_alignment( time_widget, TEXT_WIDGET_ALIGN_CENTER );

  // The cover widget. Where is it going to go? Need to specify
  // the width and height carefully so that the aspect ratio is
  // correct.
  float iw_x_pix = vc_frame_width/2.f + 2.f * border_thickness * dpmm_x;
  float iw_y_pix = vc_frame_height-border_thickness * dpmm_y;
  float iw_width_mm = tv_width / 2.f - 2.f * border_thickness;
  float iw_height_mm = iw_width_mm;
  float iw_width_pix = iw_width_mm * dpmm_x;
  float iw_height_pix = iw_height_mm * dpmm_y;
  cover_widget = image_widget_init( iw_x_pix, iw_y_pix,
				    iw_width_mm, iw_height_mm,
				    iw_width_pix, iw_height_pix );

  return 0;
}

int display_update ( const struct MPD_CURRENT* current )
{
  // For now, this is very simple. Redraw the whole screen.

  vgClear( 0, 0, window_width, window_height );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgPaintPattern( frame_paint, bg_brush );

  vgDrawPath( background_path, VG_FILL_PATH );

  vgPaintPattern( frame_paint, fg_brush );

  vgDrawPath( frame_path, VG_FILL_PATH );

  if ( current->changed &
       ( MPD_CHANGED_ARTIST | MPD_CHANGED_ALBUM | MPD_CHANGED_TITLE ) ) {
    char* buffer =
      g_markup_printf_escaped( "<span font=\"Droid Sans 26px\">%s\n<i>%s</i>\n<b>%s</b></span>",
			       current->artist->str,
			       current->album->str,
			       current->title->str );

    GString* w_buf = g_string_new( buffer ); // For counting the characters.

    text_widget_set_text( metadata_widget, w_buf );

    g_string_free( w_buf, TRUE );
  }

  if ( current->changed & ( MPD_CHANGED_ELAPSED | MPD_CHANGED_TOTAL ) ) {
    char* buffer =
      g_markup_printf_escaped( "<span font=\"Droid Sans 26px\">%02d:%02d / %02d:%02d</span>",
			       current->elapsed_time / 60,
			       current->elapsed_time % 60,
			       current->total_time / 60,
			       current->total_time % 60 );
    GString* w_buf = g_string_new( buffer ); // For counting the characters.

    text_widget_set_text( time_widget, w_buf );

    g_string_free( w_buf, TRUE );
  }

  text_widget_draw_text( metadata_widget );

  text_widget_draw_text( time_widget );

  if ( current->changed & MPD_CHANGED_ALBUM ) {
    struct IMAGE_HANDLE cover_image_handle =
      cover_image( current->artist->str, current->album->str );
    image_widget_set_image( cover_widget, cover_image_handle );
  }

  image_widget_draw_image( cover_widget );

  EGLBoolean swapped = eglSwapBuffers( egl_display, egl_surface );

  if ( swapped == EGL_FALSE ) {
    printf( "Error: Could not swap EGL buffers: %s\n", egl_carp() );
    return -1;
  }

  return 0;
}

int display_close ( void )
{
  eglTerminate( egl_display );

  return 0;
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
