#pragma once

#include <cassert>

#include "sql/loc.h"
#include "sql/sym.h"

namespace sql {

// clang-format off
#define SQL_KEY(m)                              \
    m(K_ADD,        "add",        "ADD")        \
    m(K_ALL,        "all",        "ALL")        \
    m(K_ALTER,      "alter",      "ALTER")      \
    m(K_AND,        "and",        "AND")        \
    m(K_ANY,        "any",        "ANY")        \
    m(K_AS,         "as",         "AS")         \
    m(K_ASC,        "asc",        "ASC")        \
    m(K_BACKUP,     "backup",     "BACKUP")     \
    m(K_BETWEEN,    "between",    "BETWEEN")    \
    m(K_BY,         "by",         "BY")         \
    m(K_CASE,       "case",       "CASE")       \
    m(K_CHECK,      "check",      "CHECK")      \
    m(K_COLUMN,     "column",     "COLUMN")     \
    m(K_CONSTRAINT, "constraint", "CONSTRAINT") \
    m(K_CREATE,     "create",     "CREATE")     \
    m(K_DATABASE,   "database",   "DATABASE")   \
    m(K_DEFAULT,    "default",    "DEFAULT")    \
    m(K_DELETE,     "delete",     "DELETE")     \
    m(K_DESC,       "desc",       "DESC")       \
    m(K_DISTINCT,   "distinct",   "DISTINCT")   \
    m(K_DROP,       "drop",       "DROP")       \
    m(K_EXEC,       "exec",       "EXEC")       \
    m(K_EXISTS,     "exists",     "EXISTS")     \
    m(K_FOREIGN,    "foreign",    "FOREIGN")    \
    m(K_FROM,       "from",       "FROM")       \
    m(K_FULL,       "full",       "FULL")       \
    m(K_GROUP,      "group",      "GROUP")      \
    m(K_HAVING,     "having",     "HAVING")     \
    m(K_IN,         "in",         "IN")         \
    m(K_INDEX,      "index",      "INDEX")      \
    m(K_INNER,      "inner",      "INNER")      \
    m(K_INSERT,     "insert",     "INSERT")     \
    m(K_INTO,       "into",       "INTO")       \
    m(K_IS,         "is",         "IS")         \
    m(K_JOIN,       "join",       "JOIN")       \
    m(K_KEY,        "key",        "KEY")        \
    m(K_LEFT,       "left",       "LEFT")       \
    m(K_LIKE,       "like",       "LIKE")       \
    m(K_LIMIT,      "limit",      "LIMIT")      \
    m(K_NOT,        "not",        "NOT")        \
    m(K_NULL,       "null",       "NULL")       \
    m(K_OR,         "or",         "OR")         \
    m(K_ORDER,      "order",      "ORDER")      \
    m(K_OUTER,      "outer",      "OUTER")      \
    m(K_PRIMARY,    "primary",    "PRIMARY")    \
    m(K_PROCEDURE,  "procedure",  "PROCEDURE")  \
    m(K_REPLACE,    "replace",    "REPLACE")    \
    m(K_RIGHT,      "right",      "RIGHT")      \
    m(K_ROWNUM,     "rownum",     "ROWNUM")     \
    m(K_SELECT,     "select",     "SELECT")     \
    m(K_SET,        "set",        "SET")        \
    m(K_TABLE,      "table",      "TABLE")      \
    m(K_TOP,        "top",        "TOP")        \
    m(K_TRUNCATE,   "truncate",   "TRUNCATE")   \
    m(K_UNION,      "union",      "UNION")      \
    m(K_UNIQUE,     "unique",     "UNIQUE")     \
    m(K_UPDATE,     "update",     "UPDATE")     \
    m(K_VALUES,     "values",     "VALUES")     \
    m(K_VIEW,       "view",       "VIEW")       \
    m(K_WHERE,      "where",      "WHERE")      \

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
    m(T_semicolon,  ";")
// clang-format on

class Tok {
public:
    enum class Tag {
#define CODE(t, _, __) t,
        SQL_KEY(CODE)
#undef CODE
#define CODE(t, _) t,
            SQL_TOK(CODE)
#undef CODE
    };

    Tok() {}
    Tok(Loc loc, Tag tag)
        : loc_(loc)
        , tag_(tag) {}
    Tok(Loc loc, Sym sym)
        : loc_(loc)
        , tag_(Tag::M_id)
        , sym_(sym) {}

    Loc loc() const { return loc_; }
    Tag tag() const { return tag_; }
    bool isa(Tag tag) const { return tag == tag_; }
    Sym sym() const {
        assert(isa(Tag::M_id));
        return sym_;
    }
    static std::string_view tag2str(Tok::Tag);

private:
    Loc loc_;
    Tag tag_;
    Sym sym_;
};

std::ostream& operator<<(std::ostream&, const Tok&);

} // namespace sql
