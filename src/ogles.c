/*
 * Support routines for the wordy OpenGL ES.
 */
#include <stdio.h>
#include <stdlib.h>
#include <GLES3/gl3.h>

#include "ogles.h"

GLuint OGLEScreateProgram ( const char* vertex_shader_src,
                            const char* fragmt_shader_src )
{
  GLuint vertex_shader = glCreateShader( GL_VERTEX_SHADER );
  glShaderSource( vertex_shader, 1, &vertex_shader_src, 0 );
  glCompileShader( vertex_shader );
  GLint is_compiled = 0;
  glGetShaderiv( vertex_shader, GL_COMPILE_STATUS, &is_compiled );
  if ( is_compiled == GL_FALSE ) {
    GLint max_length = 0;
    glGetShaderiv( vertex_shader, GL_INFO_LOG_LENGTH, &max_length );
    GLchar* log = malloc( max_length );
    glGetShaderInfoLog( vertex_shader, max_length, &max_length, log );
    printf( "vertex error: %s\n", log );
    free(log);
    glDeleteShader( vertex_shader );
    return -1;
  }

  GLuint fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
  glShaderSource( fragment_shader, 1, &fragmt_shader_src, 0 );
  glCompileShader( fragment_shader );
  glGetShaderiv( fragment_shader, GL_COMPILE_STATUS, &is_compiled );
  if ( is_compiled == GL_FALSE ) {
    GLint max_length = 0;
    glGetShaderiv( fragment_shader, GL_INFO_LOG_LENGTH, &max_length );
    GLchar* log = malloc( max_length );
    glGetShaderInfoLog( fragment_shader, max_length, &max_length, log );
    printf( "fragment error: %s\n", log );
    free(log);
    glDeleteShader( vertex_shader );
    glDeleteShader( fragment_shader );
    return -1;
  }

  GLuint program = glCreateProgram();
  glAttachShader( program, vertex_shader );
  glAttachShader( program, fragment_shader );
  glLinkProgram( program );

  glGetProgramiv( program, GL_LINK_STATUS, &is_compiled );
  if ( is_compiled == GL_FALSE ) {
    GLint max_length = 0;
    glGetShaderiv( program, GL_INFO_LOG_LENGTH, &max_length );
    GLchar* log = malloc( max_length );
    glGetProgramInfoLog( program, max_length, &max_length, log );
    printf( "link error: %s\n", log );
    free(log);
    glDeleteShader( vertex_shader );
    glDeleteShader( fragment_shader );
    glDeleteProgram( program );
    return -1;
  }

  glDeleteShader( vertex_shader );
  glDeleteShader( fragment_shader );

  return program;
}
