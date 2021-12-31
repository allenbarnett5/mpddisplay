/*
 * Try to wrap a logging library. The presence of convenient stdarg
 * functions makes this a little tricky.
 */
#pragma once

struct LOG_PRIVATE;

struct LOG_HANDLE {
  struct LOG_PRIVATE* d;
};

/*!
 * Initialize logging.
 * \return ?.
 */
struct LOG_HANDLE log_init ( void );
/*!
 * Log an info message to the...well...log.
 * \param[in] handle the log object.
 * \param[in] message the plain string to log.
 * \return 0 if everything went ok, -1 otherwise.
 */
int log_message_info ( struct LOG_HANDLE handle, const char* message, ... );
/*!
 * Log a warning message to the...well...log.
 * \param[in] handle the log object.
 * \param[in] message the plain string to log.
 * \return 0 if everything went ok, -1 otherwise.
 */
int log_message_warn ( struct LOG_HANDLE handle, const char* message, ... );
/*!
 * Log an error message to the...well...log.
 * \param[in] handle the log object.
 * \param[in] message the plain string to log.
 * \return 0 if everything went ok, -1 otherwise.
 */
int log_message_error ( struct LOG_HANDLE handle, const char* message, ... );
/*!
 * Close the logs.
 * \param[in] handle the log object.
 * \return 0 if everything went ok, -1 otherwise.
 */
int log_close ( struct LOG_HANDLE handle );
