/*
 * Draw a character or a box or something emblematic.
 */
#include <stdio.h>
#include <stdlib.h>

#include <GLES3/gl3.h>

#include "ogles.h"
#include "emblem_widget.h"

struct EMBLEM_WIDGET_PRIVATE {
  enum EMBLEM_WIDGET_EMBLEM emblem;
  GLuint texture;
  GLuint program;
  GLuint VAO;
}; 

static void emblem_widget_stopped ( void /*VGPaint color, VGPath path*/ );
static void emblem_widget_playing ( void /*VGPaint color, VGPath path*/ );
static void emblem_widget_paused  ( void /*VGPaint color, VGPath path*/ );
static void emblem_widget_noemblem  ( void /*VGPaint color, VGPath path*/ );

struct EMBLEM_WIDGET_HANDLE emblem_widget_init ( float x_mm, float y_mm,
                                                 float width_mm,
                                                 float height_mm,
                                                 float screen_width_mm,
                                                 float screen_height_mm )
{
  struct EMBLEM_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct EMBLEM_WIDGET_PRIVATE ) );
  handle.d->emblem = EMBLEM_WIDGET_EMBLEM_NOEMBLEM;

  const GLchar* vertex_shader_source =
    "#version 300 es       \n"
    "precision mediump float;\n"
    "uniform mat4 mv_matrix;\n"
    "in  vec2 vertex;\n"
    "in  vec2 corneruv;\n"
    "in  vec3 coloruv;\n"
    "out vec2 corner;\n"
    "out vec3 color;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( vertex.x, vertex.y, 0., 1. );\n"
    "  corner      = corneruv;\n"
    "  color       = coloruv;\n"
    "}";

  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 corner;\n"
    "in vec3 color;\n"
    "out vec4 fragColor;\n"
    "void main(void) {\n"
    "  fragColor.rgba = vec4( color, 1.f );\n"
    "}";

  handle.d->program = OGLEScreateProgram( vertex_shader_source,
                                          fragment_shader_source );

  glUseProgram( handle.d->program );

  GLuint vertex_attr = glGetAttribLocation( handle.d->program, "vertex" );
  GLuint coloruv_attr = glGetAttribLocation( handle.d->program, "coloruv" );
  GLuint corneruv_attr = glGetAttribLocation( handle.d->program, "corneruv" );
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );

  printf( "vertex attr: %d\n", vertex_attr );
  printf( "coloruv attr: %d\n", coloruv_attr );
  printf( "corneruv attr: %d\n", corneruv_attr );
  printf( "matrix u: %d\n", mv_matrix_u );

  // Include the translation (in mm) of the emblem in the modelview
  // operation.
  GLfloat mv_matrix[] = {
     2.f/screen_width_mm,          0.f,                          0.f, 0.f,
     0.f,                          2.f/screen_height_mm,         0.f, 0.f,
     0.f,                          0.f,                          1.f, 0.f,
     2.f*x_mm/screen_width_mm-1.f, 2.f*y_mm/screen_height_mm-1.f, 0.f, 1.f
  };
  glUniformMatrix4fv( mv_matrix_u, 1, GL_FALSE, mv_matrix );

  glGenVertexArrays( 1, &handle.d->VAO );
  glBindVertexArray( handle.d->VAO );
  GLuint vertex_buffer, coloruv_buffer, corneruv_buffer;
  glGenBuffers( 1, &vertex_buffer );
  glGenBuffers( 1, &coloruv_buffer );
  glGenBuffers( 1, &corneruv_buffer );

  printf( "emblem vertex buffer: %d\n", vertex_buffer );
  printf( "emblem color buffer: %d\n", coloruv_buffer );
  printf( "emblem corner buffer: %d\n", corneruv_buffer );

  // Draw in mm.
  GLfloat square_vertexes[4][2] =
    {
     { 0.f,      0.f },
     { width_mm, 0.f },
     { width_mm, height_mm },
     { 0.f,      height_mm },
    };

  // Remember: if you pass 0 to glVertexAttribPointer, you must
  // have bound the buffer already.
  glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
  glVertexAttribPointer( vertex_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_vertexes),
                square_vertexes, GL_STATIC_DRAW );
  glEnableVertexAttribArray( vertex_attr );

  // Corner colors.
  GLfloat square_coloruvs[4][3] =
    {
#if 0
     { 0.25f, 0.25f, 0.25f },
     { 0.25f, 0.25f, 0.25f },
     { 0.75f, 0.75f, 0.75f },
     { 0.75f, 0.75f, 0.75f },
#else
     { 1.f, 0.f, 0.f },
     { 0.f, 1.f, 0.f },
     { 0.f, 0.f, 1.f },
     { 1.f, 1.f, 0.f },
#endif
    };
  glBindBuffer( GL_ARRAY_BUFFER, coloruv_buffer );
  glVertexAttribPointer( coloruv_attr, 3, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[3]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_coloruvs),
                square_coloruvs, GL_STATIC_DRAW );
  glEnableVertexAttribArray( coloruv_attr );

  // Corner uv.
  GLfloat square_corneruvs[4][2] =
    {
     { -1.f, -1.f },
     {  1.f, -1.f },
     {  1.f,  1.f },
     { -1.f,  1.f },
    };
  glBindBuffer( GL_ARRAY_BUFFER, corneruv_buffer );
  glVertexAttribPointer( corneruv_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_corneruvs),
                square_corneruvs, GL_STATIC_DRAW );
  glEnableVertexAttribArray( corneruv_attr );

  glBindVertexArray( 0 );

  return handle;
}

