#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>

namespace {

using GuestLocale::String;

unsigned releases = 0;
void* lastReleased = nullptr;
unsigned deletions = 0;
GuestLocale::Facet* lastDeleted = nullptr;
int closedCatalog = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale facets: %s\n", message);
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

template<typename TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

constexpr std::size_t WideInlineCapacity = 7;
using Getcat = std::size_t (APS5_VABI*)(GuestLocale::Facet**, const GuestLocale::Implementation* const*);

const GuestLocale::Implementation* const* Classic() {
    return &_ZNSt6locale7classicEv_nid_postfix()->implementation;
}

const GuestLocale::FacetVtable* LibcVtable(Getcat getcat) {
    GuestLocale::Facet* facet = nullptr;
    getcat(&facet, Classic());
    Require(facet != nullptr, "libc's _Getcat builds a facet");
    const GuestLocale::FacetVtable* vtable = facet->vtable;
    _ZdlPv_nid_postfix(facet);
    return vtable;
}

const char* NarrowData(const String& text) {
    return text.capacity > GuestLocale::StringInlineCapacity ? text.pointer : text.buffer;
}

const char16_t* WideData(const String& text) {
    return reinterpret_cast<const char16_t*>(text.capacity > WideInlineCapacity ? text.pointer : text.buffer);
}

bool NarrowIs(const String& text, const char* expected) {
    return text.size == std::strlen(expected) && std::memcmp(NarrowData(text), expected, text.size + 1) == 0;
}

bool WideIs(const String& text, const char16_t* expected) {
    const std::size_t length = std::char_traits<char16_t>::length(expected);
    return text.size == length && std::memcmp(WideData(text), expected, (length + 1) * sizeof(char16_t)) == 0;
}

void WideRelease(String& text) {
    if (text.capacity > WideInlineCapacity) GuestStringStorage::Deallocate(text.pointer, (text.capacity + 1) * sizeof(char16_t));
}

String Narrow(const char* text) {
    String result{};
    result.capacity = GuestLocale::StringInlineCapacity;
    _ZNSs6assignEPKcm_nid_postfix(&result, text, std::strlen(text));
    return result;
}

String Wide(const char16_t* text) {
    String result{};
    result.capacity = WideInlineCapacity;
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&result, text, std::char_traits<char16_t>::length(text));
    return result;
}

template<typename TChar>
bool TextIs(const TChar* text, const char* expected) {
    std::size_t index = 0;
    for (; expected[index] != 0; ++index) {
        if (static_cast<std::uint32_t>(text[index]) != static_cast<unsigned char>(expected[index])) return false;
    }
    return text[index] == 0;
}

void APS5_VABI RecordDestroy(GuestLocale::Facet*) {}

void APS5_VABI RecordDelete(GuestLocale::Facet* self) {
    ++deletions;
    lastDeleted = self;
}

const GuestLocale::FacetVtable g_recordingVtable{RecordDestroy, RecordDelete, _ZNSt6locale5facet7_IncrefEv_nid_postfix, _ZNSt6locale5facet7_DecrefEv_nid_postfix};

void CheckFacetReferences() {
    GuestLocale::Facet facet{&g_recordingVtable, 0, 0};
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    _ZNSt6locale5facet7_IncrefEv_nid_postfix(&facet);
    Require(facet.references == 2, "_Incref adds a reference");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == nullptr && facet.references == 1, "_Decref returns null while references remain");
    Require(_ZNSt6locale5facet7_DecrefEv_nid_postfix(&facet) == &facet && facet.references == 0, "_Decref returns the facet with its last reference");
    Require(ThrowsInvalidArgument([] { _ZNSt6locale5facet7_IncrefEv_nid_postfix(nullptr); }), "_Incref rejects a null facet");
    Require(ThrowsInvalidArgument([] { _ZNSt6locale5facet7_DecrefEv_nid_postfix(nullptr); }), "_Decref rejects a null facet");
}

void CheckLocaleDestructor() {
    GuestLocale::Implementation implementation{{&g_recordingVtable, 2, 0}, nullptr, 0, 0, false, "C"};
    GuestLocaleFacade locale{&implementation};
    deletions = 0;
    _ZNSt6localeD1Ev_nid_postfix(&locale);
    Require(implementation.base.references == 1 && deletions == 0, "~locale releases one reference");
    _ZNSt6localeD1Ev_nid_postfix(&locale);
    Require(implementation.base.references == 0 && deletions == 1 && lastDeleted == &implementation.base, "~locale deletes the _Locimp its last release returns");

    GuestLocaleFacade classic = *_ZNSt6locale7classicEv_nid_postfix();
    const std::uint32_t classicReferences = classic.implementation->base.references;
    releases = 0;
    _ZNSt6localeD1Ev_nid_postfix(&classic);
    Require(classic.implementation->base.references == classicReferences && releases == 0, "~locale keeps the classic locale");

    GuestLocaleFacade versioned{_ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(true)};
    void* created = versioned.implementation;
    releases = 0;
    _ZNSt6localeD1Ev_nid_postfix(&versioned);
    Require(releases != 0 && lastReleased == created, "~locale frees a _Locimp through its deleting destructor");

    GuestLocaleFacade empty{nullptr};
    _ZNSt6localeD1Ev_nid_postfix(&empty);
    Require(ThrowsInvalidArgument([] { _ZNSt6localeD1Ev_nid_postfix(nullptr); }), "~locale rejects a null object");
}

