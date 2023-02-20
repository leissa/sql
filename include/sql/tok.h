#pragma once

#include <cassert>

#include "sql/loc.h"
#include "sql/sym.h"

namespace sql {

#define SQL_KEY(m)                \
    m(K_ADD,        "ADD")        \
    m(K_ALL,        "ALL")        \
    m(K_ALTER,      "ALTER")      \
    m(K_AND,        "AND")        \
    m(K_ANY,        "ANY")        \
    m(K_AS,         "AS")         \
    m(K_ASC,        "ASC")        \
    m(K_BACKUP,     "BACKUP")     \
    m(K_BETWEEN,    "BETWEEN")    \
    m(K_BY,         "BY")         \
    m(K_CASE,       "CASE")       \
    m(K_CHECK,      "CHECK")      \
    m(K_COLUMN,     "COLUMN")     \
    m(K_CONSTRAINT, "CONSTRAINT") \
    m(K_CREATE,     "CREATE")     \
    m(K_DATABASE,   "DATABASE")   \
    m(K_DEFAULT,    "DEFAULT")    \
    m(K_DELETE,     "DELETE")     \
    m(K_DESC,       "DESC")       \
    m(K_DISTINCT,   "DISTINCT")   \
    m(K_DROP,       "DROP")       \
    m(K_EXEC,       "EXEC")       \
    m(K_EXISTS,     "EXISTS")     \
    m(K_FOREIGN,    "FOREIGN")    \
    m(K_FROM,       "FROM")       \
    m(K_FULL,       "FULL")       \
    m(K_GROUP,      "GROUP")      \
    m(K_HAVING,     "HAVING")     \
    m(K_IN,         "IN")         \
    m(K_INDEX,      "INDEX")      \
    m(K_INNER,      "INNER")      \
    m(K_INSERT,     "INSERT")     \
    m(K_INTO,       "INTO")       \
    m(K_IS,         "IS")         \
    m(K_JOIN,       "JOIN")       \
    m(K_KEY,        "KEY")        \
    m(K_LEFT,       "LEFT")       \
    m(K_LIKE,       "LIKE")       \
    m(K_LIMIT,      "LIMIT")      \
    m(K_NOT,        "NOT")        \
    m(K_NULL,       "NULL")       \
    m(K_OR,         "OR")         \
    m(K_ORDER,      "ORDER")      \
    m(K_OUTER,      "OUTER")      \
    m(K_PRIMARY,    "PRIMARY")    \
    m(K_PROCEDURE,  "PROCEDURE")  \
    m(K_REPLACE,    "REPLACE")    \
    m(K_RIGHT,      "RIGHT")      \
    m(K_ROWNUM,     "ROWNUM")     \
    m(K_SELECT,     "SELECT")     \
    m(K_SET,        "SET")        \
    m(K_TABLE,      "TABLE")      \
    m(K_TOP,        "TOP")        \
    m(K_TRUNCATE,   "TRUNCATE")   \
    m(K_UNION,      "UNION")      \
    m(K_UNIQUE,     "UNIQUE")     \
    m(K_UPDATE,     "UPDATE")     \
    m(K_VALUES,     "VALUES")     \
    m(K_VIEW,       "VIEW")       \
    m(K_WHERE,      "WHERE")      \

#define SQL_LIT(m)                          \
    m(L_int,        "<interger literal>")   \

#define SQL_TOK(m)                      \
    /* misc */                          \
    m(M_eof,        "<end of file>")    \
    m(M_id,         "<identifier>")     \
    /* delimiter */                     \
    m(D_brace_l,    "{")                \
    m(D_brace_r,    "}")                \
    m(D_brckt_l,    "[")                \
    m(D_brckt_r,    "]")                \
    m(D_paren_l,    "(")                \
    m(D_paren_r,    ")")                \
    /* further tokens */                \
    m(T_assign,     ":=")               \
    m(T_eq,         "=")                \
    m(T_ne,         "<>")               \
    m(T_l,          "<")                \
    m(T_g,          ">")                \
    m(T_le,         "<=")               \
    m(T_ge,         ">=")               \
    m(T_dot,        ".")                \
    m(T_colon,      ":")                \
    m(T_semicolon,  ";")                \

#define CODE(t, str) + size_t(1)
constexpr auto Num_Keys = size_t(0) SQL_KEY(CODE);
#undef CODE

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
    static std::string_view tag2str(Tok::Tag);

private:
    Loc loc_;
    Tag tag_;
    Sym sym_;
};

std::ostream& operator<<(std::ostream&, const Tok&);

}
