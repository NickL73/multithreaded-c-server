/**
 * @file utils.h
 * @author nick
 * @date 7/11/25
 * @brief
 */
#ifndef UTILS_H
#define UTILS_H

#ifdef NDEBUG
#define LOG_MSG(level, ...) ((void)0)
#else

#include <stdio.h>
#include <string.h>

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

// Define the color codes associated with each log level
#define COLOR_RESET  "\x1b[0m"  // reset the terminal color
#define COLOR_DEBUG  "\x1b[37m" // white
#define COLOR_INFO   "\x1b[32m" // green
#define COLOR_WARN   "\x1b[33m" // yellow
#define COLOR_ERROR  "\x1b[31m" // red
#define COLOR_FATAL  "\x1b[31m" // also red

// Don't want the full filepath, as it can clutter the output so just get relevant info
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Set a default maximum logging level if one wasn't defined at compilation
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif // LOG_LEVEL

#define LOG_MSG(level, ...)                                                                                  \
    do                                                                                                       \
    {                                                                                                        \
        if (LOG_LEVEL <= level)                                                                              \
        {                                                                                                    \
            const char * p_level_colors[] = {COLOR_DEBUG, COLOR_INFO, COLOR_WARN, COLOR_ERROR, COLOR_FATAL}; \
            printf("[%s%s%s][%s:%d] ", p_level_colors[level], #level, COLOR_RESET, __FILENAME__, __LINE__);  \
            printf(__VA_ARGS__);                                                                             \
            printf("\n");                                                                                    \
        }                                                                                                    \
    } while (0)

#endif // NDEBUG

// Define some easier macros to use instead of just the general macro
#define LOG_DEBUG(...) LOG_MSG(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  LOG_MSG(LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...)  LOG_MSG(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) LOG_MSG(LOG_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) LOG_MSG(LOG_FATAL, __VA_ARGS__)

#endif // UTILS_H