std::uint16_t APS5_VABI TestDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return u'x'; }
std::uint16_t APS5_VABI TestThousandsSep(const GuestLocale::WideNumpunctFacet*) { return u'y'; }

String* APS5_VABI TestGrouping(String* result, const GuestLocale::WideNumpunctFacet*) {
    *result = Narrow("\5");
    return result;
}

String* APS5_VABI TestFalsename(String* result, const GuestLocale::WideNumpunctFacet*) {
    *result = Wide(u"no");
    return result;
}

String* APS5_VABI TestTruename(String* result, const GuestLocale::WideNumpunctFacet*) {
    *result = Wide(u"yes");
    return result;
}

const GuestLocale::WideNumpunctVtable g_testWideNumpunctVtable{g_recordingVtable, TestDecimalPoint, TestThousandsSep, TestGrouping, TestFalsename, TestTruename};

void CheckWideNumpunctMembers() {
    GuestLocale::Facet* created = nullptr;
    _ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&created, Classic());
    auto* facet = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(created);
    Require(_ZNKSt8numpunctIwE16do_decimal_pointEv_nid_postfix(facet) == u'.' && _ZNKSt8numpunctIwE13decimal_pointEv_nid_postfix(facet) == u'.', "the decimal point is '.'");
    Require(_ZNKSt8numpunctIwE16do_thousands_sepEv_nid_postfix(facet) == u',' && _ZNKSt8numpunctIwE13thousands_sepEv_nid_postfix(facet) == u',', "the separator is ','");
    String text{};
    Require(_ZNKSt8numpunctIwE11do_groupingEv_nid_postfix(&text, facet) == &text && NarrowIs(text, "") && text.capacity == GuestLocale::StringInlineCapacity, "do_grouping returns the empty narrow grouping");
    text = {};
    _ZNKSt8numpunctIwE8groupingEv_nid_postfix(&text, facet);
    Require(NarrowIs(text, "") && text.capacity == GuestLocale::StringInlineCapacity, "grouping returns the empty narrow grouping");
    text = {};
    Require(_ZNKSt8numpunctIwE12do_falsenameEv_nid_postfix(&text, facet) == &text && WideIs(text, u"false") && text.capacity == WideInlineCapacity, "do_falsename returns L\"false\" inline");
    text = {};
    _ZNKSt8numpunctIwE9falsenameEv_nid_postfix(&text, facet);
    Require(WideIs(text, u"false"), "falsename returns L\"false\"");
    text = {};
    Require(_ZNKSt8numpunctIwE11do_truenameEv_nid_postfix(&text, facet) == &text && WideIs(text, u"true") && text.capacity == WideInlineCapacity, "do_truename returns L\"true\" inline");
    text = {};
    _ZNKSt8numpunctIwE8truenameEv_nid_postfix(&text, facet);
    Require(WideIs(text, u"true"), "truename returns L\"true\"");
    _ZdlPv_nid_postfix(facet);

    const std::uint16_t longName[] = {'n', 'o', 't', ' ', 'a', 't', ' ', 'a', 'l', 'l', ',', ' ', 's', 'o', 'r', 'r', 'y', 0};
    const std::uint16_t shortName[] = {'s', 'i', 0};
    GuestLocale::WideNumpunctFacet custom{{&g_testWideNumpunctVtable.facet, 1, 0}, "\3\2", u'#', u'_', longName, shortName};
    Require(_ZNKSt8numpunctIwE16do_decimal_pointEv_nid_postfix(&custom) == u'#' && _ZNKSt8numpunctIwE16do_thousands_sepEv_nid_postfix(&custom) == u'_', "the do_ members read the facet");
    text = {};
    _ZNKSt8numpunctIwE11do_groupingEv_nid_postfix(&text, &custom);
    Require(NarrowIs(text, "\3\2"), "do_grouping copies the facet's grouping");
    text = {};
    _ZNKSt8numpunctIwE12do_falsenameEv_nid_postfix(&text, &custom);
    Require(WideIs(text, u"not at all, sorry") && text.capacity == (17 | WideInlineCapacity), "a long name moves to heap storage of capacity length | 7");
    WideRelease(text);
    text = {};
    _ZNKSt8numpunctIwE11do_truenameEv_nid_postfix(&text, &custom);
    Require(WideIs(text, u"si") && text.capacity == WideInlineCapacity, "a short name stays inline");

    Require(_ZNKSt8numpunctIwE13decimal_pointEv_nid_postfix(&custom) == u'x' && _ZNKSt8numpunctIwE13thousands_sepEv_nid_postfix(&custom) == u'y', "decimal_point and thousands_sep call the facet's virtuals");
    text = {};
    _ZNKSt8numpunctIwE8groupingEv_nid_postfix(&text, &custom);
    Require(NarrowIs(text, "\5"), "grouping calls the facet's do_grouping");
    text = {};
    _ZNKSt8numpunctIwE9falsenameEv_nid_postfix(&text, &custom);
    Require(WideIs(text, u"no"), "falsename calls the facet's do_falsename");
    text = {};
    _ZNKSt8numpunctIwE8truenameEv_nid_postfix(&text, &custom);
    Require(WideIs(text, u"yes"), "truename calls the facet's do_truename");

    Require(ThrowsInvalidArgument([] { _ZNKSt8numpunctIwE16do_decimal_pointEv_nid_postfix(nullptr); }), "do_decimal_point rejects a null facet");
    Require(ThrowsInvalidArgument([] { _ZNKSt8numpunctIwE13thousands_sepEv_nid_postfix(nullptr); }), "thousands_sep rejects a null facet");
    Require(ThrowsInvalidArgument([&] { _ZNKSt8numpunctIwE12do_falsenameEv_nid_postfix(nullptr, &custom); }), "do_falsename rejects a null result");
}

