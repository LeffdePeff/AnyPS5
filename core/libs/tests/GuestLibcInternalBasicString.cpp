#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);
using GuestWhat = const char* (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt12out_of_range_nid_postfix;
extern const TypeRecord _ZTISt12length_error_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

namespace {

char caught[128];
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

}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* vtable = *static_cast<const void* const* const*>(object);
    const auto what = reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]));
    std::snprintf(caught, sizeof(caught), "%s|%s", static_cast<const TypeRecord*>(vtable[-1])->name, what(object));
}

namespace {

using GuestLocale::String;

constexpr std::size_t NoPosition = std::numeric_limits<std::size_t>::max();
constexpr std::uint64_t Allocator = 0x5a5a5a5a5a5a5a5a;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal basic_string: %s\n", message);
    std::abort();
}

String Empty() {
    String string{};
    string.reserved = Allocator;
    string.capacity = GuestLocale::StringInlineCapacity;
    return string;
}

const char* Data(const String& string) {
    return string.capacity > GuestLocale::StringInlineCapacity ? string.pointer : string.buffer;
}

bool Holds(const String& string, const char* text) {
    const std::size_t length = std::strlen(text);
    return string.reserved == Allocator && string.size == length && std::memcmp(Data(string), text, length) == 0 && Data(string)[length] == 0;
}

String Make(const char* text) {
    String string = Empty();
    _ZNSs6assignEPKcm_nid_postfix(&string, text, std::strlen(text));
    return string;
}

void APS5_VABI Invoke(const void* call) {
    (*static_cast<const std::function<void()>*>(call))();
}

void RequireThrow(const std::function<void()>& call, int selector, const char* expected, const char* message) {
    SceV2CatchType = &_ZTISt12out_of_range_nid_postfix;
    SceV2OtherType = &_ZTISt12length_error_nid_postfix;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(Invoke, &call);
    Require(SceV2Selector == selector && std::strcmp(caught, expected) == 0, message);
}

void RequireOutOfRange(const std::function<void()>& call, const char* message) {
    RequireThrow(call, 2, "St12out_of_range|invalid string position", message);
}

void RequireLengthError(const std::function<void()>& call, const char* message) {
    RequireThrow(call, 1, "St12length_error|string too long", message);
}

void CheckReporters() {
    String string = Empty();
    RequireOutOfRange([&] { _ZNKSs5_XranEv_nid_postfix(&string); }, "_Xran throws out_of_range");
    RequireLengthError([&] { _ZNKSs5_XlenEv_nid_postfix(&string); }, "_Xlen throws length_error");
    RequireOutOfRange([&] { _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix(&string); }, "wstring _Xran throws out_of_range");
    RequireLengthError([&] { _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix(&string); }, "wstring _Xlen throws length_error");
}

