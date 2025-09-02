/**
 * @file pt.h
 * @brief Simplified protothread definitions for embedded systems
 */

#ifndef __PT_H__
#define __PT_H__

#include "lc.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Simple protothread state structure
    struct pt
    {
        lc_t lc; // Line continuation variable
    };

// Protothread return codes
#define PT_WAITING 0
#define PT_YIELDED 1
#define PT_EXITED 2
#define PT_ENDED 3

// Protothread initialization
#define PT_INIT(pt)   \
    do                \
    {                 \
        (pt)->lc = 0; \
    } while (0)

// Protothread begin/end macros (simplified)
#define PT_BEGIN(pt)  \
    switch ((pt)->lc) \
    {                 \
    case 0:

#define PT_END(pt) \
    }              \
    (pt)->lc = 0;  \
    return PT_ENDED;

// Wait until condition is true
#define PT_WAIT_UNTIL(pt, condition) \
    do                               \
    {                                \
        (pt)->lc = __LINE__;         \
    case __LINE__:                   \
        if (!(condition))            \
            return PT_WAITING;       \
    } while (0)

// Yield execution
#define PT_YIELD(pt)         \
    do                       \
    {                        \
        (pt)->lc = __LINE__; \
    case __LINE__:           \
        return PT_YIELDED;   \
    } while (0)

// Exit protothread
#define PT_EXIT(pt) return PT_EXITED

#ifdef __cplusplus
}
#endif

#endif /* __PT_H__ */