template<typename TFacet>
struct NumpunctApi {
    void (APS5_VABI* current1)(TFacet*, std::size_t);
    void (APS5_VABI* current2)(TFacet*, std::size_t);
    void (APS5_VABI* locinfo1)(TFacet*, const GuestLocale::LocinfoStorage*, std::size_t, bool);
    void (APS5_VABI* locinfo2)(TFacet*, const GuestLocale::LocinfoStorage*, std::size_t, bool);
    void (APS5_VABI* named1)(TFacet*, const char*, std::size_t, bool);
    void (APS5_VABI* named2)(TFacet*, const char*, std::size_t, bool);
    void (APS5_VABI* init)(TFacet*, const GuestLocale::LocinfoStorage*, bool);
    void (APS5_VABI* complete)(TFacet*);
    void (APS5_VABI* base)(TFacet*);
    void (APS5_VABI* deleting)(TFacet*);
    Getcat getcat;
};

template<typename TFacet>
bool HasValues(const TFacet& facet, const GuestLocale::FacetVtable* vtable, std::uint32_t references, char separator, const char* grouping) {
    return facet.base.vtable == vtable && facet.base.references == references && std::strcmp(facet.grouping, grouping) == 0 && facet.decimalPoint == '.' &&
        static_cast<std::uint32_t>(facet.thousandsSep) == static_cast<unsigned char>(separator) && TextIs(facet.falsename, "false") && TextIs(facet.truename, "true");
}

template<typename TFacet>
void CheckNumpunctConstructors(const NumpunctApi<TFacet>& api) {
    const GuestLocale::FacetVtable* vtable = LibcVtable(api.getcat);
    const SonyLconv* conventions = localeconv_nid_postfix();
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");

    for (auto construct : {api.current1, api.current2}) {
        TFacet facet{};
        construct(&facet, 7);
        Require(HasValues(facet, vtable, 7, ',', ""), "numpunct(refs) builds libc's C-locale facet with ',' as separator");
    }
    for (auto construct : {api.locinfo1, api.locinfo2}) {
        TFacet facet{};
        construct(&facet, &locinfo, 3, true);
        Require(HasValues(facet, vtable, 3, ',', ""), "numpunct(_Locinfo, refs, true) applies the defaults");
        construct(&facet, &locinfo, 0, false);
        Require(HasValues(facet, vtable, 0, conventions->thousands_sep[0], conventions->grouping) && facet.grouping == conventions->grouping,
            "numpunct(_Locinfo, refs, false) takes the separator and grouping from localeconv");
        Require(ThrowsInvalidArgument([&] { construct(&facet, nullptr, 0, true); }), "a null _Locinfo throws");
        Require(ThrowsInvalidArgument([&] { construct(nullptr, &locinfo, 0, true); }), "a null object throws");
    }
    for (auto construct : {api.named1, api.named2}) {
        TFacet facet{};
        construct(&facet, "C", 1, true);
        Require(HasValues(facet, vtable, 1, ',', ""), "numpunct(\"C\", refs, true) applies the defaults");
        construct(&facet, "C", 2, false);
        Require(HasValues(facet, vtable, 2, conventions->thousands_sep[0], conventions->grouping), "numpunct(\"C\", refs, false) reads localeconv");
        Require(ThrowsInvalidArgument([&] { construct(&facet, "fr_FR", 0, true); }), "a locale other than C throws");
        Require(ThrowsInvalidArgument([&] { construct(&facet, nullptr, 0, true); }), "a null locale name throws");
    }

    TFacet facet{};
    facet.base = {&g_recordingVtable, 5, 0};
    api.init(&facet, &locinfo, true);
    Require(HasValues(facet, &g_recordingVtable, 5, ',', ""), "_Init sets the members and keeps the base");
    Require(ThrowsInvalidArgument([&] { api.init(&facet, nullptr, true); }), "_Init rejects a null _Locinfo");
    Require(ThrowsInvalidArgument([&] { api.init(nullptr, &locinfo, true); }), "_Init rejects a null object");

    api.current1(&facet, 1);
    Require(facet.base.vtable->release(&facet.base) == &facet.base && facet.base.references == 0, "the constructed facet counts its references from refs");
    const TFacet before = facet;
    api.complete(&facet);
    api.base(&facet);
    Require(std::memcmp(&before, &facet, sizeof(facet)) == 0, "D1 and D2 leave the object");
    auto* heap = new (_Znwm_nid_postfix(sizeof(TFacet))) TFacet{facet};
    releases = 0;
    api.deleting(heap);
    Require(releases == 1 && lastReleased == heap, "D0 frees the object through operator delete");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
}

