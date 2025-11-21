#ifndef __INCLUDE_GLOBAL
#define __INCLUDE_GLOBAL

#include "headfile.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct proxy_global_info
    {
        int use_thread_number;
        int use_socket_number;

        long long send_size;
        long long recv_size;
    } proxy_global_info;

    extern proxy_global_info Gloabl_global_info;

#ifdef __cplusplus
}
#endif

#endif