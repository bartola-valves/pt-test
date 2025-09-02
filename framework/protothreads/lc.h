/**
 * @file lc.h
 * @brief Local continuations for protothreads
 */

#ifndef __LC_H__
#define __LC_H__

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int lc_t;

#define LC_INIT(s) s = 0

#define LC_RESUME(s) \
    switch (s)       \
    {                \
    case 0:

#define LC_SET(s) \
    s = __LINE__; \
    case __LINE__:

#define LC_END(s) }

#ifdef __cplusplus
}
#endif

#endif /* __LC_H__ */