void CheckNarrowNumpunctThroughLibc() {
    GuestLocale::NumpunctFacet facet{};
    _ZNSt8numpunctIcEC1Em_nid_postfix(&facet, 0);
    Require(_ZNKSt8numpunctIcE13decimal_pointEv_nid_postfix(&facet) == '.' && _ZNKSt8numpunctIcE13thousands_sepEv_nid_postfix(&facet) == ',', "a constructed numpunct<char> answers through libc's virtuals");
    String text{};
    _ZNKSt8numpunctIcE9falsenameEv_nid_postfix(&text, &facet);
    Require(NarrowIs(text, "false"), "a constructed numpunct<char> names false");

    GuestLocale::WideNumpunctFacet wide{};
    _ZNSt8numpunctIwEC2Em_nid_postfix(&wide, 0);
    text = {};
    _ZNKSt8numpunctIwE8truenameEv_nid_postfix(&text, &wide);
    Require(WideIs(text, u"true") && _ZNKSt8numpunctIwE13decimal_pointEv_nid_postfix(&wide) == u'.', "a constructed numpunct<wchar_t> answers through libc's virtuals");
}

struct MessagesApi {
    void (APS5_VABI* current1)(GuestLocale::Facet*, std::size_t);
    void (APS5_VABI* current2)(GuestLocale::Facet*, std::size_t);
    void (APS5_VABI* locinfo1)(GuestLocale::Facet*, const GuestLocale::LocinfoStorage*, std::size_t);
    void (APS5_VABI* locinfo2)(GuestLocale::Facet*, const GuestLocale::LocinfoStorage*, std::size_t);
    void (APS5_VABI* named1)(GuestLocale::Facet*, const char*, std::size_t);
    void (APS5_VABI* named2)(GuestLocale::Facet*, const char*, std::size_t);
    Getcat getcat;
    int (APS5_VABI* open)(const GuestLocale::Facet*, const String*, const GuestLocaleFacade*);
    String* (APS5_VABI* get)(String*, const GuestLocale::Facet*, int, int, int, const String*);
    String* (APS5_VABI* doGet)(String*, const GuestLocale::Facet*, int, int, int, const String*);
    void (APS5_VABI* close)(const GuestLocale::Facet*, int);
    String (*make)(const char16_t*);
    bool (*is)(const String&, const char16_t*);
    void (*release)(String&);
};

struct TestMessagesVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI* open)(const void*, const void*, const void*);
    String* (APS5_VABI* get)(String*, const GuestLocale::Facet*, int, int, int, const String*);
    void (APS5_VABI* close)(const void*, int);
};

int APS5_VABI TestOpen(const void*, const void*, const void*) { return 42; }

String* APS5_VABI TestGet(String* result, const GuestLocale::Facet*, int catalog, int set, int message, const String*) {
    *result = Narrow(catalog == 1 && set == 2 && message == 3 ? "routed" : "wrong");
    return result;
}

void APS5_VABI TestClose(const void*, int catalog) { closedCatalog = catalog; }

const TestMessagesVtable g_testMessagesVtable{g_recordingVtable, TestOpen, TestGet, TestClose};

String NarrowFromWide(const char16_t* text) {
    std::string narrow;
    for (; *text != 0; ++text) narrow.push_back(static_cast<char>(*text));
    return Narrow(narrow.c_str());
}

bool NarrowIsWide(const String& text, const char16_t* expected) {
    std::string narrow;
    for (; *expected != 0; ++expected) narrow.push_back(static_cast<char>(*expected));
    return NarrowIs(text, narrow.c_str());
}

void NarrowRelease(String& text) {
    GuestLocale::ReleaseStringStorage(text);
}

