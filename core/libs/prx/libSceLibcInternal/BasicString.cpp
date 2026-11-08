#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using GuestLocale::String;

constexpr std::size_t NoPosition = std::numeric_limits<std::size_t>::max();

struct Narrow {
    using Char = char;
    static constexpr std::size_t InlineCapacity = GuestLocale::StringInlineCapacity;
    static constexpr std::size_t CapacityMask = GuestLocale::StringCapacityMask;
    static constexpr std::size_t MaxSize = NoPosition - 1;
    [[noreturn]] static void LengthError(const String* string) { _ZNKSs5_XlenEv_nid_postfix(string); }
    [[noreturn]] static void RangeError(const String* string) { _ZNKSs5_XranEv_nid_postfix(string); }
};

struct Wide {
    using Char = char16_t;
    static constexpr std::size_t InlineCapacity = sizeof(String::buffer) / sizeof(char16_t) - 1;
    static constexpr std::size_t CapacityMask = InlineCapacity;
    static constexpr std::size_t MaxSize = NoPosition / sizeof(char16_t) - 1;
    [[noreturn]] static void LengthError(const String* string) { _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix(string); }
    [[noreturn]] static void RangeError(const String* string) { _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix(string); }
};

template<typename TTraits>
typename TTraits::Char* Data(String* string) {
    return reinterpret_cast<typename TTraits::Char*>(string->capacity > TTraits::InlineCapacity ? string->pointer : string->buffer);
}

template<typename TTraits>
const typename TTraits::Char* Data(const String* string) {
    return reinterpret_cast<const typename TTraits::Char*>(string->capacity > TTraits::InlineCapacity ? string->pointer : string->buffer);
}

template<typename TTraits>
std::size_t StorageBytes(std::size_t capacity) {
    return (capacity + 1) * sizeof(typename TTraits::Char);
}

template<typename TTraits>
void SetSize(String* string, std::size_t size) {
    string->size = size;
    Data<TTraits>(string)[size] = 0;
}

template<typename TTraits>
void Reserve(String* string, std::size_t size) {
    using Char = typename TTraits::Char;
    if (size > TTraits::MaxSize) TTraits::LengthError(string);
    if (size <= string->capacity) return;
    const std::size_t capacity = std::min(size | TTraits::CapacityMask, TTraits::MaxSize);
    auto* data = static_cast<Char*>(GuestStringStorage::Allocate(StorageBytes<TTraits>(capacity)));
    Char* old = Data<TTraits>(string);
    std::memcpy(data, old, (string->size + 1) * sizeof(Char));
    if (string->capacity > TTraits::InlineCapacity) GuestStringStorage::Deallocate(old, StorageBytes<TTraits>(string->capacity));
    string->pointer = reinterpret_cast<char*>(data);
    string->capacity = capacity;
}

template<typename TTraits>
std::size_t Clamp(const String* source, std::size_t position, std::size_t count) {
    if (position > source->size) TTraits::RangeError(source);
    return std::min<std::size_t>(count, source->size - position);
}

template<typename TTraits>
void EraseRange(String* string, std::size_t position, std::size_t count) {
    auto* data = Data<TTraits>(string);
    const std::size_t tail = string->size - position - count;
    if (count != 0 && tail != 0) std::memmove(data + position, data + position + count, tail * sizeof(typename TTraits::Char));
    SetSize<TTraits>(string, string->size - count);
}

template<typename TTraits>
void Assign(String* string, const typename TTraits::Char* source, std::size_t count) {
    using Char = typename TTraits::Char;
    if (count <= string->capacity) {
        if (count != 0) std::memmove(Data<TTraits>(string), source, count * sizeof(Char));
        SetSize<TTraits>(string, count);
        return;
    }
    if (count > TTraits::MaxSize) TTraits::LengthError(string);
    const std::size_t capacity = std::min(count | TTraits::CapacityMask, TTraits::MaxSize);
    auto* data = static_cast<Char*>(GuestStringStorage::Allocate(StorageBytes<TTraits>(capacity)));
    std::memcpy(data, source, count * sizeof(Char));
    data[count] = 0;
    Char* old = Data<TTraits>(string);
    const std::size_t oldCapacity = string->capacity;
    if (oldCapacity > TTraits::InlineCapacity) GuestStringStorage::Deallocate(old, StorageBytes<TTraits>(oldCapacity));
    string->pointer = reinterpret_cast<char*>(data);
    string->capacity = capacity;
    string->size = count;
}

template<typename TTraits>
String* Erase(String* string, std::size_t position, std::size_t count) {
    EraseRange<TTraits>(string, position, Clamp<TTraits>(string, position, count));
    return string;
}

template<typename TTraits>
String* AppendSubstring(String* string, const String* source, std::size_t position, std::size_t count) {
    count = Clamp<TTraits>(source, position, count);
    if (count > TTraits::MaxSize - string->size) TTraits::LengthError(string);
    if (count == 0) return string;
    const std::size_t size = string->size;
    Reserve<TTraits>(string, size + count);
    std::memmove(Data<TTraits>(string) + size, Data<TTraits>(source) + position, count * sizeof(typename TTraits::Char));
    SetSize<TTraits>(string, size + count);
    return string;
}

template<typename TTraits>
String* AppendCharacters(String* string, std::size_t count, typename TTraits::Char character) {
    if (count > TTraits::MaxSize - string->size) TTraits::LengthError(string);
    if (count == 0) return string;
    const std::size_t size = string->size;
    Reserve<TTraits>(string, size + count);
    std::fill_n(Data<TTraits>(string) + size, count, character);
    SetSize<TTraits>(string, size + count);
    return string;
}

