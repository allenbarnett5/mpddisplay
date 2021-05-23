/*
 * Display a thermometer.
 * Slowly we're recapitulating Qt. Probably should have just used it.
 */
#include <stdio.h>
#include <stdlib.h>

#include <GLES3/gl3.h>

#include "ogles.h"
#include "thermometer_widget.h"

struct THERMOMETER_WIDGET_PRIVATE {
  GLuint texture;
  GLuint program;
  GLuint VAO;
  GLuint scale_u;
};

struct THERMOMETER_WIDGET_HANDLE thermometer_widget_init ( float x_mm, float y_mm,
					       float width_mm,
					       float height_mm,
					       float screen_width_mm,
					       float screen_height_mm )
{
  struct THERMOMETER_WIDGET_HANDLE handle;
  handle.d = malloc( sizeof( struct THERMOMETER_WIDGET_PRIVATE ) );

  const GLchar* vertex_shader_source =
    "#version 300 es       \n"
    "precision mediump float;\n"
    "uniform float scale;\n"
    "uniform mat4 mv_matrix;\n"
    "in  vec2 vertex;\n"
    "in  vec2 corneruv;\n"
    "in  vec3 coloruv;\n"
    "out vec2 corner;\n"
    "out vec3 color;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( scale* vertex.x, vertex.y, 0., 1. );\n"
    "  corner      = corneruv;\n"
    "  color       = coloruv;\n"
    "}";

  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform float scale;\n"
    "in vec2 corner;\n"
    "in vec3 color;\n"
    "out vec4 fragColor;\n"
    "void main(void) {\n"
    "  const float W = 74.54;\n"
    "  const float H = 6.38;\n"
    "  const float R = 2.f;\n"
    "  float Rx = 1.f - 2.f * R / ( W * scale );\n"
    "  float Ry = 1.f - 2.f * R / H;\n"
    "  float alpha = 1.f;\n"
    "  if ( all( greaterThanEqual( abs( corner ), vec2(Rx,Ry) ) ) ) {\n"
    "    vec2 d = vec2(0.5*W*scale*(abs(corner.x)-1.f) + R,\n"
    "                  0.5*H*      (abs(corner.y)-1.f) + R );\n"
    "    float d_len = length( d );\n"
    "    float grad = fwidth( d_len );\n"
    "    alpha = 1.f - smoothstep( R-grad, R, d_len );\n"
    "  }\n"
    "  fragColor.rgba = vec4( color, alpha );\n"
    "}";

  handle.d->program = OGLEScreateProgram( vertex_shader_source,
                                          fragment_shader_source );

  glUseProgram( handle.d->program );

  GLuint vertex_attr = glGetAttribLocation( handle.d->program, "vertex" );
  GLuint coloruv_attr = glGetAttribLocation( handle.d->program, "coloruv" );
  GLuint corneruv_attr = glGetAttribLocation( handle.d->program, "corneruv" );
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );
  handle.d->scale_u  = glGetUniformLocation( handle.d->program, "scale" );

  // We draw in mm.
#if 0
  GLfloat mv_matrix[] = {
                         2.f/screen_width_mm, 0.f, 0.f, 0.f,
                         0.f, 2.f/screen_height_mm, 0.f, 0.f,
                         0.f, 0.f, 1.f, 0.f,
                         -1.f, -1.f, 0.f, 1.f
  };
#else
  // Include the translation (in mm) of the thermometer in the
  // modelview operation.
  GLfloat mv_matrix[] = {
     2.f/screen_width_mm,          0.f,                          0.f, 0.f,
     0.f,                          2.f/screen_height_mm,         0.f, 0.f,
     0.f,                          0.f,                          1.f, 0.f,
     2.f*x_mm/screen_width_mm-1.f, 2.f*y_mm/screen_width_mm-1.f, 0.f, 1.f
  };
#endif
  glUniform1f( handle.d->scale_u, 0.f );
  glUniformMatrix4fv( mv_matrix_u, 1, GL_FALSE, mv_matrix );

  glGenVertexArrays( 1, &handle.d->VAO );
  glBindVertexArray( handle.d->VAO );
  GLuint vertex_buffer, coloruv_buffer, corneruv_buffer;
  glGenBuffers( 1, &vertex_buffer );
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

  // Corner colors.
  GLfloat square_coloruvs[4][3] =
    {
#if 1
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

void thermometer_widget_set_value ( struct THERMOMETER_WIDGET_HANDLE handle,
                                    float value )
{
  if ( handle.d == NULL )
    return;
#if 1
  glUseProgram( handle.d->program );
  glUniform1f( handle.d->scale_u, value );
#endif
}

void thermometer_widget_draw_thermometer ( struct THERMOMETER_WIDGET_HANDLE handle )
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

void thermometer_widget_free_handle ( struct THERMOMETER_WIDGET_HANDLE handle )
{
  if ( handle.d != NULL ) {
    glDeleteTextures( 1, &handle.d->texture );
    glDeleteVertexArrays( 1, &handle.d->VAO );
    glDeleteProgram( handle.d->program );
    free( handle.d );
    handle.d = NULL;
  }
}
