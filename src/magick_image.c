/*
 * Use ImageMagick to convert various image formats into RGBA for
 * OpenVG.
 */
#include "wand/MagickWand.h"

#include "magick_image.h"

struct IMAGE_HANDLE_PRIVATE {
  MagickWand* wand;
  // The wand doesn't track an results from GetImageBlob.
  unsigned char* image;
  // Call this after the image data is no longer necessary.
  void (*finalizer) ( struct IMAGE_HANDLE );
};

static void image_rgba_free ( struct IMAGE_HANDLE );

struct IMAGE_HANDLE image_rgba_create ( const char* type,
					const unsigned char* data,
					size_t n_bytes )
{
  struct IMAGE_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_HANDLE_PRIVATE ) );
  MagickWandGenesis();
  // Create the wand and load the image.
  handle.d->wand = NewMagickWand();
  MagickSetImageFormat( handle.d->wand, type );
  MagickReadImageBlob( handle.d->wand, data, n_bytes );
  // Now convert the image from whatever we got into an RGBA image.
  MagickSetImageFormat( handle.d->wand, "RGBA" );

  handle.d->image     = NULL;
  handle.d->finalizer = image_rgba_free;

  return handle;
}

int image_rgba_width ( struct IMAGE_HANDLE handle )
{
  if ( handle.d && handle.d->wand ) {
    return (int)MagickGetImageWidth( handle.d->wand );
  }
  return 0;
}

int image_rgba_height ( struct IMAGE_HANDLE handle )
{
  if ( handle.d && handle.d->wand ) {
    return (int)MagickGetImageHeight( handle.d->wand );
  }
  return 0;
}

unsigned char* image_rgba_image ( struct IMAGE_HANDLE handle )
{
  if ( handle.d ) {
    if ( handle.d->image ) {
      return handle.d->image;
    }
    else if ( handle.d->wand ) {
      size_t length;
      handle.d->image = MagickGetImageBlob( handle.d->wand, &length );
      return handle.d->image;
    }
  }
  return NULL;
}

static void image_rgba_free ( struct IMAGE_HANDLE handle )
{
  if ( handle.d ) {
    if ( handle.d->image ) {
      handle.d->image = MagickRelinquishMemory( handle.d->image );
    }
    if ( handle.d->wand ) {
      handle.d->wand = DestroyMagickWand( handle.d->wand );
    }
    free( handle.d );
    handle.d = NULL;
  }
}
