#include <iostream>
#include <windows.h>
#include <vector>

#define PIECE_SIZE 256

//class Lib {
//public:
//    explicit Lib(const char* name){
//        handle = LoadLibrary(name);
//    }
//
//    template<class func>
//    func getFunc(const char *func_name) {
//        return (func) GetProcAddress(handle, func_name);
//    }
//
//    ~Lib(){
//        FreeLibrary(handle);
//    }
//protected:
//    HINSTANCE handle;
//};

//static Lib lib("liblib.dll");

HANDLE readEvent;
HANDLE writeEvent;
HANDLE exitThreadEvent;

char buffer[PIECE_SIZE + 1];

DWORD WINAPI reader_thread(LPVOID ptr){
    HANDLE tmpReadEvent = CreateEvent(nullptr, false, false, "tmpread");
    if (tmpReadEvent == nullptr){
        printf("CreateEvent(tmpReadEvent) failed\n");
        ExitProcess(1);
    }
    auto files = *(std::vector<std::string>*)ptr;
    for (const std::string &file : files){
        OVERLAPPED overlapped = {};
        overlapped.hEvent = tmpReadEvent;
        printf("rd open %s\n", file.c_str());
        HANDLE fd = CreateFile(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (fd == INVALID_HANDLE_VALUE){
            printf("CreateFile failed %lu\n", GetLastError());
            ExitProcess(3);
        }
        unsigned long readed = 0;
        overlapped.OffsetHigh = 0;
        overlapped.Offset = 0;
        do {
            printf("rd read\n");
            if(!ReadFile(fd, buffer, PIECE_SIZE, &readed, &overlapped)){
                printf("ReadFile failed %lu\n", GetLastError());
                ExitThread(2);
            }
            printf("rd wait\n");
            if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED){
                printf("WaitFor... failed %lu\n", GetLastError());
                ExitThread(4);
            }
            buffer[readed] = '\0';
            printf("rd set\n");
            if (!SetEvent(readEvent)){
                printf("SetEvent(readEvent) failed %lu\n", GetLastError());
                ExitThread(5);
            }
            printf("rd wait\n");
            if (WaitForSingleObject(writeEvent, INFINITE) == WAIT_FAILED){
                printf("WaitFor... failed %lu\n", GetLastError());
                ExitThread(4);
            }
            if (readed != PIECE_SIZE)
                break;
            overlapped.Offset += PIECE_SIZE;
        } while(true);
        CloseHandle(fd);
    }
    SetEvent(exitThreadEvent);
    printf("rd close");
    CloseHandle(tmpReadEvent);
    ExitThread(0);
}


DWORD WINAPI writer_thread(LPVOID ptr){
    auto out_file = *(std::string*)ptr;
    ptr = ((std::string*)ptr + 1);
    int count = *(int*)ptr;
    HANDLE fd = CreateFile(out_file.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (fd == INVALID_HANDLE_VALUE){
        printf("CreateFile failed %lu\n", GetLastError());
        ExitThread(3);
    }
    HANDLE tmpEvent = CreateEvent(nullptr, false, false, "tmpwrite");
    if (tmpEvent == nullptr){
        printf("CreateEvent failed %lu\n", GetLastError());
        ExitThread(2);
    }
    OVERLAPPED overlapped = {};
    overlapped.hEvent = tmpEvent;
    overlapped.OffsetHigh = 0;
    overlapped.Offset = 0;
    do{
        printf("wr wait\n");
        unsigned long writed = 0;
        if (WaitForSingleObject(readEvent, INFINITE) == WAIT_FAILED) {
            printf("WaitFor... failed %lu\n", GetLastError());
            ExitThread(4);
        }
        printf("wr write\n");
        if (!WriteFile(fd, &buffer, strlen(buffer), &writed, &overlapped)) {
            printf("WriteFile failed %lu\n", GetLastError());
            ExitThread(2);
        }
        printf("wr wait\n");
        if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED) {
            printf("WaitFor... failed %lu\n", GetLastError());
            ExitThread(4);
        }
        printf("rd set\n");
        if (!SetEvent(writeEvent)) {
            printf("SetEvent(writeEvent) failed %lu\n", GetLastError());
            ExitThread(5);
        }
        overlapped.Offset += PIECE_SIZE;
        if (WaitForSingleObject(exitThreadEvent, 0) == WAIT_OBJECT_0)
            break;
    }while(true);
    printf("wr close\n");
    CloseHandle(fd);
    CloseHandle(tmpEvent);
    ExitThread(0);
}

int main() {
    buffer[PIECE_SIZE] = '\0';
    std::vector<std::string> in_files = {"in1.txt", "in2.txt"};
    void *ptr = malloc(sizeof(std::string) + sizeof(int));
    auto *pt = (std::string*)ptr;
    *pt = "out.txt";
    pt++;
    int *size = (int*)pt;
    *size = 2;
    DWORD r_id, w_id;
    readEvent = CreateEvent(nullptr, false, false, "readEvent");
    if (readEvent == nullptr){
        printf("CreateEvent(readEvent) failed %lu\n", GetLastError());
        return 1;
    }
    writeEvent = CreateEvent(nullptr, false, false, "writeEvent");
    if (readEvent == nullptr){
        printf("CreateEvent(writeEvent) failed %lu\n", GetLastError());
        return 1;
    }
    exitThreadEvent = CreateEvent(nullptr, false, false, "exitThreadEvent");
    if (exitThreadEvent == nullptr){
        printf("CreateEvent(exitThreadEvent) failed %lu\n", GetLastError());
        return 1;
    }
    HANDLE r = CreateThread(nullptr, 0, reader_thread, (void*)&in_files, 0, &r_id);
    HANDLE w = CreateThread(nullptr, 0, writer_thread, ptr, 0, &w_id);

    WaitForSingleObject(r, INFINITE);
    WaitForSingleObject(w, INFINITE);
    CloseHandle(r);
    CloseHandle(w);
    system("pause");
    return 0;
}