void CheckMessages(const MessagesApi& api) {
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");
    GuestLocale::Facet constructed{};
    api.current1(&constructed, 9);
    const GuestLocale::FacetVtable* vtable = constructed.vtable;
    Require(vtable != nullptr && constructed.references == 9, "messages(refs) sets the vtable and the reference count");
    for (auto construct : {api.current1, api.current2}) {
        GuestLocale::Facet facet{};
        construct(&facet, 4);
        Require(facet.vtable == vtable && facet.references == 4 && facet.reserved == 0, "messages(refs) builds the facet");
    }
    for (auto construct : {api.locinfo1, api.locinfo2}) {
        GuestLocale::Facet facet{};
        construct(&facet, &locinfo, 2);
        Require(facet.vtable == vtable && facet.references == 2, "messages(_Locinfo, refs) builds the facet");
        Require(ThrowsInvalidArgument([&] { construct(&facet, nullptr, 0); }), "a null _Locinfo throws");
    }
    for (auto construct : {api.named1, api.named2}) {
        GuestLocale::Facet facet{};
        construct(&facet, "C", 1);
        Require(facet.vtable == vtable && facet.references == 1, "messages(\"C\", refs) builds the facet");
        Require(ThrowsInvalidArgument([&] { construct(&facet, "de_DE", 0); }), "a locale other than C throws");
        Require(ThrowsInvalidArgument([&] { construct(nullptr, "C", 0); }), "a null object throws");
    }

    String catalog = Narrow("catalog");
    Require(api.open(&constructed, &catalog, _ZNSt6locale7classicEv_nid_postfix()) == -1, "open reaches do_open, which opens no catalog");
    api.close(&constructed, -1);
    String fallback = api.make(u"fallback");
    String result{};
    Require(api.get(&result, &constructed, -1, 1, 2, &fallback) == &result && api.is(result, u"fallback"), "get returns the default string through do_get");
    api.release(result);
    String longFallback = api.make(u"a default message that does not fit inline");
    result = {};
    api.doGet(&result, &constructed, 0, 0, 0, &longFallback);
    Require(api.is(result, u"a default message that does not fit inline") && NarrowData(result) != NarrowData(longFallback), "do_get copies a long default into its own storage");
    api.release(result);
    api.release(longFallback);
    api.release(fallback);
    Require(ThrowsInvalidArgument([&] { api.doGet(&result, &constructed, 0, 0, 0, nullptr); }), "do_get rejects a null default");
    Require(ThrowsInvalidArgument([&] { api.get(&result, nullptr, 0, 0, 0, &catalog); }), "get rejects a null facet");

    GuestLocale::Facet routed{&g_testMessagesVtable.facet, 1, 0};
    Require(api.open(&routed, &catalog, _ZNSt6locale7classicEv_nid_postfix()) == 42, "open calls the facet's do_open");
    result = {};
    api.get(&result, &routed, 1, 2, 3, &catalog);
    Require(NarrowIs(result, "routed"), "get calls the facet's do_get with its arguments");
    closedCatalog = 0;
    api.close(&routed, 77);
    Require(closedCatalog == 77, "close calls the facet's do_close");
    NarrowRelease(catalog);

    GuestLocale::Facet* created = nullptr;
    Require(api.getcat(&created, Classic()) == 6 && created != nullptr && created->vtable == vtable && created->references == 0, "_Getcat builds a messages facet for category 6");
    GuestLocale::Facet* existing = created;
    Require(api.getcat(&existing, Classic()) == 6 && existing == created, "_Getcat keeps a facet already built");
    Require(api.getcat(nullptr, Classic()) == 6, "_Getcat without a facet slot returns the category");
    const GuestLocale::Implementation other{{&g_recordingVtable, 1, 0}, nullptr, 0, 0, false, "ja_JP"};
    const GuestLocale::Implementation* otherLocale = &other;
    GuestLocale::Facet* none = nullptr;
    Require(ThrowsInvalidArgument([&] { api.getcat(&none, &otherLocale); }) && none == nullptr, "_Getcat throws for a locale other than C");
    Require(ThrowsInvalidArgument([&] { api.getcat(&none, nullptr); }), "_Getcat throws for a null locale");
    created->vtable->retain(created);
    Require(created->references == 1 && created->vtable->release(created) == created, "the facet counts references through _Incref and _Decref");
    releases = 0;
    created->vtable->deleteObject(created);
    Require(releases == 1 && lastReleased == created, "the facet's deleting destructor frees it through operator delete");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
}

struct CollateApi {
    void (APS5_VABI* current1)(GuestLocale::CollateFacet*, std::size_t);
    void (APS5_VABI* current2)(GuestLocale::CollateFacet*, std::size_t);
    void (APS5_VABI* locinfo1)(GuestLocale::CollateFacet*, const GuestLocale::LocinfoStorage*, std::size_t);
    void (APS5_VABI* locinfo2)(GuestLocale::CollateFacet*, const GuestLocale::LocinfoStorage*, std::size_t);
    void (APS5_VABI* named1)(GuestLocale::CollateFacet*, const char*, std::size_t);
    void (APS5_VABI* named2)(GuestLocale::CollateFacet*, const char*, std::size_t);
    void (APS5_VABI* init)(GuestLocale::CollateFacet*, const GuestLocale::LocinfoStorage*);
    Getcat getcat;
    const char* low;
    const char* high;
};

const void* g_marker = &g_marker;

bool IsCollate(const GuestLocale::CollateFacet& facet, const GuestLocale::FacetVtable* vtable, std::uint32_t references) {
    return facet.base.vtable == vtable && facet.base.references == references && facet.collation == nullptr && facet.wideCollation == nullptr;
}

void CheckCollate(const CollateApi& api, std::size_t unit) {
    const GuestLocale::FacetVtable* vtable = LibcVtable(api.getcat);
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");
    for (auto construct : {api.current1, api.current2}) {
        GuestLocale::CollateFacet facet{{nullptr, 0, 0}, g_marker, g_marker};
        construct(&facet, 6);
        Require(IsCollate(facet, vtable, 6), "collate(refs) builds libc's facet with no collation tables");
    }
    for (auto construct : {api.locinfo1, api.locinfo2}) {
        GuestLocale::CollateFacet facet{{nullptr, 0, 0}, g_marker, g_marker};
        construct(&facet, &locinfo, 1);
        Require(IsCollate(facet, vtable, 1), "collate(_Locinfo, refs) builds libc's facet");
        Require(ThrowsInvalidArgument([&] { construct(&facet, nullptr, 0); }), "a null _Locinfo throws");
    }
    for (auto construct : {api.named1, api.named2}) {
        GuestLocale::CollateFacet facet{{nullptr, 0, 0}, g_marker, g_marker};
        construct(&facet, "C", 2);
        Require(IsCollate(facet, vtable, 2), "collate(\"C\", refs) builds libc's facet");
        Require(ThrowsInvalidArgument([&] { construct(&facet, "en_US", 0); }), "a locale other than C throws");
        Require(ThrowsInvalidArgument([&] { construct(nullptr, "C", 0); }), "a null object throws");
    }
    GuestLocale::CollateFacet facet{{&g_recordingVtable, 3, 0}, g_marker, g_marker};
    api.init(&facet, &locinfo);
    Require(IsCollate(facet, &g_recordingVtable, 3), "_Init clears the collation tables and keeps the base");
    Require(ThrowsInvalidArgument([&] { api.init(&facet, nullptr); }), "_Init rejects a null _Locinfo");

    api.current1(&facet, 0);
    const auto& collate = *reinterpret_cast<const GuestLocale::CollateVtable*>(facet.base.vtable);
    Require(collate.compare(&facet, api.low, api.low + unit, api.high, api.high + unit) == -1 && collate.compare(&facet, api.high, api.high + unit, api.low, api.low + unit) == 1,
        "the constructed facet compares through libc's virtuals");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
}

