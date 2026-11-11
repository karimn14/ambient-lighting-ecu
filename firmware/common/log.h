/**
 * @file    log.h
 * @brief   Non-blocking event log (SRS DIA-003).
 *
 * Line format: "[<ms since reset>] <MODULE> <event> <key=value ...>\r\n"
 *
 * Log_Event formats one line into a byte ring buffer. If the buffer has no room the whole line is dropped
 * and the dropped counter increases (a partial line is never written). Log_Flush hands buffered bytes to the
 * sink and must be called on every main-loop pass. The sink must not block: it returns how many bytes it
 * accepted, and the rest stay buffered.
 *
 * The module owns one global buffer (singleton). Log_Init resets it, which host tests rely on.
 *
 * Layer: Services. Host-testable: yes.
 */
#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#include "status.h"

#define LOG_BUFFER_SIZE  (1024u)
#define LOG_LINE_MAX     (96u)   /**< Longest formatted line including the line ending. Longer lines are truncated. */

#if defined(__GNUC__)
#define LOG_PRINTF_ATTR(fmtIndex, argIndex) __attribute__((format(printf, fmtIndex, argIndex)))
#else
#define LOG_PRINTF_ATTR(fmtIndex, argIndex)
#endif

/** Non-blocking byte sink. Returns the number of bytes accepted (0 to length). */
typedef uint16_t (*LogSinkFn_t)(const uint8_t *data, uint16_t length);

Status_t Log_Init(LogSinkFn_t sink);

/** @brief Log an event with key=value data given as a printf format, for example "from=%u to=%u". */
void Log_Event(uint32_t nowMs, const char *module, const char *event, const char *fmt, ...)
    LOG_PRINTF_ATTR(4, 5);

/** @brief Log an event with no data. */
void Log_EventSimple(uint32_t nowMs, const char *module, const char *event);

void Log_Flush(void);

/** @brief Lines dropped because the buffer was full (wraps at 65535). */
uint16_t Log_GetDroppedCount(void);

#endif /* LOG_H */
