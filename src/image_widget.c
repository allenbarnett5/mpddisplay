/*
 * Display an image in a box.
 * Slowly we're recapitulating Qt. Probably should have just used it.
 */
#include <stdio.h>
#include <stdlib.h>

#include <GLES3/gl3.h>

#include "ogles.h"
#include "image_widget.h"

struct IMAGE_WIDGET_PRIVATE {
  enum IMAGE_WIDGET_EMBLEM emblem;
  GLuint texture;
  GLuint program;
  GLuint VAO;
};
#if 0
static void image_widget_stopped ( void /*VGPaint color, VGPath path*/ );
static void image_widget_playing ( void /*VGPaint color, VGPath path*/ );
static void image_widget_paused  ( void /*VGPaint color, VGPath path*/ );
#endif
struct IMAGE_WIDGET_HANDLE image_widget_init ( float x_mm, float y_mm,
					       float width_mm,
					       float height_mm,
					       float screen_width_mm,
					       float screen_height_mm )
{
  struct IMAGE_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct IMAGE_WIDGET_PRIVATE ) );
  handle.d->emblem = IMAGE_WIDGET_EMBLEM_NOEMBLEM;

  const GLchar* vertex_shader_source =
    "#version 300 es       \n"
    "uniform mat4 mv_matrix;\n"
    "in  vec2 vertex;\n"
    "in  vec2 texuv;\n"
    "out vec2 uv;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( vertex, 0., 1. );\n"
    "  uv          = texuv;\n"
    "}";

  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 uv;\n"
    "uniform sampler2D image_tex;\n"
    "out vec4 fragColor;\n"
    "void main(void) {\n"
    "  fragColor.rgba = texture2D( image_tex, uv.xy );\n"
    "}";

  handle.d->program = OGLEScreateProgram( vertex_shader_source,
                                          fragment_shader_source );

  glUseProgram( handle.d->program );

  GLuint vertex_attr = glGetAttribLocation( handle.d->program, "vertex" );
  GLuint texuv_attr  = glGetAttribLocation( handle.d->program, "texuv" );
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );
  GLuint sampler_u   = glGetUniformLocation( handle.d->program, "image_tex" );

  // We draw in mm.
  GLfloat mv_matrix[] = {
                         2.f/screen_width_mm, 0.f, 0.f, 0.f,
                         0.f, 2.f/screen_height_mm, 0.f, 0.f,
                         0.f, 0.f, 1.f, 0.f,
                         -1.f, -1.f, 0.f, 1.f
  };

  glUniformMatrix4fv( mv_matrix_u, 1, GL_FALSE, mv_matrix );
  glUniform1i( sampler_u, 0 );

  glGenVertexArrays( 1, &handle.d->VAO );
  glBindVertexArray( handle.d->VAO );
  GLuint vertex_buffer, texuv_buffer;
  glGenBuffers( 1, &vertex_buffer );
  glGenBuffers( 1, &texuv_buffer );

  // Draw in mm.
  GLfloat square_vertexes[4][2] =
    {
     { x_mm,          y_mm },
     { x_mm+width_mm, y_mm },
     { x_mm+width_mm, y_mm+height_mm },
     { x_mm,          y_mm+height_mm },
    };

  // Remember: if you pass 0 to glVertexAttribPointer, you must
  // have bound the buffer already.
  glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
  glVertexAttribPointer( vertex_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_vertexes),
                square_vertexes, GL_STATIC_DRAW );
  glEnableVertexAttribArray( vertex_attr );

  // Our texture coordinates.
  GLfloat square_uvs[4][2] =
    {
     { 0.f, 1.f },
     { 1.f, 1.f },
     { 1.f, 0.f },
     { 0.f, 0.f },
    };
  glBindBuffer( GL_ARRAY_BUFFER, texuv_buffer );
  glVertexAttribPointer( texuv_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_uvs),
                square_uvs, GL_STATIC_DRAW );
  glEnableVertexAttribArray( texuv_attr );

  glBindVertexArray( 0 );

  glGenTextures( 1, &handle.d->texture );
  glBindTexture( GL_TEXTURE_2D, handle.d->texture );
  // The default is GL_NEAREST_MIPMAP_LINEAR. If you don't have a
  // mipmap, you get nother. GL_LINEAR looks nicer than GL_NEAREST.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

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
  if ( image_width ==  0 ||
       image_height == 0 ||
       image_data == NULL )
    return;
#if 0
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
#endif

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glBindTexture( GL_TEXTURE_2D, handle.d->texture );

  glTexImage2D( GL_TEXTURE_2D, 0, // level of detail
		GL_RGBA,
		image_width, image_height,  0, //border
		GL_RGBA, GL_UNSIGNED_BYTE, image_data );
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

#if 0
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
      image_widget_stopped( /*color, path*/ );
      break;
    case IMAGE_WIDGET_EMBLEM_PLAYING:
      image_widget_playing( /*color, path*/ );
      break;
    case IMAGE_WIDGET_EMBLEM_PAUSED:
      image_widget_paused( /*color, path*/ );
      break;
    default:
      break;
    }
  }
#endif

  // Needs some more work with respect to Emblems.
  glUseProgram( handle.d->program );

  glBindVertexArray( handle.d->VAO );
  glBindTexture( GL_TEXTURE_2D, handle.d->texture );

  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
  glDisable( GL_BLEND );
}

void image_widget_free_handle ( struct IMAGE_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    glDeleteTextures( 1, &handle.d->texture );
    glDeleteVertexArrays( 1, &handle.d->VAO );
    glDeleteProgram( handle.d->program );
    free( handle.d );
    handle.d = NULL;
  }
}

#if 0
void image_widget_stopped ( void /*VGPaint color, VGPath path*/ )
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

void image_widget_playing ( void /*VGPaint color, VGPath path*/ )
{
  VGfloat triangle[] = {  0.f,  0.f,
			  0.f, 10.f,
			 10.f,  5.f };
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguPolygon( path, triangle, 3, VG_TRUE );
}

void image_widget_paused ( void /*VGPaint color, VGPath path*/ )
{
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguRect( path, 0.f, 0.f, 3.f, 10.f );
  vguRect( path, 7.f, 0.f, 3.f, 10.f );
}
#endif
