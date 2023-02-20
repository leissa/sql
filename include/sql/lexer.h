#pragma once

#include <cassert>

#include <istream>
#include <unordered_map>

#include "sql/driver.h"
#include "sql/sym.h"
#include "sql/tok.h"

namespace sql {

class Lexer {
public:
    Lexer(Driver& drv, Sym filename, std::istream&);

    Loc loc() const { return loc_; }
    Tok lex(); ///< Get next Tok in stream.
    Driver& driver() { return driver_; }

private:
    Tok tok(Tok::Tag tag) { return {loc(), tag}; } ///< Factory method to create a Tok.
    bool eof() const {
        peek();
        return stream_.eof();
    } ///< Have we reached the end of file?

    /// @return `true` if @p pred holds.
    /// In this case invoke Lexer::next() and append to Lexer::str_.
    template<class Pred>
    bool accept_if(Pred pred, bool lower = false) {
        if (pred(peek())) {
            str_ += lower ? std::tolower(next()) : next();
            return true;
        }
        return false;
    }

    bool accept(int val) {
        return accept_if([val](int p) { return p == val; });
    }

    /// Get next byte in @p stream_ and increase @p loc_ / @p peek_pos_.
    int next();
    int peek() const { return stream_.peek(); }
    void eat_comments();

    Driver& driver_;
    Loc loc_;      ///< Loc%ation of the Tok%en we are currently constructing within Lexer::str_,
    Pos peek_pos_; ///< Pos%ition of the current Lexer::peek().
    std::istream& stream_;
    std::string str_;
    SymMap<Tok::Tag> keywords_;
};

} // namespace sql