int APS5_VABI TestIn(GuestLocale::Facet*, std::uint64_t*, const char*, const char*, const char**, char*, char*, char**) { return 1; }
int APS5_VABI TestOut(GuestLocale::Facet*, std::uint64_t*, const char*, const char*, const char**, char*, char*, char**) { return 2; }
int APS5_VABI TestUnshift(GuestLocale::Facet*, std::uint64_t*, char*, char*, char**) { return 1; }
std::size_t APS5_VABI TestLength(GuestLocale::Facet*, std::uint64_t*, const char*, const char*, std::size_t) { return 99; }
bool APS5_VABI TestNoconv(const GuestLocale::Facet*) { return false; }
int APS5_VABI TestOne(const GuestLocale::Facet*) { return 1; }

const GuestLocale::CodecvtVtable g_testCodecvtVtable{g_recordingVtable, TestNoconv, TestOne, TestOne, TestIn, TestOut, TestUnshift, TestLength};

void CheckCodecvt() {
    const GuestLocale::FacetVtable* vtable = LibcVtable(&_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix);
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");
    for (auto construct : {&_ZNSt7codecvtIcc9_MbstatetEC1Em_nid_postfix, &_ZNSt7codecvtIcc9_MbstatetEC2Em_nid_postfix}) {
        GuestLocale::Facet facet{};
        construct(&facet, 8);
        Require(facet.vtable == vtable && facet.references == 8, "codecvt(refs) builds libc's facet");
    }
    for (auto construct : {&_ZNSt7codecvtIcc9_MbstatetEC1ERKSt8_Locinfom_nid_postfix, &_ZNSt7codecvtIcc9_MbstatetEC2ERKSt8_Locinfom_nid_postfix}) {
        GuestLocale::Facet facet{};
        construct(&facet, &locinfo, 1);
        Require(facet.vtable == vtable && facet.references == 1, "codecvt(_Locinfo, refs) builds libc's facet");
        Require(ThrowsInvalidArgument([&] { construct(&facet, nullptr, 0); }), "a null _Locinfo throws");
    }
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);

    GuestLocale::Facet facet{};
    _ZNSt7codecvtIcc9_MbstatetEC1Em_nid_postfix(&facet, 0);
    std::uint64_t state = 0;
    const char input[] = "abcdef";
    char output[8] = {};
    const char* next1 = nullptr;
    char* next2 = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(&facet), "do_always_noconv is true");
    Require(_ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, input, input + 6, &next1, output, output + 8, &next2) == 3 && next1 == input && next2 == output && output[0] == 0,
        "do_in converts nothing and reports noconv");
    next1 = nullptr;
    next2 = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, input + 1, input + 6, &next1, output + 2, output + 8, &next2) == 3 && next1 == input + 1 && next2 == output + 2,
        "do_out converts nothing and reports noconv");
    next2 = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(&facet, &state, output + 3, output + 8, &next2) == 0 && next2 == output + 3, "do_unshift writes nothing and reports ok");
    Require(_ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(&facet, &state, input, input + 6, 4) == 4, "do_length stops at the count");
    Require(_ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(&facet, &state, input, input + 6, 100) == 6, "do_length stops at the end of the range");
    Require(_ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(&facet, &state, nullptr, nullptr, 5) == 0, "do_length of an empty range is 0");
    Require(ThrowsInvalidArgument([&] { _ZNKSt7codecvtIcc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix(&facet, &state, input + 2, input, 5); }), "do_length rejects a reversed range");
    Require(ThrowsInvalidArgument([&] { _ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, input, input, nullptr, output, output, &next2); }), "do_in rejects a null next pointer");
    Require(ThrowsInvalidArgument([&] { _ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(&facet, &state, output, output, nullptr); }), "do_unshift rejects a null next pointer");

    next1 = nullptr;
    next2 = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, input, input + 6, &next1, output, output + 8, &next2) == 3 && next1 == input && next2 == output, "in reaches do_in");
    Require(_ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, input, input + 6, &next1, output, output + 8, &next2) == 3, "out reaches do_out");
    Require(_ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix(&facet, &state, output, output + 8, &next2) == 0, "unshift reaches do_unshift");
    Require(_ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix(&facet, &state, input, input + 6, 2) == 2, "length reaches do_length");

    GuestLocale::Facet routed{&g_testCodecvtVtable.facet, 1, 0};
    Require(_ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&routed, &state, input, input, &next1, output, output, &next2) == 1, "in calls the facet's do_in");
    Require(_ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&routed, &state, input, input, &next1, output, output, &next2) == 2, "out calls the facet's do_out");
    Require(_ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix(&routed, &state, output, output, &next2) == 1, "unshift calls the facet's do_unshift");
    Require(_ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix(&routed, &state, input, input, 0) == 99, "length calls the facet's do_length");
    Require(ThrowsInvalidArgument([&] { _ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(nullptr, &state, input, input, &next1, output, output, &next2); }), "in rejects a null facet");
}

