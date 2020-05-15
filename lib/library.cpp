#include "library.h"
#ifdef __linux__
#   include<pthread.h>
#   include<sys/shm.h>
#   include<sys/sem.h>
#   include<fcntl.h>
#   include<aio.h>
#else
#   include<windows.h>
#endif

#include <iostream>
#include <vector>
#include <list>
#include <cstring>
#ifdef __linux__
namespace constants {
    const int KEY_ID_SEMAPHORE = 234;
    const int KEY_ID_SH_MEMORY = 2345;
    const int SEMAPHORE_AMOUNT = 1;
    const int SEMAPHORE_INDEX = 0;
    const char INITIAL_PATH[] = "/home/grigoriy/Projects/GLOBAL/Win/VS/SPO_5/linux/main.cpp";
    const int BUFFER_SIZE = 10;
}


typedef struct argReaderThread {
    std::list<std::string> *sourceFiles;
    int shMemoryId;
    int semaphoreId;
} readerThreadArgs;

typedef struct argWriterThread {
    std::string outputFileName;
    int shMemoryId;
    int semaphoreId;
} writerThreadArgs;

int createSemaphoreSet(key_t key) {
    int check = 0;
    int id = semget(key, constants::SEMAPHORE_AMOUNT, IPC_CREAT | SHM_R | SHM_W);
    if (id != -1) {
        int setArray[constants::SEMAPHORE_AMOUNT] = {0};
        check = semctl(id, 0, SETALL, setArray);
    }
    return (check == -1) ? check : id;
}

void deleteSemaphoreSet(int semaphoreId) {
    semctl(semaphoreId, 0, IPC_RMID, NULL);
}

