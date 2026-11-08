#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ios_base destructor: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
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

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct GuardedStream {
    std::uint64_t before = 0x12345678;
    GuestLocale::IosBase stream{};
    std::uint64_t after = 0x87654321;
};

std::int32_t Apply(StreamManipulator manipulator, std::int32_t flags) {
    GuardedStream object;
    object.stream.flags = flags;
    object.stream.width = 3;
    Require(manipulator.function(&object.stream, manipulator.argument) == &object.stream, "a manipulator returns its stream");
    Require(object.before == 0x12345678 && object.after == 0x87654321 && object.stream.width == 3, "a manipulator writes only the format flags");
    return object.stream.flags;
}

void CheckManipulators() {
    const StreamManipulator octal = _ZSt7setbasei_nid_postfix(8);
    Require(octal.argument == 8, "setbase keeps its argument");
    Require(Apply(octal, 0x4051) == 0x4021, "setbase(8) replaces the base field with oct");
    Require(Apply(_ZSt7setbasei_nid_postfix(10), 0x60) == 0x10, "setbase(10) selects dec");
    Require(Apply(_ZSt7setbasei_nid_postfix(16), 0x211) == 0x241, "setbase(16) selects hex");
    Require(Apply(_ZSt7setbasei_nid_postfix(2), 0x271) == 0x201, "another base clears the base field");
    Require(Apply(_ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0x10280), 0x1) == 0x281, "setiosflags sets the flags below 0x10000");
    Require(Apply(_ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0x10000), 0x10001) == 0x1, "setiosflags clears a masked flag above 0xffff");
    Require(Apply(_ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0x30), 0x10271) == 0x10241, "resetiosflags clears the flags");
    Require(Throws([] { _ZSt7setbasei_nid_postfix(8).function(nullptr, 8); }), "setbase rejects a null stream");
    Require(Throws([] { _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(1).function(nullptr, 1); }), "setiosflags rejects a null stream");
    Require(Throws([] { _ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(1).function(nullptr, 1); }), "resetiosflags rejects a null stream");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    GuardedStream object;
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(&object.stream);
    Require(object.stream.locale == nullptr, "the destructor releases the classic locale");
    Require(object.before == 0x12345678 && object.after == 0x87654321, "the destructor writes only the stream");

    GuardedStream expected;
    GuardedStream actual;
    expected.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    actual.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    expected.stream.width = actual.stream.width = 7;
    expected.stream.flags = actual.stream.flags = 0x201;
    _ZNSt8ios_baseD2Ev_nid_postfix(&expected.stream);
    sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(&actual.stream);
    Require(actual.stream.locale == expected.stream.locale && actual.stream.width == expected.stream.width && actual.stream.flags == expected.stream.flags,
        "D1 leaves the stream as D2 does");

    Require(Throws([] { sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(nullptr); }), "a null stream is rejected");

    auto* heapStream = new (_Znwm_nid_postfix(sizeof(GuestLocale::IosBase))) GuestLocale::IosBase{};
    heapStream->locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    releases = 0;
    sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(heapStream);
    Require(releases == 1 && lastReleased == heapStream, "the deleting destructor frees the stream through operator delete");
    Require(Throws([] { sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(nullptr); }), "a null stream is rejected by the deleting destructor");

    CheckManipulators();

    std::puts("libc internal ios_base destructor checks passed");
    return 0;
}
