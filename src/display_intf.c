/*
 * Draw on the screen. Really only going to work on my Raspberry Pi
 * with the tiny TV monitor.
 * Now featuring DRM/GBM/KMS/EGL/OpenGL ES.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include "EGL/egl.h"
#include <GLES3/gl3.h>

#include "glib.h"

#include "mpd_intf.h"
#include "text_widget.h"
#include "image_widget.h"
#include "thermometer_widget.h"
#include "cover_image.h"
#include "image_intf.h"
#include "log_intf.h"
#include "display_intf.h"
#include "emblem_widget.h"

static const char* egl_carp ( void );

// The frame texture is compiled into the code.
extern const unsigned char _binary_pattern_png_start;
extern const unsigned char _binary_pattern_png_end;

// Rather than burying these in the code, here are some constants
// which we can play with to make the screen look better.
#if 0
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
// Thermometer gap (mm).
static const float thermometer_gap = 0.5f;
#elif 0
// The size of border decoration in mm.
static const float border_thickness = 1.f;
// Approximate height of *my* TV in mm
static const float tv_height = 65.89f; //53.975f;
// Approximate width of *my* TV in mm
static const float tv_width = 108.74f; //95.25f;
// The basic height of the font in mm.
static const float font_size_mm = 3.f;
// Resolution in X d/mm. (converts a distance to pixels)
static const float dpmm_x = 800.f/108.74f; //698./95.25; // vc_frame_width / tv_width;
// Resolution in Y d/mm. (converts a distance to pixels)
static const float dpmm_y = 480.f/65.89f; //461./53.975; // vc_frame_height /
#else
// The size of border decoration in mm.
static const float border_thickness = 1.f;
// Approximate height of Pi 7" touchscreen (mm)
static const float tv_height = 85.92f;
// Approximate width of Pi 7" touchscreen (mm)
static const float tv_width = 154.08f;
// The basic height of the font in mm.
static const float font_size_mm = 3.f;
// Resolution in X d/mm. (converts a distance to pixels)
static const float dpmm_x = 800.f/tv_width;
// Resolution in Y d/mm. (converts a distance to pixels)
static const float dpmm_y = 480.f/tv_height;
// Thermometer gap (mm).
static const float thermometer_gap = 0.5f;
// Emblem size (mm).
static const float emblem_size = 10.f;
#endif
struct DISPLAY_PRIVATE {
  int status;
  struct IMAGE_DB_HANDLE image_db;
  struct MPD_HANDLE mpd;
  int fd;
  drmModeConnector* connector;
  drmModeModeInfo* mode;
  drmModeCrtc* crtc;
  struct gbm_surface* gbm_surface;
  EGLDisplay egl_display;
  EGLSurface egl_surface;
  struct gbm_bo* previous_bo;
  struct drm_fb {
    struct gbm_bo* bo;
    uint32_t       fb_id;
  } drm_fb;
  // Our metadata widget.
  struct TEXT_WIDGET_HANDLE metadata_widget;
  // Our time widget.
  struct TEXT_WIDGET_HANDLE time_widget;
  // The album cover widget.
  struct IMAGE_WIDGET_HANDLE cover_widget;
  // The thermometer widget.
  struct THERMOMETER_WIDGET_HANDLE thermometer_widget;
  // The emblem widget.
  struct EMBLEM_WIDGET_HANDLE emblem_widget;
  // Logger
  struct LOG_HANDLE logger;
};

static void drm_fb_destroy_callback ( struct gbm_bo* bo, void* data )
{
  printf( "Destroy callback called\n" );
  int fd = gbm_device_get_fd( gbm_bo_get_device( bo ) );
  struct drm_fb* fb = (struct drm_fb*)data;
  if ( fb && fb->fb_id ) {
    drmModeRmFB( fd, fb->fb_id );
  }
  free( fb );
}

struct DISPLAY_HANDLE display_init ( struct IMAGE_DB_HANDLE image_db,
				     struct MPD_HANDLE mpd,
                                     struct LOG_HANDLE logger )
{
  struct DISPLAY_HANDLE handle;
  handle.d = malloc( sizeof( struct DISPLAY_PRIVATE ) );
  handle.d->status      = 0;
  handle.d->image_db    = image_db;
  handle.d->mpd         = mpd;
  handle.d->mode        = NULL;
  handle.d->egl_display = EGL_NO_DISPLAY;
  handle.d->egl_surface = EGL_NO_SURFACE;
  handle.d->previous_bo = NULL;
  handle.d->logger      = logger;
  
  // This should be a command line option.
  // Even better is to search the available devices until we find one
  // which works.
#define MAX_DRM_DEVICES (64)
  drmDevicePtr devices[MAX_DRM_DEVICES] = { NULL };
  int num_devices = -1;
  num_devices = drmGetDevices2( 0, devices, MAX_DRM_DEVICES );
  log_message_info( logger, "Found %d devices\n", num_devices );
  int d = 0;
  for ( ; d < num_devices; ++d ) {
    drmDevicePtr device = devices[d];
    if ( ! ( device->available_nodes & ( 1 << DRM_NODE_PRIMARY ) ) ) {
      log_message_info( logger, "skipping dev %d '%s' (not primary)", d, 
                        device->nodes[0] );
      continue;
    }

    handle.d->fd = open( device->nodes[DRM_NODE_PRIMARY], O_RDWR );

    if ( handle.d->fd < 0 ) {
      log_message_info( logger, "Could not open dri device %d '%s': %s", d,
                        device->nodes[DRM_NODE_PRIMARY],
                        strerror( errno ) );
      continue;
    }
#if 0
    drmModePlaneRes* pln_res = drmModeGetPlaneResources( fd );
    if ( pln_res != NULL ) {
      for ( int i = 0; i < pln_res->count_planes; ++i ) {
	drmModePlane* ovr = drmModeGetPlane( fd, pln_res->planes[i] );
	drmModeFreePlane( ovr );
      }
    }
    drmModeFreePlaneResources( pln_res );
#endif
    drmModeRes* resources;
    resources = drmModeGetResources( handle.d->fd );

    if ( resources == NULL ) {
      log_message_info( logger, "Could not get resources for %d '%s'", d,
                        device->nodes[DRM_NODE_PRIMARY] );
      handle.d->status = -1;
      continue;
    }

    handle.d->connector =
      drmModeGetConnector( handle.d->fd, resources->connectors[0] );

    if ( handle.d->connector == NULL ) {
      log_message_info( logger, "Could not get connector for %d '%s'", d, 
                        device->nodes[DRM_NODE_PRIMARY] );
      handle.d->status = -1;
      continue;
    }

    for ( int i = 0; i < handle.d->connector->count_modes; ++i ) {
      handle.d->mode = &handle.d->connector->modes[i];
      if ( handle.d->mode->type & DRM_MODE_TYPE_PREFERRED ) {
        break;
      }
    }

    if ( handle.d->mode == NULL ) {
      log_message_info( logger, 
                        "Could not find preferred mode for %d '%s'", d, 
                        device->nodes[DRM_NODE_PRIMARY] );
      handle.d->status = -1;
      continue;
    }

    drmModeEncoder* encoder = drmModeGetEncoder( handle.d->fd,
					 handle.d->connector->encoder_id );

    if ( encoder == NULL ) {
      log_message_info( logger, "Could not get encoder for %d '%s'", d,
                        device->nodes[DRM_NODE_PRIMARY] );
      handle.d->status = -1;
      continue;
    }

    handle.d->crtc = drmModeGetCrtc( handle.d->fd, encoder->crtc_id );

    if ( handle.d->crtc == NULL ) {
      log_message_info( logger, "Could not get crtc for %d '%s'", d, 
                        device->nodes[DRM_NODE_PRIMARY] );
      handle.d->status = -1;
      continue;
    }

    log_message_info( logger, "using device %d '%s'", d, 
                      devices[d]->nodes[DRM_NODE_PRIMARY] );

    struct gbm_device* gbm = gbm_create_device( handle.d->fd );

    handle.d->gbm_surface =
      gbm_surface_create( gbm,
			  handle.d->connector->modes[0].hdisplay,
			  handle.d->connector->modes[0].vdisplay,
			  GBM_FORMAT_ARGB8888, 
			  GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );

    handle.d->egl_display = eglGetDisplay( gbm /*EGL_DEFAULT_DISPLAY*/ );

    if ( handle.d->egl_display == EGL_NO_DISPLAY ) {
      log_message_error( logger, "Could not open the EGL display" );
      handle.d->status = -1;
      continue;
    }

    EGLBoolean egl_inited = eglInitialize( handle.d->egl_display, NULL, NULL );

    if ( egl_inited == EGL_FALSE ) {
      log_message_error( logger, "Could not initialize EGL display: %s",
                         egl_carp() );
      handle.d->status = -1;
      continue;
    }
