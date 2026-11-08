#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t MessagesCategory = 6;
constexpr int CodecvtOk = 0;
constexpr int CodecvtNoconv = 3;
constexpr std::size_t WideInlineCapacity = sizeof(GuestLocale::String::buffer) / sizeof(char16_t) - 1;
const std::uint16_t g_wideFalseName[] = {'f', 'a', 'l', 's', 'e', 0};
const std::uint16_t g_wideTrueName[] = {'t', 'r', 'u', 'e', 0};

using Getcat = std::size_t (APS5_VABI*)(GuestLocale::Facet**, const GuestLocale::Implementation* const*);

const GuestLocale::FacetVtable* LibcVtable(Getcat getcat) {
    GuestLocale::Facet* facet = nullptr;
    getcat(&facet, &_ZNSt6locale7classicEv_nid_postfix()->implementation);
    const GuestLocale::FacetVtable* vtable = facet->vtable;
    _ZdlPv_nid_postfix(facet);
    return vtable;
}

const GuestLocale::FacetVtable* NumpunctVtable() {
    static const GuestLocale::FacetVtable* const vtable = LibcVtable(&_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix);
    return vtable;
}

const GuestLocale::FacetVtable* WideNumpunctVtable() {
    static const GuestLocale::FacetVtable* const vtable = LibcVtable(&_ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix);
    return vtable;
}

const GuestLocale::FacetVtable* CollateVtable() {
    static const GuestLocale::FacetVtable* const vtable = LibcVtable(&_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix);
    return vtable;
}

const GuestLocale::FacetVtable* WideCollateVtable() {
    static const GuestLocale::FacetVtable* const vtable = LibcVtable(&_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix);
    return vtable;
}

const GuestLocale::FacetVtable* CodecvtVtable() {
    static const GuestLocale::FacetVtable* const vtable = LibcVtable(&_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix);
    return vtable;
}

class LocinfoScope {
public:
    explicit LocinfoScope(const char* localeName) { _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, localeName); }
    ~LocinfoScope() { _ZNSt8_LocinfoD1Ev_nid_postfix(&storage); }
    LocinfoScope(const LocinfoScope&) = delete;
    LocinfoScope& operator=(const LocinfoScope&) = delete;
    const GuestLocale::LocinfoStorage* Get() const { return &storage; }

private:
    GuestLocale::LocinfoStorage storage{};
};

void ConstructFacet(GuestLocale::Facet* self, const GuestLocale::FacetVtable* vtable, std::size_t references, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    *self = {vtable, static_cast<std::uint32_t>(references), 0};
}

void RequireLocinfo(const GuestLocale::LocinfoStorage* locinfo, const char* function) {
    if (locinfo == nullptr) throw std::invalid_argument(std::string(function) + ": null _Locinfo");
}

void ValidateRange(const void* first, const void* last, const char* function) {
    if ((first == nullptr) != (last == nullptr) || std::less<const void*>()(last, first)) throw std::invalid_argument(std::string(function) + ": invalid character range");
}

GuestLocale::String* AssignNarrow(GuestLocale::String* result, const char* text, const char* function) {
    if (result == nullptr || text == nullptr) throw std::invalid_argument(std::string(function) + ": null argument");
    *result = {};
    result->capacity = GuestLocale::StringInlineCapacity;
    return _ZNSs6assignEPKcm_nid_postfix(result, text, std::strlen(text));
}

GuestLocale::String* AssignWide(GuestLocale::String* result, const std::uint16_t* text, const char* function) {
    if (result == nullptr || text == nullptr) throw std::invalid_argument(std::string(function) + ": null argument");
    std::size_t length = 0;
    while (text[length] != 0) ++length;
    *result = {};
    result->capacity = WideInlineCapacity;
    return _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(result, reinterpret_cast<const char16_t*>(text), length);
}

struct NarrowNumpunct {
    using Facet = GuestLocale::NumpunctFacet;
    using Char = char;
    static const GuestLocale::FacetVtable* Vtable() { return NumpunctVtable(); }
    static const char* FalseName() { return "false"; }
    static const char* TrueName() { return "true"; }
};