void CheckAssignCharacters() {
    String string = Empty();
    Require(_ZNSs6assignEmc_nid_postfix(&string, 3, 'x') == &string, "assign(n, c) returns *this");
    Require(Holds(string, "xxx") && string.capacity == 15, "assign(n, c) stays in the inline buffer");
    _ZNSs6assignEmc_nid_postfix(&string, 20, 'y');
    Require(Holds(string, std::string(20, 'y').c_str()) && string.capacity == 31, "assign(n, c) grows to size | 15");
    _ZNSs6assignEmc_nid_postfix(&string, 0, 'z');
    Require(Holds(string, "") && string.capacity == 31, "assign(0, c) keeps the storage");
    RequireLengthError([&] { _ZNSs6assignEmc_nid_postfix(&string, NoPosition, 'a'); }, "assign(npos, c) throws length_error");
    Require(Holds(string, ""), "a failed assign leaves the string unchanged");
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAssignPointer() {
    String string = Empty();
    const char text[] = "hello\0world";
    Require(_ZNSs6assignEPKcm_nid_postfix(&string, text, 11) == &string, "assign(s, n) returns *this");
    Require(string.size == 11 && std::memcmp(string.buffer, text, 12) == 0, "assign(s, n) copies embedded nulls");
    _ZNSs6assignEPKcm_nid_postfix(&string, "abcdefghijklmnopqrstuvwxyz", 26);
    Require(Holds(string, "abcdefghijklmnopqrstuvwxyz") && string.capacity == 31, "assign(s, n) moves to the heap");
    char* storage = string.pointer;
    _ZNSs6assignEPKcm_nid_postfix(&string, string.pointer + 3, 4);
    Require(Holds(string, "defg") && string.pointer == storage && string.capacity == 31, "assign(s, n) from inside *this reuses the storage");
    _ZNSs6assignEPKcm_nid_postfix(&string, string.pointer, 2);
    Require(Holds(string, "de"), "assign(s, n) from the start of *this truncates");
    GuestLocale::ReleaseStringStorage(string);

    String terminator = Make("abc");
    _ZNSs6assignEPKcm_nid_postfix(&terminator, Data(terminator) + 1, 3);
    Require(terminator.size == 3 && std::memcmp(Data(terminator), "bc\0", 3) == 0 && Data(terminator)[3] == 0,
        "assign(s, n) preserves the initialized terminator in an overlapping range");
    GuestLocale::ReleaseStringStorage(terminator);

    String inlineGrowth = Make("abcdefghijklmno");
    _ZNSs6assignEPKcm_nid_postfix(&inlineGrowth, Data(inlineGrowth), 16);
    Require(inlineGrowth.size == 16 && inlineGrowth.capacity == 31
        && std::memcmp(Data(inlineGrowth), "abcdefghijklmno\0", 16) == 0 && Data(inlineGrowth)[16] == 0,
        "assign(s, n) copies an overlapping inline range before growing");
    GuestLocale::ReleaseStringStorage(inlineGrowth);

    String heapOverlap = Make("abcdefghijklmnopqrs");
    heapOverlap.pointer[20] = 'x';
    _ZNSs6assignEPKcm_nid_postfix(&heapOverlap, heapOverlap.pointer + 1, 20);
    Require(heapOverlap.size == 20 && std::memcmp(Data(heapOverlap), "bcdefghijklmnopqrs\0x", 20) == 0
        && Data(heapOverlap)[20] == 0,
        "assign(s, n) moves an overlapping heap range beyond the logical terminator");
    GuestLocale::ReleaseStringStorage(heapOverlap);
}

void CheckAssignSubstring() {
    String source = Make("0123456789abcdefghij");
    String string = Make("old");
    Require(_ZNSs6assignERKSsmm_nid_postfix(&string, &source, 4, 3) == &string, "assign(str, pos, n) returns *this");
    Require(Holds(string, "456"), "assign(str, pos, n) copies the substring");
    _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 2, NoPosition);
    Require(Holds(string, "23456789abcdefghij") && string.capacity == 31, "assign(str, pos, npos) copies to the end");
    _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 20, 5);
    Require(Holds(string, ""), "assign(str, size(), n) is empty");
    RequireOutOfRange([&] { _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 21, 1); }, "assign(str, pos > size(), n) throws out_of_range");
    _ZNSs6assignERKSsmm_nid_postfix(&source, &source, 5, 6);
    Require(Holds(source, "56789a") && source.capacity == 31, "assign(*this, pos, n) keeps the storage");
    _ZNSs6assignERKSsmm_nid_postfix(&source, &source, 0, NoPosition);
    Require(Holds(source, "56789a"), "assign(*this, 0, npos) is unchanged");
    GuestLocale::ReleaseStringStorage(source);
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAppendSubstring() {
    String source = Make("ABCDEFGH");
    String string = Make("ab");
    Require(_ZNSs6appendERKSsmm_nid_postfix(&string, &source, 1, 3) == &string, "append(str, pos, n) returns *this");
    Require(Holds(string, "abBCD") && string.capacity == 15, "append(str, pos, n) appends the substring");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &source, 6, NoPosition);
    Require(Holds(string, "abBCDGH"), "append(str, pos, npos) appends to the end");
    RequireOutOfRange([&] { _ZNSs6appendERKSsmm_nid_postfix(&string, &source, 9, 0); }, "append(str, pos > size(), n) throws out_of_range");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &string, 0, NoPosition);
    Require(Holds(string, "abBCDGHabBCDGH") && string.capacity == 15, "append(*this) within the inline buffer");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &string, 2, 5);
    Require(Holds(string, "abBCDGHabBCDGHBCDGH") && string.capacity == 31, "append(*this) across a reallocation");
    GuestLocale::ReleaseStringStorage(source);
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAppendCharacters() {
    String string = Make("go");
    Require(_ZNSs6appendEmc_nid_postfix(&string, 3, 'o') == &string, "append(n, c) returns *this");
    Require(Holds(string, "goooo"), "append(n, c) appends the characters");
    _ZNSs6appendEmc_nid_postfix(&string, 0, 'x');
    Require(Holds(string, "goooo"), "append(0, c) is unchanged");
    RequireLengthError([&] { _ZNSs6appendEmc_nid_postfix(&string, NoPosition - 5, 'x'); }, "append past max_size() throws length_error");
    _ZNSs6appendEmc_nid_postfix(&string, 5000, '!');
    Require(string.size == 5005 && string.capacity == (5005 | 15) && string.reserved == Allocator, "append(n, c) grows a large allocation");
    Require((reinterpret_cast<std::uintptr_t>(string.pointer) & 0x1f) == 0, "large storage is 32-byte aligned");
    Require(std::memcmp(string.pointer, "goooo!", 6) == 0 && string.pointer[5004] == '!' && string.pointer[5005] == 0, "append(n, c) keeps the prefix and terminates");
    GuestLocale::ReleaseStringStorage(string);
}

