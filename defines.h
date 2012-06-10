#ifndef DEFINES_H
#define DEFINES_H

#define SENTINEL 0xdeadbeef

#ifndef DEBUG_ENABLED 
#define DEBUG_ENABLED 0
#endif

#define DEBUG(fmt, args...) if (DEBUG_ENABLED) dr_fprintf(STDERR, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ## args);

#endif // DEFINES_H