struct WideNumpunct {
    using Facet = GuestLocale::WideNumpunctFacet;
    using Char = std::uint16_t;
    static const GuestLocale::FacetVtable* Vtable() { return WideNumpunctVtable(); }
    static const std::uint16_t* FalseName() { return g_wideFalseName; }
    static const std::uint16_t* TrueName() { return g_wideTrueName; }
};

template<typename TNumpunct>
void InitNumpunct(typename TNumpunct::Facet* self, const GuestLocale::LocinfoStorage* locinfo, bool defaults, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    RequireLocinfo(locinfo, function);
    const SonyLconv* conventions = localeconv_nid_postfix();
    self->grouping = defaults ? "" : conventions->grouping;
    self->falsename = TNumpunct::FalseName();
    self->truename = TNumpunct::TrueName();
    self->decimalPoint = static_cast<typename TNumpunct::Char>(defaults ? '.' : static_cast<unsigned char>(conventions->decimal_point[0]));
    self->thousandsSep = static_cast<typename TNumpunct::Char>(defaults ? ',' : static_cast<unsigned char>(conventions->thousands_sep[0]));
}

template<typename TNumpunct>
void ConstructNumpunct(typename TNumpunct::Facet* self, std::size_t references, const GuestLocale::LocinfoStorage* locinfo, bool defaults, const char* function) {
    ConstructFacet(self == nullptr ? nullptr : &self->base, TNumpunct::Vtable(), references, function);
    InitNumpunct<TNumpunct>(self, locinfo, defaults, function);
}

template<typename TNumpunct>
void ConstructNamedNumpunct(typename TNumpunct::Facet* self, const char* localeName, std::size_t references, bool defaults, const char* function) {
    const LocinfoScope locinfo(localeName);
    ConstructNumpunct<TNumpunct>(self, references, locinfo.Get(), defaults, function);
}

template<typename TNumpunct>
void ConstructCurrentNumpunct(typename TNumpunct::Facet* self, std::size_t references, const char* function) {
    ConstructNamedNumpunct<TNumpunct>(self, "C", references, false, function);
    if (self->thousandsSep == 0) self->thousandsSep = ',';
}

const GuestLocale::WideNumpunctFacet* RequireWideNumpunct(const GuestLocale::WideNumpunctFacet* self, const char* function) {
    if (self == nullptr || self->base.vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return self;
}

const GuestLocale::WideNumpunctVtable& WidePunctuation(const GuestLocale::WideNumpunctFacet* self, const char* function) {
    return *reinterpret_cast<const GuestLocale::WideNumpunctVtable*>(RequireWideNumpunct(self, function)->base.vtable);
}

void APS5_VABI DestroyFacet(GuestLocale::Facet*) {}

void APS5_VABI DeleteFacet(GuestLocale::Facet* self) {
    _ZdlPv_nid_postfix(self);
}

struct MessagesVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI* open)(const void* self, const void* name, const void* locale);
    GuestLocale::String* (APS5_VABI* get)(GuestLocale::String* result, const GuestLocale::Facet* self, int catalog, int set, int message, const GuestLocale::String* fallback);
    void (APS5_VABI* close)(const void* self, int catalog);
};

static_assert(offsetof(MessagesVtable, open) == 0x20);
static_assert(offsetof(MessagesVtable, get) == 0x28);
static_assert(offsetof(MessagesVtable, close) == 0x30);

const MessagesVtable g_messagesVtable{
    {DestroyFacet, DeleteFacet, _ZNSt6locale5facet7_IncrefEv_nid_postfix, _ZNSt6locale5facet7_DecrefEv_nid_postfix},
    sceLibcInternalPending4_007_nid_no_patch,
    _ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix,
    sceLibcInternalPending4_008_nid_no_patch};

const MessagesVtable g_wideMessagesVtable{
    {DestroyFacet, DeleteFacet, _ZNSt6locale5facet7_IncrefEv_nid_postfix, _ZNSt6locale5facet7_DecrefEv_nid_postfix},
    sceLibcInternalPending4_013_nid_no_patch,
    _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix,
    sceLibcInternalPending4_014_nid_no_patch};

const MessagesVtable& Messages(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const MessagesVtable*>(self->vtable);
}

void ConstructNamedMessages(GuestLocale::Facet* self, const MessagesVtable& vtable, const char* localeName, std::size_t references, const char* function) {
    const LocinfoScope locinfo(localeName);
    ConstructFacet(self, &vtable.facet, references, function);
}

