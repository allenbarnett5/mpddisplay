/*
 * Draw on the screen. Really only going to work on my Raspberry Pi
 * with the tiny TV monitor. We're primarily working with OpenVG here,
 * but we may need to throw in some VideoCore Dispmanx stuff as well.
 */
#include <stdio.h>

#include "bcm_host.h"
#include "EGL/egl.h"

#include "display_intf.h"

static const char* egl_carp ( void );

static EGLDisplay egl_display = EGL_NO_DISPLAY;

int display_init ( void )
{
  // There is a lot which can go wrong here. But evidently this can't
  // fail!
  bcm_host_init();

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
#if 0
    // These are all defaults.
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#endif
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

  EGLContext egl_context = eglCreateContext( egl_display, egl_config,
					     EGL_NO_CONTEXT, // no sharing
					     NULL );

  if ( egl_context == EGL_NO_CONTEXT ) {
    printf( "Error: Failed to create a rendering context: %s\n",
	    egl_carp() );
    return -1;
  }

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

  int window_width = dspmx_width;
  int window_height = dspmx_height;

  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.width = window_width << 16;
  src_rect.height = window_height << 16;
#if 0
  // The default for the window is to let any lower window appear
  // where the alpha of this window is < 1.0. However, you also
  // have to ask for a EGL context with an alpha in order for this
  // layer to not be opaque anyway.
  VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE /*| DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS*/, 
				255, /*alpha 0->255*/
				0 };
#endif
  DISPMANX_ELEMENT_HANDLE_T dspmx_element;
  dspmx_element = vc_dispmanx_element_add( dspmx_update,
					   dspmx_display,
					   0, /* layer */
					   &dst_rect,
					   0, /* src */
					   &src_rect,
					   DISPMANX_PROTECTION_NONE,
#if 0
					   &alpha, /* alpha */
#else
					   0, /* alpha */
#endif
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

  // Now connect the DISPMANX window to the EGL context.
  EGL_DISPMANX_WINDOW_T native_window;
  native_window.element = dspmx_element;
  native_window.width   = window_width;
  native_window.height  = window_height;

  EGLSurface egl_surface = eglCreateWindowSurface( egl_display, egl_config,
						   &native_window, NULL );

  if ( egl_surface == EGL_NO_SURFACE ) {
    printf( "Error: Could not bind VC window to EGL surface: %s\n",
	    egl_carp() );
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
  }
  return "unknown error";
}
