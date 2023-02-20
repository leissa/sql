#pragma once

#include <cassert>

#include "sql/loc.h"
#include "sql/sym.h"

namespace sql {

// clang-format off
#define SQL_KEY(m) \
    m(K_ABSOLUTE, "absolute", "ABSOLUTE") \
    m(K_ACTION, "action", "ACTION") \
    m(K_ADD, "add", "ADD") \
    m(K_ADMIN, "admin", "ADMIN") \
    m(K_AFTER, "after", "AFTER") \
    m(K_AGGREGATE, "aggregate", "AGGREGATE") \
    m(K_ALIAS, "alias", "ALIAS") \
    m(K_ALL, "all", "ALL") \
    m(K_ALLOCATE, "allocate", "ALLOCATE") \
    m(K_ALTER, "alter", "ALTER") \
    m(K_AND, "and", "AND") \
    m(K_ANY, "any", "ANY") \
    m(K_ARE, "are", "ARE") \
    m(K_ARRAY, "array", "ARRAY") \
    m(K_AS, "as", "AS") \
    m(K_ASC, "asc", "ASC") \
    m(K_ASSERTION, "assertion", "ASSERTION") \
    m(K_AT, "at", "AT") \
    m(K_AUTHORIZATION, "authorization", "AUTHORIZATION") \
    m(K_BEFORE, "before", "BEFORE") \
    m(K_BEGIN, "begin", "BEGIN") \
    m(K_BINARY, "binary", "BINARY") \
    m(K_BIT, "bit", "BIT") \
    m(K_BLOB, "blob", "BLOB") \
    m(K_BOOLEAN, "boolean", "BOOLEAN") \
    m(K_BOTH, "both", "BOTH") \
    m(K_BREADTH, "breadth", "BREADTH") \
    m(K_BY, "by", "BY") \
    m(K_CALL, "call", "CALL") \
    m(K_CASCADE, "cascade", "CASCADE") \
    m(K_CASCADED, "cascaded", "CASCADED") \
    m(K_CASE, "case", "CASE") \
    m(K_CAST, "cast", "CAST") \
    m(K_CATALOG, "catalog", "CATALOG") \
    m(K_CHAR, "char", "CHAR") \
    m(K_CHARACTER, "character", "CHARACTER") \
    m(K_CHECK, "check", "CHECK") \
    m(K_CLASS, "class", "CLASS") \
    m(K_CLOB, "clob", "CLOB") \
    m(K_CLOSE, "close", "CLOSE") \
    m(K_COLLATE, "collate", "COLLATE") \
    m(K_COLLATION, "collation", "COLLATION") \
    m(K_COLUMN, "column", "COLUMN") \
    m(K_COMMIT, "commit", "COMMIT") \
    m(K_COMPLETION, "completion", "COMPLETION") \
    m(K_CONNECT, "connect", "CONNECT") \
    m(K_CONNECTION, "connection", "CONNECTION") \
    m(K_CONSTRAINT, "constraint", "CONSTRAINT") \
    m(K_CONSTRAINTS, "constraints", "CONSTRAINTS") \
    m(K_CONSTRUCTOR, "constructor", "CONSTRUCTOR") \
    m(K_CONTINUE, "continue", "CONTINUE") \
    m(K_CORRESPONDING, "corresponding", "CORRESPONDING") \
    m(K_CREATE, "create", "CREATE") \
    m(K_CROSS, "cross", "CROSS") \
    m(K_CUBE, "cube", "CUBE") \
    m(K_CURRENT, "current", "CURRENT") \
    m(K_CURRENT_DATE, "current_date", "CURRENT_DATE") \
    m(K_CURRENT_PATH, "current_path", "CURRENT_PATH") \
    m(K_CURRENT_ROLE, "current_role", "CURRENT_ROLE") \
    m(K_CURRENT_TIME, "current_time", "CURRENT_TIME") \
    m(K_CURRENT_TIMESTAMP, "current_timestamp", "CURRENT_TIMESTAMP") \
    m(K_CURRENT_USER, "current_user", "CURRENT_USER") \
    m(K_CURSOR, "cursor", "CURSOR") \
    m(K_CYCLE, "cycle", "CYCLE") \
    m(K_DATA, "data", "DATA") \
    m(K_DATE, "date", "DATE") \
    m(K_DAY, "day", "DAY") \
    m(K_DEALLOCATE, "deallocate", "DEALLOCATE") \
    m(K_DEC, "dec", "DEC") \
    m(K_DECIMAL, "decimal", "DECIMAL") \
    m(K_DECLARE, "declare", "DECLARE") \
    m(K_DEFAULT, "default", "DEFAULT") \
    m(K_DEFERRABLE, "deferrable", "DEFERRABLE") \
    m(K_DEFERRED, "deferred", "DEFERRED") \
    m(K_DELETE, "delete", "DELETE") \
    m(K_DEPTH, "depth", "DEPTH") \
    m(K_DEREF, "deref", "DEREF") \
    m(K_DESC, "desc", "DESC") \
    m(K_DESCRIBE, "describe", "DESCRIBE") \
    m(K_DESCRIPTOR, "descriptor", "DESCRIPTOR") \
    m(K_DESTROY, "destroy", "DESTROY") \
    m(K_DESTRUCTOR, "destructor", "DESTRUCTOR") \
    m(K_DETERMINISTIC, "deterministic", "DETERMINISTIC") \
    m(K_DICTIONARY, "dictionary", "DICTIONARY") \
    m(K_DIAGNOSTICS, "diagnostics", "DIAGNOSTICS") \
    m(K_DISCONNECT, "disconnect", "DISCONNECT") \
    m(K_DISTINCT, "distinct", "DISTINCT") \
    m(K_DOMAIN, "domain", "DOMAIN") \
    m(K_DOUBLE, "double", "DOUBLE") \
    m(K_DROP, "drop", "DROP") \
    m(K_DYNAMIC, "dynamic", "DYNAMIC") \
    m(K_EACH, "each", "EACH") \
    m(K_ELSE, "else", "ELSE") \
    m(K_END, "end", "END") \
    m(K_END_EXEC, "end-exec", "END-EXEC") \
    m(K_EQUALS, "equals", "EQUALS") \
    m(K_ESCAPE, "escape", "ESCAPE") \
    m(K_EVERY, "every", "EVERY") \
    m(K_EXCEPT, "except", "EXCEPT") \
    m(K_EXCEPTION, "exception", "EXCEPTION") \
    m(K_EXEC, "exec", "EXEC") \
    m(K_EXECUTE, "execute", "EXECUTE") \
    m(K_EXTERNAL, "external", "EXTERNAL") \
    m(K_FALSE, "false", "FALSE") \
    m(K_FETCH, "fetch", "FETCH") \
    m(K_FIRST, "first", "FIRST") \
    m(K_FLOAT, "float", "FLOAT") \
    m(K_FOR, "for", "FOR") \
    m(K_FOREIGN, "foreign", "FOREIGN") \
    m(K_FOUND, "found", "FOUND") \
    m(K_FROM, "from", "FROM") \
    m(K_FREE, "free", "FREE") \
    m(K_FULL, "full", "FULL") \
    m(K_FUNCTION, "function", "FUNCTION") \
    m(K_GENERAL, "general", "GENERAL") \
    m(K_GET, "get", "GET") \
    m(K_GLOBAL, "global", "GLOBAL") \
    m(K_GO, "go", "GO") \
    m(K_GOTO, "goto", "GOTO") \
    m(K_GRANT, "grant", "GRANT") \
    m(K_GROUP, "group", "GROUP") \
    m(K_GROUPING, "grouping", "GROUPING") \
    m(K_HAVING, "having", "HAVING") \
    m(K_HOST, "host", "HOST") \
    m(K_HOUR, "hour", "HOUR") \
    m(K_IDENTITY, "identity", "IDENTITY") \
    m(K_IGNORE, "ignore", "IGNORE") \
    m(K_IMMEDIATE, "immediate", "IMMEDIATE") \
    m(K_IN, "in", "IN") \
    m(K_INDICATOR, "indicator", "INDICATOR") \
    m(K_INITIALIZE, "initialize", "INITIALIZE") \
    m(K_INITIALLY, "initially", "INITIALLY") \
    m(K_INNER, "inner", "INNER") \
    m(K_INOUT, "inout", "INOUT") \
    m(K_INPUT, "input", "INPUT") \
    m(K_INSERT, "insert", "INSERT") \
    m(K_INT, "int", "INT") \
    m(K_INTEGER, "integer", "INTEGER") \
    m(K_INTERSECT, "intersect", "INTERSECT") \
    m(K_INTERVAL, "interval", "INTERVAL") \
    m(K_INTO, "into", "INTO") \
    m(K_IS, "is", "IS") \
    m(K_ISOLATION, "isolation", "ISOLATION") \
    m(K_ITERATE, "iterate", "ITERATE") \
    m(K_JOIN, "join", "JOIN") \
    m(K_KEY, "key", "KEY") \
    m(K_LANGUAGE, "language", "LANGUAGE") \
    m(K_LARGE, "large", "LARGE") \
    m(K_LAST, "last", "LAST") \
    m(K_LATERAL, "lateral", "LATERAL") \
    m(K_LEADING, "leading", "LEADING") \
    m(K_LEFT, "left", "LEFT") \
    m(K_LESS, "less", "LESS") \
    m(K_LEVEL, "level", "LEVEL") \
    m(K_LIKE, "like", "LIKE") \
    m(K_LIMIT, "limit", "LIMIT") \
    m(K_LOCAL, "local", "LOCAL") \
    m(K_LOCALTIME, "localtime", "LOCALTIME") \
    m(K_LOCALTIMESTAMP, "localtimestamp", "LOCALTIMESTAMP") \
    m(K_LOCATOR, "locator", "LOCATOR") \
    m(K_MAP, "map", "MAP") \
    m(K_MATCH, "match", "MATCH") \
    m(K_MINUTE, "minute", "MINUTE") \
    m(K_MODIFIES, "modifies", "MODIFIES") \
    m(K_MODIFY, "modify", "MODIFY") \
    m(K_MODULE, "module", "MODULE") \
    m(K_MONTH, "month", "MONTH") \
    m(K_NAMES, "names", "NAMES") \
    m(K_NATIONAL, "national", "NATIONAL") \
    m(K_NATURAL, "natural", "NATURAL") \
    m(K_NCHAR, "nchar", "NCHAR") \
    m(K_NCLOB, "nclob", "NCLOB") \
    m(K_NEW, "new", "NEW") \
    m(K_NEXT, "next", "NEXT") \
    m(K_NO, "no", "NO") \
    m(K_NONE, "none", "NONE") \
    m(K_NOT, "not", "NOT") \
    m(K_NULL, "null", "NULL") \
    m(K_NUMERIC, "numeric", "NUMERIC") \
    m(K_OBJECT, "object", "OBJECT") \
    m(K_OF, "of", "OF") \
    m(K_OFF, "off", "OFF") \
    m(K_OLD, "old", "OLD") \
    m(K_ON, "on", "ON") \
    m(K_ONLY, "only", "ONLY") \
    m(K_OPEN, "open", "OPEN") \
    m(K_OPERATION, "operation", "OPERATION") \
    m(K_OPTION, "option", "OPTION") \
    m(K_OR, "or", "OR") \
    m(K_ORDER, "order", "ORDER") \
    m(K_ORDINALITY, "ordinality", "ORDINALITY") \
    m(K_OUT, "out", "OUT") \
    m(K_OUTER, "outer", "OUTER") \
    m(K_OUTPUT, "output", "OUTPUT") \
    m(K_PAD, "pad", "PAD") \
    m(K_PARAMETER, "parameter", "PARAMETER") \
    m(K_PARAMETERS, "parameters", "PARAMETERS") \
    m(K_PARTIAL, "partial", "PARTIAL") \
    m(K_PATH, "path", "PATH") \
    m(K_POSTFIX, "postfix", "POSTFIX") \
    m(K_PRECISION, "precision", "PRECISION") \
    m(K_PREFIX, "prefix", "PREFIX") \
    m(K_PREORDER, "preorder", "PREORDER") \
    m(K_PREPARE, "prepare", "PREPARE") \
    m(K_PRESERVE, "preserve", "PRESERVE") \
    m(K_PRIMARY, "primary", "PRIMARY") \
    m(K_PRIOR, "prior", "PRIOR") \
    m(K_PRIVILEGES, "privileges", "PRIVILEGES") \
    m(K_PROCEDURE, "procedure", "PROCEDURE") \
    m(K_PUBLIC, "public", "PUBLIC") \
    m(K_READ, "read", "READ") \
    m(K_READS, "reads", "READS") \
    m(K_REAL, "real", "REAL") \
    m(K_RECURSIVE, "recursive", "RECURSIVE") \
    m(K_REF, "ref", "REF") \
    m(K_REFERENCES, "references", "REFERENCES") \
    m(K_REFERENCING, "referencing", "REFERENCING") \
    m(K_RELATIVE, "relative", "RELATIVE") \
    m(K_RESTRICT, "restrict", "RESTRICT") \
    m(K_RESULT, "result", "RESULT") \
    m(K_RETURN, "return", "RETURN") \
    m(K_RETURNS, "returns", "RETURNS") \
    m(K_REVOKE, "revoke", "REVOKE") \
    m(K_RIGHT, "right", "RIGHT") \
    m(K_ROLE, "role", "ROLE") \
    m(K_ROLLBACK, "rollback", "ROLLBACK") \
    m(K_ROLLUP, "rollup", "ROLLUP") \
    m(K_ROUTINE, "routine", "ROUTINE") \
    m(K_ROW, "row", "ROW") \
    m(K_ROWS, "rows", "ROWS") \
    m(K_SAVEPOINT, "savepoint", "SAVEPOINT") \
    m(K_SCHEMA, "schema", "SCHEMA") \
    m(K_SCROLL, "scroll", "SCROLL") \
    m(K_SCOPE, "scope", "SCOPE") \
    m(K_SEARCH, "search", "SEARCH") \
    m(K_SECOND, "second", "SECOND") \
    m(K_SECTION, "section", "SECTION") \
    m(K_SELECT, "select", "SELECT") \
    m(K_SEQUENCE, "sequence", "SEQUENCE") \
    m(K_SESSION, "session", "SESSION") \
    m(K_SESSION_USER, "session_user", "SESSION_USER") \
    m(K_SET, "set", "SET") \
    m(K_SETS, "sets", "SETS") \
    m(K_SIZE, "size", "SIZE") \
    m(K_SMALLINT, "smallint", "SMALLINT") \
    m(K_SOME, "some", "SOME") \
    m(K_SPACE, "space", "SPACE") \
    m(K_SPECIFIC, "specific", "SPECIFIC") \
    m(K_SPECIFICTYPE, "specifictype", "SPECIFICTYPE") \
    m(K_SQL, "sql", "SQL") \
    m(K_SQLEXCEPTION, "sqlexception", "SQLEXCEPTION") \
    m(K_SQLSTATE, "sqlstate", "SQLSTATE") \
    m(K_SQLWARNING, "sqlwarning", "SQLWARNING") \
    m(K_START, "start", "START") \
    m(K_STATE, "state", "STATE") \
    m(K_STATEMENT, "statement", "STATEMENT") \
    m(K_STATIC, "static", "STATIC") \
    m(K_STRUCTURE, "structure", "STRUCTURE") \
    m(K_SYSTEM_USER, "system_user", "SYSTEM_USER") \
    m(K_TABLE, "table", "TABLE") \
    m(K_TEMPORARY, "temporary", "TEMPORARY") \
    m(K_TERMINATE, "terminate", "TERMINATE") \
    m(K_THAN, "than", "THAN") \
    m(K_THEN, "then", "THEN") \
    m(K_TIME, "time", "TIME") \
    m(K_TIMESTAMP, "timestamp", "TIMESTAMP") \
    m(K_TIMEZONE_HOUR, "timezone_hour", "TIMEZONE_HOUR") \
    m(K_TIMEZONE_MINUTE, "timezone_minute", "TIMEZONE_MINUTE") \
    m(K_TO, "to", "TO") \
    m(K_TRAILING, "trailing", "TRAILING") \
    m(K_TRANSACTION, "transaction", "TRANSACTION") \
    m(K_TRANSLATION, "translation", "TRANSLATION") \
    m(K_TREAT, "treat", "TREAT") \
    m(K_TRIGGER, "trigger", "TRIGGER") \
    m(K_TRUE, "true", "TRUE") \
    m(K_UNDER, "under", "UNDER") \
    m(K_UNION, "union", "UNION") \
    m(K_UNIQUE, "unique", "UNIQUE") \
    m(K_UNKNOWN, "unknown", "UNKNOWN") \
    m(K_UNNEST, "unnest", "UNNEST") \
    m(K_UPDATE, "update", "UPDATE") \
    m(K_USAGE, "usage", "USAGE") \
    m(K_USER, "user", "USER") \
    m(K_USING, "using", "USING") \
    m(K_VALUE, "value", "VALUE") \
    m(K_VALUES, "values", "VALUES") \
    m(K_VARCHAR, "varchar", "VARCHAR") \
    m(K_VARIABLE, "variable", "VARIABLE") \
    m(K_VARYING, "varying", "VARYING") \
    m(K_VIEW, "view", "VIEW") \
    m(K_WHEN, "when", "WHEN") \
    m(K_WHENEVER, "whenever", "WHENEVER") \
    m(K_WHERE, "where", "WHERE") \
    m(K_WITH, "with", "WITH") \
    m(K_WITHOUT, "without", "WITHOUT") \
    m(K_WORK, "work", "WORK") \
    m(K_WRITE, "write", "WRITE") \
    m(K_YEAR, "year", "YEAR") \
    m(K_ZONE, "zone", "ZONE") \

#define SQL_LIT(m)                          \
    m(L_i,          "<interger literal>")   \

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
    m(T_add,  "+")                \
    m(T_sub,  "-")                \
    m(T_mul,  "*")                \
    m(T_div,  "/")                \

class Tok {
public:
    // clang-format off
    enum class Tag {
#define CODE(t, _, __) t,
        SQL_KEY(CODE)
#undef CODE
#define CODE(t, _) t,
        SQL_LIT(CODE)
        SQL_TOK(CODE)
#undef CODE
        K_IS_NOT ///< Not an actual keyword but we use this for UnExpr::tag.
    };
    // clang-format on

    enum class Prec {
        Bot,
        Or,
        And,
        Not,
        Comp,
        Add,
        Mul,
        Unary,
    };

    Tok() {}
    Tok(Loc loc, Tag tag)
        : loc_(loc)
        , tag_(tag) {}
    Tok(Loc loc, Sym sym)
        : loc_(loc)
        , tag_(Tag::M_id)
        , sym_(sym) {}
    Tok(Loc loc, uint64_t u64)
        : loc_(loc)
        , tag_(Tag::L_i)
        , u64_(u64) {}

    Loc loc() const { return loc_; }
    Tag tag() const { return tag_; }
    bool isa(Tag tag) const { return tag == tag_; }

    Sym sym() const {
        assert(isa(Tag::M_id));
        return sym_;
    }
    uint64_t u64() const { return u64_; }

    static std::string_view str(Tok::Tag);
    static std::optional<Prec> un_prec(Tok::Tag);
    static std::optional<Prec> bin_prec(Tok::Tag);

private:
    Loc loc_;
    Tag tag_;
    union {
        Sym sym_;
        uint64_t u64_;
    };
};

std::ostream& operator<<(std::ostream&, Tok::Tag);
std::ostream& operator<<(std::ostream&, Tok);

} // namespace sql
