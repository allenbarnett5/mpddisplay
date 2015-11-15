/*
 * Use GDK-PIXBUF to convert various image formats into RGBA for
 * OpenVG.
 */
#include <stdlib.h>

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "image_intf.h"

struct IMAGE_HANDLE_PRIVATE {
  // The GdkPixbuf handles practically all the loading and conversion
  // duties.
  GdkPixbuf* pb;
};

struct IMAGE_HANDLE image_rgba_create ( const unsigned char* data,
					size_t n_bytes )
{
  struct IMAGE_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_HANDLE_PRIVATE ) );

  // Convert the blob we loaded from the database into a GInputStream.
  // The last argument is a destructor callback which we don't
  // appear to need.

  GInputStream* stream = g_memory_input_stream_new_from_data( data, n_bytes,
							      NULL );
  GError* err = NULL;

  GdkPixbuf* original = gdk_pixbuf_new_from_stream( stream, NULL, &err );

  // OpenVG insists on an alpha channel.
  handle.d->pb = gdk_pixbuf_add_alpha( original, FALSE, 0, 0, 0 );

  // Hopefully, we've made a copy of the data now.
  g_clear_object( &original );
  g_clear_object( &stream );

  return handle;
}

int image_rgba_width ( struct IMAGE_HANDLE handle )
{
  if ( handle.d && handle.d->pb ) {
    return gdk_pixbuf_get_width( handle.d->pb );
  }
  return 0;
}

int image_rgba_height ( struct IMAGE_HANDLE handle )
{
  if ( handle.d && handle.d->pb ) {
    return gdk_pixbuf_get_height( handle.d->pb );
  }
  return 0;
}

unsigned char* image_rgba_image ( struct IMAGE_HANDLE handle )
{
  if ( handle.d && handle.d->pb ) {
    return gdk_pixbuf_get_pixels( handle.d->pb );
  }
  return NULL;
}

void image_rgba_free ( struct IMAGE_HANDLE handle )
{
  if ( handle.d ) {
    if ( handle.d->pb ) {
      g_clear_object( &handle.d->pb );
    }
    free( handle.d );
    handle.d = NULL;
  }
}
