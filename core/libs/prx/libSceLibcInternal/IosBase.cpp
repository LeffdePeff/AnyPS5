#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t FormatMask = 0xffff;
constexpr std::int32_t Decimal = 0x10;
constexpr std::int32_t Octal = 0x20;
constexpr std::int32_t Hexadecimal = 0x40;
constexpr std::int32_t BaseField = Decimal | Octal | Hexadecimal;

GuestLocale::IosBase* SetFormat(GuestLocale::IosBase* stream, std::int32_t flags, std::int32_t mask, const char* function) {
    if (stream == nullptr) throw std::invalid_argument(std::string(function) + ": null stream");
    stream->flags = (stream->flags & ~mask) | (flags & mask & FormatMask);
    return stream;
}

GuestLocale::IosBase* APS5_VABI ResetFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    return SetFormat(stream, 0, mask, "resetiosflags");
}

GuestLocale::IosBase* APS5_VABI SetFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    return SetFormat(stream, FormatMask, mask, "setiosflags");
}

GuestLocale::IosBase* APS5_VABI SetBase(GuestLocale::IosBase* stream, std::int32_t base) {
    const std::int32_t field = base == 8 ? Octal : base == 10 ? Decimal : base == 16 ? Hexadecimal : 0;
    return SetFormat(stream, field, BaseField, "setbase");
}

}

extern "C" {

StreamManipulator APS5_VABI _ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&ResetFlags, mask};
}

StreamManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&SetFlags, mask};
}

StreamManipulator APS5_VABI _ZSt7setbasei_nid_postfix(std::int32_t base) {
    return {&SetBase, base};
}

void APS5_VABI sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
}
APS5_EXPORT("X9D8WWSG3As", sceLibcInternalIosBaseCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("I5jcbATyIWo", sceLibcInternalIosBaseDeletingDestructor_nid_no_patch);

}