#if 0
    printf("===================================\n");
    printf("EGL information:\n");
    printf("  version: \"%s\"\n", eglQueryString(handle.d->egl_display, EGL_VERSION));
    printf("  vendor: \"%s\"\n", eglQueryString(handle.d->egl_display, EGL_VENDOR));
    printf("===================================\n");
#endif
    EGLBoolean egl_got_api = eglBindAPI( EGL_OPENGL_ES_API );

    if ( egl_got_api == EGL_FALSE ) {
      log_message_error( logger, "Could not bind OpenGL ES API: %s (OpenGL ES is probably not supported)", egl_carp() );
      handle.d->status = -1;
      continue;
    }

    EGLint    egl_attrs[] = {
    // These are all defaults.
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, EGL_DONT_CARE,
    EGL_DEPTH_SIZE, EGL_DONT_CARE,
    EGL_NONE
    };
    EGLConfig egl_config = NULL;
    // You have to scan through the list of matching configs to find
    // one which agrees with your GBM surface.
    {
      EGLint    egl_n_configs = 0;
      if ( ! eglGetConfigs( handle.d->egl_display, NULL, 0, &egl_n_configs ) ) {
        log_message_error( logger, "No configs to choose from at all(?): %s",
                           egl_carp() );
        continue;
      }
      EGLint matched = 0;
      EGLConfig* egl_configs = malloc( sizeof(EGLConfig) * egl_n_configs );
      if ( ! ( eglChooseConfig( handle.d->egl_display,
                                egl_attrs, egl_configs, egl_n_configs,
                                &matched ) ) ) {
        log_message_error( logger, "No configs with desired attributes: %s",
                           egl_carp() );
        continue;
      }

      for ( int ec = 0; ec < egl_n_configs; ++ec ) {
        EGLint id;
        if ( ! ( eglGetConfigAttrib( handle.d->egl_display, egl_configs[ec],
                                     EGL_NATIVE_VISUAL_ID, &id ) ) ) {
          continue;
        }
        if ( id == GBM_FORMAT_ARGB8888 ) {
          egl_config = egl_configs[ec];
          break;
        }
      }

      if ( egl_config == NULL ) {
        log_message_error( logger, "No EGL configs match GBM surface format!" );
        handle.d->status = -1;
        continue;
      }
    }

    handle.d->egl_surface = eglCreateWindowSurface( handle.d->egl_display,
						    egl_config,
						    handle.d->gbm_surface,
						    NULL );

    if ( handle.d->egl_surface == EGL_NO_SURFACE ) {
      log_message_error( logger, "Could not bind VC window to EGL surface: %s",
                         egl_carp() );
      handle.d->status = -1;
      continue;
    }

    EGLint context_attribs[] = {
       EGL_CONTEXT_CLIENT_VERSION, 3,
       EGL_NONE
    };

    EGLContext egl_context = eglCreateContext( handle.d->egl_display,
                                               egl_config,
					       EGL_NO_CONTEXT, // no sharing
					       context_attribs );

    if ( egl_context == EGL_NO_CONTEXT ) {
      log_message_error( logger, "Failed to create a rendering context: %s",
                         egl_carp() );
      handle.d->status = -1;
      continue;
    }

    EGLBoolean egl_current = eglMakeCurrent( handle.d->egl_display,
					     handle.d->egl_surface,
					     handle.d->egl_surface,
					     egl_context );

    if ( egl_current == EGL_FALSE ) {
      log_message_error( logger, "Failed to make context current: %s",
                         egl_carp() );
      handle.d->status = -1;
      continue;
    }
