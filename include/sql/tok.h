#pragma once

#include <cassert>

#include <fe/format.h>
#include <fe/loc.h>
#include <fe/sym.h>

namespace sql {

using fe::Loc;
using fe::Pos;
using fe::Sym;

#define SQL_TOK(m)                              \
    m(EoF,          "<end of file>")            \
    /* value: contains sth beyond the tag */    \
    m(V_int,        "<interger value>")         \
    m(V_id,         "<identifier>")             \
    /* delimiter */                             \
    m(D_brace_l,    "{")                        \
    m(D_brace_r,    "}")                        \
    m(D_brckt_l,    "[")                        \
    m(D_brckt_r,    "]")                        \
    m(D_paren_l,    "(")                        \
    m(D_paren_r,    ")")                        \
    /* further tokens */                        \
    m(T_assign,     ":=")                       \
    m(T_eq,         "=")                        \
    m(T_ne,         "<>")                       \
    m(T_l,          "<")                        \
    m(T_g,          ">")                        \
    m(T_le,         "<=")                       \
    m(T_ge,         ">=")                       \
    m(T_ue,         "!=")                       \
    m(T_dot,        ".")                        \
    m(T_colon,      ":")                        \
    m(T_comma,      ",")                        \
    m(T_semicolon,  ";")                        \
    m(T_add,        "+")                        \
    m(T_sub,        "-")                        \
    m(T_mul,        "*")                        \
    m(T_div,        "/")                        \

// clang-format off
// These are the *real* keywords - "reserved words"
#define SQL_KEY(m) \
    m(K_ABS, "ABS") \
    m(K_ALL, "ALL") \
    m(K_ALLOCATE, "ALLOCATE") \
    m(K_ALTER, "ALTER") \
    m(K_AND, "AND") \
    m(K_ANY, "ANY") \
    m(K_ARE, "ARE") \
    m(K_ARRAY, "ARRAY") \
    m(K_ARRAY_AGG, "ARRAY_AGG") \
    m(K_ARRAY_MAX_CARDINALITY, "ARRAY_MAX_CARDINALITY") \
    m(K_AS, "AS") \
    m(K_ASENSITIVE, "ASENSITIVE") \
    m(K_ASYMMETRIC, "ASYMMETRIC") \
    m(K_AT, "AT") \
    m(K_ATOMIC, "ATOMIC") \
    m(K_AUTHORIZATION, "AUTHORIZATION") \
    m(K_AVG, "AVG") \
    m(K_BEGIN, "BEGIN") \
    m(K_BEGIN_FRAME, "BEGIN_FRAME") \
    m(K_BEGIN_PARTITION, "BEGIN_PARTITION") \
    m(K_BETWEEN, "BETWEEN") \
    m(K_BIGINT, "BIGINT") \
    m(K_BINARY, "BINARY") \
    m(K_BLOB, "BLOB") \
    m(K_BOOLEAN, "BOOLEAN") \
    m(K_BOTH, "BOTH") \
    m(K_BY, "BY") \
    m(K_CALL, "CALL") \
    m(K_CALLED, "CALLED") \
    m(K_CARDINALITY, "CARDINALITY") \
    m(K_CASCADED, "CASCADED") \
    m(K_CASE, "CASE") \
    m(K_CAST, "CAST") \
    m(K_CEIL, "CEIL") \
    m(K_CEILING, "CEILING") \
    m(K_CHAR, "CHAR") \
    m(K_CHAR_LENGTH, "CHAR_LENGTH") \
    m(K_CHARACTER, "CHARACTER") \
    m(K_CHARACTER_LENGTH, "CHARACTER_LENGTH") \
    m(K_CHECK, "CHECK") \
    m(K_CLOB, "CLOB") \
    m(K_CLOSE, "CLOSE") \
    m(K_COALESCE, "COALESCE") \
    m(K_COLLATE, "COLLATE") \
    m(K_COLLECT, "COLLECT") \
    m(K_COLUMN, "COLUMN") \
    m(K_COMMIT, "COMMIT") \
    m(K_CONDITION, "CONDITION") \
    m(K_CONNECT, "CONNECT") \
    m(K_CONSTRAINT, "CONSTRAINT") \
    m(K_CONTAINS, "CONTAINS") \
    m(K_CONVERT, "CONVERT") \
    m(K_CORR, "CORR") \
    m(K_CORRESPONDING, "CORRESPONDING") \
    m(K_COUNT, "COUNT") \
    m(K_COVAR_POP, "COVAR_POP") \
    m(K_COVAR_SAMP, "COVAR_SAMP") \
    m(K_CREATE, "CREATE") \
    m(K_CROSS, "CROSS") \
    m(K_CUBE, "CUBE") \
    m(K_CUME_DIST, "CUME_DIST") \
    m(K_CURRENT, "CURRENT") \
    m(K_CURRENT_CATALOG, "CURRENT_CATALOG") \
    m(K_CURRENT_DATE, "CURRENT_DATE") \
    m(K_CURRENT_DEFAULT_TRANSFORM_GROUP, "CURRENT_DEFAULT_TRANSFORM_GROUP") \
    m(K_CURRENT_PATH, "CURRENT_PATH") \
    m(K_CURRENT_ROLE, "CURRENT_ROLE") \
    m(K_CURRENT_ROW, "CURRENT_ROW") \
    m(K_CURRENT_SCHEMA, "CURRENT_SCHEMA") \
    m(K_CURRENT_TIME, "CURRENT_TIME") \
    m(K_CURRENT_TIMESTAMP, "CURRENT_TIMESTAMP") \
    m(K_CURRENT_TRANSFORM_GROUP_FOR_TYPE, "CURRENT_TRANSFORM_GROUP_FOR_TYPE") \
    m(K_CURRENT_USER, "CURRENT_USER") \
    m(K_CURSOR, "CURSOR") \
    m(K_CYCLE, "CYCLE") \
    m(K_DATE, "DATE") \
    m(K_DAY, "DAY") \
    m(K_DEALLOCATE, "DEALLOCATE") \
    m(K_DEC, "DEC") \
    m(K_DECIMAL, "DECIMAL") \
    m(K_DECLARE, "DECLARE") \
    m(K_DEFAULT, "DEFAULT") \
    m(K_DELETE, "DELETE") \
    m(K_DENSE_RANK, "DENSE_RANK") \
    m(K_DEREF, "DEREF") \
    m(K_DESCRIBE, "DESCRIBE") \
    m(K_DETERMINISTIC, "DETERMINISTIC") \
    m(K_DISCONNECT, "DISCONNECT") \
    m(K_DISTINCT, "DISTINCT") \
    m(K_DOUBLE, "DOUBLE") \
    m(K_DROP, "DROP") \
    m(K_DYNAMIC, "DYNAMIC") \
    m(K_EACH, "EACH") \
    m(K_ELEMENT, "ELEMENT") \
    m(K_ELSE, "ELSE") \
    m(K_END, "END") \
    m(K_END_FRAME, "END_FRAME") \
    m(K_END_PARTITION, "END_PARTITION") \
    m(K_END_EXEC, "END-EXEC") \
    m(K_EQUALS, "EQUALS") \
    m(K_ESCAPE, "ESCAPE") \
    m(K_EVERY, "EVERY") \
    m(K_EXCEPT, "EXCEPT") \
    m(K_EXEC, "EXEC") \
    m(K_EXECUTE, "EXECUTE") \
    m(K_EXISTS, "EXISTS") \
    m(K_EXP, "EXP") \
    m(K_EXTERNAL, "EXTERNAL") \
    m(K_EXTRACT, "EXTRACT") \
    m(K_FALSE, "FALSE") \
    m(K_FETCH, "FETCH") \
    m(K_FILTER, "FILTER") \
    m(K_FIRST_VALUE, "FIRST_VALUE") \
    m(K_FLOAT, "FLOAT") \
    m(K_FLOOR, "FLOOR") \
    m(K_FOR, "FOR") \
    m(K_FOREIGN, "FOREIGN") \
    m(K_FRAME_ROW, "FRAME_ROW") \
    m(K_FREE, "FREE") \
    m(K_FROM, "FROM") \
    m(K_FULL, "FULL") \
    m(K_FUNCTION, "FUNCTION") \
    m(K_FUSION, "FUSION") \
    m(K_GET, "GET") \
    m(K_GLOBAL, "GLOBAL") \
    m(K_GRANT, "GRANT") \
    m(K_GROUP, "GROUP") \
    m(K_GROUPING, "GROUPING") \
    m(K_GROUPS, "GROUPS") \
    m(K_HAVING, "HAVING") \
    m(K_HOLD, "HOLD") \
    m(K_HOUR, "HOUR") \
    m(K_IDENTITY, "IDENTITY") \
    m(K_IN, "IN") \
    m(K_INDICATOR, "INDICATOR") \
    m(K_INNER, "INNER") \
    m(K_INOUT, "INOUT") \
    m(K_INSENSITIVE, "INSENSITIVE") \
    m(K_INSERT, "INSERT") \
    m(K_INT, "INT") \
    m(K_INTEGER, "INTEGER") \
    m(K_INTERSECT, "INTERSECT") \
    m(K_INTERSECTION, "INTERSECTION") \
    m(K_INTERVAL, "INTERVAL") \
    m(K_INTO, "INTO") \
    m(K_IS, "IS") \
    m(K_JOIN, "JOIN") \
    m(K_LAG, "LAG") \
    m(K_LANGUAGE, "LANGUAGE") \
    m(K_LARGE, "LARGE") \
    m(K_LAST_VALUE, "LAST_VALUE") \
    m(K_LATERAL, "LATERAL") \
    m(K_LEAD, "LEAD") \
    m(K_LEADING, "LEADING") \
    m(K_LEFT, "LEFT") \
    m(K_LIKE, "LIKE") \
    m(K_LIKE_REGEX, "LIKE_REGEX") \
    m(K_LN, "LN") \
    m(K_LOCAL, "LOCAL") \
    m(K_LOCALTIME, "LOCALTIME") \
    m(K_LOCALTIMESTAMP, "LOCALTIMESTAMP") \
    m(K_LOWER, "LOWER") \
    m(K_MATCH, "MATCH") \
    m(K_MAX, "MAX") \
    m(K_MEMBER, "MEMBER") \
    m(K_MERGE, "MERGE") \
    m(K_METHOD, "METHOD") \
    m(K_MIN, "MIN") \
    m(K_MINUTE, "MINUTE") \
    m(K_MOD, "MOD") \
    m(K_MODIFIES, "MODIFIES") \
    m(K_MODULE, "MODULE") \
    m(K_MONTH, "MONTH") \
    m(K_MULTISET, "MULTISET") \
    m(K_NATIONAL, "NATIONAL") \
    m(K_NATURAL, "NATURAL") \
    m(K_NCHAR, "NCHAR") \
    m(K_NCLOB, "NCLOB") \
    m(K_NEW, "NEW") \
    m(K_NO, "NO") \
    m(K_NONE, "NONE") \
    m(K_NORMALIZE, "NORMALIZE") \
    m(K_NOT, "NOT") \
    m(K_NTH_VALUE, "NTH_VALUE") \
    m(K_NTILE, "NTILE") \
    m(K_NULL, "NULL") \
    m(K_NULLIF, "NULLIF") \
    m(K_NUMERIC, "NUMERIC") \
    m(K_OCTET_LENGTH, "OCTET_LENGTH") \
    m(K_OCCURRENCES_REGEX, "OCCURRENCES_REGEX") \
    m(K_OF, "OF") \
    m(K_OFFSET, "OFFSET") \
    m(K_OLD, "OLD") \
    m(K_ON, "ON") \
    m(K_ONLY, "ONLY") \
    m(K_OPEN, "OPEN") \
    m(K_OR, "OR") \
    m(K_ORDER, "ORDER") \
    m(K_OUT, "OUT") \
    m(K_OUTER, "OUTER") \
    m(K_OVER, "OVER") \
    m(K_OVERLAPS, "OVERLAPS") \
    m(K_OVERLAY, "OVERLAY") \
    m(K_PARAMETER, "PARAMETER") \
    m(K_PARTITION, "PARTITION") \
    m(K_PERCENT, "PERCENT") \
    m(K_PERCENT_RANK, "PERCENT_RANK") \
    m(K_PERCENTILE_CONT, "PERCENTILE_CONT") \
    m(K_PERCENTILE_DISC, "PERCENTILE_DISC") \
    m(K_PERIOD, "PERIOD") \
    m(K_PORTION, "PORTION") \
    m(K_POSITION, "POSITION") \
    m(K_POSITION_REGEX, "POSITION_REGEX") \
    m(K_POWER, "POWER") \
    m(K_PRECEDES, "PRECEDES") \
    m(K_PRECISION, "PRECISION") \
    m(K_PREPARE, "PREPARE") \
    m(K_PRIMARY, "PRIMARY") \
    m(K_PROCEDURE, "PROCEDURE") \
    m(K_RANGE, "RANGE") \
    m(K_RANK, "RANK") \
    m(K_READS, "READS") \
    m(K_REAL, "REAL") \
    m(K_RECURSIVE, "RECURSIVE") \
    m(K_REF, "REF") \
    m(K_REFERENCES, "REFERENCES") \
    m(K_REFERENCING, "REFERENCING") \
    m(K_REGR_AVGX, "REGR_AVGX") \
    m(K_REGR_AVGY, "REGR_AVGY") \
    m(K_REGR_COUNT, "REGR_COUNT") \
    m(K_REGR_INTERCEPT, "REGR_INTERCEPT") \
    m(K_REGR_R2, "REGR_R2") \
    m(K_REGR_SLOPE, "REGR_SLOPE") \
    m(K_REGR_SXX, "REGR_SXX") \
    m(K_REGR_SXY, "REGR_SXY") \
    m(K_REGR_SYY, "REGR_SYY") \
    m(K_RELEASE, "RELEASE") \
    m(K_RESULT, "RESULT") \
    m(K_RETURN, "RETURN") \
    m(K_RETURNS, "RETURNS") \
    m(K_REVOKE, "REVOKE") \
    m(K_RIGHT, "RIGHT") \
    m(K_ROLLBACK, "ROLLBACK") \
    m(K_ROLLUP, "ROLLUP") \
    m(K_ROW, "ROW") \
    m(K_ROW_NUMBER, "ROW_NUMBER") \
    m(K_ROWS, "ROWS") \
    m(K_SAVEPOINT, "SAVEPOINT") \
    m(K_SCOPE, "SCOPE") \
    m(K_SCROLL, "SCROLL") \
    m(K_SEARCH, "SEARCH") \
    m(K_SECOND, "SECOND") \
    m(K_SELECT, "SELECT") \
    m(K_SENSITIVE, "SENSITIVE") \
    m(K_SESSION_USER, "SESSION_USER") \
    m(K_SET, "SET") \
    m(K_SIMILAR, "SIMILAR") \
    m(K_SMALLINT, "SMALLINT") \
    m(K_SOME, "SOME") \
    m(K_SPECIFIC, "SPECIFIC") \
    m(K_SPECIFICTYPE, "SPECIFICTYPE") \
    m(K_SQL, "SQL") \
    m(K_SQLEXCEPTION, "SQLEXCEPTION") \
    m(K_SQLSTATE, "SQLSTATE") \
    m(K_SQLWARNING, "SQLWARNING") \
    m(K_SQRT, "SQRT") \
    m(K_START, "START") \
    m(K_STATIC, "STATIC") \
    m(K_STDDEV_POP, "STDDEV_POP") \
    m(K_STDDEV_SAMP, "STDDEV_SAMP") \
    m(K_SUBMULTISET, "SUBMULTISET") \
    m(K_SUBSTRING, "SUBSTRING") \
    m(K_SUBSTRING_REGEX, "SUBSTRING_REGEX") \
    m(K_SUCCEEDS, "SUCCEEDS") \
    m(K_SUM, "SUM") \
    m(K_SYMMETRIC, "SYMMETRIC") \
    m(K_SYSTEM, "SYSTEM") \
    m(K_SYSTEM_TIME, "SYSTEM_TIME") \
    m(K_SYSTEM_USER, "SYSTEM_USER") \
    m(K_TABLE, "TABLE") \
    m(K_TABLESAMPLE, "TABLESAMPLE") \
    m(K_THEN, "THEN") \
    m(K_TIME, "TIME") \
    m(K_TIMESTAMP, "TIMESTAMP") \
    m(K_TIMEZONE_HOUR, "TIMEZONE_HOUR") \
    m(K_TIMEZONE_MINUTE, "TIMEZONE_MINUTE") \
    m(K_TO, "TO") \
    m(K_TRAILING, "TRAILING") \
    m(K_TRANSLATE, "TRANSLATE") \
    m(K_TRANSLATE_REGEX, "TRANSLATE_REGEX") \
    m(K_TRANSLATION, "TRANSLATION") \
    m(K_TREAT, "TREAT") \
    m(K_TRIGGER, "TRIGGER") \
    m(K_TRUNCATE, "TRUNCATE") \
    m(K_TRIM, "TRIM") \
    m(K_TRIM_ARRAY, "TRIM_ARRAY") \
    m(K_TRUE, "TRUE") \
    m(K_UESCAPE, "UESCAPE") \
    m(K_UNION, "UNION") \
    m(K_UNIQUE, "UNIQUE") \
    m(K_UNKNOWN, "UNKNOWN") \
    m(K_UNNEST, "UNNEST") \
    m(K_UPDATE, "UPDATE") \
    m(K_UPPER, "UPPER") \
    m(K_USER, "USER") \
    m(K_USING, "USING") \
    m(K_VALUE, "VALUE") \
    m(K_VALUES, "VALUES") \
    m(K_VALUE_OF, "VALUE_OF") \
    m(K_VAR_POP, "VAR_POP") \
    m(K_VAR_SAMP, "VAR_SAMP") \
    m(K_VARBINARY, "VARBINARY") \
    m(K_VARCHAR, "VARCHAR") \
    m(K_VARYING, "VARYING") \
    m(K_VERSIONING, "VERSIONING") \
    m(K_WHEN, "WHEN") \
    m(K_WHENEVER, "WHENEVER") \
    m(K_WHERE, "WHERE") \
    m(K_WIDTH_BUCKET, "WIDTH_BUCKET") \
    m(K_WINDOW, "WINDOW") \
    m(K_WITH, "WITH") \
    m(K_WITHIN, "WITHIN") \
    m(K_WITHOUT, "WITHOUT") \
    m(K_YEAR, "YEAR")

// These are *non-reserved words* that only have a special meaning in particular contexts and can be used as identifiers in other contexts.
#define SQL_NON_KEY(m) \
    m(N_A, "A") \
    m(N_ABSOLUTE, "ABSOLUTE") \
    m(N_ACTION, "ACTION") \
    m(N_ADA, "ADA") \
    m(N_ADD, "ADD") \
    m(N_ADMIN, "ADMIN") \
    m(N_AFTER, "AFTER") \
    m(N_ALWAYS, "ALWAYS") \
    m(N_ASC, "ASC") \
    m(N_ASSERTION, "ASSERTION") \
    m(N_ASSIGNMENT, "ASSIGNMENT") \
    m(N_ATTRIBUTE, "ATTRIBUTE") \
    m(N_ATTRIBUTES, "ATTRIBUTES") \
    m(N_BEFORE, "BEFORE") \
    m(N_BERNOULLI, "BERNOULLI") \
    m(N_BREADTH, "BREADTH") \
    m(N_C, "C") \
    m(N_CASCADE, "CASCADE") \
    m(N_CATALOG, "CATALOG") \
    m(N_CATALOG_NAME, "CATALOG_NAME") \
    m(N_CHAIN, "CHAIN") \
    m(N_CHARACTER_SET_CATALOG, "CHARACTER_SET_CATALOG") \
    m(N_CHARACTER_SET_NAME, "CHARACTER_SET_NAME") \
    m(N_CHARACTER_SET_SCHEMA, "CHARACTER_SET_SCHEMA") \
    m(N_CHARACTERISTICS, "CHARACTERISTICS") \
    m(N_CHARACTERS, "CHARACTERS") \
    m(N_CLASS_ORIGIN, "CLASS_ORIGIN") \
    m(N_COBOL, "COBOL") \
    m(N_COLLATION, "COLLATION") \
    m(N_COLLATION_CATALOG, "COLLATION_CATALOG") \
    m(N_COLLATION_NAME, "COLLATION_NAME") \
    m(N_COLLATION_SCHEMA, "COLLATION_SCHEMA") \
    m(N_COLUMN_NAME, "COLUMN_NAME") \
    m(N_COMMAND_FUNCTION, "COMMAND_FUNCTION") \
    m(N_COMMAND_FUNCTION_CODE, "COMMAND_FUNCTION_CODE") \
    m(N_COMMITTED, "COMMITTED") \
    m(N_CONDITION_NUMBER, "CONDITION_NUMBER") \
    m(N_CONNECTION, "CONNECTION") \
    m(N_CONNECTION_NAME, "CONNECTION_NAME") \
    m(N_CONSTRAINT_CATALOG, "CONSTRAINT_CATALOG") \
    m(N_CONSTRAINT_NAME, "CONSTRAINT_NAME") \
    m(N_CONSTRAINT_SCHEMA, "CONSTRAINT_SCHEMA") \
    m(N_CONSTRAINTS, "CONSTRAINTS") \
    m(N_CONSTRUCTOR, "CONSTRUCTOR") \
    m(N_CONTINUE, "CONTINUE") \
    m(N_CURSOR_NAME, "CURSOR_NAME") \
    m(N_DATA, "DATA") \
    m(N_DATETIME_INTERVAL_CODE, "DATETIME_INTERVAL_CODE") \
    m(N_DATETIME_INTERVAL_PRECISION, "DATETIME_INTERVAL_PRECISION") \
    m(N_DEFAULTS, "DEFAULTS") \
    m(N_DEFERRABLE, "DEFERRABLE") \
    m(N_DEFERRED, "DEFERRED") \
    m(N_DEFINED, "DEFINED") \
    m(N_DEFINER, "DEFINER") \
    m(N_DEGREE, "DEGREE") \
    m(N_DEPTH, "DEPTH") \
    m(N_DERIVED, "DERIVED") \
    m(N_DESC, "DESC") \
    m(N_DESCRIPTOR, "DESCRIPTOR") \
    m(N_DIAGNOSTICS, "DIAGNOSTICS") \
    m(N_DISPATCH, "DISPATCH") \
    m(N_DOMAIN, "DOMAIN") \
    m(N_DYNAMIC_FUNCTION, "DYNAMIC_FUNCTION") \
    m(N_DYNAMIC_FUNCTION_CODE, "DYNAMIC_FUNCTION_CODE") \
    m(N_ENFORCED, "ENFORCED") \
    m(N_EXCLUDE, "EXCLUDE") \
    m(N_EXCLUDING, "EXCLUDING") \
    m(N_EXPRESSION, "EXPRESSION") \
    m(N_FINAL, "FINAL") \
    m(N_FIRST, "FIRST") \
    m(N_FLAG, "FLAG") \
    m(N_FOLLOWING, "FOLLOWING") \
    m(N_FORTRAN, "FORTRAN") \
    m(N_FOUND, "FOUND") \
    m(N_G, "G") \
    m(N_GENERAL, "GENERAL") \
    m(N_GENERATED, "GENERATED") \
    m(N_GO, "GO") \
    m(N_GOTO, "GOTO") \
    m(N_GRANTED, "GRANTED") \
    m(N_HIERARCHY, "HIERARCHY") \
    m(N_IGNORE, "IGNORE") \
    m(N_IMMEDIATE, "IMMEDIATE") \
    m(N_IMMEDIATELY, "IMMEDIATELY") \
    m(N_IMPLEMENTATION, "IMPLEMENTATION") \
    m(N_INCLUDING, "INCLUDING") \
    m(N_INCREMENT, "INCREMENT") \
    m(N_INITIALLY, "INITIALLY") \
    m(N_INPUT, "INPUT") \
    m(N_INSTANCE, "INSTANCE") \
    m(N_INSTANTIABLE, "INSTANTIABLE") \
    m(N_INSTEAD, "INSTEAD") \
    m(N_INVOKER, "INVOKER") \
    m(N_ISOLATION, "ISOLATION") \
    m(N_K, "K") \
    m(N_KEY, "KEY") \
    m(N_KEY_MEMBER, "KEY_MEMBER") \
    m(N_KEY_TYPE, "KEY_TYPE") \
    m(N_LAST, "LAST") \
    m(N_LENGTH, "LENGTH") \
    m(N_LEVEL, "LEVEL") \
    m(N_LOCATOR, "LOCATOR") \
    m(N_M, "M") \
    m(N_MAP, "MAP") \
    m(N_MATCHED, "MATCHED") \
    m(N_MAXVALUE, "MAXVALUE") \
    m(N_MESSAGE_LENGTH, "MESSAGE_LENGTH") \
    m(N_MESSAGE_OCTET_LENGTH, "MESSAGE_OCTET_LENGTH") \
    m(N_MESSAGE_TEXT, "MESSAGE_TEXT") \
    m(N_MINVALUE, "MINVALUE") \
    m(N_MORE, "MORE") \
    m(N_MUMPS, "MUMPS") \
    m(N_NAME, "NAME") \
    m(N_NAMES, "NAMES") \
    m(N_NESTING, "NESTING") \
    m(N_NEXT, "NEXT") \
    m(N_NFC, "NFC") \
    m(N_NFD, "NFD") \
    m(N_NFKC, "NFKC") \
    m(N_NFKD, "NFKD") \
    m(N_NORMALIZED, "NORMALIZED") \
    m(N_NULLABLE, "NULLABLE") \
    m(N_NULLS, "NULLS") \
    m(N_NUMBER, "NUMBER") \
    m(N_OBJECT, "OBJECT") \
    m(N_OCTETS, "OCTETS") \
    m(N_OPTION, "OPTION") \
    m(N_OPTIONS, "OPTIONS") \
    m(N_ORDERING, "ORDERING") \
    m(N_ORDINALITY, "ORDINALITY") \
    m(N_OTHERS, "OTHERS") \
    m(N_OUTPUT, "OUTPUT") \
    m(N_OVERRIDING, "OVERRIDING") \
    m(N_P, "P") \
    m(N_PAD, "PAD") \
    m(N_PARAMETER_MODE, "PARAMETER_MODE") \
    m(N_PARAMETER_NAME, "PARAMETER_NAME") \
    m(N_PARAMETER_ORDINAL_POSITION, "PARAMETER_ORDINAL_POSITION") \
    m(N_PARAMETER_SPECIFIC_CATALOG, "PARAMETER_SPECIFIC_CATALOG") \
    m(N_PARAMETER_SPECIFIC_NAME, "PARAMETER_SPECIFIC_NAME") \
    m(N_PARAMETER_SPECIFIC_SCHEMA, "PARAMETER_SPECIFIC_SCHEMA") \
    m(N_PARTIAL, "PARTIAL") \
    m(N_PASCAL, "PASCAL") \
    m(N_PATH, "PATH") \
    m(N_PLACING, "PLACING") \
    m(N_PLI, "PLI") \
    m(N_PRECEDING, "PRECEDING") \
    m(N_PRESERVE, "PRESERVE") \
    m(N_PRIOR, "PRIOR") \
    m(N_PRIVILEGES, "PRIVILEGES") \
    m(N_PUBLIC, "PUBLIC") \
    m(N_READ, "READ") \
    m(N_RELATIVE, "RELATIVE") \
    m(N_REPEATABLE, "REPEATABLE") \
    m(N_RESPECT, "RESPECT") \
    m(N_RESTART, "RESTART") \
    m(N_RESTRICT, "RESTRICT") \
    m(N_RETURNED_CARDINALITY, "RETURNED_CARDINALITY") \
    m(N_RETURNED_LENGTH, "RETURNED_LENGTH") \
    m(N_RETURNED_OCTET_LENGTH, "RETURNED_OCTET_LENGTH") \
    m(N_RETURNED_SQLSTATE, "RETURNED_SQLSTATE") \
    m(N_ROLE, "ROLE") \
    m(N_ROUTINE, "ROUTINE") \
    m(N_ROUTINE_CATALOG, "ROUTINE_CATALOG") \
    m(N_ROUTINE_NAME, "ROUTINE_NAME") \
    m(N_ROUTINE_SCHEMA, "ROUTINE_SCHEMA") \
    m(N_ROW_COUNT, "ROW_COUNT") \
    m(N_SCALE, "SCALE") \
    m(N_SCHEMA, "SCHEMA") \
    m(N_SCHEMA_NAME, "SCHEMA_NAME") \
    m(N_SCOPE_CATALOG, "SCOPE_CATALOG") \
    m(N_SCOPE_NAME, "SCOPE_NAME") \
    m(N_SCOPE_SCHEMA, "SCOPE_SCHEMA") \
    m(N_SECTION, "SECTION") \
    m(N_SECURITY, "SECURITY") \
    m(N_SELF, "SELF") \
    m(N_SEQUENCE, "SEQUENCE") \
    m(N_SERIALIZABLE, "SERIALIZABLE") \
    m(N_SERVER_NAME, "SERVER_NAME") \
    m(N_SESSION, "SESSION") \
    m(N_SETS, "SETS") \
    m(N_SIMPLE, "SIMPLE") \
    m(N_SIZE, "SIZE") \
    m(N_SOURCE, "SOURCE") \
    m(N_SPACE, "SPACE") \
    m(N_SPECIFIC_NAME, "SPECIFIC_NAME") \
    m(N_STATE, "STATE") \
    m(N_STATEMENT, "STATEMENT") \
    m(N_STRUCTURE, "STRUCTURE") \
    m(N_STYLE, "STYLE") \
    m(N_SUBCLASS_ORIGIN, "SUBCLASS_ORIGIN") \
    m(N_T, "T") \
    m(N_TABLE_NAME, "TABLE_NAME") \
    m(N_TEMPORARY, "TEMPORARY") \
    m(N_TIES, "TIES") \
    m(N_TOP_LEVEL_COUNT, "TOP_LEVEL_COUNT") \
    m(N_TRANSACTION, "TRANSACTION") \
    m(N_TRANSACTION_ACTIVE, "TRANSACTION_ACTIVE") \
    m(N_TRANSACTIONS_COMMITTED, "TRANSACTIONS_COMMITTED") \
    m(N_TRANSACTIONS_ROLLED_BACK, "TRANSACTIONS_ROLLED_BACK") \
    m(N_TRANSFORM, "TRANSFORM") \
    m(N_TRANSFORMS, "TRANSFORMS") \
    m(N_TRIGGER_CATALOG, "TRIGGER_CATALOG") \
    m(N_TRIGGER_NAME, "TRIGGER_NAME") \
    m(N_TRIGGER_SCHEMA, "TRIGGER_SCHEMA") \
    m(N_TYPE, "TYPE") \
    m(N_UNBOUNDED, "UNBOUNDED") \
    m(N_UNCOMMITTED, "UNCOMMITTED") \
    m(N_UNDER, "UNDER") \
    m(N_UNNAMED, "UNNAMED") \
    m(N_USAGE, "USAGE") \
    m(N_USER_DEFINED_TYPE_CATALOG, "USER_DEFINED_TYPE_CATALOG") \
    m(N_USER_DEFINED_TYPE_CODE, "USER_DEFINED_TYPE_CODE") \
    m(N_USER_DEFINED_TYPE_NAME, "USER_DEFINED_TYPE_NAME") \
    m(N_USER_DEFINED_TYPE_SCHEMA, "USER_DEFINED_TYPE_SCHEMA") \
    m(N_VIEW, "VIEW") \
    m(N_WORK, "WORK") \
    m(N_WRITE, "WRITE") \
    m(N_ZONE, "ZONE")

enum class NonKey {
#define CODE(t, _) t,
    SQL_NON_KEY(CODE)
#undef CODE
};

#define CODE(t, str) + 1
constexpr auto Num_Keys = 0 SQL_KEY(CODE);
constexpr auto Num_Non_Keys = 0 SQL_NON_KEY(CODE);
#undef CODE

class Tok {
public:
    // clang-format off
    enum class Tag {
        Nil,
#define CODE(t, _) t,
        SQL_KEY(CODE)
        SQL_TOK(CODE)
#undef CODE
        K_IS_NOT ///< Not an actual keyword but we use this for UnExpr::tag.
    };
    // clang-format on

    enum class Prec {
        Bot,
        Join,
        Or,
        And,
        Between,
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
        , tag_(Tag::V_id)
        , sym_(sym) {}
    Tok(Loc loc, uint64_t u64)
        : loc_(loc)
        , tag_(Tag::V_int)
        , u64_(u64) {}

    Loc loc() const { return loc_; }
    Tag tag() const { return tag_; }
    bool isa(Tag tag) const { return tag == tag_; }
    bool isa_key() const { return (int)tag() < Num_Keys; }
    explicit operator bool() const { return tag_ != Tag::Nil; }

    Sym sym() const {
        assert(isa(Tag::V_id));
        return sym_;
    }
    uint64_t u64() const { return u64_; }

    static std::string_view str(Tok::Tag);
    static std::optional<Prec> un_prec(Tok::Tag);
    static std::optional<Prec> bin_prec(Tok::Tag);

private:
    Loc loc_;
    Tag tag_ = Tag::Nil;
    union {
        Sym sym_;
        uint64_t u64_;
    };
};

std::ostream& operator<<(std::ostream&, Tok::Tag);
std::ostream& operator<<(std::ostream&, Tok);

} // namespace sql

template<> struct std::formatter<sql::Tok> : fe::ostream_formatter {};
