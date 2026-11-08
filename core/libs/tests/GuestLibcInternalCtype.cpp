#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ctype<char>: %s\n", message);
    std::abort();
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

void CheckCharacters() {
    for (int value = 0; value < 256; ++value) {
        const char character = static_cast<char>(value);
        const char lower = value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : character;
        const char upper = value >= 'a' && value <= 'z' ? static_cast<char>(value - 32) : character;
        Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(nullptr, character) == lower, "do_tolower(c) maps the C locale's upper case letters only");
        Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(nullptr, character) == upper, "do_toupper(c) maps the C locale's lower case letters only");
    }
}

void CheckRanges() {
    char text[] = "Hello, World! 123 \xc0\xe9\xff";
    const char* last = text + sizeof(text) - 1;
    Require(_ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(nullptr, text, last) == last, "do_tolower(first, last) returns last");
    Require(std::strcmp(text, "hello, world! 123 \xc0\xe9\xff") == 0, "do_tolower(first, last) converts the range in place");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(nullptr, text + 7, text + 12) == text + 12, "do_toupper(first, last) returns last");
    Require(std::strcmp(text, "hello, WORLD! 123 \xc0\xe9\xff") == 0, "do_toupper(first, last) converts only the range");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(nullptr, text, text) == text && std::strcmp(text, "hello, WORLD! 123 \xc0\xe9\xff") == 0, "an empty range is unchanged");
    Require(_ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(nullptr, nullptr, nullptr) == nullptr, "a null range returns null");
    Require(ThrowsInvalidArgument([&] { _ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(nullptr, text + 2, text + 1); }), "a reversed range throws");
    Require(ThrowsInvalidArgument([&] { _ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(nullptr, nullptr, text); }), "a range with one null end throws");
}

}

int main() {
    CheckCharacters();
    CheckRanges();
    std::puts("libc internal ctype<char>: ok");
    return 0;
}
