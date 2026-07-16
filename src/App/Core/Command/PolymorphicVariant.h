#pragma once
#include "DTL.h"
#include <concepts>
#include <type_traits>
#include <utility>

// A closed std::variant of concrete types that ALSO share an abstract base.
//
// This formalizes the pattern the old CommandVariant reached for, and buys both halves
// at once:
//
//   * exhaustive, compile-checked Visit() over a known type list, and
//   * a uniform Base& for code that has no business knowing the variant exists.
//
// Get() returns a reference to the LIVE subobject — std::visit hands out a reference to
// the stored alternative and we upcast it, so nothing is ever sliced or copied.
//
// Why not `Base` behind a unique_ptr:
//   * vector<PolymorphicVariant<...>> is contiguous — no per-command heap allocation.
//   * it stays copyable, so deep-copying history for edit mode is `a = b;`. No clone()
//     virtual, no move-only restriction.
//   * serialization becomes a variant-index switch, not a factory/registry lookup.
//
// As<T>() uses std::get_if, NOT dynamic_cast: the variant already knows its own type,
// so the check is a tag compare and needs no RTTI. (The base still declares a virtual
// destructor — required for correctness the moment anyone holds a Base&.)
//
// Recursion: an alternative may contain std::vector<Outer> where Outer is the derived
// variant type, still incomplete at that point. std::vector tolerates an incomplete
// element type so long as it is complete before any member is instantiated. Forward
// declare Outer, use it inside the composite, and define the composite's out-of-line
// members after Outer is complete. See SketchCmd.h for the worked example.
//
// Note on -fno-exceptions: std::visit can in principle throw bad_variant_access on a
// valueless variant, but valueless_by_exception is only reachable via a THROWING move
// constructor. With exceptions disabled that state is unreachable, so the throw path is
// dead code. This is load-bearing, not incidental.
template <typename Base, typename... Ts>
requires(std::derived_from<Ts, Base>&&...) class PolymorphicVariant {
public:
    using BaseType = Base;
    using VariantType = DTL::Variant<Ts...>;

    PolymorphicVariant() = default;

    // Construct from any alternative. Constrained away from copy/move so those still
    // bind to the implicit copy/move constructors rather than this template.
    template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, PolymorphicVariant>) && (std::same_as<std::remove_cvref_t<T>, Ts> || ...) PolymorphicVariant(T&& v)
        : storage(std::forward<T>(v))
    {
    }

    // The live subobject as its shared base. Never slices.
    Base& Get()
    {
        return std::visit([](auto& v) -> Base& { return static_cast<Base&>(v); }, storage);
    }
    const Base& Get() const
    {
        return std::visit([](const auto& v) -> const Base& { return static_cast<const Base&>(v); }, storage);
    }

    Base* operator->() { return &Get(); }
    const Base* operator->() const { return &Get(); }
    Base& operator*() { return Get(); }
    const Base& operator*() const { return Get(); }

    // Narrow to a concrete alternative; nullptr when it holds something else.
    template <typename T>
    T* As() { return std::get_if<T>(&storage); }
    template <typename T>
    const T* As() const { return std::get_if<T>(&storage); }

    template <typename T>
    bool Is() const { return std::holds_alternative<T>(storage); }

    // Which alternative is active. Stable for a given type list — this is the tag a
    // serializer writes.
    u32 Index() const { return static_cast<u32>(storage.index()); }

    template <typename F>
    decltype(auto) Visit(F&& f) { return std::visit(std::forward<F>(f), storage); }
    template <typename F>
    decltype(auto) Visit(F&& f) const { return std::visit(std::forward<F>(f), storage); }

    VariantType& Storage() { return storage; }
    const VariantType& Storage() const { return storage; }

private:
    VariantType storage;
};
