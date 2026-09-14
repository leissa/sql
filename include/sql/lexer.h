#pragma once

#include <cassert>

#include <array>

#include <fe/lexer.h>

#include "sql/driver.h"
#include "sql/tok.h"

namespace sql {

class Lexer : public fe::Lexer<1, Lexer> {
public:
    Lexer(Driver&, const fe::Src&);

    Tok lex();                           ///< Get next Tok in stream.
    Driver& driver() { return driver_; } ///< fe::Lexer's default diagnostics go to its Driver::error.

private:
    void eat_comments();
    Sym lex_word(); ///< Lex an identifier-shaped word and intern it, case-folded.
    Tok lex_num();  ///< Lex an integer or real literal.
    Tok lex_str(char32_t delim, Tok::Tag);
    Sym sym_str(uint32_t begin, uint32_t end, char32_t delim, bool esc);
    std::string unquote(std::string_view body, uint32_t begin, char32_t delim);

    std::array<char, 64> word_; ///< Scratch for Lexer::lex_word; a longer word goes through fe::Lexer::lower.
    Driver& driver_;
    const Keys& keys_; ///< The Driver's reserved words - see Driver::keys.
};

} // namespace sql
