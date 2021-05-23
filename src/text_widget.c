/*
 * A text widget which provides some layout services and font
 * selection.
 */
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "pango/pangoft2.h"
#include <GLES3/gl3.h>
#include "ogles.h"
#include "text_widget.h"

struct TEXT_WIDGET_PRIVATE {
  PangoFontMap* font_map;
  PangoContext* context;
  PangoLayout* layout;
  FT_Bitmap paragraph;
  GLuint texture;
  GLuint program;
  GLuint VAO;
};

struct TEXT_WIDGET_HANDLE text_widget_init ( float x_mm, float y_mm,
					     float width_mm,
					     float height_mm,
					     float dpmm_x,
					     float dpmm_y,
                                             float screen_width_mm,
                                             float screen_height_mm )
{
  struct TEXT_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct TEXT_WIDGET_PRIVATE ) );
  handle.d->font_map = pango_ft2_font_map_new();

  // Note: FreeType works in DPI.
  pango_ft2_font_map_set_resolution( (PangoFT2FontMap*)handle.d->font_map,
				     dpmm_x * 25.4, dpmm_y * 25.4 );

  handle.d->context  = pango_font_map_create_context( handle.d->font_map );
  handle.d->layout   = pango_layout_new( handle.d->context );

  // Pango works in Pango Units. Not exactly clear how the resolution of
  // the font and the size of the rendering box fit together.
  int b_width = lrint( dpmm_x * width_mm );
  int b_height = lrint( dpmm_y * height_mm );
  int width = pango_units_from_double( b_width );
  int height = pango_units_from_double( b_height );
  pango_layout_set_width( handle.d->layout, width );
  pango_layout_set_height( handle.d->layout, height );

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
    "  float x = texture2D( image_tex, uv.xy ).r;\n"
    "  fragColor.rgba = vec4( 1., 1., 1., x );\n"
    "}";

  handle.d->program = OGLEScreateProgram( vertex_shader_source,
                                          fragment_shader_source );

  glUseProgram( handle.d->program );

  GLuint vertex_attr = glGetAttribLocation( handle.d->program, "vertex" );
  GLuint texuv_attr  = glGetAttribLocation( handle.d->program, "texuv" );
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );
  GLuint sampler_u   = glGetUniformLocation( handle.d->program, "image_tex" );

  // Glep. It's column major (or something like that). Could transpose
  // it in the upload, too.

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

  // Is this the easiest way? Leave it to Pango to initialize
  // the bitmap...
  handle.d->paragraph.rows = b_height;
  handle.d->paragraph.width = b_width;
  handle.d->paragraph.pitch = b_width;
  handle.d->paragraph.num_grays = 256;
  handle.d->paragraph.pixel_mode = FT_PIXEL_MODE_GRAY;
  handle.d->paragraph.buffer = 
    g_malloc( handle.d->paragraph.rows * handle.d->paragraph.pitch );

  glActiveTexture( GL_TEXTURE0 );
  glGenTextures( 1, &handle.d->texture );
  glBindTexture( GL_TEXTURE_2D, handle.d->texture );
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); */
  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); */
  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); */
  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); */

  // Allocate the space for later.
  glTexImage2D( GL_TEXTURE_2D, 0, // level of detail
		GL_RED,
		handle.d->paragraph.width, handle.d->paragraph.rows,  0,
		GL_RED, GL_UNSIGNED_BYTE, NULL );

  return handle;
}

void text_widget_set_alignment ( struct TEXT_WIDGET_HANDLE handle,
				 enum TEXT_WIDGET_ALIGNMENT alignment )
{
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;
  switch ( alignment ) {
  case TEXT_WIDGET_ALIGN_LEFT:
    pango_layout_set_alignment( handle.d->layout, PANGO_ALIGN_LEFT ); break;
  case TEXT_WIDGET_ALIGN_CENTER:
    pango_layout_set_alignment( handle.d->layout, PANGO_ALIGN_CENTER ); break;
  case TEXT_WIDGET_ALIGN_RIGHT:
    pango_layout_set_alignment( handle.d->layout, PANGO_ALIGN_RIGHT ); break;
  }
}

void text_widget_set_text ( struct TEXT_WIDGET_HANDLE handle,
			    const char* text, int length )
{
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;

  pango_layout_set_markup( handle.d->layout, text, length );

  memset( handle.d->paragraph.buffer, 0, 
          handle.d->paragraph.rows * handle.d->paragraph.pitch );

  // This is cheaping out somewhat. Just let Pango draw the text as a
  // complete image. Later, we just draw the image.
  pango_ft2_render_layout( &handle.d->paragraph, handle.d->layout, 0, 0 );

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glBindTexture( GL_TEXTURE_2D, handle.d->texture );
  glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
                   handle.d->paragraph.pitch, handle.d->paragraph.rows,
                   GL_RED, GL_UNSIGNED_BYTE, handle.d->paragraph.buffer );
}

void text_widget_draw_text ( struct TEXT_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL || handle.d->layout == NULL )
    return;
  // Too simple now?
  glUseProgram( handle.d->program );
  glBindVertexArray( handle.d->VAO );
  glBindTexture( GL_TEXTURE_2D, handle.d->texture );

  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
  glDisable( GL_BLEND );
}

void text_widget_free_handle ( struct TEXT_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    g_object_unref( handle.d->layout );
    g_object_unref( handle.d->context );
    g_object_unref( handle.d->font_map );
    glDeleteTextures( 1, &handle.d->texture );
    glDeleteVertexArrays( 1, &handle.d->VAO );
    glDeleteProgram( handle.d->program );
    free( handle.d );
    handle.d = NULL;
  }
}