void CheckDeletingDestructors() {
    using Deleting = void (APS5_VABI*)(void*);
    for (Deleting destroy : {&libSceLibcInternalInventory_090_nid_no_patch, &libSceLibcInternalInventory_092_nid_no_patch, &libSceLibcInternalInventory_096_nid_no_patch,
             &libSceLibcInternalInventory_101_nid_no_patch, &libSceLibcInternalInventory_110_nid_no_patch, &libSceLibcInternalInventory_116_nid_no_patch,
             &libSceLibcInternalInventory_124_nid_no_patch, &libSceLibcInternalInventory_131_nid_no_patch, &libSceLibcInternalInventory_136_nid_no_patch,
             &libSceLibcInternalInventory_195_nid_no_patch, &libSceLibcInternalInventory_208_nid_no_patch, &libSceLibcInternalInventory_212_nid_no_patch,
             &libSceLibcInternalInventory_223_nid_no_patch, &libSceLibcInternalInventory_232_nid_no_patch, &_ZNSt23_Generic_error_categoryD0Ev_nid_postfix}) {
        void* object = _Znwm_nid_postfix(0xc0);
        std::memset(object, 0x5a, 0xc0);
        releases = 0;
        destroy(object);
        Require(releases == 1 && lastReleased == object, "a deleting destructor frees the object once through operator delete");
    }
}

void CheckLocinfo() {
    GuestLocale::LocinfoStorage storage{};
    for (auto construct : {&_ZNSt8_LocinfoC1EiPKc_nid_postfix, &_ZNSt8_LocinfoC2EiPKc_nid_postfix}) {
        construct(&storage, 0x3f, "C");
        _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
        construct(&storage, 4, "C");
        _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
        Require(ThrowsInvalidArgument([&] { construct(&storage, 0x3f, "POSIX"); }), "_Locinfo(category, name) rejects a locale other than C");
        Require(ThrowsInvalidArgument([&] { construct(&storage, 0x3f, nullptr); }), "_Locinfo(category, name) rejects a null name");
    }
    for (auto construct : {&_ZNSt8_LocinfoC1ERKSs_nid_postfix, &_ZNSt8_LocinfoC2ERKSs_nid_postfix}) {
        String name = Narrow("C");
        construct(&storage, &name);
        _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
        String longName = Narrow("C.UTF-8@a_long_modifier");
        Require(ThrowsInvalidArgument([&] { construct(&storage, &longName); }), "_Locinfo(string) rejects a locale other than C");
        GuestLocale::ReleaseStringStorage(longName);
        Require(ThrowsInvalidArgument([&] { construct(&storage, nullptr); }), "_Locinfo(string) rejects a null string");
    }
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, "C");
    Require(_ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(&storage, 0x3f, "C") == &storage, "_Addcats adds the C locale and returns the object");
    Require(ThrowsInvalidArgument([&] { _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(&storage, 1, "fr_FR"); }), "_Addcats rejects a locale other than C");
    Require(ThrowsInvalidArgument([&] { _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(&storage, 1, nullptr); }), "_Addcats rejects a null name");
    Require(ThrowsInvalidArgument([&] { _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(nullptr, 1, "C"); }), "_Addcats rejects a null object");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
}

