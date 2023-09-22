#ifndef GPIO_DEBUG_H
#define GPIO_DEBUG_H

#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#define DEBUG(...) if (ENABLE_DEBUG) DEBUG_PRINT(__VA_ARGS__)

#endif
