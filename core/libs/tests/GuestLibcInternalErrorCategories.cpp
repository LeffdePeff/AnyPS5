#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using Message = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const LibcException::CategoryObject*, int);
using Condition = LibcException::ErrorCode (APS5_VABI *)(const LibcException::CategoryObject*, int);

namespace {

long live = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    ++live;
    return std::malloc(size);
}
void APS5_VABI TestFree(void* pointer) {
    if (pointer != nullptr) --live;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    ++live;
    return std::calloc(count, size);
}
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    ++live;
    return std::malloc(size);
}
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++live;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal error categories: %s\n", message);
    std::abort();
}

template<typename TFunction>
TFunction Slot(const LibcException::CategoryObject* category, std::size_t index) {
    return std::bit_cast<TFunction>(static_cast<const void* const*>(category->vtable)[index]);
}

const char* Data(const GuestLocale::String& string) {
    return string.capacity > GuestLocale::StringInlineCapacity ? string.pointer : string.buffer;
}

void Release(GuestLocale::String& string) {
    if (string.capacity > GuestLocale::StringInlineCapacity) _ZdlPv_nid_postfix(string.pointer);
}

void CheckSameMessage(Message actual, Message expected, const LibcException::CategoryObject* self, int value, const char* text, const char* label) {
    GuestLocale::String reference{};
    Require(expected(&reference, self, value) == &reference, label);
    if (text != nullptr) Require(std::strcmp(Data(reference), text) == 0, label);
    constexpr std::uint64_t marker = 0xA55AA55AA55AA55Aull;
    struct Guarded { std::uint64_t before; GuestLocale::String result; std::uint64_t after; };
    Guarded guarded{marker, {}, marker};
    std::memset(&guarded.result, 0xcd, sizeof(guarded.result));
    *__error_nid_postfix() = 61;
    Require(actual(&guarded.result, self, value) == &guarded.result, label);
    Require(*__error_nid_postfix() == 61, label);
    Require(guarded.before == marker && guarded.after == marker, label);
    Require(guarded.result.reserved == reference.reserved && guarded.result.size == reference.size && guarded.result.capacity == reference.capacity, label);
    Require(std::memcmp(Data(guarded.result), Data(reference), reference.size + 1) == 0, label);
    Release(reference);
    Release(guarded.result);
}

void CheckSystemCategory() {
    const LibcException::CategoryObject* system = _ZSt15system_categoryv_nid_postfix();
    const LibcException::CategoryObject* generic = _ZSt16generic_categoryv_nid_postfix();
    const auto message = Slot<Message>(system, 3);
    const auto condition = Slot<Condition>(system, 4);
    for (int value = -64; value <= 256; ++value) {
        CheckSameMessage(&sceLibcInternalSystemErrorCategoryMessage_nid_no_patch, message, system, value, nullptr, "_System_error_category::message matches the system category");
        const LibcException::ErrorCode expected = condition(system, value);
        const LibcException::ErrorCode actual = sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch(system, value);
        Require(actual.reserved == expected.reserved && actual.value == expected.value && actual.category == expected.category, "_System_error_category::default_error_condition matches the system category");
    }
    CheckSameMessage(&sceLibcInternalSystemErrorCategoryMessage_nid_no_patch, message, system, 2, "no such file or directory", "_System_error_category::message names a known error");
    CheckSameMessage(&sceLibcInternalSystemErrorCategoryMessage_nid_no_patch, message, system, INT_MIN, "unknown error", "_System_error_category::message falls back to unknown error");
    const LibcException::ErrorCode known = sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch(system, 2);
    Require(known.value == 2 && known.category == generic, "a known system error maps to the generic category");
    const LibcException::ErrorCode unknown = sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch(system, 44);
    Require(unknown.value == 44 && unknown.category == system, "an unknown system error keeps the queried category");
    const LibcException::CategoryObject other{system->vtable};
    Require(sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch(&other, 44).category == &other, "default_error_condition names the category it was called on");
}

void CheckIostreamCategory() {
    const LibcException::CategoryObject* generic = _ZSt16generic_categoryv_nid_postfix();
    const auto genericMessage = Slot<Message>(generic, 3);
    for (int value = -64; value <= 256; ++value) {
        if (value == 1) continue;
        CheckSameMessage(&sceLibcInternalIostreamErrorCategoryMessage_nid_no_patch, genericMessage, generic, value, nullptr, "_Iostream_error_category::message uses the generic message outside io_errc::stream");
    }
    constexpr std::uint64_t marker = 0xA55AA55AA55AA55Aull;
    struct Guarded { std::uint64_t before; GuestLocale::String result; std::uint64_t after; };
    Guarded guarded{marker, {}, marker};
    std::memset(&guarded.result, 0xcd, sizeof(guarded.result));
    Require(sceLibcInternalIostreamErrorCategoryMessage_nid_no_patch(&guarded.result, generic, 1) == &guarded.result, "_Iostream_error_category::message returns its result");
    Require(guarded.before == marker && guarded.after == marker, "_Iostream_error_category::message writes only its result");
    Require(guarded.result.reserved == 0 && guarded.result.size == 21 && guarded.result.capacity == 31, "io_errc::stream builds a heap string");
    Require(std::strcmp(Data(guarded.result), "iostream stream error") == 0, "io_errc::stream is iostream stream error");
    Release(guarded.result);

    LibcException::CategoryObject object{generic->vtable};
    sceLibcInternalIostreamErrorCategoryCompleteDestructor_nid_no_patch(&object);
    Require(object.vtable == generic->vtable, "_Iostream_error_category D1 leaves the object");
}

void CheckFutureErrorMap() {
    struct Expected { int value; const char* text; };
    static constexpr Expected names[] {{1, "broken promise"}, {2, "future already retrieved"}, {3, "promise already satisfied"}, {4, "no state"}};
    for (const auto& name : names) {
        const char* text = sceLibcInternalFutureErrorMap_nid_no_patch(name.value);
        Require(text != nullptr && std::strcmp(text, name.text) == 0, "_Future_error_map names future_errc values");
        Require(sceLibcInternalFutureErrorMap_nid_no_patch(name.value) == text, "_Future_error_map returns static strings");
    }
    for (int value : {INT_MIN, -1, 0, 5, 22, INT_MAX}) Require(sceLibcInternalFutureErrorMap_nid_no_patch(value) == nullptr, "_Future_error_map returns null outside future_errc");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const long before = live;
    CheckSystemCategory();
    CheckIostreamCategory();
    CheckFutureErrorMap();
    Require(live == before, "every message string is released");
    std::puts("libc internal error categories: ok");
    return 0;
}
