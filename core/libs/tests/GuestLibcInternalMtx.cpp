#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal mtx: %s\n", message);
    std::abort();
}

template<typename TCall>
int OnOtherThread(TCall call, PthreadMutex* mutex) {
    int result = -1;
    std::thread other([&] { result = call(mutex); });
    other.join();
    return result;
}

int TryLock(PthreadMutex* mutex) {
    const int result = pthread_mutex_trylock_nid_postfix(mutex);
    if (result == 0) pthread_mutex_unlock_nid_postfix(mutex);
    return result;
}

int LockAndUnlock(PthreadMutex* mutex) {
    if (_Mtxlock_nid_postfix(mutex) != 0) return 1;
    return _Mtxunlock_nid_postfix(mutex);
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void CheckMutexObject() {
    PthreadMutex complete = nullptr;
    _ZNSt6_MutexC1Ev_nid_postfix(&complete);
    Require(complete != nullptr, "_Mutex::_Mutex (C1) creates a mutex");
    sceLibcInternalMutexLock_nid_no_patch(&complete);
    sceLibcInternalMutexLock_nid_no_patch(&complete);
    Require(OnOtherThread(TryLock, &complete) != 0, "_Mutex::_Lock holds the mutex and is recursive");
    sceLibcInternalMutexUnlock_nid_no_patch(&complete);
    Require(OnOtherThread(TryLock, &complete) != 0, "_Mutex stays held until every _Lock is matched");
    sceLibcInternalMutexUnlock_nid_no_patch(&complete);
    Require(OnOtherThread(LockAndUnlock, &complete) == 0, "_Mutex::_Unlock releases the mutex");
    Require(Throws([&] { sceLibcInternalMutexUnlock_nid_no_patch(&complete); }), "_Mutex::_Unlock of a released mutex throws");
    sceLibcInternalMutexCompleteDestructor_nid_no_patch(&complete);
    Require(Throws([&] { sceLibcInternalMutexLock_nid_no_patch(&complete); }), "_Mutex::_Lock after ~_Mutex (D1) throws");

    PthreadMutex base = nullptr;
    sceLibcInternalMutexBaseConstructor_nid_no_patch(&base);
    Require(base != nullptr, "_Mutex::_Mutex (C2) creates a mutex");
    sceLibcInternalMutexLock_nid_no_patch(&base);
    Require(OnOtherThread(TryLock, &base) != 0, "_Mutex::_Lock on a C2 mutex holds it");
    sceLibcInternalMutexUnlock_nid_no_patch(&base);
    Require(OnOtherThread(LockAndUnlock, &base) == 0, "a C2 mutex is released by _Unlock");
    sceLibcInternalMutexBaseDestructor_nid_no_patch(&base);
    Require(Throws([&] { sceLibcInternalMutexBaseDestructor_nid_no_patch(&base); }), "~_Mutex (D2) of a destroyed mutex throws");

    Require(Throws([] { _ZNSt6_MutexC1Ev_nid_postfix(nullptr); }), "_Mutex::_Mutex on a null object throws");
    Require(Throws([] { sceLibcInternalMutexLock_nid_no_patch(nullptr); }), "_Mutex::_Lock on a null object throws");
}

}

int main() {
    PthreadMutex mutex = nullptr;
    Require(_Mtxinit_nid_postfix(&mutex, "File") == 0, "init with a name succeeds");
    Require(mutex != nullptr, "init stores a mutex");
    Require(_Mtxlock_nid_postfix(&mutex) == 0, "first lock succeeds");
    Require(_Mtxlock_nid_postfix(&mutex) == 0, "the mutex is recursive");
    Require(OnOtherThread(TryLock, &mutex) != 0, "another thread cannot take a held mutex");
    Require(OnOtherThread(_Mtxunlock_nid_postfix, &mutex) == 1, "another thread cannot unlock it");
    Require(_Mtxunlock_nid_postfix(&mutex) == 0, "first unlock succeeds");
    Require(OnOtherThread(TryLock, &mutex) != 0, "the mutex stays held until every lock is released");
    Require(_Mtxunlock_nid_postfix(&mutex) == 0, "second unlock succeeds");
    Require(_Mtxunlock_nid_postfix(&mutex) == 1, "unlocking a released mutex fails");
    Require(OnOtherThread(LockAndUnlock, &mutex) == 0, "another thread takes the released mutex");
    Require(_Mtxdst_nid_postfix(&mutex) == 0, "destroy succeeds");
    Require(_Mtxdst_nid_postfix(&mutex) == 1, "destroying twice fails");
    Require(_Mtxlock_nid_postfix(&mutex) == 1, "locking a destroyed mutex fails");

    PthreadMutex unnamed = nullptr;
    Require(_Mtxinit_nid_postfix(&unnamed, nullptr) == 0, "init without a name succeeds");
    Require(LockAndUnlock(&unnamed) == 0, "an unnamed mutex locks");
    Require(_Mtxdst_nid_postfix(&unnamed) == 0, "an unnamed mutex is destroyed");

    PthreadMutex longName = nullptr;
    Require(_Mtxinit_nid_postfix(&longName, "ANameLongerThanTheThirtyTwoByteLabel") == 0, "a long name is truncated");
    Require(_Mtxdst_nid_postfix(&longName) == 0, "a mutex with a long name is destroyed");

    PthreadMutex zeroed = nullptr;
    Require(_Mtxunlock_nid_postfix(&zeroed) == 1, "unlocking a zeroed mutex fails");
    Require(LockAndUnlock(&zeroed) == 0, "a zeroed mutex is initialized on first lock");
    Require(_Mtxdst_nid_postfix(&zeroed) == 0, "an initialized zeroed mutex is destroyed");

    PthreadMutex untouched = nullptr;
    Require(_Mtxdst_nid_postfix(&untouched) == 0, "destroying a zeroed mutex succeeds");

    constexpr int Plain = 0x01;
    constexpr int Recursive = 0x100;
    constexpr int Busy = 3;
    PthreadMutex overridden = nullptr;
    Require(_ZNSt8__sce_v236_Mtx_init_with_default_name_overrideEPP13pthread_mutexiPKc_nid_postfix(reinterpret_cast<void**>(&overridden), Plain | Recursive, "Override") == 0, "a named override init succeeds");
    Require(overridden != nullptr, "a named override init stores a mutex");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&overridden)) == 0, "the override mutex locks");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&overridden)) == 0, "a recursive override mutex locks again");
    Require(OnOtherThread(TryLock, &overridden) != 0, "another thread cannot take a held override mutex");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&overridden)) == 0, "the override mutex unlocks once");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&overridden)) == 0, "the override mutex unlocks twice");
    Require(_ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&overridden)) == 0, "the override mutex is destroyed");

    PthreadMutex plainOverride = nullptr;
    Require(_ZNSt8__sce_v236_Mtx_init_with_default_name_overrideEPP13pthread_mutexiPKc_nid_postfix(reinterpret_cast<void**>(&plainOverride), Plain, nullptr) == 0, "an override init without a name succeeds");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&plainOverride)) == 0, "the plain override mutex locks");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&plainOverride)) == Busy, "a plain override mutex refuses a second lock");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&plainOverride)) == 0, "the plain override mutex unlocks");
    Require(_ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&plainOverride)) == 0, "the plain override mutex is destroyed");

    CheckMutexObject();
    return 0;
}
