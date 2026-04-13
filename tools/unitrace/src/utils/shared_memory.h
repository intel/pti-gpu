//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_SHARED_MEMORY_H_
#define PTI_TOOLS_UNITRACE_UTILS_SHARED_MEMORY_H_

#include <memory>

#ifdef _WIN32
#include <windows.h>
#else /* _WIN32 */
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#endif /* _WIN32 */

#include <iostream>

#define SHM_NAME_MAX 256
#define SHM_NAME_PREFIX "/uctrl"

enum SharedMemoryReturnStatus
{
    SHM_FAILED,
    SHM_SUCCESS,
    SHM_STOPPED
};

enum SharedMemoryOpenMode {
    SHM_OPEN_NONE,
    SHM_OPEN_READ,
    SHM_OPEN_WRITE
};

// Helper function to get error message in a thread-safe way
inline std::string GetErrorMessage(int err) {
#ifdef _WIN32
    char buffer[256];
    strerror_s(buffer, sizeof(buffer), err);
    return std::string(buffer);
#else /* _WIN32 */
    return std::string(strerror(err));
#endif /* _WIN32 */
}

class SharedMemory{
private:
    SharedMemoryOpenMode open_mode = SHM_OPEN_NONE;
    void* p_data_ = nullptr;
    char shm_name_[SHM_NAME_MAX] = { 0 };
    size_t shm_size_ = 0;
#ifndef _WIN32
    int handle_ = -1;
#else /* _WIN32 */
    HANDLE hMapFile_ = nullptr;
#endif /* _WIN32 */
    bool ValidateSessionName(const char* session) {
        if (strlen(session) > (SHM_NAME_MAX - strlen(SHM_NAME_PREFIX) - 1)) {
            std::cerr << "[ERROR] Session identifier is too long (maximum " << SHM_NAME_MAX - strlen(SHM_NAME_PREFIX) - 1 << " }" << std::endl;
            return false;
        }
        return true;
    }

