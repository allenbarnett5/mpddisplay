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
    "out vec3 color;\n"
    "void main(void) {\n"
    "  gl_Position = mv_matrix * vec4( vertex.x, vertex.y, 0., 1. );\n"
    "  color       = coloruv;\n"
    "}";

  const GLchar* fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
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
  GLuint mv_matrix_u = glGetUniformLocation( handle.d->program, "mv_matrix" );

  printf( "vertex attr: %d\n", vertex_attr );
  printf( "coloruv attr: %d\n", coloruv_attr );
  printf( "matrix u: %d\n", mv_matrix_u );

  // Include the translation (in mm) of the emblem in the modelview
  // operation. For this one, I'm putting the mm scale in the
  // modelview to save some typing.
  GLfloat mv_matrix[] = {
     2.f/screen_width_mm, 0.f,                            0.f, 0.f,
     0.f,                          2.f/screen_height_mm, 0.f, 0.f,
     0.f,                          0.f,                            1.f, 0.f,
     2.f*x_mm/screen_width_mm-1.f, 2.f*y_mm/screen_height_mm-1.f,  0.f, 1.f
  };
  glUniformMatrix4fv( mv_matrix_u, 1, GL_FALSE, mv_matrix );

  glGenVertexArrays( 1, &handle.d->VAO );
  glBindVertexArray( handle.d->VAO );
  GLuint vertex_buffer, index_buffer, coloruv_buffer/*, corneruv_buffer*/;
  glGenBuffers( 1, &vertex_buffer );
  glGenBuffers( 1, &index_buffer );
  glGenBuffers( 1, &coloruv_buffer );
  printf( "emblem vertex buffer: %d\n", vertex_buffer );
  printf( "emblem color buffer: %d\n", coloruv_buffer );

  // Draw in mm.
  // Octagon corners.

  const float OS = ( sqrtf(2.f) - 1.f ) / 2.f;

  GLfloat square_vertexes[][2] =
    {
     // Enter the octagon.
     { 1.f, 0.5f+OS },
     { 0.5f+OS, 1.f },
     { 0.5f-OS, 1.f },
     { 0.f, 0.5f+OS },
     { 0.f, 0.5f-OS },
     { 0.5f-OS, 0.f },
     { 0.5f+OS, 0.f },
     { 1.f, 0.5f-OS },
     // Trianguly.
     { 0.f, 0.f  },
     { 1.f, 0.5f },
     { 0.f, 1.f },
     // Pause, please. Tricky.
     { 0.f, 0.f }, // Left bulb.
     { 0.375, 0.f },
     { 0.375, 1.f },
     { 0.f, 1.f },
     { 0.625f, 0.f }, // Right bulb.
     { 1.f, 0.f },
     { 1.f, 1.f },
     { 0.625f, 1.f },
    };

  const float T = 1.f; // thickness, mm

  // Save some typing.
  GLfloat  vbuf[16+6+16][2];
  GLushort ibuf[18+8+18];
  GLfloat (*vertexes)[2] = &vbuf[0];
  GLushort* indexes = &ibuf[0];

  // Octagon.
  //  printf( "octagon\n" );
  const float OR = 10.f / sqrtf(2.f + sqrt(2.f));
  const float CRX = 5.f;
  const float CRY = 5.f;
  for ( int i = 0; i < 8; ++i ) {
    const float theta = M_PI * (2*i+1) / 8.f;
    const float cosx = cos(theta);
    const float sinx = sin(theta);
    //    printf( "%f: %f %f\n", theta, OR*cosx+CRX, OR*sinx+CRY );
    vertexes[2*i][0] = OR*cosx+CRX;
    vertexes[2*i][1] = OR*sinx+CRY;
    vertexes[2*i+1][0] = (OR-T)*cosx+CRX;
    vertexes[2*i+1][1] = (OR-T)*sinx+CRY;
    indexes[2*i] = 2*i;
    indexes[2*i+1] = 2*i+1;
  }
  indexes[2*8] = 0;
  indexes[2*8+1] = 1;

  // Triangle.
  //  printf( "triangle\n" );
  vertexes = &vbuf[16];
  indexes  = &ibuf[18];
  const float OT = 10.f / sqrtf(3.f);
  const float CTX = 10.f / (2.f* sqrt(3.f));
  const float CTY = 5.f;
  for ( int i = 0; i < 3; ++i ) {
    const float theta = M_PI * (2*i) / 3.f;
    const float cosx = cos(theta);
    const float sinx = sin(theta);
    //    printf( "%f: %f %f\n", theta, OT*cosx+CTX, OT*sinx+CTY );
    vertexes[2*i][0] = OT*cosx+CRX;
    vertexes[2*i][1] = OT*sinx+CRY;
    vertexes[2*i+1][0] = (OT-T)*cosx+CRX;
    vertexes[2*i+1][1] = (OT-T)*sinx+CRY;
    indexes[2*i] = 2*i + 16;
    indexes[2*i+1] = 2*i+1 + 16;
  }
  indexes[2*3] = 0 + 16;
  indexes[2*3+1] = 1 + 16;

  // Pause (||).
  //  printf( "pause\n" );
  vertexes += 6;
  indexes  += 8;
  // Start with a template shape defined at the origin.
  vertexes[0][0] = 0.f;
  vertexes[0][1] = 0.f;
  vertexes[1][0] = cos(M_PI/4.f) * T;
  vertexes[1][1] = sin(M_PI/4.f) * T;
  indexes[0] = 0 + 22;
  indexes[1] = 1 + 22;
  vertexes[2][0] = 3.f;
  vertexes[2][1] = 0.f;
  vertexes[3][0] = 3.f - cos(M_PI/4.f) * T;
  vertexes[3][1] = sin(M_PI/4.f) * T;
  indexes[2] = 2 + 22;
  indexes[3] = 3 + 22;
  vertexes[4][0] = 3.f;
  vertexes[4][1] = 10.f;
  vertexes[5][0] = 3.f - cos(M_PI/4.f) * T;
  vertexes[5][1] = 10.f - sin(M_PI/4.f) * T;
  indexes[4] = 4 + 22;
  indexes[5] = 5 + 22;
  vertexes[6][0] = 0.f;
  vertexes[6][1] = 10.f;
  vertexes[7][0] = cos(M_PI/4.f) * T;
  vertexes[7][1] = 10.f - sin(M_PI/4.f) * T;
  indexes[6] = 6 + 22;
  indexes[7] = 7 + 22;
  indexes[8] = 0 + 22;
  indexes[9] = 1 + 22;

  for ( int i = 0; i < 36; ++i ) {
    if ( i < 30 ) {
      printf( "%d %12f %12f %d\n", i, vbuf[i][0], vbuf[i][1], ibuf[i] );
    }
    else {
      printf( "%d                          %d\n", i, ibuf[i] );
    }
  }
  
  // Remember: if you pass 0 to glVertexAttribPointer, you must
  // have bound the buffer already.
  glBindBuffer( GL_ARRAY_BUFFER, vertex_buffer );
  glVertexAttribPointer( vertex_attr, 2, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[2]), 0 );