std::size_t GetcatMessages(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale, const MessagesVtable& vtable, const char* function) {
    if (facet != nullptr && *facet == nullptr) {
        if (locale == nullptr || *locale == nullptr) throw std::invalid_argument(std::string(function) + ": null locale");
        const LocinfoScope locinfo((*locale)->name);
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        ConstructFacet(object, &vtable.facet, 0, function);
        *facet = object;
    }
    return MessagesCategory;
}

void InitCollate(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    RequireLocinfo(locinfo, function);
    self->collation = nullptr;
    self->wideCollation = nullptr;
}

void ConstructCollate(GuestLocale::CollateFacet* self, const GuestLocale::FacetVtable* vtable, std::size_t references, const GuestLocale::LocinfoStorage* locinfo, const char* function) {
    ConstructFacet(self == nullptr ? nullptr : &self->base, vtable, references, function);
    InitCollate(self, locinfo, function);
}

void ConstructNamedCollate(GuestLocale::CollateFacet* self, const GuestLocale::FacetVtable* vtable, const char* localeName, std::size_t references, const char* function) {
    const LocinfoScope locinfo(localeName);
    ConstructCollate(self, vtable, references, locinfo.Get(), function);
}

void RequireNext(const void* next, const char* function) {
    if (next == nullptr) throw std::invalid_argument(std::string(function) + ": null next pointer");
}

const GuestLocale::CodecvtVtable& Conversions(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const GuestLocale::CodecvtVtable*>(self->vtable);
}

template<typename TUnit>
int CompareSequences(const TUnit* first1, const TUnit* last1, const TUnit* first2, const TUnit* last2, const char* function) {
    ValidateRange(first1, last1, function);
    ValidateRange(first2, last2, function);
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (*first1 != *first2) return *first1 < *first2 ? -1 : 1;
    }
    if (first2 != last2) return -1;
    return first1 != last1 ? 1 : 0;
}

const char* NarrowData(const GuestLocale::String* text, const char* function) {
    if (text == nullptr) throw std::invalid_argument(std::string(function) + ": null locale name");
    return text->capacity > GuestLocale::StringInlineCapacity ? text->pointer : text->buffer;
}

}

