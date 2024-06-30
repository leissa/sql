#include "sql/lexer.h"

#include <ranges>

#include <fe/loc.cpp.h>

using namespace std::literals;

namespace sql {

namespace utf8 = fe::utf8;

static std::string to_lower(std::string_view sv) {
    std::string res;
    for (auto c : sv) res += tolower(c);
    return res;
}

Lexer::Lexer(Driver& driver, std::istream& istream, const std::filesystem::path* path)
    : fe::Lexer<1, Lexer>(istream, path)
    , driver_(driver) {
    if (!istream_) throw std::runtime_error("stream is bad");
#define CODE(t, str) keywords_[driver_.sym(to_lower(str##s))] = Tok::Tag::t;
    SQL_KEY(CODE)
#undef CODE
}

Tok Lexer::lex() {
    while (true) {
        start();

        if (accept(utf8::EoF)) return {loc_, Tok::Tag::EoF};
        if (accept(utf8::isspace)) continue;
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
        if (accept('>')) {
            if (accept('=')) return {loc_, Tok::Tag::T_ge};
            return {loc_, Tok::Tag::T_g};
        }
        if (accept('=')) return {loc_, Tok::Tag::T_eq};
        if (accept(',')) return {loc_, Tok::Tag::T_comma};
        if (accept('.')) return {loc_, Tok::Tag::T_dot};
        if (accept(';')) return {loc_, Tok::Tag::T_semicolon};
        if (accept('+')) return {loc_, Tok::Tag::T_add};
        if (accept('-')) return {loc_, Tok::Tag::T_sub};
        if (accept('*')) return {loc_, Tok::Tag::T_mul};
        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            if (accept('/')) {
                while (ahead() != utf8::EoF && ahead() != '\n') next();
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
            while (accept<Append::Lower>([](char32_t c) { return c == '_' || utf8::isalpha(c) || utf8::isdigit(c); })) {}
            auto sym = driver_.sym(str_);
            if (auto i = keywords_.find(sym); i != keywords_.end()) return {loc_, i->second}; // keyword
            return {loc_, sym};                                                               // identifier
        }

        // lex string
        if (accept('\'')){
            while (lex_char() != '\'') {}
            str_.pop_back(); // remove final '
            auto sym = driver_.sym(str_);
            return {loc_, sym};
        } 
        if (accept('\"')){
            while (lex_char() != '"') {}
            str_.pop_back(); // remove final "
            auto sym = driver_.sym(str_);
            return {loc_, sym};
        }

        if (accept(utf8::Null)) {
            driver().err(loc_, "invalid UTF-8 character");
            continue;
        }

        driver_.err({loc_.path, peek_}, "invalid input char: '{}'", (char)ahead());
        next();
    }
}

char8_t Lexer::lex_char() {
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
        else driver_.err(loc_.anew_finis(), "invalid escape character '\\{}'", (char)ahead());
        // clang-format on
        return str_.back();
    }
    auto c = next();
    str_ += c;
    if (utf8::isascii(c)) return c;
    driver_.err(loc_, "invalid character '{}'", (char)c);
}

void Lexer::eat_comments() {
    while (true) {
        while (ahead() != utf8::EoF && ahead() != '*') next();
        if (ahead() == utf8::EoF) {
            driver_.err(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace sql