void CheckLStrcoll() {
    const char abc[] = "abc";
    const char abd[] = "abd";
    const char high[] = "\x80";
    const char low[] = "\x7f";
    const auto narrow = &_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix;
    Require(narrow(abc, abc + 3, abc, abc + 3, nullptr) == 0, "equal sequences compare equal");
    Require(narrow(abc, abc + 3, abd, abd + 3, nullptr) == -1 && narrow(abd, abd + 3, abc, abc + 3, nullptr) == 1, "the first differing character orders the sequences");
    Require(narrow(abc, abc + 2, abc, abc + 3, nullptr) == -1 && narrow(abc, abc + 3, abc, abc + 2, nullptr) == 1, "a proper prefix orders first");
    Require(narrow(high, high + 1, low, low + 1, nullptr) == 1, "characters compare as unsigned values");
    Require(narrow(nullptr, nullptr, nullptr, nullptr, nullptr) == 0 && narrow(nullptr, nullptr, abc, abc + 1, nullptr) == -1, "empty ranges compare first");
    Require(ThrowsInvalidArgument([&] { narrow(abc + 1, abc, abc, abc, nullptr); }), "a reversed range throws");
    Require(ThrowsInvalidArgument([&] { narrow(abc, abc, nullptr, abc, nullptr); }), "a range with one null end throws");

    const char16_t first[] = u"\x00ff";
    const char16_t second[] = u"\x0100";
    const char16_t top[] = u"\xffff";
    const auto wide = &_ZSt9_LStrcollIwEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix;
    Require(wide(first, first + 1, second, second + 1, nullptr) == -1 && wide(second, second + 1, first, first + 1, nullptr) == 1, "wide characters compare as 16-bit values");
    Require(wide(top, top + 1, first, first + 1, nullptr) == 1, "wide characters compare as unsigned values");
    Require(wide(first, first + 1, first, first + 1, nullptr) == 0 && wide(first, first, first, first + 1, nullptr) == -1, "wide prefixes order first");
    Require(ThrowsInvalidArgument([&] { wide(first + 1, first, first, first, nullptr); }), "a reversed wide range throws");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckFacetReferences();
    CheckLocaleDestructor();
    CheckWideNumpunctMembers();
    CheckNumpunctConstructors(NumpunctApi<GuestLocale::NumpunctFacet>{
        &_ZNSt8numpunctIcEC1Em_nid_postfix, &_ZNSt8numpunctIcEC2Em_nid_postfix,
        &_ZNSt8numpunctIcEC1ERKSt8_Locinfomb_nid_postfix, &_ZNSt8numpunctIcEC2ERKSt8_Locinfomb_nid_postfix,
        &_ZNSt8numpunctIcEC1EPKcmb_nid_postfix, &_ZNSt8numpunctIcEC2EPKcmb_nid_postfix,
        &_ZNSt8numpunctIcE5_InitERKSt8_Locinfob_nid_postfix,
        &_ZNSt8numpunctIcED1Ev_nid_postfix, &_ZNSt8numpunctIcED2Ev_nid_postfix, &_ZNSt8numpunctIcED0Ev_nid_postfix,
        &_ZNSt8numpunctIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix});
    CheckNumpunctConstructors(NumpunctApi<GuestLocale::WideNumpunctFacet>{
        &_ZNSt8numpunctIwEC1Em_nid_postfix, &_ZNSt8numpunctIwEC2Em_nid_postfix,
        &_ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix, &_ZNSt8numpunctIwEC2ERKSt8_Locinfomb_nid_postfix,
        &_ZNSt8numpunctIwEC1EPKcmb_nid_postfix, &_ZNSt8numpunctIwEC2EPKcmb_nid_postfix,
        &_ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix,
        &_ZNSt8numpunctIwED1Ev_nid_postfix, &_ZNSt8numpunctIwED2Ev_nid_postfix, &_ZNSt8numpunctIwED0Ev_nid_postfix,
        &_ZNSt8numpunctIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix});
    CheckNarrowNumpunctThroughLibc();
    CheckMessages(MessagesApi{
        &_ZNSt8messagesIcEC1Em_nid_postfix, &_ZNSt8messagesIcEC2Em_nid_postfix,
        &_ZNSt8messagesIcEC1ERKSt8_Locinfom_nid_postfix, &_ZNSt8messagesIcEC2ERKSt8_Locinfom_nid_postfix,
        &_ZNSt8messagesIcEC1EPKcm_nid_postfix, &_ZNSt8messagesIcEC2EPKcm_nid_postfix,
        &_ZNSt8messagesIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix,
        &_ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix, &_ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix,
        &_ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix, &_ZNKSt8messagesIcE5closeEi_nid_postfix,
        &NarrowFromWide, &NarrowIsWide, &NarrowRelease});
    CheckMessages(MessagesApi{
        &_ZNSt8messagesIwEC1Em_nid_postfix, &_ZNSt8messagesIwEC2Em_nid_postfix,
        &_ZNSt8messagesIwEC1ERKSt8_Locinfom_nid_postfix, &_ZNSt8messagesIwEC2ERKSt8_Locinfom_nid_postfix,
        &_ZNSt8messagesIwEC1EPKcm_nid_postfix, &_ZNSt8messagesIwEC2EPKcm_nid_postfix,
        &_ZNSt8messagesIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix,
        &_ZNKSt8messagesIwE4openERKSsRKSt6locale_nid_postfix, &_ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix,
        &_ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix, &_ZNKSt8messagesIwE5closeEi_nid_postfix,
        &Wide, &WideIs, &WideRelease});
    const char16_t wideLow[] = u"a";
    const char16_t wideHigh[] = u"b";
    CheckCollate(CollateApi{
        &_ZNSt7collateIcEC1Em_nid_postfix, &_ZNSt7collateIcEC2Em_nid_postfix,
        &_ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix, &_ZNSt7collateIcEC2ERKSt8_Locinfom_nid_postfix,
        &_ZNSt7collateIcEC1EPKcm_nid_postfix, &_ZNSt7collateIcEC2EPKcm_nid_postfix,
        &_ZNSt7collateIcE5_InitERKSt8_Locinfo_nid_postfix,
        &_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix, "a", "b"}, 1);
    CheckCollate(CollateApi{
        &_ZNSt7collateIwEC1Em_nid_postfix, &_ZNSt7collateIwEC2Em_nid_postfix,
        &_ZNSt7collateIwEC1ERKSt8_Locinfom_nid_postfix, &_ZNSt7collateIwEC2ERKSt8_Locinfom_nid_postfix,
        &_ZNSt7collateIwEC1EPKcm_nid_postfix, &_ZNSt7collateIwEC2EPKcm_nid_postfix,
        &_ZNSt7collateIwE5_InitERKSt8_Locinfo_nid_postfix,
        &_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix,
        reinterpret_cast<const char*>(wideLow), reinterpret_cast<const char*>(wideHigh)}, sizeof(char16_t));
    CheckCodecvt();
    CheckDeletingDestructors();
    CheckLocinfo();
    CheckLStrcoll();

    std::puts("libc internal locale facets: ok");
    return 0;
}
