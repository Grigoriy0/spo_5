#include<iostream>
#include<stdio.h>
#include<dlfcn.h>
#include<vector>
#include<aio.h>
#include<pthread.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>

#define BUFFER_SIZE 256
volatile char buffer[BUFFER_SIZE + 1];
volatile int to_write = BUFFER_SIZE;
volatile sig_atomic_t time_to_close = 0;

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

pthread_mutex_t mutex;

void *reader_thread(void*cb){
    auto files = *(std::vector<std::string>*)cb;
    for (const std::string &filename : files){
        aiocb *cb = (aiocb*)calloc(sizeof(aiocb), 1);
        printf("r open\n");
        cb->aio_fildes = open(filename.c_str(), O_RDONLY);
        cb->aio_offset = 0;
        cb->aio_buf = buffer;
        cb->aio_nbytes = BUFFER_SIZE;

        long readed = 0;
        do{
            if (pthread_mutex_lock(&mutex) == -1){
                printf("pthread_mutex_lock failed %d\n", errno);
                break;
            }
            printf("r lock\n");
            if (readed != BUFFER_SIZE){
                time_to_close = 1;
                pthread_mutex_unlock(&mutex);
                break;
            }
            printf("r read\n");
            if(aio_read(cb) == -1){
                printf("aio_read failed %d\n", errno);
                close(cb->aio_fildes);
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }
            printf("r suspend\n");
            timespec time{};
            time.tv_sec = 1;
            if (aio_suspend(&cb, 1, &time) == -1){
                printf("aio_suspend failed %d\n", errno);
                close(cb->aio_fildes);
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }
            readed = aio_return(cb);
            if (readed == -1){
                printf("aio returned with error %d\n", errno);
                close(cb->aio_fildes);
                pthread_mutex_unlock(&mutex);
                return nullptr;
            }
            printf("r unlock\n");
            to_write = readed;
            pthread_mutex_unlock(&mutex);
            cb->aio_offset += BUFFER_SIZE;
            usleep(100);
        } while(readed == BUFFER_SIZE);
        close(cb->aio_fildes);
    }
    printf("r close\n");
    return nullptr;
}


void *writer_thread(void*ptr){
    auto out_file_name = *reinterpret_cast<std::string*>(ptr);
    int fd = open(out_file_name.c_str(), O_WRONLY);
    long writed = 0;
    aiocb *cb = (aiocb*)calloc(sizeof(aiocb), 1);
    cb->aio_offset = 0;
    cb->aio_buf = buffer;
    cb->aio_fildes = fd;
    do{
        if (pthread_mutex_lock(&mutex) == -1){
            printf("pthread_mutex_lock failed %d\n", errno);
            break;
        }
        printf("w lock\n");
        if (time_to_close == 1){
            pthread_mutex_unlock(&mutex);
            break;
        }
        cb->aio_nbytes = to_write;
        printf("readed %d bytes\n", to_write);
        if (to_write == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        if (aio_write(cb) == -1){
            printf("aio_write failed %d\n", errno);
            break;
        }
        printf("w suspend\n");
        timespec time{};
        time.tv_sec = 1;
        if (aio_suspend(&cb, 1, &time) == -1) {
            printf("aio_suspend failed %d\n", errno);
            break;
        }
        for (int i = 0; i < BUFFER_SIZE; ++i)
            putchar(buffer[i]);
        putchar('\n');
        writed = aio_return(cb);
        if (writed == -1){
            printf("aio_return returned -1 %d\n", errno);
            break;
        }
        printf("w unlock\n");
        pthread_mutex_unlock(&mutex);
        usleep(100);
        cb->aio_offset += to_write;
    } while(writed == BUFFER_SIZE);
    printf("w close\n");
    pthread_mutex_unlock(&mutex);
    close(fd);
    return nullptr;
}




int main(){
    if (pthread_mutex_init(&mutex, nullptr) == -1){
        printf("pthread_mutex_init failed %d\n", errno);
        return 1;
    }
    //Lib lib("/home/grigoriy/Projects/GLOBAL/Win/VS/SPO_5/linux/liblib.so");
    //Lib lib("/mnt/d/Projects/Win/VS/SPO_5/linux/liblib.so");
    //lib.getFunc<void(*)(const char*)>("hello")(" World!\n");
    std::vector<std::string> files = {"in1.txt", "in2.txt"};
    std::string out_file = "out.txt";
    pthread_t r, w;
    pthread_create(&r, nullptr, reader_thread, (void*)&files);
    pthread_create(&w, nullptr, writer_thread, (void*)&out_file);

    pthread_mutex_destroy(&mutex);
    pthread_join(r, nullptr);
    pthread_join(w, nullptr);
    return 0;
}