#if 0
    printf("OpenGL ES 3.x information:\n");
    printf("  version: \"%s\"\n", glGetString(GL_VERSION));
    printf("  shading language version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
    printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
    printf("  extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));
    printf("===================================\n");
#endif
    if ( eglGetError() != EGL_SUCCESS ) {
      log_message_error( logger, "Hmm. something happened which wasn't reported: 0x%x",
	      eglGetError() );
      handle.d->status = -1;
      continue;
    }
    // So, perhaps this device is good. But should probably continue
    // with the rest of the setup in this loop.
    handle.d->status = 0;
    break;
  }

  if ( d >= MAX_DRM_DEVICES || handle.d->status == -1 ) {
    handle.d->status = -1;
    log_message_error( logger, "Could not find any useful DRM devices" );
    return handle;
  }

  float image_edge_length = tv_width / 2.f - 1.5 * border_thickness;

  handle.d->metadata_widget =
    text_widget_init( border_thickness, border_thickness,
		      tv_width / 2.f - 1.5f * border_thickness,
		      tv_height - 2.f * border_thickness,
		      dpmm_x, dpmm_y, tv_width, tv_height );

  handle.d->emblem_widget =
    emblem_widget_init( tv_width / 2.f - emblem_size - border_thickness,
                        border_thickness,
                        emblem_size, emblem_size,
                        tv_width, tv_height );

  // \bug widget height is a judgement text_widget should really
  // have a vertical centering option.
  // This should really be a function of the font height.
  float therm_height = 2.f * font_size_mm;
  handle.d->time_widget =
    text_widget_init( tv_width / 2.f + border_thickness / 2.f,
		      border_thickness,
		      image_edge_length, therm_height - border_thickness,
		      dpmm_x, dpmm_y, tv_width, tv_height );

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
					      /*dpmm_x, dpmm_y*/
                                              tv_width, tv_height );
  // Initialize this. Maybe defer this to the widget?
  struct IMAGE_HANDLE cover_image_handle =
    cover_image( handle.d->image_db, NULL, NULL );
  image_widget_set_image( handle.d->cover_widget, cover_image_handle );
  image_rgba_free( cover_image_handle );
