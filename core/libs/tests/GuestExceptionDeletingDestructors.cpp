#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Message { std::size_t length; std::size_t capacity; std::atomic<std::ptrdiff_t> references; };

using Thrower = void (APS5_VABI *)(const void*);
using Deleter = void (APS5_VABI *)(LibcException::ExceptionObject*);
using Slot = void (APS5_VABI *)(void*);

extern "C" {
extern const TypeRecord _ZTISt11range_error_nid_postfix;
extern const TypeRecord _ZTISt12system_error_nid_postfix;
extern std::uintptr_t _ZTVSt12future_error_nid_postfix[6];

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

namespace {

constexpr std::size_t FilesystemErrorSize = 0x90;
constexpr std::size_t FilesystemErrorPath1 = 0x40;
constexpr std::size_t FilesystemErrorPath2 = 0x68;

unsigned releases = 0;
std::array<void*, 2> released{};
const void* caughtVtable = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest exception deleting destructors: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size ? size : 1); }
void APS5_VABI TestFree(void* pointer) {
    if (pointer == nullptr) return;
    if (releases < released.size()) released[releases] = pointer;
    ++releases;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

LibcException::ExceptionObject* NewObject(std::size_t size) {
    void* storage = _Znwm_nid_postfix(size);
    std::memset(storage, 0, size);
    return static_cast<LibcException::ExceptionObject*>(storage);
}

Slot DeleteSlot(const void* object) {
    return std::bit_cast<Slot>(static_cast<const void* const*>(static_cast<const LibcException::ExceptionObject*>(object)->vtable)[1]);
}

std::ptrdiff_t References(const LibcException::ExceptionObject& object) {
    return (reinterpret_cast<const Message*>(object.message) - 1)->references.load();
}

void CheckPlain() {
    static constexpr Deleter deleters[] {
        _ZNSt9exceptionD0Ev_nid_postfix, _ZNSt8bad_castD0Ev_nid_postfix, _ZNSt10bad_typeidD0Ev_nid_postfix,
        _ZNSt9bad_allocD0Ev_nid_postfix, _ZNSt20bad_array_new_lengthD0Ev_nid_postfix, _ZNSt17bad_function_callD0Ev_nid_postfix,
        _ZNSt12bad_weak_ptrD0Ev_nid_postfix, _ZNSt19bad_optional_accessD0Ev_nid_postfix};
    for (Deleter deleter : deleters) {
        auto* object = NewObject(sizeof(LibcException::ExceptionObject));
        _ZNSt9bad_allocC1Ev_nid_postfix(object);
        releases = 0;
        deleter(object);
        Require(releases == 1 && released[0] == object, "a plain deleting destructor releases the object through operator delete");
    }
    auto* object = NewObject(sizeof(LibcException::ExceptionObject));
    _ZNSt9bad_allocC1Ev_nid_postfix(object);
    releases = 0;
    DeleteSlot(object)(object);
    Require(releases == 1 && released[0] == object, "the plain vtable delete slot releases the object through operator delete");
}

void CheckMessage(Deleter deleter, std::size_t size) {
    auto* object = NewObject(size);
    _ZNSt13runtime_errorC1EPKc_nid_postfix(object, "deleted message");
    LibcException::ExceptionObject copy{};
    _ZNSt13runtime_errorC1ERKS__nid_postfix(&copy, object);
    Require(References(copy) == 1, "the copy shares the message");
    releases = 0;
    if (deleter) deleter(object);
    else DeleteSlot(object)(object);
    Require(releases == 1 && released[0] == object, "a message deleting destructor releases the object through operator delete");
    Require(References(copy) == 0 && std::strcmp(copy.message, "deleted message") == 0, "a message deleting destructor releases its message reference");
    _ZNSt13runtime_errorD1Ev_nid_postfix(&copy);
}

void CheckMessages() {
    struct Case { Deleter deleter; std::size_t size; };
    static constexpr Case cases[] {
        {_ZNSt11logic_errorD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt13runtime_errorD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt16invalid_argumentD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt12out_of_rangeD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt12domain_errorD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt12length_errorD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt12system_errorD0Ev_nid_postfix, sizeof(LibcException::SystemErrorObject)},
        {_ZNSt11regex_errorD0Ev_nid_postfix, sizeof(LibcException::SystemErrorObject)},
        {_ZNSt11range_errorD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)},
        {_ZNSt8ios_base7failureD0Ev_nid_postfix, sizeof(LibcException::SystemErrorObject)},
        {_ZNSt13__sce_cxxvers16_Error_exceptionD0Ev_nid_postfix, sizeof(LibcException::ExceptionObject)}};
    for (const Case& test : cases) CheckMessage(test.deleter, test.size);
    CheckMessage(nullptr, sizeof(LibcException::ExceptionObject));
}

void CheckFutureError() {
    auto* object = new (_Znwm_nid_postfix(sizeof(LibcException::FutureErrorObject))) LibcException::FutureErrorObject{};
    object->vtable = &_ZTVSt12future_error_nid_postfix[2];
    object->message.capacity = 24;
    object->message.size = 24;
    object->message.pointer = static_cast<char*>(_Znwm_nid_postfix(25));
    std::memcpy(object->message.pointer, "future already retrieved", 25);
    object->code = {0, 2, _ZSt15future_categoryv_nid_postfix()};
    char* storage = object->message.pointer;
    releases = 0;
    DeleteSlot(object)(object);
    Require(releases == 2 && released[0] == storage && released[1] == object, "the future_error delete slot releases the message and then the object through operator delete");
}

void APS5_VABI ThrowFilesystemError(const void*) {
    _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix("remove", {0, 2, _ZSt16generic_categoryv_nid_postfix()});
}

void CheckFilesystemError() {
    SceV2CatchType = &_ZTISt12system_error_nid_postfix;
    SceV2OtherType = &_ZTISt11range_error_nid_postfix;
    SceV2Selector = 0;
    caughtVtable = nullptr;
    SceV2Catch(ThrowFilesystemError, nullptr);
    Require(SceV2Selector == 2 && caughtVtable != nullptr, "filesystem_error is caught as system_error");
    auto* object = NewObject(FilesystemErrorSize);
    object->vtable = caughtVtable;
    auto* bytes = reinterpret_cast<unsigned char*>(object);
    auto* path1 = reinterpret_cast<GuestLocale::String*>(bytes + FilesystemErrorPath1);
    auto* path2 = reinterpret_cast<GuestLocale::String*>(bytes + FilesystemErrorPath2);
    path1->capacity = 31;
    path1->size = 20;
    path1->pointer = static_cast<char*>(GuestLocale::AllocateStringStorage(32));
    std::memcpy(path1->pointer, "/app0/deleted/path.1", 21);
    path2->capacity = GuestLocale::StringInlineCapacity;
    char* storage = path1->pointer;
    releases = 0;
    DeleteSlot(object)(object);
    Require(releases == 2 && released[0] == storage && released[1] == object, "the filesystem_error delete slot releases the path and then the object through operator delete");
}

}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    caughtVtable = static_cast<const LibcException::ExceptionObject*>(object)->vtable;
}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckPlain();
    CheckMessages();
    CheckFutureError();
    CheckFilesystemError();
    std::puts("guest exception deleting destructors: ok");
    return 0;
}
