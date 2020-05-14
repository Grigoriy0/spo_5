#include "library.h"



#include <iostream>
#include <vector>

namespace aio {
    void
#ifndef __linux__
    __cdecl
#endif
    hello(const char *s) {
        printf("Hello %s\n", s);
    }

#ifndef __linux__
    HANDLE aio__done = nullptr;
    HANDLE exitThreadEvent;

#define PIECE_SIZE 256
    char buffer[PIECE_SIZE + 1] = {};
    volatile int to_write = PIECE_SIZE;

    DWORD WINAPI reader_thread(LPVOID ptr) {
        if (aio__done == nullptr) {
            aio__done = CreateEvent(nullptr, false, false, "aio__done");
            if (aio__done == nullptr) {
                printf("CreateEvent(aio__done) failed %lu\n", GetLastError());
                return 1;
            }
        }
        HANDLE tmpEvent = CreateEvent(nullptr, false, false, "tmpread");
        exitThreadEvent = CreateEvent(nullptr, false, false, "exitThreadEvent");
        if (exitThreadEvent == nullptr) {
            printf("CreateEvent(exitThreadEvent) failed %lu\n", GetLastError());
            return 1;
        }
        if (tmpEvent == nullptr) {
            printf("CreateEvent(tmpReadEvent) failed\n");
            ExitProcess(1);
        }
        auto files = *(std::vector<std::string> *) ptr;
        for (const std::string &file : files) {
            OVERLAPPED overlapped = {};
            overlapped.hEvent = tmpEvent;
            printf("r open %s\n", file.c_str());
            HANDLE fd = CreateFile(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (fd == INVALID_HANDLE_VALUE) {
                printf("CreateFile failed %lu\n", GetLastError());
                ExitProcess(3);
            }
            unsigned long readed = 0;
            overlapped.OffsetHigh = 0;
            overlapped.Offset = 0;
            do {
                printf("r read\n");
                if (!ReadFile(fd, buffer, PIECE_SIZE, &readed, &overlapped)) {
                    printf("ReadFile failed %lu\n", GetLastError());
                    ExitThread(2);
                }
                printf("r wait\n");
                if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED) {
                    printf("WaitForSingleObject failed %lu\n", GetLastError());
                    ExitThread(4);
                }
                printf("readed %d\n", readed);
                to_write = readed;
                buffer[readed] = '\0';
                printf("r set\n");
                if (!SetEvent(aio__done)) {
                    printf("SetEvent(aio__done) failed %lu\n", GetLastError());
                    ExitThread(5);
                }
                if (readed != PIECE_SIZE)
                    break;
                printf("r wait\n");
                Sleep(1);
                if (WaitForSingleObject(aio__done, INFINITE) == WAIT_FAILED) {
                    printf("WaitForSingleObject(aio__done) failed %lu\n", GetLastError());
                    ExitThread(4);
                }
                overlapped.Offset += PIECE_SIZE;
            } while (true);
            CloseHandle(fd);
        }
        Sleep(2);
        WaitForSingleObject(aio__done, INFINITE);
        SetEvent(exitThreadEvent);
        printf("r close\n");
        CloseHandle(tmpEvent);
        ExitThread(0);
    }


    DWORD WINAPI writer_thread(LPVOID ptr) {
        auto out_file = *(std::string *) ptr;
        HANDLE fd = CreateFile(out_file.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (fd == INVALID_HANDLE_VALUE) {
            printf("CreateFile failed %lu\n", GetLastError());
            ExitThread(3);
        }
        HANDLE tmpEvent = CreateEvent(nullptr, false, false, "tmpwrite");
        if (tmpEvent == nullptr) {
            printf("CreateEvent failed %lu\n", GetLastError());
            ExitThread(2);
        }
        OVERLAPPED overlapped = {};
        overlapped.hEvent = tmpEvent;
        overlapped.OffsetHigh = 0;
        overlapped.Offset = 0;
        do {
            printf("\tw wait\n");
            unsigned long writed = 0;
            do {
                int res = WaitForSingleObject(aio__done, 3);
                if (res == WAIT_FAILED) {
                    printf("WaitForSingleObject(aio__done) failed %lu\n", GetLastError());
                    ExitThread(4);
                } else if (res == WAIT_TIMEOUT)
                    if (WaitForSingleObject(exitThreadEvent, 0) == WAIT_OBJECT_0) {
                        printf("\tw close\n");
                        CloseHandle(fd);
                        CloseHandle(tmpEvent);
                        ExitThread(0);
                    }
                if (res == WAIT_OBJECT_0)
                    break;
            } while (true);
            printf("\tw write\n");
            if (!WriteFile(fd, &buffer, to_write, &writed, &overlapped)) {
                printf("WriteFile failed %lu\n", GetLastError());
                ExitThread(2);
            }
            printf("\tw wait\n");
            if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED) {
                printf("WaitForSingleObject failed %lu\n", GetLastError());
                ExitThread(4);
            }
            printf("\twrited %d\n", writed);
            printf("\tw set\n");
            if (!SetEvent(aio__done)) {
                printf("SetEvent(aio_write_done) failed %lu\n", GetLastError());
                ExitThread(5);
            }
            overlapped.Offset += writed;
            if (WaitForSingleObject(exitThreadEvent, 0) == WAIT_OBJECT_0)
                break;
            Sleep(1);
        } while (true);
        printf("\tw close\n");
        CloseHandle(fd);
        CloseHandle(tmpEvent);
        ExitThread(0);
    }

#endif

}