#if 0
  float thermometer_width =
    (float)times.elapsed / (float)times.total *
    ( tv_width / 2.f - 1.5f * border_thickness - 2.f * thermometer_gap );
#endif
  float tw_x_mm = tv_width / 2.f + border_thickness / 2.f + thermometer_gap;
  float tw_y_mm = border_thickness + thermometer_gap;
  float image_edge = tv_width / 2.f - 1.5 * border_thickness;
  float thermometer_width =
    ( tv_width / 2.f - 1.5f * border_thickness - 2.f * thermometer_gap );
  float time_height = tv_height - border_thickness - image_edge - border_thickness - border_thickness - 2.f * thermometer_gap;

  handle.d->thermometer_widget =
    thermometer_widget_init( tw_x_mm, tw_y_mm,
                             thermometer_width, time_height,
                             tv_width, tv_height );

  return handle;
}

void display_update ( struct DISPLAY_HANDLE handle )
{
  // For now, this is very simple. Redraw the whole screen.
#if 0
  vgClear( 0, 0, window_width, window_height );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();
  vgScale( 0.1, 0.1 );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgPaintPattern( frame_paint, bg_brush );

  vgDrawPath( background_path, VG_FILL_PATH );

  vgPaintPattern( frame_paint, fg_brush );
#endif
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
#if 0
    vgClearPath( thermometer_path, VG_PATH_CAPABILITY_ALL );
#endif
    if ( times.total > 0 ) {
      thermometer_widget_set_value( handle.d->thermometer_widget,
                                 (float)times.elapsed / (float)times.total );
#if 0
      vguRoundRect( thermometer_path, 
		    tv_width / 2.f + border_thickness / 2.f + thermometer_gap,
		    border_thickness + thermometer_gap,
		    thermometer_width,
		    time_height,
		    round_radius, round_radius );
#endif
    }
  }
#if 0
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();

  vgSetPaint( thermometer_paint, VG_FILL_PATH );
  vgDrawPath( thermometer_path, VG_FILL_PATH );
#endif
#if 0
  text_widget_draw_text( handle.d->metadata_widget );
#endif
#if 0
  text_widget_draw_text( handle.d->time_widget );
#endif
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

  if ( mpd_changed( handle.d->mpd, MPD_CHANGED_STATUS ) ) {
    enum MPD_PLAY_STATUS status = mpd_play_status( handle.d->mpd );
    enum EMBLEM_WIDGET_EMBLEM emblem = EMBLEM_WIDGET_EMBLEM_NOEMBLEM;
    switch ( status ) {
    case MPD_PLAY_STATUS_STOPPED:
      if ( strlen( mpd_artist( handle.d->mpd ) ) == 0 ||
	   strlen( mpd_album( handle.d->mpd ) ) == 0 ) {
	emblem = EMBLEM_WIDGET_EMBLEM_NOEMBLEM;
      }
      else {
	emblem = EMBLEM_WIDGET_EMBLEM_STOPPED;
      }
      break;
    case MPD_PLAY_STATUS_PLAYING:
      emblem = EMBLEM_WIDGET_EMBLEM_PLAYING; break;
    case MPD_PLAY_STATUS_PAUSED:
      emblem = EMBLEM_WIDGET_EMBLEM_PAUSED; break;
    default:
      break;
    }
    emblem_widget_set_emblem( handle.d->emblem_widget, emblem );
  }