void CheckNarrowInsert() {
    String string = Make("abcdef");
    Require(_ZNSs6insertEmmc_nid_postfix(&string, 2, 3, '-') == &string, "insert(pos, n, c) returns *this");
    Require(Holds(string, "ab---cdef") && string.capacity == 15, "insert(pos, n, c) inserts in the inline buffer");
    _ZNSs6insertEmmc_nid_postfix(&string, string.size, 2, '!');
    Require(Holds(string, "ab---cdef!!"), "insert(size(), n, c) appends");
    _ZNSs6insertEmmc_nid_postfix(&string, 0, 10, '<');
    Require(Holds(string, "<<<<<<<<<<ab---cdef!!") && string.capacity == 31, "insert(0, n, c) grows to size | 15");
    _ZNSs6insertEmmc_nid_postfix(&string, 4, 0, '?');
    Require(Holds(string, "<<<<<<<<<<ab---cdef!!"), "insert(pos, 0, c) is unchanged");
    RequireOutOfRange([&] { _ZNSs6insertEmmc_nid_postfix(&string, string.size + 1, 1, 'x'); }, "insert(pos > size(), n, c) throws out_of_range");
    RequireLengthError([&] { _ZNSs6insertEmmc_nid_postfix(&string, 0, NoPosition - 5, 'x'); }, "insert past max_size() throws length_error");
    Require(Holds(string, "<<<<<<<<<<ab---cdef!!"), "a failed insert leaves the string unchanged");
    GuestLocale::ReleaseStringStorage(string);
}

template<typename TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckNarrowCopy() {
    String string = Make("abcdef");
    _ZNSs5_CopyEmm_nid_postfix(&string, 40, 6);
    Require(Holds(string, "abcdef") && string.capacity == 47, "_Copy(n, size()) grows to n | 15 and keeps the text");
    _ZNSs5_CopyEmm_nid_postfix(&string, 20, 4);
    Require(Holds(string, "abcd") && string.capacity == 31, "_Copy(n, length) reallocates to n | 15 and keeps length characters");
    _ZNSs5_CopyEmm_nid_postfix(&string, 10, 3);
    Require(Holds(string, "abc") && string.capacity == 15, "_Copy to an inline size moves back into the buffer");
    _ZNSs5_CopyEmm_nid_postfix(&string, 16, 3);
    Require(Holds(string, "abc") && string.capacity == 31, "_Copy past the inline buffer moves to the heap");
    Require(ThrowsInvalidArgument([&] { _ZNSs5_CopyEmm_nid_postfix(&string, 40, 4); }) && Holds(string, "abc"), "_Copy keeping more than size() characters throws");
    Require(ThrowsInvalidArgument([&] { _ZNSs5_CopyEmm_nid_postfix(&string, 2, 3); }) && Holds(string, "abc"), "_Copy keeping more than n characters throws");
    RequireLengthError([&] { _ZNSs5_CopyEmm_nid_postfix(&string, NoPosition, 3); }, "_Copy past max_size() throws length_error");
    GuestLocale::ReleaseStringStorage(string);
}

constexpr std::size_t WideInlineCapacity = 7;
constexpr std::size_t WideNoPosition = NoPosition / sizeof(char16_t);

