#ifndef LIB_LIBRARY_H
#define LIB_LIBRARY_H

#ifndef __linux__
#include <windows.h>
#else

#endif

namespace aio {
#ifdef __cplusplus
    extern "C" {
#endif

#ifndef __linux__
    DWORD WINAPI reader_thread(LPVOID ptr);
#endif


#ifndef __linux__
    DWORD WINAPI writer_thread(LPVOID ptr);
#endif


    void
#ifndef __linux__
    __cdecl
#endif
    hello(const char *s);


#ifdef __cplusplus
    }
#endif
#endif //LIB_LIBRARY_H
}