extern "C" {

void APS5_VABI _ZNSt6locale5facet7_IncrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale::facet::_Incref: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI _ZNSt6locale5facet7_DecrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale::facet::_Decref: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

void APS5_VABI _ZNSt6localeD1Ev_nid_postfix(GuestLocaleFacade* self) {
    if (self == nullptr) throw std::invalid_argument("locale destructor: null object");
    GuestLocale::Implementation* implementation = self->implementation;
    if (implementation == nullptr) return;
    if (GuestLocale::Facet* released = implementation->base.vtable->release(&implementation->base)) released->vtable->deleteObject(released);
}

std::uint16_t APS5_VABI _ZNKSt8numpunctIwE16do_decimal_pointEv_nid_postfix(const GuestLocale::WideNumpunctFacet* self) {
    return RequireWideNumpunct(self, "numpunct<wchar_t>::do_decimal_point")->decimalPoint;
}

std::uint16_t APS5_VABI _ZNKSt8numpunctIwE16do_thousands_sepEv_nid_postfix(const GuestLocale::WideNumpunctFacet* self) {
    return RequireWideNumpunct(self, "numpunct<wchar_t>::do_thousands_sep")->thousandsSep;
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE11do_groupingEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignNarrow(result, RequireWideNumpunct(self, "numpunct<wchar_t>::do_grouping")->grouping, "numpunct<wchar_t>::do_grouping");
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE12do_falsenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignWide(result, RequireWideNumpunct(self, "numpunct<wchar_t>::do_falsename")->falsename, "numpunct<wchar_t>::do_falsename");
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE11do_truenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return AssignWide(result, RequireWideNumpunct(self, "numpunct<wchar_t>::do_truename")->truename, "numpunct<wchar_t>::do_truename");
}

std::uint16_t APS5_VABI _ZNKSt8numpunctIwE13decimal_pointEv_nid_postfix(const GuestLocale::WideNumpunctFacet* self) {
    return WidePunctuation(self, "numpunct<wchar_t>::decimal_point").decimalPoint(self);
}

std::uint16_t APS5_VABI _ZNKSt8numpunctIwE13thousands_sepEv_nid_postfix(const GuestLocale::WideNumpunctFacet* self) {
    return WidePunctuation(self, "numpunct<wchar_t>::thousands_sep").thousandsSep(self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE8groupingEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return WidePunctuation(self, "numpunct<wchar_t>::grouping").grouping(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE9falsenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return WidePunctuation(self, "numpunct<wchar_t>::falsename").falsename(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIwE8truenameEv_nid_postfix(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return WidePunctuation(self, "numpunct<wchar_t>::truename").truename(result, self);
}

void APS5_VABI _ZNSt8numpunctIcE5_InitERKSt8_Locinfob_nid_postfix(GuestLocale::NumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, bool defaults) {
    InitNumpunct<NarrowNumpunct>(self, locinfo, defaults, "numpunct<char>::_Init");
}

void APS5_VABI _ZNSt8numpunctIcEC1Em_nid_postfix(GuestLocale::NumpunctFacet* self, std::size_t references) {
    ConstructCurrentNumpunct<NarrowNumpunct>(self, references, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcEC2Em_nid_postfix(GuestLocale::NumpunctFacet* self, std::size_t references) {
    ConstructCurrentNumpunct<NarrowNumpunct>(self, references, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcEC1ERKSt8_Locinfomb_nid_postfix(GuestLocale::NumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool defaults) {
    ConstructNumpunct<NarrowNumpunct>(self, references, locinfo, defaults, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcEC2ERKSt8_Locinfomb_nid_postfix(GuestLocale::NumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool defaults) {
    ConstructNumpunct<NarrowNumpunct>(self, references, locinfo, defaults, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcEC1EPKcmb_nid_postfix(GuestLocale::NumpunctFacet* self, const char* localeName, std::size_t references, bool defaults) {
    ConstructNamedNumpunct<NarrowNumpunct>(self, localeName, references, defaults, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcEC2EPKcmb_nid_postfix(GuestLocale::NumpunctFacet* self, const char* localeName, std::size_t references, bool defaults) {
    ConstructNamedNumpunct<NarrowNumpunct>(self, localeName, references, defaults, "numpunct<char> constructor");
}

void APS5_VABI _ZNSt8numpunctIcED1Ev_nid_postfix(GuestLocale::NumpunctFacet*) {}

void APS5_VABI _ZNSt8numpunctIcED2Ev_nid_postfix(GuestLocale::NumpunctFacet*) {}

void APS5_VABI _ZNSt8numpunctIcED0Ev_nid_postfix(GuestLocale::NumpunctFacet* self) {
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, bool defaults) {
    InitNumpunct<WideNumpunct>(self, locinfo, defaults, "numpunct<wchar_t>::_Init");
}

void APS5_VABI _ZNSt8numpunctIwEC1Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    ConstructCurrentNumpunct<WideNumpunct>(self, references, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwEC2Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    ConstructCurrentNumpunct<WideNumpunct>(self, references, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool defaults) {
    ConstructNumpunct<WideNumpunct>(self, references, locinfo, defaults, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwEC2ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool defaults) {
    ConstructNumpunct<WideNumpunct>(self, references, locinfo, defaults, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwEC1EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool defaults) {
    ConstructNamedNumpunct<WideNumpunct>(self, localeName, references, defaults, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwEC2EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool defaults) {
    ConstructNamedNumpunct<WideNumpunct>(self, localeName, references, defaults, "numpunct<wchar_t> constructor");
}

void APS5_VABI _ZNSt8numpunctIwED1Ev_nid_postfix(GuestLocale::WideNumpunctFacet*) {}

void APS5_VABI _ZNSt8numpunctIwED2Ev_nid_postfix(GuestLocale::WideNumpunctFacet*) {}

void APS5_VABI _ZNSt8numpunctIwED0Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self) {
    _ZdlPv_nid_postfix(self);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet*, int, int, int, const GuestLocale::String* fallback) {
    if (result == nullptr || fallback == nullptr) throw std::invalid_argument("messages<char>::do_get: null argument");
    *result = {};
    result->capacity = GuestLocale::StringInlineCapacity;
    return _ZNSs6assignERKSsmm_nid_postfix(result, fallback, 0, static_cast<std::size_t>(-1));
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet*, int, int, int, const GuestLocale::String* fallback) {
    if (result == nullptr || fallback == nullptr) throw std::invalid_argument("messages<wchar_t>::do_get: null argument");
    *result = {};
    result->capacity = WideInlineCapacity;
    return _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(result, fallback, 0, static_cast<std::size_t>(-1));
}

int APS5_VABI _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(const GuestLocale::Facet* self, const GuestLocale::String* name, const GuestLocaleFacade* locale) {
    return Messages(self, "messages<char>::open").open(self, name, locale);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet* self, int catalog, int set, int message, const GuestLocale::String* fallback) {
    return Messages(self, "messages<char>::get").get(result, self, catalog, set, message, fallback);
}

void APS5_VABI _ZNKSt8messagesIcE5closeEi_nid_postfix(const GuestLocale::Facet* self, int catalog) {
    Messages(self, "messages<char>::close").close(self, catalog);
}

int APS5_VABI _ZNKSt8messagesIwE4openERKSsRKSt6locale_nid_postfix(const GuestLocale::Facet* self, const GuestLocale::String* name, const GuestLocaleFacade* locale) {
    return Messages(self, "messages<wchar_t>::open").open(self, name, locale);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet* self, int catalog, int set, int message, const GuestLocale::String* fallback) {
    return Messages(self, "messages<wchar_t>::get").get(result, self, catalog, set, message, fallback);
}

void APS5_VABI _ZNKSt8messagesIwE5closeEi_nid_postfix(const GuestLocale::Facet* self, int catalog) {
    Messages(self, "messages<wchar_t>::close").close(self, catalog);
}

void APS5_VABI _ZNSt8messagesIcEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, &g_messagesVtable.facet, references, "messages<char> constructor");
}

void APS5_VABI _ZNSt8messagesIcEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, &g_messagesVtable.facet, references, "messages<char> constructor");
}

void APS5_VABI _ZNSt8messagesIcEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "messages<char> constructor");
    ConstructFacet(self, &g_messagesVtable.facet, references, "messages<char> constructor");
}

void APS5_VABI _ZNSt8messagesIcEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "messages<char> constructor");
    ConstructFacet(self, &g_messagesVtable.facet, references, "messages<char> constructor");
}

void APS5_VABI _ZNSt8messagesIcEC1EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructNamedMessages(self, g_messagesVtable, localeName, references, "messages<char> constructor");
}

void APS5_VABI _ZNSt8messagesIcEC2EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructNamedMessages(self, g_messagesVtable, localeName, references, "messages<char> constructor");
}

std::size_t APS5_VABI _ZNSt8messagesIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return GetcatMessages(facet, locale, g_messagesVtable, "messages<char>::_Getcat");
}

void APS5_VABI _ZNSt8messagesIwEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, &g_wideMessagesVtable.facet, references, "messages<wchar_t> constructor");
}

void APS5_VABI _ZNSt8messagesIwEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, &g_wideMessagesVtable.facet, references, "messages<wchar_t> constructor");
}

void APS5_VABI _ZNSt8messagesIwEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "messages<wchar_t> constructor");
    ConstructFacet(self, &g_wideMessagesVtable.facet, references, "messages<wchar_t> constructor");
}

void APS5_VABI _ZNSt8messagesIwEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "messages<wchar_t> constructor");
    ConstructFacet(self, &g_wideMessagesVtable.facet, references, "messages<wchar_t> constructor");
}

