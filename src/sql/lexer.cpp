#include "sql/lexer.h"

#include <ranges>

using namespace std::literals;

namespace sql {

Lexer::Lexer(Driver& driver, Sym filename, std::istream& stream)
    : driver_(driver)
    , loc_(filename)
    , peek_pos_({1, 1})
    , stream_(stream) {
    if (!stream_) throw std::runtime_error("stream is bad");

#define CODE(t, str, _) keywords_[driver_.sym(str##s)] = Tok::Tag::t;
    SQL_KEY(CODE)
#undef CODE
}

int Lexer::next() {
    loc_.finis = peek_pos_;
    int c      = stream_.get();

    if (c == '\n') {
        ++peek_pos_.row;
        peek_pos_.col = 1;
    } else {
        ++peek_pos_.col;
    }

    return c;
}

Tok Lexer::lex() {
    while (true) {
        loc_.begin = peek_pos_;
        str_.clear();

        if (eof()) return tok(Tok::Tag::M_eof);
        if (accept_if(isspace)) continue;
        if (accept('{')) return tok(Tok::Tag::D_brace_l);
        if (accept('}')) return tok(Tok::Tag::D_brace_r);
        if (accept('[')) return tok(Tok::Tag::D_brckt_l);
        if (accept(']')) return tok(Tok::Tag::D_brckt_r);
        if (accept('(')) return tok(Tok::Tag::D_paren_l);
        if (accept(')')) return tok(Tok::Tag::D_paren_r);
        if (accept('<')) {
            if (accept('>')) return tok(Tok::Tag::T_ne);
            if (accept('=')) return tok(Tok::Tag::T_le);
            return tok(Tok::Tag::T_l);
        }
        if (accept('>')) {
            if (accept('=')) return tok(Tok::Tag::T_ge);
            return tok(Tok::Tag::T_g);
        }
        if (accept('=')) return tok(Tok::Tag::T_assign);
        if (accept('.')) return tok(Tok::Tag::T_dot);
        if (accept(';')) return tok(Tok::Tag::T_semicolon);
        if (accept('+')) return tok(Tok::Tag::T_add);
        if (accept('-')) return tok(Tok::Tag::T_sub);
        if (accept('*')) return tok(Tok::Tag::T_mul);
        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            if (accept('/')) {
                while (!eof() && peek() != '\n') next();
                continue;
            }

            return tok(Tok::Tag::T_div);
        }

        // integer literal
        if (accept_if(isdigit)) {
            while (accept_if(isdigit)) {}
            return {loc(), std::strtoull(str_.c_str(), nullptr, 10)};
        }

        // lex identifier or keyword
        if (accept_if([](int i) { return i == '_' || isalpha(i); }, true)) {
            while (accept_if([](int i) { return i == '_' || isalpha(i) || isdigit(i); }, true)) {}
            auto sym = driver_.sym(str_);
            if (auto i = keywords_.find(sym); i != keywords_.end()) return tok(i->second); // keyword
            return {loc(), sym};                                                           // identifier
        }

        driver_.err({loc_.file, peek_pos_}, "invalid input char: '{}'", (char)peek());
        next();
    }
}

void Lexer::eat_comments() {
    while (true) {
        while (!eof() && peek() != '*') next();
        if (eof()) {
            driver_.err(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace sql
