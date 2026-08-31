#include "sql/lexer.h"

#include <ranges>

using namespace std::literals;

namespace sql {

namespace utf8 = fe::utf8;

static std::string to_lower(std::string_view sv) {
    std::string res;
    for (auto c : sv)
        res += tolower(c);
    return res;
}

Lexer::Lexer(Driver& driver, const fe::Src& src)
    : fe::Lexer<1, Lexer>(src)
    , driver_(driver) {
#define CODE(t, str) keywords_[driver_.sym(to_lower(str##s))] = Tok::Tag::t;
    SQL_KEY(CODE)
#undef CODE
}

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
            driver_.error(peek(), "invalid input following `!`: `{}`", (char)ahead());
        }
        if (accept('>')) {
            if (accept('=')) return {loc_, Tok::Tag::T_ge};
            return {loc_, Tok::Tag::T_g};
        }
        if (accept('=')) return {loc_, Tok::Tag::T_eq};
        if (accept(',')) return {loc_, Tok::Tag::T_comma};
        if (accept('.')) return {loc_, Tok::Tag::T_dot};
        if (accept(';')) return {loc_, Tok::Tag::T_semicolon};
        if (accept(':')) {
            if (accept('=')) return {loc_, Tok::Tag::T_assign};
            return {loc_, Tok::Tag::T_colon};
        }
        if (accept('+')) return {loc_, Tok::Tag::T_add};
        if (accept('*')) return {loc_, Tok::Tag::T_mul};

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

        // integer value
        if (accept(utf8::isdigit)) {
            while (accept(utf8::isdigit)) {}
            return {loc_, std::strtoull(str_.c_str(), nullptr, 10)};
        }

        // lex identifier or keyword
        if (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c); })) {
            while (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c) || utf8::isdigit(c); })) {
            }
            auto sym = driver_.sym(str_);
            if (auto i = keywords_.find(sym); i != keywords_.end()) return {loc_, i->second}; // keyword
            return {loc_, sym};                                                               // identifier
        }

        // string literal or - double-quoted, hence case-sensitive - delimited identifier
        if (accept<Append::Off>('\'')) return lex_str('\'', Tok::Tag::V_str);
        if (accept<Append::Off>('\"')) return lex_str('\"', Tok::Tag::V_id);

        recover_char();
    }
}

Tok Lexer::lex_str(char32_t delim, Tok::Tag tag) {
    while (true) {
        if (accept<Append::Off>(delim)) {
            if (!accept<Append::Off>(delim)) break;
            str_ += (char)delim;
        } else if (ahead() == utf8::EoF) {
            driver_.error(loc_, "unterminated string literal");
            break;
        } else {
            lex_char();
        }
    }

    return {loc_, tag, driver_.sym(str_)};
}

void Lexer::lex_char() {
    if (accept<Append::Off>('\\')) {
        // clang-format off
        if (false) {}
        else if (accept<Append::Off>('\'')) str_ += '\'';
        else if (accept<Append::Off>('\\')) str_ += '\\';
        else if (accept<Append::Off>( '"')) str_ += '\"';
        else if (accept<Append::Off>( '0')) str_ += '\0';
        else if (accept<Append::Off>( 'a')) str_ += '\a';
        else if (accept<Append::Off>( 'b')) str_ += '\b';
        else if (accept<Append::Off>( 'f')) str_ += '\f';
        else if (accept<Append::Off>( 'n')) str_ += '\n';
        else if (accept<Append::Off>( 'r')) str_ += '\r';
        else if (accept<Append::Off>( 't')) str_ += '\t';
        else if (accept<Append::Off>( 'v')) str_ += '\v';
        else driver_.error(loc_.anew_end(), "invalid escape character `\\{}`", (char)ahead());
        // clang-format on
        return;
    }

    // Append the original bytes: re-encoding via `str_ += char32_t` would truncate multi-byte UTF-8.
    auto loc = peek();
    next();
    str_.append(buf_.substr(loc.begin.off, loc.size()));
}

void Lexer::eat_comments() {
    while (true) {
        while (ahead() != utf8::EoF && ahead() != '*')
            next();
        if (ahead() == utf8::EoF) {
            driver_.error(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace sql
