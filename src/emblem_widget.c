/*
 * Draw a character or a box or something emblematic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
#if 0
    "in  vec2 corneruv;\n"
#endif
    "in  vec3 coloruv;\n"
#if 0
    "out vec2 corner;\n"
#endif
    "out vec3 color;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( vertex.x, vertex.y, 0., 1. );\n"
#if 0
    "  corner      = corneruv;\n"
#endif
    "  color       = coloruv;\n"
    "}";

  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
#if 0
    "in vec2 corner;\n"
#endif
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
#if 0
  GLuint corneruv_attr = glGetAttribLocation( handle.d->program, "corneruv" );
#endif
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );

  printf( "vertex attr: %d\n", vertex_attr );
  printf( "coloruv attr: %d\n", coloruv_attr );
#if 0
  printf( "corneruv attr: %d\n", corneruv_attr );
#endif
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
  GLuint vertex_buffer, index_buffer, coloruv_buffer/*, corneruv_buffer*/;
  glGenBuffers( 1, &vertex_buffer );
  glGenBuffers( 1, &index_buffer );
  glGenBuffers( 1, &coloruv_buffer );
#if 0
  glGenBuffers( 1, &corneruv_buffer );
#endif
  printf( "emblem vertex buffer: %d\n", vertex_buffer );
  printf( "emblem color buffer: %d\n", coloruv_buffer );
#if 0
  printf( "emblem corner buffer: %d\n", corneruv_buffer );
#endif
  // Draw in mm.
  // Octagon corners.
  const float OS = ( sqrtf(2.f) - 1.f ) / 2.f;

  GLfloat square_vertexes[][2] =
    {
     // Enter the octagon.
     { width_mm,             height_mm * (0.5f+OS) },
     { width_mm * (0.5f+OS), height_mm },
     { width_mm * (0.5f-OS), height_mm },
     { 0.f,                  height_mm * (0.5f+OS) },
     { 0.f,                  height_mm * (0.5f-OS) },
     { width_mm * (0.5f-OS), 0.f },
     { width_mm * (0.5f+OS), 0.f },
     { width_mm ,            height_mm * (0.5f-OS ) },
     // Trianguly.
     { 0.f,                 0.f  },
     { width_mm,            height_mm * 0.5f },
     { 0.f,                 height_mm },
     // Pause, please. Tricky.
     { 0.f, 0.f }, // Left bulb.
     { width_mm * 0.375, 0.f },
     { width_mm * 0.375, height_mm },
     { 0.f, height_mm },
     { width_mm * 0.625f, 0.f }, // Right bulb.
     { width_mm, 0.f },
     { width_mm, height_mm },
     { width_mm * 0.625f, height_mm },
    };

  // Remember: if you pass 0 to glVertexAttribPointer, you must
  // have bound the buffer already.
  glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
  glVertexAttribPointer( vertex_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_vertexes),
                square_vertexes, GL_STATIC_DRAW );
  glEnableVertexAttribArray( vertex_attr );

  // VAO is implicit(?)
  GLushort square_indexes[] = {
                                0,1,2,3,4,5,6,7,
                                8,9,10,
                                11, 12, 13, 14, 65535, 15, 16, 17, 18
  };
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, index_buffer );
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(square_indexes),
                square_indexes, GL_STATIC_DRAW );


  // Corner colors.
  GLfloat square_coloruvs[][3] =
    {
#if 0
     { 0.25f, 0.25f, 0.25f },
     { 0.25f, 0.25f, 0.25f },
     { 0.75f, 0.75f, 0.75f },
     { 0.75f, 0.75f, 0.75f },
#else
     // The Octagon.
     { 1.f, 0.f, 1.f },
     { 1.f, 0.f, 0.f },
     { 1.f, 0.f, 0.f },
     { 0.f, 0.f, 1.f },
     { 1.f, 0.f, 0.f },
     { 1.f, 0.f, 0.f },
     { 1.f, 0.f, 0.f },
     { 0.f, 1.f, 0.f },
     // Trianguly.
     { 1.f, 0.f, 1.f },
     { 1.f, 0.f, 1.f },
     { 1.f, 0.f, 1.f },
     // Pause, please.
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
     { 0.f, 1.f, 1.f },
#endif
    };
  glBindBuffer( GL_ARRAY_BUFFER, coloruv_buffer );
  glVertexAttribPointer( coloruv_attr, 3, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[3]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_coloruvs),
                square_coloruvs, GL_STATIC_DRAW );
  glEnableVertexAttribArray( coloruv_attr );
#if 0
  // Corner uv.
  GLfloat square_corneruvs[8][2] =
    {
     { -1.f, -1.f },
     {  1.f, -1.f },
     {  1.f,  1.f },
     { -1.f,  1.f },
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
#endif
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
#if 0
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
#endif
}

void emblem_widget_draw_emblem ( struct EMBLEM_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  if ( handle.d->emblem == EMBLEM_WIDGET_EMBLEM_NOEMBLEM )
    return;

  glUseProgram( handle.d->program );

  glBindVertexArray( handle.d->VAO );
#if 1
  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glLineWidth( 8.f );
#endif
#if 0
  glDrawArrays( GL_LINE_LOOP, 0, 8 );
#else
  switch ( handle.d->emblem ) {
  case EMBLEM_WIDGET_EMBLEM_STOPPED:
    glDrawElements( GL_LINE_LOOP, 8, GL_UNSIGNED_SHORT, 0 );
    break;
  case EMBLEM_WIDGET_EMBLEM_PLAYING:
    glDrawElements( GL_LINE_LOOP, 3, GL_UNSIGNED_SHORT,
                    (void*)(8*sizeof(GLushort)) );
    break;
  case EMBLEM_WIDGET_EMBLEM_PAUSED:
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements( GL_LINE_LOOP, 9, GL_UNSIGNED_SHORT,
                    (void*)((8+3)*sizeof(GLushort)) );
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    break;
  case EMBLEM_WIDGET_EMBLEM_NOEMBLEM:
    break;
  }
#endif
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
#elif 0
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
#endif
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
#elif 0
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
#elif 0
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
#elif 0
  GLint param;
  glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param );
#endif
}
