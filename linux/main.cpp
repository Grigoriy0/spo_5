#include <vector>
#include <list>
#include <iostream>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <aio.h>
#include <fcntl.h>
#include <dlfcn.h>




class Lib{
public:
    Lib(const std::string &libname){
        handle = dlopen(libname.c_str(), RTLD_LAZY);
        if (handle == NULL){
            printf("dlopen failed \n%s %d\n", dlerror(), errno);
            return;
        }
    }

    template <typename Func>
        Func getFunc(const std::string &name){
            dlerror();
            Func f = (Func)dlsym(handle, name.c_str());
            if (f == NULL){
                printf("ldopen failed \n%s\n", dlerror());
            }
            return f;
        }

    ~Lib(){
        if(handle != NULL && dlclose(handle) != 0){
            printf("dlclose failed \n%s\n", dlerror());
        }
    }
private:
    void *handle;
};


int main(){
    Lib lib("/home/grigoriy/Projects/GLOBAL/Win/VS/SPO_5/linux/liblib.so");
    //Lib lib("/mnt/d/Projects/Win/VS/SPO_5/linux/liblib.so");
    lib.getFunc<void(*)(const char*)>("hello")(" World!\n");
    
    std::list<std::string> in = {"in1.txt", "in2.txt"};
    std::string out = "out.txt";
    
    auto concatFiles = lib.getFunc<void(*)(std::list<std::string>,std::string)>("concatFiles");
    
    concatFiles(in, out);
    return 0;
}
