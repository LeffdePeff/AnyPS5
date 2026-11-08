#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

constexpr int MutexTypeRecursive = 2;
constexpr std::size_t MutexNameSize = 32;

void RequireMutex(PthreadMutex* mutex, int (APS5_VABI *operation)(PthreadMutex*), const char* what) {
    if (mutex == nullptr) throw std::invalid_argument(std::string(what) + ": null mutex");
    if (operation(mutex) != 0) throw std::runtime_error(std::string(what) + " failed");
}

int APS5_VABI InitializeUnnamed(PthreadMutex* mutex) {
    return _Mtxinit_nid_postfix(mutex, nullptr);
}

}

extern "C" {

int APS5_VABI _Mtxinit_nid_postfix(PthreadMutex* mutex, const char* name) {
    char mutexName[MutexNameSize];
    if (name != nullptr)
        std::snprintf(mutexName, sizeof(mutexName), "SceLibcI_%s", name);
    else
        std::snprintf(mutexName, sizeof(mutexName), "SceLibcI");
    PthreadMutexattr attr = nullptr;
    if (scePthreadMutexattrInit(&attr) != 0) return 1;
    if (scePthreadMutexattrSettype(&attr, MutexTypeRecursive) != 0) {
        scePthreadMutexattrDestroy(&attr);
        return 1;
    }
    const int initialized = scePthreadMutexInit(mutex, &attr, mutexName);
    const int released = scePthreadMutexattrDestroy(&attr);
    return initialized != 0 || released != 0;
}

int APS5_VABI _ZNSt8__sce_v236_Mtx_init_with_default_name_overrideEPP13pthread_mutexiPKc_nid_postfix(void** mutex, int type, const char* name) {
    return _ZNSt8__sce_v219_Mtx_init_with_nameEPP13pthread_mutexiPKc_nid_postfix(mutex, type, name);
}

int APS5_VABI _Mtxlock_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_lock_nid_postfix(mutex) != 0;
}

int APS5_VABI _Mtxunlock_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_unlock_nid_postfix(mutex) != 0;
}

int APS5_VABI _Mtxdst_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_destroy_nid_postfix(mutex) != 0;
}

void APS5_VABI _ZNSt6_MutexC1Ev_nid_postfix(PthreadMutex* self) {
    RequireMutex(self, InitializeUnnamed, "_Mutex::_Mutex: _Mtxinit");
}

void APS5_VABI sceLibcInternalMutexBaseConstructor_nid_no_patch(PthreadMutex* self) {
    RequireMutex(self, InitializeUnnamed, "_Mutex::_Mutex: _Mtxinit");
}
APS5_EXPORT("log6zy2C9iQ", sceLibcInternalMutexBaseConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalMutexCompleteDestructor_nid_no_patch(PthreadMutex* self) {
    RequireMutex(self, _Mtxdst_nid_postfix, "_Mutex::~_Mutex: _Mtxdst");
}
APS5_EXPORT("djHbPE+TFIo", sceLibcInternalMutexCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalMutexBaseDestructor_nid_no_patch(PthreadMutex* self) {
    RequireMutex(self, _Mtxdst_nid_postfix, "_Mutex::~_Mutex: _Mtxdst");
}
APS5_EXPORT("j7e7EQBD6ZA", sceLibcInternalMutexBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalMutexLock_nid_no_patch(PthreadMutex* self) {
    RequireMutex(self, _Mtxlock_nid_postfix, "_Mutex::_Lock: _Mtxlock");
}
APS5_EXPORT("U54NBtdj6UY", sceLibcInternalMutexLock_nid_no_patch);

void APS5_VABI sceLibcInternalMutexUnlock_nid_no_patch(PthreadMutex* self) {
    RequireMutex(self, _Mtxunlock_nid_postfix, "_Mutex::_Unlock: _Mtxunlock");
}
APS5_EXPORT("7OCTkL2oWyg", sceLibcInternalMutexUnlock_nid_no_patch);

}
