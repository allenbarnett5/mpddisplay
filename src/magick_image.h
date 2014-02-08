/*
 * Wrap the ImageMagick MagickWand in another layer.
 */
#ifndef MAGICK_IMAGE_H
#define MAGICK_IMAGE_H

#include <stddef.h>

struct IMAGE_HANDLE {
  struct IMAGE_HANDLE_PRIVATE* d;
};

/*!
 * This function takes a pointer to a memory buffer containing the
 * given kind of image. The handle it returns can be used to extract
 * an RGBA image suitable for instantiating in OpenVG. It uses the
 * ImageMagick library to do all the work.
 * \param type the type of data in the buffer. Must be something
 * that ImageMagick recognizes like "PNG" or "JPG".
 * \param data pointer to the data.
 * \param n_bytes number of bytes in the data.
 */
struct IMAGE_HANDLE image_rgba_create ( const char* type,
					const unsigned char* data,
					size_t n_bytes );
/*!
 * So many layers of indirection just to get the image width.
 */
int image_rgba_width ( struct IMAGE_HANDLE handle );

/*!
 * So many layers of indirection just to get the image height.
 */
int image_rgba_height ( struct IMAGE_HANDLE handle );

/*!
 * So many layers of indirection just to get the image RGBA bits.
 * Which we note are actually in ABGR order when you take into account
 * the little endian nature of this chip.
 */
unsigned char* image_rgba_image ( struct IMAGE_HANDLE handle );

#endif