String WideEmpty() {
    String string{};
    string.reserved = Allocator;
    string.capacity = WideInlineCapacity;
    return string;
}

const char16_t* WideData(const String& string) {
    return reinterpret_cast<const char16_t*>(string.capacity > WideInlineCapacity ? string.pointer : string.buffer);
}

bool WideHolds(const String& string, std::u16string_view text) {
    return string.reserved == Allocator && string.size == text.size()
        && std::memcmp(WideData(string), text.data(), text.size() * sizeof(char16_t)) == 0 && WideData(string)[text.size()] == 0;
}

String WideMake(std::u16string_view text) {
    String string = WideEmpty();
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&string, text.data(), text.size());
    return string;
}

void WideRelease(String& string) {
    if (string.capacity > WideInlineCapacity) GuestStringStorage::Deallocate(string.pointer, (string.capacity + 1) * sizeof(char16_t));
    string = WideEmpty();
}

void CheckWideAssign() {
    String string = WideEmpty();
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&string, u"abc", 3) == &string, "wstring assign(s, n) returns *this");
    Require(WideHolds(string, u"abc") && string.capacity == 7, "wstring assign(s, n) stays in the 7-character buffer");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&string, u"abcdefg", 7);
    Require(WideHolds(string, u"abcdefg") && string.capacity == 7, "a 7-character wstring fits the buffer");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&string, u"abcdefgh", 8);
    Require(WideHolds(string, u"abcdefgh") && string.capacity == 15, "an 8-character wstring moves to the heap with capacity size | 7");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&string, WideData(string) + 2, 3);
    Require(WideHolds(string, u"cde") && string.capacity == 15, "wstring assign(s, n) from inside *this reuses the storage");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(&string, 20, u'y');
    Require(WideHolds(string, std::u16string(20, u'y')) && string.capacity == 23, "wstring assign(n, c) grows to n | 7");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(&string, 0, u'z');
    Require(WideHolds(string, u"") && string.capacity == 23, "wstring assign(0, c) keeps the storage");
    RequireLengthError([&] { _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(&string, WideNoPosition, u'a'); }, "wstring assign past max_size() throws length_error");
    WideRelease(string);

    String source = WideMake(u"0123456789abcdefghij");
    String target = WideMake(u"old");
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&target, &source, 4, 3) == &target && WideHolds(target, u"456"), "wstring assign(str, pos, n) copies the substring");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&target, &source, 2, NoPosition);
    Require(WideHolds(target, u"23456789abcdefghij") && target.capacity == 23, "wstring assign(str, pos, npos) copies to the end");
    RequireOutOfRange([&] { _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&target, &source, 21, 1); }, "wstring assign(str, pos > size(), n) throws out_of_range");
    _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&source, &source, 5, 6);
    Require(WideHolds(source, u"56789a") && source.capacity == 23, "wstring assign(*this, pos, n) keeps the storage");
    WideRelease(source);
    WideRelease(target);
}

void CheckWideAppend() {
    String source = WideMake(u"ABCDEFGH");
    String string = WideMake(u"ab");
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&string, &source, 1, 3) == &string && WideHolds(string, u"abBCD") && string.capacity == 7, "wstring append(str, pos, n) appends in the buffer");
    _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&string, &source, 6, NoPosition);
    Require(WideHolds(string, u"abBCDGH") && string.capacity == 7, "wstring append(str, pos, npos) fills the buffer");
    _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&string, &string, 0, NoPosition);
    Require(WideHolds(string, u"abBCDGHabBCDGH") && string.capacity == 15, "wstring append(*this) across a reallocation");
    RequireOutOfRange([&] { _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&string, &source, 9, 0); }, "wstring append(str, pos > size(), n) throws out_of_range");
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&string, 3, u'!') == &string && WideHolds(string, u"abBCDGHabBCDGH!!!") && string.capacity == 23, "wstring append(n, c) grows to size | 7");
    RequireLengthError([&] { _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&string, WideNoPosition - 5, u'x'); }, "wstring append past max_size() throws length_error");
    _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&string, 3000, u'#');
    Require(string.size == 3017 && string.capacity == (3017 | 7) && string.reserved == Allocator, "wstring append(n, c) grows a large allocation");
    Require((reinterpret_cast<std::uintptr_t>(string.pointer) & 0x1f) == 0, "large wide storage is 32-byte aligned");
    Require(WideData(string)[16] == u'!' && WideData(string)[17] == u'#' && WideData(string)[3016] == u'#' && WideData(string)[3017] == 0, "wstring append(n, c) keeps the prefix and terminates");
    WideRelease(source);
    WideRelease(string);
}

