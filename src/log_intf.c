/*
 * Do some more generalized logging than printf. This implementation
 * is based on log4c (at least for now).
 */
#include <stdlib.h>

#include "log4c.h"

#include "log_intf.h"

struct LOG_PRIVATE {
  log4c_category_t* category;
};

struct LOG_HANDLE log_init ( void )
{
  struct LOG_HANDLE handle;

  handle.d = malloc( sizeof( struct LOG_PRIVATE ) );

  if ( log4c_init() ) {
  }
  else {
    handle.d->category = log4c_category_get( "mpddisplay" );
  }

  return handle;
}

int log_message_info ( struct LOG_HANDLE handle, const char* message, ... )
{
  if ( handle.d != NULL ) {
    va_list argp;
    va_start( argp, message );
    log4c_category_vlog( handle.d->category, LOG4C_PRIORITY_INFO, message,
			 argp );
    va_end( argp );
  }
  return 0;
}

int log_message_warn ( struct LOG_HANDLE handle, const char* message, ... )
{
  if ( handle.d != NULL ) {
    va_list argp;
    va_start( argp, message );
    log4c_category_vlog( handle.d->category, LOG4C_PRIORITY_WARN, message,
			 argp );
    va_end( argp );
  }
  return 0;
}

int log_message_error ( struct LOG_HANDLE handle, const char* message, ... )
{
  if ( handle.d != NULL ) {
    va_list argp;
    va_start( argp, message );
    log4c_category_vlog( handle.d->category, LOG4C_PRIORITY_ERROR, message,
			 argp );
    va_end( argp );
  }
  return 0;
}

int log_close ( struct LOG_HANDLE handle )
{
  if ( handle.d != NULL ) {
    if ( log4c_fini() ) {
    }
    free( handle.d );
  }
  return 0;
}
