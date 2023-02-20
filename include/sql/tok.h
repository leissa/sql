#pragma once

#include <cassert>

#include "sql/loc.h"
#include "sql/sym.h"

namespace sql {

#define SQL_KEY(m) \
    m(K_select, "select")     \
    m(K_where, "where")

#define CODE(t, str) + size_t(1)
constexpr auto Num_Keys = size_t(0) SQL_KEY(CODE);
#undef CODE

#define SQL_LIT(m)  \

#define SQL_TOK(m)           \
    /* misc */                  \
    m(M_eof, "<end of file>") \
    m(M_id,  "<identifier>") \
    /* delimiter */         \
    m(D_brace_l,    "{")                \
    m(D_brace_r,    "}")                \
    m(D_brckt_l,    "[")                \
    m(D_brckt_r,    "]")                \
    m(D_paren_l,    "(")                \
    m(D_paren_r,    ")")                \
    /* further tokens */            \
    m(T_assign,     "=")    \
    m(T_dot,        ".")    \
    m(T_semicolon,  ";")    \

class Tok {
public:
    enum class Tag {
#define CODE(t, str) t,
        SQL_KEY(CODE)
        SQL_TOK(CODE)
#undef CODE
    };

    Tok() {}
    Tok(Loc loc, Tag tag)
        : loc_(loc)
        , tag_(tag)
    {}
    Tok(Loc loc, Sym sym)
        : loc_(loc)
        , tag_(Tag::M_id)
        , sym_(sym)
    {}

    Loc loc() const { return loc_; }
    Tag tag() const { return tag_; }
    bool isa(Tag tag) const { return tag == tag_; }
    Sym sym() const { assert(isa(Tag::M_id)); return sym_; }
    static const char* tag2str(Tok::Tag);

private:
    Loc loc_;
    Tag tag_;
    Sym sym_;
};

std::ostream& operator<<(std::ostream&, const Tok&);

}
