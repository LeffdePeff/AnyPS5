#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <bit>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Message = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const LibcException::CategoryObject*, int);
using Condition = LibcException::ErrorCode (APS5_VABI *)(const LibcException::CategoryObject*, int);

constexpr std::size_t MessageSlot = 3;
constexpr std::size_t ConditionSlot = 4;
constexpr int IostreamStreamError = 1;
constexpr char IostreamStreamErrorMessage[] = "iostream stream error";

template<typename TFunction>
TFunction Slot(const LibcException::CategoryObject* category, std::size_t index) {
    return std::bit_cast<TFunction>(static_cast<const void* const*>(category->vtable)[index]);
}

}

extern "C" {

void APS5_VABI _ZNSt22_Future_error_categoryD1Ev_nid_postfix(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
}

void APS5_VABI sceLibcInternalIostreamErrorCategoryCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt14error_categoryD2Ev_nid_postfix(static_cast<LibcException::CategoryObject*>(self));
}
APS5_EXPORT("27Z-Cx1jbkU", sceLibcInternalIostreamErrorCategoryCompleteDestructor_nid_no_patch);

void APS5_VABI _ZNSt23_Generic_error_categoryD0Ev_nid_postfix(void* self) {
    sceLibcInternalGenericErrorCategoryDestructor_nid_no_patch(self);
    _ZdlPv_nid_postfix(self);
}

LibcException::ErrorCode APS5_VABI sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch(const LibcException::CategoryObject* self, int value) {
    return Slot<Condition>(_ZSt15system_categoryv_nid_postfix(), ConditionSlot)(self, value);
}
APS5_EXPORT("ww3UUl317Ng", sceLibcInternalSystemErrorCategoryDefaultErrorCondition_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalSystemErrorCategoryMessage_nid_no_patch(GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    return Slot<Message>(_ZSt15system_categoryv_nid_postfix(), MessageSlot)(result, self, value);
}
APS5_EXPORT("HpAlvhNKb2Y", sceLibcInternalSystemErrorCategoryMessage_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalIostreamErrorCategoryMessage_nid_no_patch(GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    if (value != IostreamStreamError) return _ZNKSt23_Generic_error_category7messageEi_nid_postfix(result, self, value);
    if (!result) APS5_INVALID_ARG_EX;
    *result = {};
    result->capacity = GuestLocale::StringInlineCapacity;
    return _ZNSs6assignEPKcm_nid_postfix(result, IostreamStreamErrorMessage, sizeof(IostreamStreamErrorMessage) - 1);
}
APS5_EXPORT("n9-NJEULZ-0", sceLibcInternalIostreamErrorCategoryMessage_nid_no_patch);

const char* APS5_VABI sceLibcInternalFutureErrorMap_nid_no_patch(int value) {
    switch (value) {
    case 1: return "broken promise";
    case 2: return "future already retrieved";
    case 3: return "promise already satisfied";
    case 4: return "no state";
    default: return nullptr;
    }
}
APS5_EXPORT("0r8rbw2SFqk", sceLibcInternalFutureErrorMap_nid_no_patch);

}