#if 0
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_vertexes),
                square_vertexes, GL_STATIC_DRAW );
#else
  glBufferData( GL_ARRAY_BUFFER, sizeof(vbuf), vbuf, GL_STATIC_DRAW );
#endif
  glEnableVertexAttribArray( vertex_attr );

  // VAO is implicit(?) and doesn't need to be enabled(?)
  GLushort square_indexes[] = {
                                0,1,2,3,4,5,6,7,
                                8,9,10,
                                11, 12, 13, 14, 65535, 15, 16, 17, 18
  };
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, index_buffer );
#if 0
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(square_indexes),
                square_indexes, GL_STATIC_DRAW );
#else
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(ibuf), ibuf, GL_STATIC_DRAW );
#endif
#if 0
  // Corner colors.
  GLfloat square_coloruvs[][3] =
    {
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
    };

  glBindBuffer( GL_ARRAY_BUFFER, coloruv_buffer );
  glVertexAttribPointer( coloruv_attr, 3, GL_FLOAT, GL_FALSE,
                         sizeof(GLfloat[3]), 0 );
  glBufferData( GL_ARRAY_BUFFER, sizeof(square_coloruvs),
                square_coloruvs, GL_STATIC_DRAW );
  glEnableVertexAttribArray( coloruv_attr );
#else
  glVertexAttrib3f( coloruv_attr, 0.f, 1.f, 1.f );
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
}

void emblem_widget_draw_emblem ( struct EMBLEM_WIDGET_HANDLE handle )
{
  if ( handle.d == NULL )
    return;
  if ( handle.d->emblem == EMBLEM_WIDGET_EMBLEM_NOEMBLEM )
    return;

  glUseProgram( handle.d->program );

  glBindVertexArray( handle.d->VAO );

  switch ( handle.d->emblem ) {
  case EMBLEM_WIDGET_EMBLEM_STOPPED:
#if 0
    glDrawElements( GL_LINE_LOOP, 8, GL_UNSIGNED_SHORT, 0 );
#else
    glDrawElements( GL_TRIANGLE_STRIP, 18, GL_UNSIGNED_SHORT, 0 );
#endif
    break;
  case EMBLEM_WIDGET_EMBLEM_PLAYING:
#if 0
    glDrawElements( GL_LINE_LOOP, 3, GL_UNSIGNED_SHORT,
                    (void*)(8*sizeof(GLushort)) );
#else
    glDrawElements( GL_TRIANGLE_STRIP, 8, GL_UNSIGNED_SHORT, 
                    (void*)(18*sizeof(GLushort)) );
#endif
    break;
  case EMBLEM_WIDGET_EMBLEM_PAUSED:
#if 0
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements( GL_LINE_LOOP, 9, GL_UNSIGNED_SHORT,
                    (void*)((8+3)*sizeof(GLushort)) );
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#elif 1
    glDrawElements( GL_TRIANGLE_STRIP, 10, GL_UNSIGNED_SHORT,
                    (void*)((18+8)*sizeof(GLushort)) );
#else
    glDrawElements( GL_TRIANGLE_STRIP, 18, GL_UNSIGNED_SHORT, 0 );
#endif
    break;
  case EMBLEM_WIDGET_EMBLEM_NOEMBLEM:
    break;
  }
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