void *getShMemory(key_t shMemoryId) {
    void *shMemoryAddress = shmat(shMemoryId, nullptr, 0);
    if (shMemoryAddress == nullptr) {
        std::cerr << "shmat failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    return shMemoryAddress;
}

key_t getSemaphoreKey() {
    return ftok(constants::INITIAL_PATH, constants::KEY_ID_SEMAPHORE);
}

key_t getShMemoryKey() {
    return ftok(constants::INITIAL_PATH, constants::KEY_ID_SH_MEMORY);
}

int getSemaphoreId() {
    key_t key = getSemaphoreKey();
    if (key == -1) {
        return -1;
    }
    return createSemaphoreSet(key);
}

int getShMemoryId() {
    key_t key = getShMemoryKey();
    if (key == -1) {
        return -1;
    }
    return shmget(key, 1, IPC_CREAT | SHM_R | SHM_W);
}

std::string readFromFile(const std::string &fileName) {
    struct aiocb aiocb{};
    char *buffer = new char[constants::BUFFER_SIZE];

    FILE *file = fopen(fileName.c_str(), "r");
    if (file == nullptr) {
        std::cout << "Error while reading file: " << fileName << std::endl;
        return "";
    }

    std::string result = std::string();
    aiocb.aio_fildes = fileno(file);
    aiocb.aio_nbytes = constants::BUFFER_SIZE;
    aiocb.aio_buf = buffer;

    int offset = 0;
    do {
        for (int i = 0; i < constants::BUFFER_SIZE; ++i) {
            buffer[i] = 0;
        }
        aiocb.aio_offset = offset;

        if (aio_read(&aiocb) == -1) {
            std::cout << "Error while reading file: " << fileName << std::endl;
            fclose(file);
            delete[] (buffer);
            return result;
        }

        while (aio_error(&aiocb) == EINPROGRESS);

        if (aio_return(&aiocb) == -1) {
            std::cout << "Error while reading file: " << fileName << std::endl;
            fclose(file);
            delete[] (buffer);
            return result;
        }

        result.append(buffer);
        offset += constants::BUFFER_SIZE;
    } while (buffer[constants::BUFFER_SIZE - 1]);

    fclose(file);
    delete[] (buffer);
    return result;
}

void writeToFile(const std::string &fileName, std::string &data) {
    struct aiocb aiocb{};

    FILE *file = fopen(fileName.c_str(), "a+");
    if (file == nullptr) {
        std::cout << "Error while writing" << std::endl;
        return;
    }

    aiocb.aio_fildes = fileno(file);
    aiocb.aio_nbytes = data.length();
    aiocb.aio_buf = (char *) data.c_str();

    if (aio_write(&aiocb) == -1) {
        std::cout << "Error while writing" << std::endl;
        fclose(file);
        return;
    }

    while (aio_error(&aiocb) == EINPROGRESS);

    if (aio_return(&aiocb) == -1) {
        std::cout << "Error while writing" << std::endl;
    }

    fclose(file);
}

void *readerThreadRoutine(void *arg) {
    struct sembuf semaphoreSet{};
    auto args = static_cast<readerThreadArgs *> (arg);

    for (auto &fileName : *args->sourceFiles) {

        strcpy((char *) getShMemory(args->shMemoryId), readFromFile(fileName).c_str());

        semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
        semaphoreSet.sem_op = 1;
        semaphoreSet.sem_flg = SEM_UNDO;
        semop(args->semaphoreId, &semaphoreSet, 1);

        semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
        semaphoreSet.sem_op = -1;
        semaphoreSet.sem_flg = SEM_UNDO;
        semop(args->semaphoreId, &semaphoreSet, 1);
    }

    semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
    semaphoreSet.sem_op = 2;
    semaphoreSet.sem_flg = SEM_UNDO;
    semop(args->semaphoreId, &semaphoreSet, 1);

    semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
    semaphoreSet.sem_op = -3;
    semaphoreSet.sem_flg = SEM_UNDO;
    semop(args->semaphoreId, &semaphoreSet, 1);

    return nullptr;
}

void *writerThreadRoutine(void *arg) {
    struct sembuf semaphoreSet{};
    auto args = static_cast<writerThreadArgs *> (arg);

    while (true) {
        semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
        semaphoreSet.sem_op = -1;
        semaphoreSet.sem_flg = SEM_UNDO;
        semop(args->semaphoreId, &semaphoreSet, 1);

        if (semctl(args->semaphoreId, constants::SEMAPHORE_INDEX, GETVAL) == 1) {
            semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
            semaphoreSet.sem_op = 3;
            semaphoreSet.sem_flg = SEM_UNDO;
            semop(args->semaphoreId, &semaphoreSet, 1);

            return nullptr;
        }

        std::string data = (char *) getShMemory(args->shMemoryId);
        writeToFile(args->outputFileName, data);

        semaphoreSet.sem_num = constants::SEMAPHORE_INDEX;
        semaphoreSet.sem_op = 1;
        semaphoreSet.sem_flg = SEM_UNDO;
        semop(args->semaphoreId, &semaphoreSet, 1);
    }
}

int startReaderThread(pthread_t &thread, std::list<std::string> &sourceFiles, int &shMemoryId, int &semaphoreId) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    auto *args = (readerThreadArgs *) malloc(sizeof(readerThreadArgs));
    args->sourceFiles = &sourceFiles;
    args->shMemoryId = shMemoryId;
    args->semaphoreId = semaphoreId;

    return pthread_create(&thread, &attr, readerThreadRoutine, args);
}

int startWriterThread(pthread_t &thread, std::string &outputFileName, int &shMemoryId, int &semaphoreId) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    auto *args = (writerThreadArgs *) malloc(sizeof(writerThreadArgs));
    args->outputFileName = outputFileName;
    args->shMemoryId = shMemoryId;
    args->semaphoreId = semaphoreId;

    return pthread_create(&thread, &attr, writerThreadRoutine, args);
}
#ifdef __cplusplus
extern "C" void concatFiles(std::list<std::string> &sourceFiles, std::string &outputFileName) {
    struct shmid_ds shMemoryStruct{};

    int semaphoreId = getSemaphoreId();
    int shMemoryId = getShMemoryId();
    pthread_t readerThread, writerThread;
    void *threadExitStatus;

    if (semaphoreId == -1) {
        std::cerr << "semaphoreId = -1: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    if (shMemoryId == -1) {
        std::cerr << "shMemoryId = -1: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    if (startWriterThread(writerThread, outputFileName, shMemoryId, semaphoreId)) {
        std::cout << "Error while creation thread" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (startReaderThread(readerThread, sourceFiles, shMemoryId, semaphoreId)) {
        std::cout << "Error while creation thread" << std::endl;
        exit(EXIT_FAILURE);
    }

    pthread_join(readerThread, &threadExitStatus);

    deleteSemaphoreSet(semaphoreId);
    shmdt(getShMemory(shMemoryId));
    shmctl(shMemoryId, IPC_RMID, &shMemoryStruct);
}
#endif
#endif


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