    bool SetSessionNameSize(const char* session, size_t size) {
        if (!ValidateSessionName(session)) {
            return false;
        }

        shm_size_ = size;
        strcpy(shm_name_, SHM_NAME_PREFIX);
        strcat(shm_name_, session);
        return true;
    }

public:
    void* GetPtr() {
        return p_data_;
    }

#ifndef _WIN32
    SharedMemoryReturnStatus Create(const char* session, size_t size, bool force_recreate = false) {
        // create shared memory object
        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        handle_ = shm_open(shm_name_, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (handle_ == -1) {
            if (errno == EEXIST && force_recreate) {
                // Stale shared memory from abnormal termination; clean up and recreate
                shm_unlink(shm_name_);
                handle_ = shm_open(shm_name_, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            } else if (errno == EEXIST) {
                handle_ = shm_open(shm_name_, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (handle_ != -1) {
                    std::cerr << "[WARNING] Session " << session << " was not stopped before reusing" << std::endl;
                }
            }
            if (handle_ == -1) {
                std::cerr << "[ERROR] Failed to create shared memory for session " << session << " (" << GetErrorMessage(errno) << ")" << std::endl;
                return SHM_FAILED;
            }
        }

        // set size of shared memory
        if (ftruncate(handle_, size) == -1) {
            std::cerr << "[ERROR] Failed to set temporal control size" << std::endl;
            return SHM_FAILED;
        }

        // map shared memory
        p_data_ = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle_, 0);
        if (p_data_ == MAP_FAILED) {
            std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }

        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus AttachWrite(const char* session, size_t size) {
        if (p_data_ != nullptr) {
            if (open_mode == SHM_OPEN_WRITE) {
                std::cout << "[INFO] session " << session << " data already mapped" << std::endl;
                return SHM_SUCCESS;
            }

            std::cout << "[INFO] session " << session << " is already mapped in read-only mode" << std::endl;
            SoftRelease();
        }

        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        handle_ = shm_open(shm_name_, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (handle_ == -1) {
            std::cerr << "[WARNING] Session " << session << " is already stopped" << std::endl;
            return SHM_STOPPED;
        }

        // map shared memory
        p_data_ = mmap(0, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, handle_, 0);
        if (p_data_ == MAP_FAILED) {
            std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }
        open_mode = SHM_OPEN_WRITE;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus AttachRead(const char* session, size_t size) {
        if (p_data_ != nullptr) {
            if (open_mode == SHM_OPEN_READ) {
                std::cout << "[INFO] session " << session << " data already mapped" << std::endl;
                return SHM_SUCCESS;
            }

            std::cout << "[INFO] session " << session << " is already mapped in write mode" << std::endl;
            SoftRelease();
        }

        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        handle_ = shm_open(shm_name_, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (handle_ == -1) {
            std::cerr << "[WARNING] Session " << session << " is already stopped" << std::endl;
            return SHM_STOPPED;
        }

        // map shared memory
        p_data_ = mmap(0, shm_size_, PROT_READ, MAP_SHARED, handle_, 0);
        if (p_data_ == MAP_FAILED) {
            std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }
        open_mode = SHM_OPEN_READ;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus SoftRelease() {
        if (p_data_ != nullptr) {
            // unmap shared memory
            if (munmap((void*)p_data_, shm_size_) != 0) {
                std::cerr << "[ERROR] Failed to unmap shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
                return SHM_FAILED;
            }
            p_data_ = nullptr;

            if (close(handle_) != 0) {
                std::cerr << "[ERROR] Failed to close shared memory descriptor (" << GetErrorMessage(errno) << ")" << std::endl;
                return SHM_FAILED;
            }
        }

        open_mode = SHM_OPEN_NONE;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus Release() {
        if (p_data_ != nullptr) {
            if (SoftRelease() == SHM_FAILED) {
                return SHM_FAILED;
            }

            // unlink the shared memory (delete the memory)
            if (shm_unlink(shm_name_) != 0) {
                std::cerr << "[ERROR] Failed to unlink shared memory " << shm_name_ << " (" << GetErrorMessage(errno) << ")" << std::endl;
                return SHM_FAILED;
            }

            p_data_ = nullptr;
        }

        open_mode = SHM_OPEN_NONE;
        return SHM_SUCCESS;
    }

#else /* _WIN32 */

    SharedMemoryReturnStatus Create(const char* session, size_t size, bool force_recreate = false) {
        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        hMapFile_ = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(size),
            shm_name_
        );

        if (!hMapFile_) {
            std::cerr <<"[ERROR] Failed to create shared memory for session " << session << " (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS && !force_recreate) {
            std::cerr << "[WARNING] Session " << session << " was not stopped before reusing" << std::endl;
        }

        p_data_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, shm_size_);
        if (!p_data_) {
            CloseHandle(hMapFile_);
            hMapFile_ = nullptr;
            std::cerr << "[ERROR] Failed to map shared memory for session " << session << " (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }

        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus AttachWrite(const char* session, size_t size) {
        if (p_data_ != nullptr) {
            if (open_mode == SHM_OPEN_WRITE) {
                std::cout << "[INFO] session " << session << " data already mapped" << std::endl;
                return SHM_SUCCESS;
            }

            std::cout << "[INFO] session " << session << " is already mapped in read-only mode" << std::endl;
            UnmapViewOfFile(p_data_);
            p_data_ = MapViewOfFile(hMapFile_, FILE_MAP_WRITE, 0, 0, shm_size_);
            if (!p_data_) {
                std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
                CloseHandle(hMapFile_);
                return SHM_FAILED;
            }
            open_mode = SHM_OPEN_WRITE;
            return SHM_SUCCESS;
        }

        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        hMapFile_ = OpenFileMappingA(FILE_MAP_WRITE, FALSE, shm_name_);
        if (!hMapFile_) {
            std::cerr << "[ERROR] Session " << session << " does not exist or cannot be opened (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_FAILED;
        }

        p_data_ = MapViewOfFile(hMapFile_, FILE_MAP_WRITE, 0, 0, shm_size_);
        if (!p_data_) {
            std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
            CloseHandle(hMapFile_);
            return SHM_FAILED;
        }

        open_mode = SHM_OPEN_WRITE;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus AttachRead(const char* session, size_t size) {
        if (p_data_ != nullptr) {
            if (open_mode == SHM_OPEN_READ) {
                std::cout << "[INFO] session " << session << " data already mapped" << std::endl;
                return SHM_SUCCESS;
            }

            std::cout << "[INFO] session " << session << " is already mapped in write mode" << std::endl;
            UnmapViewOfFile(p_data_);
            p_data_ = MapViewOfFile(hMapFile_, FILE_MAP_READ, 0, 0, shm_size_);
            if (!p_data_) {
                std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
                CloseHandle(hMapFile_);
                return SHM_FAILED;
            }
            open_mode = SHM_OPEN_READ;
            return SHM_SUCCESS;
        }

        if (!SetSessionNameSize(session, size)) {
            return SHM_FAILED;
        }

        hMapFile_ = OpenFileMappingA(FILE_MAP_READ, FALSE, shm_name_);
        if (!hMapFile_) {
            std::cerr << "[ERROR] Session " << session << " does not exist or cannot be opened or stopped (" << GetErrorMessage(errno) << ")" << std::endl;
            return SHM_STOPPED;
        }

        p_data_ = MapViewOfFile(hMapFile_, FILE_MAP_READ, 0, 0, shm_size_);
        if (!p_data_) {
            std::cerr << "[ERROR] Failed to map shared memory (" << GetErrorMessage(errno) << ")" << std::endl;
            CloseHandle(hMapFile_);
            return SHM_FAILED;
        }

        open_mode = SHM_OPEN_READ;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus SoftRelease() {
        if (p_data_ != nullptr) {
            UnmapViewOfFile(p_data_);
            p_data_ = nullptr;

            if (hMapFile_) {
                if (!CloseHandle(hMapFile_)) {
                    std::cerr << "[ERROR] Failed to close shared memory handle (" << GetErrorMessage(errno) << ")" << std::endl;
                    return SHM_FAILED;
                }
            }
        }

        open_mode = SHM_OPEN_NONE;
        return SHM_SUCCESS;
    }

    SharedMemoryReturnStatus Release() {
        return SoftRelease();
    }
#endif /* _WIN32 */
};

#endif // PTI_TOOLS_UNITRACE_UTILS_SHARED_MEMORY_H_