template<typename TTraits>
String* AssignSubstring(String* string, const String* source, std::size_t position, std::size_t count) {
    count = Clamp<TTraits>(source, position, count);
    if (source != string) {
        Assign<TTraits>(string, Data<TTraits>(source) + position, count);
        return string;
    }
    EraseRange<TTraits>(string, position + count, string->size - position - count);
    EraseRange<TTraits>(string, 0, position);
    return string;
}

template<typename TTraits>
String* AssignCharacters(String* string, std::size_t count, typename TTraits::Char character) {
    Reserve<TTraits>(string, count);
    std::fill_n(Data<TTraits>(string), count, character);
    SetSize<TTraits>(string, count);
    return string;
}

template<typename TTraits>
String* InsertCharacters(String* string, std::size_t position, std::size_t count, typename TTraits::Char character) {
    if (position > string->size) TTraits::RangeError(string);
    if (count > TTraits::MaxSize - string->size) TTraits::LengthError(string);
    if (count == 0) return string;
    const std::size_t size = string->size;
    Reserve<TTraits>(string, size + count);
    auto* data = Data<TTraits>(string);
    std::memmove(data + position + count, data + position, (size - position) * sizeof(typename TTraits::Char));
    std::fill_n(data + position, count, character);
    SetSize<TTraits>(string, size + count);
    return string;
}

template<typename TTraits>
void Copy(String* string, std::size_t newSize, std::size_t oldLength) {
    using Char = typename TTraits::Char;
    if (newSize > TTraits::MaxSize) TTraits::LengthError(string);
    if (oldLength > string->size || oldLength > newSize) APS5_INVALID_ARG_EX;
    const std::size_t oldCapacity = string->capacity;
    Char* old = Data<TTraits>(string);
    const std::size_t capacity = std::min(newSize | TTraits::CapacityMask, TTraits::MaxSize);
    if (capacity <= TTraits::InlineCapacity) {
        if (oldCapacity > TTraits::InlineCapacity) {
            Char kept[TTraits::InlineCapacity + 1];
            std::memcpy(kept, old, oldLength * sizeof(Char));
            GuestStringStorage::Deallocate(old, StorageBytes<TTraits>(oldCapacity));
            std::memcpy(string->buffer, kept, oldLength * sizeof(Char));
            string->capacity = TTraits::InlineCapacity;
        }
        SetSize<TTraits>(string, oldLength);
        return;
    }
    auto* data = static_cast<Char*>(GuestStringStorage::Allocate(StorageBytes<TTraits>(capacity)));
    std::memcpy(data, old, oldLength * sizeof(Char));
    if (oldCapacity > TTraits::InlineCapacity) GuestStringStorage::Deallocate(old, StorageBytes<TTraits>(oldCapacity));
    string->pointer = reinterpret_cast<char*>(data);
    string->capacity = capacity;
    SetSize<TTraits>(string, oldLength);
}

}

extern "C" {

[[noreturn]] void APS5_VABI _ZNKSs5_XranEv_nid_postfix(const String*) {
    _ZSt14_Xout_of_rangePKc_nid_postfix("invalid string position");
}

[[noreturn]] void APS5_VABI _ZNKSs5_XlenEv_nid_postfix(const String*) {
    _ZSt14_Xlength_errorPKc_nid_postfix("string too long");
}

void APS5_VABI _ZNSs5_CopyEmm_nid_postfix(String* self, std::size_t newSize, std::size_t oldLength) {
    Copy<Narrow>(self, newSize, oldLength);
}

String* APS5_VABI _ZNSs5eraseEmm_nid_postfix(String* self, std::size_t position, std::size_t count) {
    return Erase<Narrow>(self, position, count);
}

String* APS5_VABI _ZNSs6appendERKSsmm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    return AppendSubstring<Narrow>(self, source, position, count);
}

String* APS5_VABI _ZNSs6appendEmc_nid_postfix(String* self, std::size_t count, char character) {
    return AppendCharacters<Narrow>(self, count, character);
}

String* APS5_VABI _ZNSs6assignERKSsmm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    return AssignSubstring<Narrow>(self, source, position, count);
}

String* APS5_VABI _ZNSs6assignEPKcm_nid_postfix(String* self, const char* source, std::size_t count) {
    Assign<Narrow>(self, source, count);
    return self;
}

String* APS5_VABI _ZNSs6assignEmc_nid_postfix(String* self, std::size_t count, char character) {
    return AssignCharacters<Narrow>(self, count, character);
}

String* APS5_VABI _ZNSs6insertEmmc_nid_postfix(String* self, std::size_t position, std::size_t count, char character) {
    return InsertCharacters<Narrow>(self, position, count, character);
}

[[noreturn]] void APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix(const String*) {
    _ZSt14_Xout_of_rangePKc_nid_postfix("invalid string position");
}

[[noreturn]] void APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix(const String*) {
    _ZSt14_Xlength_errorPKc_nid_postfix("string too long");
}

void APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm_nid_postfix(String* self, std::size_t newSize, std::size_t oldLength) {
    Copy<Wide>(self, newSize, oldLength);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(String* self, std::size_t position, std::size_t count) {
    return Erase<Wide>(self, position, count);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    return AppendSubstring<Wide>(self, source, position, count);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(String* self, std::size_t count, char16_t character) {
    return AppendCharacters<Wide>(self, count, character);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(String* self, const char16_t* source, std::size_t count) {
    Assign<Wide>(self, source, count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    return AssignSubstring<Wide>(self, source, position, count);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(String* self, std::size_t count, char16_t character) {
    return AssignCharacters<Wide>(self, count, character);
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(String* self, std::size_t position, std::size_t count, char16_t character) {
    return InsertCharacters<Wide>(self, position, count, character);
}

}
