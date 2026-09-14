#include "sql/lexer.h"

#include <charconv>

#include <algorithm>
#include <limits>
#include <ranges>

using namespace std::literals;

namespace sql {

namespace utf8 = fe::utf8;

namespace {
/// Most SQL identifiers are already lower-case; the ones that are not are nearly always keywords.
bool needs_fold(std::string_view sv) {
    return std::ranges::any_of(sv, [](char c) { return utf8::isupper(c); });
}
} // namespace

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
            if (utf8::isalpha(ahead()) || ahead() == '_') {
                accept_while([](char32_t c) { return c == '_' || utf8::isalnum(c); });
                return {loc_, Tok::Tag::V_param, needs_fold(view()) ? driver_.sym(lower()) : driver_.sym(view())};
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
        if (accept('?')) return {loc_, Tok::Tag::V_param, driver_.sym(view())};
        if (accept('$')) {
            while (accept(utf8::isdigit)) {}
            return {loc_, Tok::Tag::V_param, driver_.sym(view())};
        }

        // sub or single-line comment
        if (accept('-')) {
            if (accept('-')) {
                accept_while([](char32_t c) { return c != '\n'; });
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
        if (utf8::isalpha(ahead()) || ahead() == '_') {
            accept_while([](char32_t c) { return c == '_' || utf8::isalnum(c); });
            auto sym = needs_fold(view()) ? driver_.sym(lower()) : driver_.sym(view());
            if (auto tag = keys_.find(sym)) return {loc_, *tag}; // keyword
            return {loc_, sym};                                  // identifier
        }

        // string literal or - double-quoted, hence case-sensitive - delimited identifier
        if (accept('\'')) return lex_str('\'', Tok::Tag::V_str);
        if (accept('\"')) return lex_str('\"', Tok::Tag::V_id);

        recover_char();
    }
}

/// Lexes a numeric literal. A `.` or an exponent makes it a Tok::Tag::V_real, whose Sym keeps the
/// literal verbatim - that is what lets the printer emit it back unchanged.
/// @note Lexer::lex has already consumed a leading `.`, if there was one.
Tok Lexer::lex_num() {
    bool real = view() == ".";
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
        std::from_chars(view().data(), view().data() + view().size(), u64);
        return {loc_, u64};
    }
    return {loc_, Tok::Tag::V_real, driver_.sym(view())};
}

/// Lexes the body of a @p delim-quoted literal; a doubled @p delim escapes one occurrence of it.
/// The body is a slice of Lexer::buf_ unless an escape made it diverge - see Lexer::unquote.
Tok Lexer::lex_str(char32_t delim, Tok::Tag tag) {
    auto begin = loc_.end.off; // just past the opening delim
    bool esc   = false;

    while (true) {
        if (accept(delim)) {
            if (!accept(delim)) return {loc_, tag, sym_str(begin, loc_.end.off - 1, delim, esc)};
            esc = true;
        } else if (ahead() == utf8::EoF) {
            error().e(loc_, "unterminated string literal");
            return {loc_, tag, sym_str(begin, loc_.end.off, delim, esc)};
        } else if (accept('\\')) {
            esc = true;
            if (ahead() != utf8::EoF) next();
        } else {
            next();
        }
    }
}

Sym Lexer::sym_str(uint32_t begin, uint32_t end, char32_t delim, bool esc) {
    auto body = buf_.substr(begin, end - begin);
    return driver_.sym(esc ? std::string_view(unquote(body, begin, delim)) : body);
}

/// Resolves the escapes of @p body, which starts at byte @p begin of Lexer::buf_.
std::string Lexer::unquote(std::string_view body, uint32_t begin, char32_t delim) {
    std::string res;
    res.reserve(body.size());

    for (size_t i = 0, e = body.size(); i != e; ++i) {
        auto c = body[i];
        if (c == (char)delim) { // a doubled delim stands for one
            ++i;
        } else if (c == '\\' && i + 1 != e) {
            switch (body[++i]) {
                    // clang-format off
                case '\'': res += '\''; break;
                case '\\': res += '\\'; break;
                case  '"': res += '\"'; break;
                case  '0': res += '\0'; break;
                case  'a': res += '\a'; break;
                case  'b': res += '\b'; break;
                case  'f': res += '\f'; break;
                case  'n': res += '\n'; break;
                case  'r': res += '\r'; break;
                case  't': res += '\t'; break;
                case  'v': res += '\v'; break;
                // clang-format on
                default:
                    auto loc = fe::Loc(src_, fe::Pos(begin + (uint32_t)i - 1), fe::Pos(begin + (uint32_t)i + 1));
                    error().e(loc, "invalid escape character `\\{}`", body[i]);
                    res += body[i]; // recover by taking it at face value
            }
            continue;
        }
        res += c;
    }

    return res;
}

void Lexer::eat_comments() {
    while (true) {
        accept_while([](char32_t c) { return c != '*'; });
        if (ahead() == utf8::EoF) {
            error().e(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace sql
