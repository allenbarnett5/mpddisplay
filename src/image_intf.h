/*
 * Wrap the image handling library in another layer.
 */
#ifndef IMAGE_INTF_H
#define IMAGE_INTF_H

struct IMAGE_HANDLE {
  struct IMAGE_HANDLE_PRIVATE* d;
};

/*!
 * This function takes a pointer to a memory buffer containing the
 * given kind of image. The handle it returns can be used to extract
 * an RGBA image suitable for instantiating in OpenVG.
 * \param type the type of data in the buffer.
 * \param data pointer to the data.
 * \param n_bytes number of bytes in the data.
 */
struct IMAGE_HANDLE image_rgba_create ( const char* type,
					const unsigned char* data,
					size_t n_bytes );

/*!
 * Release any resources associated with the image.
 * \param handle the image to free.
 */
void image_rgba_free ( struct IMAGE_HANDLE handle );

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
