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
  GLuint emblem_u;
}; 

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
    "in  vec3 coloruv;\n"
    "in  vec2 corneruv;\n"
    "out vec3 color;\n"
    "out vec2 corner;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( vertex.x, vertex.y, 0., 1. );\n"
    "  corner      = corneruv;\n"
    "  color       = coloruv;\n"
    "}";

  // Draw the selected emblem.
  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform int emblem;\n"
    "in vec3 color;\n"
    "in vec2 corner;\n"
    "out vec4 fragColor;\n"
    "void sdOctagon ( in vec2 p, in float r, out float d ) {\n"
    "  const vec3 k = vec3( -0.9238795325, 0.3826834323, 0.4142135623 );\n"
    "  p = abs(p);\n"
    "  p -= 2.0*min(dot(vec2( k.x,k.y),p),0.0)*vec2( k.x,k.y);\n"
    "  p -= 2.0*min(dot(vec2(-k.x,k.y),p),0.0)*vec2(-k.x,k.y);\n"
    "  p -= vec2( clamp( p.x, -k.z*r, k.z*r), r );\n"
    "  d = length(p) * sign(p.y);\n"
    "}\n"
    "void sdEquilateral ( in vec2 p, in float r, out float d ) {\n"
    "  const float k = sqrt(3.);\n"
    "  p.x = abs(p.x) - r;\n"
    "  p.y = p.y + r/k;\n"
    "  if ( p.x +  k * p.y > 0. ) p = vec2( p.x-k*p.y, -k*p.x-p.y ) / 2.;\n"
    "  p.x -= clamp( p.x, -2.*r, 0. );\n"
    "  d = -length(p) * sign( p.y );\n"
    "}\n"
    "void sdPause ( in vec2 p, in vec2 b, out float d ) {\n"
    "  // Somewhat repetitive...\n"
    "  vec2 ql = abs(p+vec2(0.5,0.)) - b;\n"
    "  vec2 qr = abs(p-vec2(0.5,0.)) - b;\n"
    "  float dl = length(max(ql,0.)) + min(max(ql.x,ql.y),0.);\n"
    "  float dr = length(max(qr,0.)) + min(max(qr.x,qr.y),0.);\n"
    "  d = min( dl, dr );\n"
    "}\n"
    "void main(void) {\n"
    "  float d;\n"
    "  const float EPS = 2.5e-2;\n"
    "  const vec2 b = vec2( .25, 0.9 );\n"
    "  switch( emblem ) {\n"
    "  case 0:\n"
    "    sdOctagon( corner, 0.9, d );\n"
    "    break;\n"
    "  case 1:\n"
    "    // Call with 90degree rotation\n"
    "    sdEquilateral( vec2(-corner.y,corner.x), 0.9, d );\n"
    "    break;\n"
    "  case 2:\n"
    "    sdPause( corner, b, d );\n"
    "    break;\n"
    "  }\n"
    "  // Not the fanciest drawing one might imagine\n"
    "  vec3 col  = abs(d) < EPS ? vec3(1.) : color;\n"
    "  fragColor = vec4( col, d < EPS ? 1. : 0. );\n"
    "}";

  handle.d->program = OGLEScreateProgram( vertex_shader_source,
                                          fragment_shader_source );

  glUseProgram( handle.d->program );

  GLuint vertex_attr = glGetAttribLocation( handle.d->program, "vertex" );
  GLuint coloruv_attr = glGetAttribLocation( handle.d->program, "coloruv" );
  GLuint corneruv_attr = glGetAttribLocation( handle.d->program, "corneruv" );
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );
  handle.d->emblem_u = glGetUniformLocation( handle.d->program, "emblem" );

  // Include the translation (in mm) of the emblem in the modelview
  // operation. For this one, I'm putting the mm scale in the
  // modelview to save some typing.
  GLfloat mv_matrix[] = {
     2.f/screen_width_mm, 0.f,                            0.f, 0.f,
     0.f,                          2.f/screen_height_mm, 0.f, 0.f,
     0.f,                          0.f,                            1.f, 0.f,
     2.f*x_mm/screen_width_mm-1.f, 2.f*y_mm/screen_height_mm-1.f,  0.f, 1.f
  };
  glUniform1i( handle.d->emblem_u, EMBLEM_WIDGET_EMBLEM_NOEMBLEM );
  glUniformMatrix4fv( mv_matrix_u, 1, GL_FALSE, mv_matrix );

  glGenVertexArrays( 1, &handle.d->VAO );
  glBindVertexArray( handle.d->VAO );
  GLuint vertex_buffer, index_buffer, coloruv_buffer, corneruv_buffer;
  glGenBuffers( 1, &vertex_buffer );
  glGenBuffers( 1, &index_buffer );
  glGenBuffers( 1, &coloruv_buffer );
  glGenBuffers( 1, &corneruv_buffer );

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

  // Corner colors. (Like thermometer...)
  GLfloat square_coloruvs[4][3] =
    {
     { 0.25f, 0.25f, 0.25f },
     { 0.25f, 0.25f, 0.25f },
     { 0.75f, 0.75f, 0.75f },
     { 0.75f, 0.75f, 0.75f },
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

  handle.d->emblem = emblem;

  glUseProgram( handle.d->program );
  glUniform1i( handle.d->emblem_u, emblem );
}

void emblem_widget_draw_emblem ( struct EMBLEM_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  if ( handle.d->emblem == EMBLEM_WIDGET_EMBLEM_NOEMBLEM )
    return;

  glUseProgram( handle.d->program );

  glBindVertexArray( handle.d->VAO );

  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
  glDisable( GL_BLEND );
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