void CheckWideEraseInsert() {
    String string = WideMake(u"0123456789abcdefghij");
    Require(_ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(&string, 2, 3) == &string && WideHolds(string, u"0156789abcdefghij") && string.capacity == 23, "wstring erase(pos, n) removes the range");
    _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(&string, 10, NoPosition);
    Require(WideHolds(string, u"0156789abc") && string.capacity == 23, "wstring erase(pos, npos) truncates");
    RequireOutOfRange([&] { _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(&string, 11, 0); }, "wstring erase(pos > size(), n) throws out_of_range");
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&string, 2, 3, u'-') == &string && WideHolds(string, u"01---56789abc"), "wstring insert(pos, n, c) inserts");
    _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&string, string.size, 2, u'!');
    Require(WideHolds(string, u"01---56789abc!!") && string.capacity == 23, "wstring insert(size(), n, c) appends");
    _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&string, 0, 10, u'<');
    Require(WideHolds(string, u"<<<<<<<<<<01---56789abc!!") && string.capacity == 31, "wstring insert(0, n, c) grows to size | 7");
    RequireOutOfRange([&] { _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&string, string.size + 1, 1, u'x'); }, "wstring insert(pos > size(), n, c) throws out_of_range");
    RequireLengthError([&] { _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&string, 0, WideNoPosition, u'x'); }, "wstring insert past max_size() throws length_error");
    WideRelease(string);
}

void CheckWideCopy() {
    String string = WideMake(u"abcdef");
    _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(&string, 40, 6);
    Require(WideHolds(string, u"abcdef") && string.capacity == 47, "wstring _Copy(n, size()) grows to n | 7");
    _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(&string, 20, 4);
    Require(WideHolds(string, u"abcd") && string.capacity == 23, "wstring _Copy(n, length) keeps length characters");
    _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(&string, 5, 3);
    Require(WideHolds(string, u"abc") && string.capacity == 7, "wstring _Copy to an inline size moves back into the buffer");
    Require(ThrowsInvalidArgument([&] { _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(&string, 40, 4); }) && WideHolds(string, u"abc"), "wstring _Copy keeping more than size() characters throws");
    RequireLengthError([&] { _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(&string, WideNoPosition, 3); }, "wstring _Copy past max_size() throws length_error");
    WideRelease(string);
}

void CheckErase() {
    String string = Make("0123456789abcdefghij");
    Require(_ZNSs5eraseEmm_nid_postfix(&string, 2, 3) == &string, "erase returns *this");
    Require(Holds(string, "0156789abcdefghij") && string.capacity == 31, "erase(pos, n) removes the range");
    _ZNSs5eraseEmm_nid_postfix(&string, 10, NoPosition);
    Require(Holds(string, "0156789abc"), "erase(pos, npos) truncates");
    _ZNSs5eraseEmm_nid_postfix(&string, 10, 4);
    Require(Holds(string, "0156789abc"), "erase(size(), n) is unchanged");
    RequireOutOfRange([&] { _ZNSs5eraseEmm_nid_postfix(&string, 11, 0); }, "erase(pos > size(), n) throws out_of_range");
    _ZNSs5eraseEmm_nid_postfix(&string, 0, NoPosition);
    Require(Holds(string, "") && string.capacity == 31, "erase(0, npos) keeps the storage");
    GuestLocale::ReleaseStringStorage(string);
    String small = Make("abc");
    _ZNSs5eraseEmm_nid_postfix(&small, 0, 1);
    Require(Holds(small, "bc") && small.capacity == 15, "erase works on the inline buffer");
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
    CheckReporters();
    CheckAssignCharacters();
    CheckAssignPointer();
    CheckAssignSubstring();
    CheckAppendSubstring();
    CheckAppendCharacters();
    CheckErase();
    CheckNarrowInsert();
    CheckNarrowCopy();
    CheckWideAssign();
    CheckWideAppend();
    CheckWideEraseInsert();
    CheckWideCopy();
    Require(live == before, "every string allocation is released");
    std::puts("libc internal basic_string: ok");
    return 0;
}
