#include <iostream>
#include <windows.h>
#include <vector>


class Lib {
public:
    explicit Lib(const char* name){
        handle = LoadLibrary(name);
    }

    template<class func>
    func getFunc(const char *func_name) {
        return (func) GetProcAddress(handle, func_name);
    }

    template<typename var>
    var getSym(const char *var_name) {
        return (var) GetProcAddress(handle, var_name);
    }

    ~Lib(){
        FreeLibrary(handle);
    }
protected:
    HINSTANCE handle;
};

static Lib lib("liblib.dll");





int main() {
    std::vector<std::string> in_files = {"in1.txt", "in2.txt"};
    std::string out_file = "out.txt";
    DWORD r_id, w_id;
    LPTHREAD_START_ROUTINE reader_thread = lib.getFunc<LPTHREAD_START_ROUTINE >("reader_thread@4");
    if (reader_thread == nullptr){
        printf("reader_thread not found %d\n", GetLastError());
        system("pause");
        return 1;
    }
    LPTHREAD_START_ROUTINE writer_thread = lib.getFunc<LPTHREAD_START_ROUTINE>("writer_thread@4");
    if (writer_thread == nullptr){
        printf("writer_thread not found%d\n", GetLastError());
        system("pause");
        return 1;
    }
    HANDLE r = CreateThread(nullptr, 0, reader_thread, &in_files, 0, &r_id);
    HANDLE w = CreateThread(nullptr, 0, writer_thread, &out_file, 0, &w_id);

    WaitForSingleObject(r, INFINITE);
    WaitForSingleObject(w, INFINITE);
    CloseHandle(r);
    CloseHandle(w);
    system("pause");
    return 0;
}