#if 0
  glViewport( 0, 0, 500, 500 );
#endif
#if 0
  double r = drand48();
  double g = drand48();
  double b = drand48();
#endif
  glClearColor( 0., 0.25, 0.25, 1. );
  glClear( GL_COLOR_BUFFER_BIT );

  text_widget_draw_text( handle.d->metadata_widget );
  image_widget_draw_image( handle.d->cover_widget );
  thermometer_widget_draw_thermometer( handle.d->thermometer_widget );
  text_widget_draw_text( handle.d->time_widget );
  emblem_widget_draw_emblem( handle.d->emblem_widget );

#if 0
  vgSeti( VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE );
  vgLoadIdentity();
  vgScale( dpmm_x, dpmm_y );

  vgSeti( VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER );
  vgLoadIdentity();
  vgScale( 0.1, 0.1 );

  vgSetPaint( frame_paint, VG_FILL_PATH );

  vgDrawPath( frame_path, VG_FILL_PATH );
#endif

  EGLBoolean swapped = eglSwapBuffers( handle.d->egl_display,
				       handle.d->egl_surface );

  if ( swapped == EGL_FALSE ) {
    log_message_error( handle.d->logger, "Could not swap EGL buffers: %s",
                       egl_carp() );
    handle.d->status = -1;
    return;
  }

  // If only we could stop there, but we still have to swap the actual
  // display buffer.
  struct gbm_bo* bo = gbm_surface_lock_front_buffer( handle.d->gbm_surface );
  int fd = gbm_device_get_fd( gbm_bo_get_device( bo ) );
  struct drm_fb* fb = (struct drm_fb*)gbm_bo_get_user_data( bo );
  // If we've already got an FB, we can skip this next bit.
  if ( fb == NULL ) {
    fb = malloc( sizeof(struct drm_fb) );
    fb->bo = bo;
    uint32_t width = gbm_bo_get_width( bo );
    uint32_t height = gbm_bo_get_height( bo );
    uint32_t format = gbm_bo_get_format( bo );
    uint32_t handles[4] = { 0 };
    uint32_t strides[4] = { 0 };
    uint32_t offsets[4] = { 0 };
    uint64_t modifiers[4] = { 0 };
    int flags = 0;
    //modifiers[0] = gbm_bo_get_modifier( bo );
    const int num_planes = gbm_bo_get_plane_count( bo );
    for ( int i = 0; i < num_planes; ++i ) {
      handles[i] = gbm_bo_get_handle_for_plane( bo, i ).u32;
      strides[i] = gbm_bo_get_stride_for_plane( bo, i );
      offsets[i] = gbm_bo_get_offset( bo, i );
      modifiers[i] = modifiers[0];
    }

    int ret = drmModeAddFB2WithModifiers( fd, width, height, format,
                                          handles, strides, offsets,
                                          modifiers, &fb->fb_id, flags);

    if ( ret != 0 ) {
      log_message_error( handle.d->logger, "Failed to create fb: %s",
                         strerror( errno ) );
      free( fb );
      return; // Need an error signal...
    }

    gbm_bo_set_user_data( bo, fb, drm_fb_destroy_callback );
  }

  /* set mode: */
  int ret = drmModeSetCrtc( fd, handle.d->crtc->crtc_id, fb->fb_id, 0, 0,
			    &handle.d->connector->connector_id, 1,
                            handle.d->mode );

  if ( ret != 0 ) {
    log_message_error( handle.d->logger, "failed to set CRTC mode: %s", 
                       strerror(errno));
    return;
  }

  gbm_surface_release_buffer( handle.d->gbm_surface, handle.d->previous_bo );
  handle.d->previous_bo = bo;
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

void display_dispatch_touch ( struct DISPLAY_HANDLE handle,
                              const struct TOUCH_EVENT* touch )
{
  (void)handle;
  int brightness = 255 * ( 1.f - (float)touch->y/480.f );
  FILE* f = fopen( "/sys/class/backlight/rpi_backlight/brightness", "w" );
  fprintf( f, "%d", brightness );
  fclose(f);
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
