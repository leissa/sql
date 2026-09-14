#pragma once

#include <array>

#include <fe/driver.h>

#include "sql/tok.h"

namespace sql {

/// The reserved words the Lexer looks up, keyed by the Sym it has just interned.
using Keys = fe::SymTab<Tok::Tag, Num_Keys>;

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