void emblem_widget_set_emblem ( struct EMBLEM_WIDGET_HANDLE handle,
                                enum EMBLEM_WIDGET_EMBLEM emblem )
{
  if ( handle.d == NULL )
    return;

  printf( "Hello: %d\n", emblem );

  handle.d->emblem = emblem;

  glBindVertexArray( handle.d->VAO );

  // Does it know what's bound now?
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  printf( "param 0: %d\n", param );
  glGetVertexAttribiv( 1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  printf( "param 1: %d\n", param );
  glGetVertexAttribiv( 2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  printf( "param 2: %d\n", param );
  glGetVertexAttribiv( 3, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  printf( "param 3: %d\n", param );
  glGetVertexAttribiv( 4, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  printf( "param 4: %d\n", param );

  switch ( handle.d->emblem ) {
  case EMBLEM_WIDGET_EMBLEM_STOPPED:
    emblem_widget_stopped(); break;
  case EMBLEM_WIDGET_EMBLEM_PLAYING:
    emblem_widget_playing(); break;
  case EMBLEM_WIDGET_EMBLEM_PAUSED:
    emblem_widget_paused(); break;
  case EMBLEM_WIDGET_EMBLEM_NOEMBLEM:
    emblem_widget_noemblem(); break;
  }
}

void emblem_widget_draw_emblem ( struct EMBLEM_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;

  glUseProgram( handle.d->program );

  glBindVertexArray( handle.d->VAO );
#if 1
  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
#endif
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
#if 1
  glDisable( GL_BLEND );
#endif
}

void emblem_widget_free_handle ( struct EMBLEM_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    glDeleteTextures( 1, &handle.d->texture );
    glDeleteVertexArrays( 1, &handle.d->VAO );
    glDeleteProgram( handle.d->program );
    free( handle.d );
    handle.d = NULL;
  }
}

void emblem_widget_stopped ( void /*VGPaint color, VGPath path*/ )
{
#if 0
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
#else
  GLfloat square_vertexes[4][2] =
    {
     { 2.5f, 2.5f },
     { 7.5f, 2.5f },
     { 7.5f, 7.5f },
     { 2.5f, 7.5f },
    };
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
  glBindBuffer( GL_ARRAY_BUFFER, param );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_vertexes),
                square_vertexes, GL_STATIC_DRAW );
w#endif
}

void emblem_widget_playing ( void /*VGPaint color, VGPath path*/ )
{
#if 0
  VGfloat triangle[] = {  0.f,  0.f,
			  0.f, 10.f,
			 10.f,  5.f };
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguPolygon( path, triangle, 3, VG_TRUE );
#else
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
#endif
}

void emblem_widget_paused ( void /*VGPaint color, VGPath path*/ )
{
#if 0
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguRect( path, 0.f, 0.f, 3.f, 10.f );
  vguRect( path, 7.f, 0.f, 3.f, 10.f );
#else
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
#endif
}

void emblem_widget_noemblem ( void /*VGPaint color, VGPath path*/ )
{
#if 0
  VGfloat foreground[] = { 1.f, 1.f, 1.f, 0.25f };
  vgSetParameterfv( color, VG_PAINT_COLOR, 4, foreground );
  vguRect( path, 0.f, 0.f, 3.f, 10.f );
  vguRect( path, 7.f, 0.f, 3.f, 10.f );
#else
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
#endif
}
