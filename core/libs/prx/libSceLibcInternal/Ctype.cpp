#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <stdexcept>
#include <string>

namespace {

char Convert(char character, bool upper) {
    const int value = static_cast<unsigned char>(character);
    return static_cast<char>(upper ? toupper_nid_postfix(value) : tolower_nid_postfix(value));
}

const char* ConvertRange(char* first, const char* last, bool upper, const char* function) {
    if ((first == nullptr) != (last == nullptr) || last < first) throw std::invalid_argument(std::string(function) + ": invalid character range");
    for (; first != last; ++first) *first = Convert(*first, upper);
    return last;
}

}

extern "C" {

char APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(const void*, char character) {
    return Convert(character, false);
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(const void*, char* first, const char* last) {
    return ConvertRange(first, last, false, "ctype<char>::do_tolower");
}

char APS5_VABI _ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(const void*, char character) {
    return Convert(character, true);
}

const char* APS5_VABI _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(const void*, char* first, const char* last) {
    return ConvertRange(first, last, true, "ctype<char>::do_toupper");
}

}
