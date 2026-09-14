#pragma once

#include <cassert>

#include <array>
#include <bit>

#include <fe/driver.h>

#include "sql/tok.h"

namespace sql {

/// The reserved words, keyed by the Sym the Lexer has just interned.
/// Not a fe::SymMap: that one hashes and compares the very same interned pointer, but pays a
/// splitmix64 and a SwissTable group probe for it - four times the instructions of one open probe.
class Keys {
public:
    static constexpr size_t Num_Slots = std::bit_ceil(size_t(2 * Num_Keys)); ///< Keeps the load factor below 1/2.
    static constexpr size_t Log_Slots = std::bit_width(Num_Slots) - 1;
    static constexpr size_t Mask      = Num_Slots - 1;
    static constexpr uint64_t Magic   = 0x9E3779B97F4A7C15ull; ///< 2^64/phi - Fibonacci hashing.

    void emplace(Sym sym, Tok::Tag tag) {
        for (auto i = idx(sym);; i = (i + 1) & Mask) {
            assert(slots_[i].sym != sym && "duplicate reserved word");
            if (slots_[i].sym.empty()) {
                slots_[i] = {sym, tag};
                return;
            }
        }
    }

    /// Tok::Tag::Nil if @p sym does not spell a reserved word.
    Tok::Tag operator[](Sym sym) const {
        for (auto i = idx(sym);; i = (i + 1) & Mask) {
            if (slots_[i].sym == sym) return slots_[i].tag;
            if (slots_[i].sym.empty()) return Tok::Tag::Nil;
        }
    }

private:
    struct Slot {
        Sym sym;
        Tok::Tag tag = Tok::Tag::Nil;
    };

    /// Takes the *high* bits of the product: a long Sym is an 8-byte aligned pointer and a short
    /// one's low byte is a size of 1..7, so masking the low bits would cluster.
    static size_t idx(Sym sym) { return (sym.raw() * Magic) >> (64 - Log_Slots); }

    std::array<Slot, Num_Slots> slots_ = {};
};

/// Owns the SymPool - and, with it, everything interned from it once and for all: the reserved words
/// the Lexer looks up, the non-reserved ones the Parser compares against, and the Sym that stands in
/// for one that failed to parse.
/// They live here rather than in the Lexer and the Parser because there are a few hundred of them:
/// interning them anew for every statement would cost far more than parsing it.
class Driver : public fe::Driver {
public:
    Driver();

    template<class T, class... Args>
    auto ast(Args&&... args) {
        return arena_.ref<const T>(std::forward<Args&&>(args)...);
    }

    /// An Arena-allocated copy of @p range - for the few lists that do not sit behind a node.
    template<class R>
    auto copy(const R& range) {
        return arena_.copy(range);
    }

    /// @name Interned words
    /// Built once, in the constructor, and borrowed by every Lexer and Parser this Driver serves.
    ///@{
    const Keys& keys() const { return keys_; }
    const std::array<Sym, Num_Non_Keys>& non_keys() const { return non_keys_; } ///< Indexed by NonKey.
    Sym sym_error() const { return sym_error_; }
    ///@}

private:
    fe::Arena arena_;
    Keys keys_;
    std::array<Sym, Num_Non_Keys> non_keys_;
    Sym sym_error_;
};

} // namespace sql