void APS5_VABI _ZNSt8messagesIwEC1EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructNamedMessages(self, g_wideMessagesVtable, localeName, references, "messages<wchar_t> constructor");
}

void APS5_VABI _ZNSt8messagesIwEC2EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructNamedMessages(self, g_wideMessagesVtable, localeName, references, "messages<wchar_t> constructor");
}

std::size_t APS5_VABI _ZNSt8messagesIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return GetcatMessages(facet, locale, g_wideMessagesVtable, "messages<wchar_t>::_Getcat");
}

void APS5_VABI _ZNSt7collateIcE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo) {
    InitCollate(self, locinfo, "collate<char>::_Init");
}

void APS5_VABI _ZNSt7collateIcEC1Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, CollateVtable(), "C", references, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIcEC2Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, CollateVtable(), "C", references, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructCollate(self, CollateVtable(), references, locinfo, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIcEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructCollate(self, CollateVtable(), references, locinfo, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIcEC1EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, CollateVtable(), localeName, references, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIcEC2EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, CollateVtable(), localeName, references, "collate<char> constructor");
}

void APS5_VABI _ZNSt7collateIwE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo) {
    InitCollate(self, locinfo, "collate<wchar_t>::_Init");
}

void APS5_VABI _ZNSt7collateIwEC1Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, WideCollateVtable(), "C", references, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7collateIwEC2Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, WideCollateVtable(), "C", references, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7collateIwEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructCollate(self, WideCollateVtable(), references, locinfo, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7collateIwEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructCollate(self, WideCollateVtable(), references, locinfo, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7collateIwEC1EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, WideCollateVtable(), localeName, references, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7collateIwEC2EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, WideCollateVtable(), localeName, references, "collate<wchar_t> constructor");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, CodecvtVtable(), references, "codecvt<char> constructor");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructFacet(self, CodecvtVtable(), references, "codecvt<char> constructor");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "codecvt<char> constructor");
    ConstructFacet(self, CodecvtVtable(), references, "codecvt<char> constructor");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    RequireLocinfo(locinfo, "codecvt<char> constructor");
    ConstructFacet(self, CodecvtVtable(), references, "codecvt<char> constructor");
}

