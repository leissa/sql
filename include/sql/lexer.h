#pragma once

#include <cassert>

#include <fe/lexer.h>

#include "sql/driver.h"
#include "sql/tok.h"

namespace sql {

class Lexer : public fe::Lexer<1, Lexer> {
public:
    Lexer(Driver&, const fe::Src&);

    Tok lex(); ///< Get next Tok in stream.
    Driver& driver() { return driver_; }

private:
    void eat_comments();
    /// Lex the body of a @p delim-quoted literal; a doubled @p delim escapes one occurrence of it.
    Tok lex_str(char32_t delim, Tok::Tag);
    void lex_char();

    Driver& driver_;
    fe::SymMap<Tok::Tag> keywords_;
};

} // namespace sql
