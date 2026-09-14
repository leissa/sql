#include "sql/lexer.h"

#include <charconv>

#include <limits>
#include <ranges>

using namespace std::literals;

namespace sql {

namespace utf8 = fe::utf8;

Lexer::Lexer(Driver& driver, const fe::Src& src)
    : fe::Lexer<1, Lexer>(src)
    , driver_(driver)
    , keys_(driver.keys()) {}

// The one hot loop in the whole parser: `accept` and utf8::decode have to end up inside it. GCC
// budgets inlining per translation unit, and this one is small, so say so outright.
#if defined(__GNUC__) || defined(__clang__)
[[gnu::flatten]]
#endif
Tok Lexer::lex() {
    while (true) {
        start();

        if (accept(utf8::EoF)) return {loc_, Tok::Tag::EoF};
        if (accept(utf8::isspace)) continue;
        if (recover_utf8()) continue;
        if (accept('{')) return {loc_, Tok::Tag::D_brace_l};
        if (accept('}')) return {loc_, Tok::Tag::D_brace_r};
        if (accept('[')) return {loc_, Tok::Tag::D_brckt_l};
        if (accept(']')) return {loc_, Tok::Tag::D_brckt_r};
        if (accept('(')) return {loc_, Tok::Tag::D_paren_l};
        if (accept(')')) return {loc_, Tok::Tag::D_paren_r};
        if (accept('<')) {
            if (accept('>')) return {loc_, Tok::Tag::T_ne};
            if (accept('=')) return {loc_, Tok::Tag::T_le};
            return {loc_, Tok::Tag::T_l};
        }
        if (accept('!')) {
            if (accept('=')) return {loc_, Tok::Tag::T_ue};
            error().e(peek(), "invalid input following `!`: `{}`", (char)ahead());
        }
        if (accept('>')) {
            if (accept('=')) return {loc_, Tok::Tag::T_ge};
            return {loc_, Tok::Tag::T_g};
        }
        if (accept('=')) return {loc_, Tok::Tag::T_eq};
        if (accept(',')) return {loc_, Tok::Tag::T_comma};
        if (accept('.')) {
            if (utf8::isdigit(ahead())) return lex_num();
            return {loc_, Tok::Tag::T_dot};
        }
        if (accept(';')) return {loc_, Tok::Tag::T_semicolon};
        if (accept(':')) {
            if (accept('=')) return {loc_, Tok::Tag::T_assign};
            // `:name` is a named parameter marker - one character of lookahead settles it.
            if (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c); })) {
                while (accept<Append::Lower>(
                    [](char32_t c) { return c == '_' || utf8::isalpha(c) || utf8::isdigit(c); })) {}
                return {loc_, Tok::Tag::V_param, driver_.sym(str())};
            }
            return {loc_, Tok::Tag::T_colon};
        }
        if (accept('+')) return {loc_, Tok::Tag::T_add};
        if (accept('*')) return {loc_, Tok::Tag::T_mul};
        if (accept('%')) return {loc_, Tok::Tag::T_mod};
        if (accept('|')) {
            if (accept('|')) return {loc_, Tok::Tag::T_concat};
            error().e(peek(), "invalid input following `|`: `{}`", (char)ahead());
            continue;
        }

        // A dynamic parameter marker: `?`, `$1`, or `:name`. Sym holds the marker verbatim.
        if (accept('?')) return {loc_, Tok::Tag::V_param, driver_.sym(str())};
        if (accept('$')) {
            while (accept(utf8::isdigit)) {}
            return {loc_, Tok::Tag::V_param, driver_.sym(str())};
        }

        // sub or single-line comment
        if (accept('-')) {
            if (accept('-')) {
                while (ahead() != utf8::EoF && ahead() != '\n')
                    next();
                continue;
            }
            return {loc_, Tok::Tag::T_sub};
        }

        // div or multi-line comment
        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            return {loc_, Tok::Tag::T_div};
        }

        // integer or real value
        if (utf8::isdigit(ahead())) return lex_num();

        // lex identifier or keyword
        if (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c); })) {
            while (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c) || utf8::isdigit(c); })) {
            }
            auto sym = driver_.sym(str());
            if (auto i = keys_.find(sym); i != keys_.end()) return {loc_, i->second}; // keyword
            return {loc_, sym};                                                       // identifier
        }

        // string literal or - double-quoted, hence case-sensitive - delimited identifier
        if (accept<Append::Off>('\'')) return lex_str('\'', Tok::Tag::V_str);
        if (accept<Append::Off>('\"')) return lex_str('\"', Tok::Tag::V_id);

        recover_char();
    }
}

/// Lexes a numeric literal. A `.` or an exponent makes it a Tok::Tag::V_real, whose Sym keeps the
/// literal verbatim - that is what lets the printer emit it back unchanged.
/// @note Lexer::lex has already consumed a leading `.`, if there was one.
Tok Lexer::lex_num() {
    bool real = str() == ".";
    while (accept(utf8::isdigit)) {}
    if (!real && accept('.')) {
        real = true;
        while (accept(utf8::isdigit)) {}
    }
    if (accept([](char32_t c) { return c == 'e' || c == 'E'; })) {
        real = true;
        if (!accept('+')) accept('-');
        if (!accept(utf8::isdigit)) error().e(loc_, "exponent of a numeric literal has no digits");
        while (accept(utf8::isdigit)) {}
    }

    if (!real) {
        uint64_t u64 = std::numeric_limits<uint64_t>::max(); // std::from_chars leaves it alone on overflow
        std::from_chars(str().data(), str().data() + str().size(), u64);
        return {loc_, u64};
    }
    return {loc_, Tok::Tag::V_real, driver_.sym(str())};
}

Tok Lexer::lex_str(char32_t delim, Tok::Tag tag) {
    while (true) {
        if (accept<Append::Off>(delim)) {
            if (!accept<Append::Off>(delim)) break;
            append((char)delim);
        } else if (ahead() == utf8::EoF) {
            error().e(loc_, "unterminated string literal");
            break;
        } else {
            lex_char();
        }
    }

    return {loc_, tag, driver_.sym(str())};
}

void Lexer::lex_char() {
    if (accept<Append::Off>('\\')) {
        // clang-format off
        if      (accept<Append::Off>('\'')) append('\'');
        else if (accept<Append::Off>('\\')) append('\\');
        else if (accept<Append::Off>( '"')) append('\"');
        else if (accept<Append::Off>( '0')) append('\0');
        else if (accept<Append::Off>( 'a')) append('\a');
        else if (accept<Append::Off>( 'b')) append('\b');
        else if (accept<Append::Off>( 'f')) append('\f');
        else if (accept<Append::Off>( 'n')) append('\n');
        else if (accept<Append::Off>( 'r')) append('\r');
        else if (accept<Append::Off>( 't')) append('\t');
        else if (accept<Append::Off>( 'v')) append('\v');
        else error().e(loc_.anew_end(), "invalid escape character `\\{}`", (char)ahead());
        // clang-format on
        return;
    }

    // The original bytes, not `char32_t`: a multi-byte code point must survive verbatim.
    auto loc = peek();
    append(buf_.substr(loc.begin.off, loc.size()));
    next();
}

void Lexer::eat_comments() {
    while (true) {
        while (ahead() != utf8::EoF && ahead() != '*')
            next();
        if (ahead() == utf8::EoF) {
            error().e(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace sql