bool APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(const GuestLocale::Facet*) {
    return true;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    RequireNext(next1, "codecvt<char>::do_in");
    RequireNext(next2, "codecvt<char>::do_in");
    *next1 = first1;
    *next2 = first2;
    return CodecvtNoconv;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    RequireNext(next1, "codecvt<char>::do_out");
    RequireNext(next2, "codecvt<char>::do_out");
    *next1 = first1;
    *next2 = first2;
    return CodecvtNoconv;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    RequireNext(next2, "codecvt<char>::do_unshift");
    *next2 = first2;
    return CodecvtOk;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char* last1, std::size_t count) {
    ValidateRange(first1, last1, "codecvt<char>::do_length");
    return static_cast<int>(std::min(static_cast<std::size_t>(last1 - first1), count));
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return Conversions(self, "codecvt<char>::in").in(self, state, first1, last1, next1, first2, last2, next2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return Conversions(self, "codecvt<char>::out").out(self, state, first1, last1, next1, first2, last2, next2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, char* first2, char* last2, char** next2) {
    return Conversions(self, "codecvt<char>::unshift").unshift(self, state, first2, last2, next2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    return static_cast<int>(Conversions(self, "codecvt<char>::length").length(self, state, first1, last1, count));
}

void APS5_VABI _ZNSt8_LocinfoC1EiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, localeName);
}

void APS5_VABI _ZNSt8_LocinfoC2EiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, localeName);
}

void APS5_VABI _ZNSt8_LocinfoC1ERKSs_nid_postfix(GuestLocale::LocinfoStorage* self, const GuestLocale::String* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, NarrowData(localeName, "_Locinfo"));
}

void APS5_VABI _ZNSt8_LocinfoC2ERKSs_nid_postfix(GuestLocale::LocinfoStorage* self, const GuestLocale::String* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, NarrowData(localeName, "_Locinfo"));
}

GuestLocale::LocinfoStorage* APS5_VABI _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int, const char* localeName) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo::_Addcats: null object");
    const LocinfoScope added(localeName);
    return self;
}

int APS5_VABI _ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(const char* first1, const char* last1, const char* first2, const char* last2, const void*) {
    return CompareSequences(reinterpret_cast<const unsigned char*>(first1), reinterpret_cast<const unsigned char*>(last1), reinterpret_cast<const unsigned char*>(first2), reinterpret_cast<const unsigned char*>(last2), "_LStrcoll<char>");
}

int APS5_VABI _ZSt9_LStrcollIwEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(const char16_t* first1, const char16_t* last1, const char16_t* first2, const char16_t* last2, const void*) {
    return CompareSequences(first1, last1, first2, last2, "_LStrcoll<wchar_t>");
}

}
