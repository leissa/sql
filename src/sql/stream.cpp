#include <cctype>
#include <cstdlib>

#include <iostream>
#include <string>

#include "sql/ast.h"

namespace sql {

// stream

void Node::dump() const { stream(std::cout) << std::endl; }

/// Streams an identifier so that it lexes back to the same Sym: the Lexer folds unquoted
/// identifiers to lower case, so anything else has to go out as a delimited identifier.
static std::ostream& stream_sym(std::ostream& o, Sym sym) {
    auto sv    = *sym;
    auto plain = !sv.empty() && !std::isdigit((unsigned char)sv.front());
    for (auto c : sv)
        plain &= c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');

    if (plain) return o << sv;

    o << '"';
    for (auto c : sv) {
        if (c == '"') o << '"';
        o << c;
    }
    return o << '"';
}

/// Streams a string literal, doubling the delimiter the way SQL escapes it.
static std::ostream& stream_str(std::ostream& o, Sym sym) {
    o << '\'';
    for (auto c : *sym) {
        if (c == '\'') o << '\'';
        o << c;
    }
    return o << '\'';
}

/// Streams a possibly qualified name such as `s.t` - no parentheses, unlike stream_syms below.
static std::ostream& stream_name(std::ostream& o, const Syms& syms) {
    for (auto sep = ""; auto&& sym : syms) {
        stream_sym(o << sep, sym);
        sep = ".";
    }
    return o;
}

/// Streams a parenthesized argument list, or nothing at all if @p args is empty.
template<class T>
static std::ostream& stream_args(std::ostream& o, const T& args) {
    if (args.empty()) return o;

    o << '(';
    for (auto sep = ""; const auto& arg : args) {
        arg->stream(o << sep);
        sep = ", ";
    }
    return o << ')';
}

static std::ostream& stream_syms(std::ostream& o, const Syms& syms) {
    o << '(';
    for (auto sep = ""; auto&& sym : syms) {
        stream_sym(o << sep, sym);
        sep = ", ";
    }
    return o << ')';
}

/// Streams a comma-separated list of Node%s - no delimiters of its own.
template<class T>
static std::ostream& stream_list(std::ostream& o, const T& list) {
    for (auto sep = ""; const auto& elem : list) {
        elem->stream(o << sep);
        sep = ", ";
    }
    return o;
}

/*
 * Interval
 */

std::ostream& Interval::stream(std::ostream& o) const {
    o << from();
    stream_args(o, from_args());
    if (to() != Tok::Tag::Nil) {
        o << " TO " << to();
        stream_args(o, to_args());
    }
    return o;
}

/*
 * Type
 */

std::ostream& SimpleType::stream(std::ostream& o) const {
    // Bare `DOUBLE` is not a type name of its own - the standard spells it `DOUBLE PRECISION`.
    o << tag() << (tag() == Tok::Tag::K_DOUBLE ? " PRECISION" : "");
    if (varying()) o << " VARYING";
    stream_args(o, args());
    if (interval()) interval()->stream(o << ' ');
    if (zone() != Tok::Tag::Nil) o << ' ' << zone() << " TIME ZONE";
    if (not_null()) o << " NOT NULL";
    return o;
}

std::ostream& NamedType::stream(std::ostream& o) const {
    stream_sym(o, sym());
    stream_args(o, args());
    if (not_null()) o << " NOT NULL";
    return o;
}

/*
 * Order / Window
 */

std::ostream& Order::stream(std::ostream& o) const {
    expr()->stream(o);
    o << (desc() ? " DESC" : " ASC");

    // clang-format off
    switch (nulls()) {
        case Nulls_First: o << " NULLS FIRST"; break;
        case Nulls_Last:  o << " NULLS LAST";  break;
        case Nulls_None:                       break;
    }
    // clang-format on
    return o;
}

std::ostream& Frame::Bound::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Unbounded_Preceding: return o << "UNBOUNDED PRECEDING";
        case Unbounded_Following: return o << "UNBOUNDED FOLLOWING";
        case Current_Row:         return o << "CURRENT ROW";
        case Preceding:           return expr()->stream(o) << " PRECEDING";
        case Following:           return expr()->stream(o) << " FOLLOWING";
    }
    // clang-format on
    return o;
}

std::ostream& Frame::stream(std::ostream& o) const {
    o << unit() << ' ';
    if (hi()) {
        lo()->stream(o << "BETWEEN ");
        hi()->stream(o << " AND ");
    } else {
        lo()->stream(o);
    }

    // clang-format off
    switch (exclude()) {
        case Exclude_Current_Row: o << " EXCLUDE CURRENT ROW"; break;
        case Exclude_Group:       o << " EXCLUDE GROUP";       break;
        case Exclude_Ties:        o << " EXCLUDE TIES";        break;
        case Exclude_No_Others:   o << " EXCLUDE NO OTHERS";   break;
        case Exclude_None:                                     break;
    }
    // clang-format on
    return o;
}

std::ostream& Window::stream(std::ostream& o) const {
    // `OVER w` refers to a window defined elsewhere and takes no parentheses.
    if (!paren()) return stream_sym(o, name());

    o << '(';
    auto sep = "";
    if (name()) {
        stream_sym(o, name());
        sep = " ";
    }

    if (!partitions().empty()) {
        stream_list(o << sep << "PARTITION BY ", partitions());
        sep = " ";
    }

    if (!orders().empty()) {
        stream_list(o << sep << "ORDER BY ", orders());
        sep = " ";
    }

    if (frame()) frame()->stream(o << sep);
    return o << ')';
}

/*
 * Constraint
 */

/// Streams the referential action of an `ON DELETE`/`ON UPDATE` clause, if there is one.
static std::ostream& stream_action(std::ostream& o, std::string_view when, Constraint::Action action) {
    // clang-format off
    switch (action) {
        case Constraint::No_Action:   return o << " ON " << when << " NO ACTION";
        case Constraint::Restrict:    return o << " ON " << when << " RESTRICT";
        case Constraint::Cascade:     return o << " ON " << when << " CASCADE";
        case Constraint::Set_Null:    return o << " ON " << when << " SET NULL";
        case Constraint::Set_Default: return o << " ON " << when << " SET DEFAULT";
        case Constraint::Action_None: return o;
    }
    // clang-format on
    return o;
}

std::ostream& Constraint::stream(std::ostream& o) const {
    if (name()) stream_sym(o << "CONSTRAINT ", name()) << ' ';

    switch (tag()) {
        case Primary_Key:
            o << "PRIMARY KEY";
            if (!cols().empty()) stream_syms(o << ' ', cols());
            break;
        case Unique:
            o << "UNIQUE";
            if (!cols().empty()) stream_syms(o << ' ', cols());
            break;
        case Check: expr()->stream(o << "CHECK (") << ')'; break;
        case Default: expr()->stream(o << "DEFAULT "); break;
        case References:
            stream_name(o << "REFERENCES ", table());
            if (!ref_cols().empty()) stream_syms(o << ' ', ref_cols());
            break;
        case Foreign_Key:
            stream_syms(o << "FOREIGN KEY ", cols());
            stream_name(o << " REFERENCES ", table());
            if (!ref_cols().empty()) stream_syms(o << ' ', ref_cols());
            break;
    }

    stream_action(o, "DELETE", on_delete());
    stream_action(o, "UPDATE", on_update());
    return o;
}

/*
 * Val
 */

double RealVal::f64() const { return std::strtod(std::string(*sym()).c_str(), nullptr); }

// clang-format off
std::ostream& ErrExpr  ::stream(std::ostream& o) const { return o << "<error expression>"; }
std::ostream& SimpleVal::stream(std::ostream& o) const { return o << tag(); }
std::ostream& IntVal   ::stream(std::ostream& o) const { return o << u64(); }
std::ostream& RealVal  ::stream(std::ostream& o) const { return o << *sym(); }
std::ostream& StrVal   ::stream(std::ostream& o) const { return stream_str(o, sym()); }
std::ostream& Param    ::stream(std::ostream& o) const { return o << *sym(); }
std::ostream& Table    ::stream(std::ostream& o) const { return stream_name(o << "TABLE ", syms()); }
std::ostream& Truncate ::stream(std::ostream& o) const { return stream_name(o << "TRUNCATE TABLE ", syms()); }
// clang-format on

std::ostream& TypedVal::stream(std::ostream& o) const {
    stream_str(o << tag() << ' ', sym());
    if (interval()) interval()->stream(o << ' ');
    return o;
}

/*
 * Expr
 */

std::ostream& Id::stream(std::ostream& o) const {
    stream_name(o, syms());
    if (asterisk()) o << ".*";
    return o;
}

std::ostream& UnExpr::stream(std::ostream& o) const {
    o << '(' << tag() << ' ';
    rhs()->stream(o);
    return o << ')';
}

std::ostream& Func::stream(std::ostream& o) const {
    stream_name(o, syms()) << '(';
    if (distinct()) o << "DISTINCT ";
    stream_list(o, args());
    o << ')';

    if (!withins().empty()) stream_list(o << " WITHIN GROUP (ORDER BY ", withins()) << ')';
    if (filter()) filter()->stream(o << " FILTER (WHERE ") << ')';
    if (over()) over()->stream(o << " OVER ");
    return o;
}

std::ostream& Between::stream(std::ostream& o) const {
    o << '(';
    expr()->stream(o);
    if (negated()) o << " NOT";
    lo()->stream(o << " BETWEEN ");
    hi()->stream(o << " AND ");
    return o << ')';
}

std::ostream& Like::stream(std::ostream& o) const {
    o << '(';
    expr()->stream(o);
    if (negated()) o << " NOT";
    o << (similar() ? " SIMILAR TO " : " LIKE ");
    pattern()->stream(o);
    if (escape()) escape()->stream(o << " ESCAPE ");
    return o << ')';
}

std::ostream& Cast::stream(std::ostream& o) const {
    expr()->stream(o << "CAST(");
    type()->stream(o << " AS ");
    return o << ')';
}

std::ostream& Collate::stream(std::ostream& o) const {
    expr()->stream(o << '(');
    return stream_name(o << " COLLATE ", syms()) << ')';
}

std::ostream& CaseExpr::stream(std::ostream& o) const {
    o << "CASE";
    if (operand()) operand()->stream(o << ' ');
    for (const auto& when : whens())
        when->stream(o << ' ');
    if (elze()) elze()->stream(o << " ELSE ");
    return o << " END";
}

std::ostream& CaseExpr::When::stream(std::ostream& o) const {
    cond()->stream(o << "WHEN ");
    return then()->stream(o << " THEN ");
}

std::ostream& Extract::stream(std::ostream& o) const {
    stream_sym(o << "EXTRACT(", field());
    return expr()->stream(o << " FROM ") << ')';
}

std::ostream& Substring::stream(std::ostream& o) const {
    expr()->stream(o << "SUBSTRING(");
    from()->stream(o << " FROM ");
    if (four()) four()->stream(o << " FOR ");
    return o << ')';
}

std::ostream& Trim::stream(std::ostream& o) const {
    o << "TRIM(";
    if (tag() != Tok::Tag::Nil) o << tag() << ' ';
    if (chars()) chars()->stream(o) << ' ';
    if (tag() != Tok::Tag::Nil || chars()) o << "FROM ";
    return expr()->stream(o) << ')';
}

std::ostream& Position::stream(std::ostream& o) const {
    needle()->stream(o << "POSITION(");
    return haystack()->stream(o << " IN ") << ')';
}

std::ostream& Overlay::stream(std::ostream& o) const {
    expr()->stream(o << "OVERLAY(");
    placing()->stream(o << " PLACING ");
    from()->stream(o << " FROM ");
    if (four()) four()->stream(o << " FOR ");
    return o << ')';
}

std::ostream& ParenExprList::stream(std::ostream& o) const {
    o << '(';
    stream_list(o, args());
    return o << ')';
}

std::ostream& BinExpr::stream(std::ostream& o) const {
    o << '(';
    lhs()->stream(o);
    o << ' ' << tag() << ' ';
    rhs()->stream(o);
    return o << ')';
}

std::ostream& BinExprWithPreTag::stream(std::ostream& o) const {
    o << '(';
    lhs()->stream(o);
    o << ' ' << pretag() << ' ' << tag() << ' ';
    rhs()->stream(o);
    return o << ')';
}

std::ostream& QuantExpr::stream(std::ostream& o) const {
    o << '(';
    lhs()->stream(o);
    o << ' ' << tag() << ' ' << quant() << ' ';
    rhs()->stream(o);
    return o << ')';
}

std::ostream& Grouping::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Rollup: o << "ROLLUP ";        break;
        case Cube:   o << "CUBE ";          break;
        case Sets:   o << "GROUPING SETS "; break;
        case Empty:  return o << "()";
    }
    // clang-format on
    o << '(';
    stream_list(o, args());
    return o << ')';
}

std::ostream& Values::stream(std::ostream& o) const { return stream_list(o << "VALUES ", rows()); }

/*
 * DDL
 */

/// Streams a `CASCADE`/`RESTRICT` drop behavior, if there is one.
static std::ostream& stream_behavior(std::ostream& o, Behavior behavior) {
    // clang-format off
    switch (behavior) {
        case Behavior::Cascade:  o << " CASCADE";  break;
        case Behavior::Restrict: o << " RESTRICT"; break;
        case Behavior::None:                       break;
    }
    // clang-format on
    return o;
}

std::ostream& Create::stream(std::ostream& o) const {
    o << "CREATE " << (temporary() ? "TEMPORARY " : "") << "TABLE ";
    if (if_not_exists()) o << "IF NOT EXISTS ";
    stream_name(o, syms());

    if (query()) return query()->stream(o << " AS ");

    o << " (";
    auto sep = "";
    for (auto&& elem : elems()) {
        elem->stream(o << sep);
        sep = ", ";
    }
    for (auto&& constraint : constraints()) {
        constraint->stream(o << sep);
        sep = ", ";
    }
    return o << ")";
}

std::ostream& Create::Elem::stream(std::ostream& o) const {
    stream_sym(o, sym()) << " ";
    type()->stream(o);
    for (auto&& constraint : constraints())
        constraint->stream(o << ' ');
    return o;
}

std::ostream& CreateView::stream(std::ostream& o) const {
    o << "CREATE " << (replace() ? "OR REPLACE " : "") << "VIEW ";
    stream_name(o, syms());
    if (!cols().empty()) stream_syms(o << ' ', cols());
    query()->stream(o << " AS ");

    if (check() != Tok::Tag::Nil) {
        o << " WITH ";
        if (check() != Tok::Tag::K_CHECK) o << check() << ' ';
        o << "CHECK OPTION";
    }
    return o;
}

std::ostream& CreateIndex::stream(std::ostream& o) const {
    o << "CREATE " << (unique() ? "UNIQUE " : "") << "INDEX ";
    if (if_not_exists()) o << "IF NOT EXISTS ";
    stream_sym(o, sym());
    stream_name(o << " ON ", table());
    stream_list(o << " (", cols());
    return o << ')';
}

std::ostream& CreateSchema::stream(std::ostream& o) const {
    o << "CREATE SCHEMA ";
    if (if_not_exists()) o << "IF NOT EXISTS ";
    return stream_name(o, syms());
}

std::ostream& Alter::stream(std::ostream& o) const {
    stream_name(o << "ALTER TABLE ", table());

    switch (tag()) {
        case Add_Column: elem()->stream(o << " ADD COLUMN "); break;
        case Add_Constraint: constraint()->stream(o << " ADD "); break;
        case Drop_Column: stream_behavior(stream_sym(o << " DROP COLUMN ", sym()), behavior()); break;
        case Drop_Constraint: stream_behavior(stream_sym(o << " DROP CONSTRAINT ", sym()), behavior()); break;
        case Set_Default: expr()->stream(stream_sym(o << " ALTER COLUMN ", sym()) << " SET DEFAULT "); break;
        case Drop_Default: stream_sym(o << " ALTER COLUMN ", sym()) << " DROP DEFAULT"; break;
        case Set_Not_Null: stream_sym(o << " ALTER COLUMN ", sym()) << " SET NOT NULL"; break;
        case Drop_Not_Null: stream_sym(o << " ALTER COLUMN ", sym()) << " DROP NOT NULL"; break;
        case Set_Data_Type: type()->stream(stream_sym(o << " ALTER COLUMN ", sym()) << " SET DATA TYPE "); break;
        case Rename_Table: stream_sym(o << " RENAME TO ", sym()); break;
        case Rename_Column: stream_sym(stream_sym(o << " RENAME COLUMN ", sym()) << " TO ", sym2()); break;
    }

    return o;
}

std::ostream& Drop::stream(std::ostream& o) const {
    o << "DROP ";
    // clang-format off
    switch (tag()) {
        case Table:  o << "TABLE ";  break;
        case View:   o << "VIEW ";   break;
        case Index:  o << "INDEX ";  break;
        case Schema: o << "SCHEMA "; break;
    }
    // clang-format on
    if (if_exists()) o << "IF EXISTS ";
    return stream_behavior(stream_name(o, syms()), behavior());
}

std::ostream& Transact::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Start:       return o << "START TRANSACTION";
        case Commit:      return o << "COMMIT";
        case Rollback:    return o << "ROLLBACK";
        case Rollback_To: return stream_sym(o << "ROLLBACK TO SAVEPOINT ", sym());
        case Savepoint:   return stream_sym(o << "SAVEPOINT ", sym());
        case Release:     return stream_sym(o << "RELEASE SAVEPOINT ", sym());
    }
    // clang-format on
    return o;
}

/*
 * Query expressions
 */

std::ostream& Join::stream(std::ostream& o) const {
    o << '(';
    lhs()->stream(o);

    if (tag() == Cross) {
        o << " CROSS";
    } else {
        if (tag() & Natural) o << " NATURAL";
        // Full is Left|Right, so mask first - a plain Left would match `& Full` too.
        // clang-format off
        switch (tag() & Full) {
            case Full:  o << " FULL";  break;
            case Left:  o << " LEFT";  break;
            case Right: o << " RIGHT"; break;
            default:    o << " INNER"; break;
        }
        // clang-format on
    }
    o << " JOIN ";
    rhs()->stream(o);

    if (auto on = std::get_if<On>(&spec()))
        (*on)->stream(o << " ON ");
    else if (auto syms = std::get_if<Using>(&spec()))
        stream_syms(o << " USING ", *syms);
    return o << ')';
}

std::ostream& Select::stream(std::ostream& o) const {
    o << "SELECT ";
    if (distinct()) o << "DISTINCT ";

    if (elems().empty())
        o << "*";
    else
        stream_list(o, elems());

    // A `SELECT` without a `FROM` is fine - `SELECT 1` needs no table to compute its value.
    if (!froms().empty()) stream_list(o << " FROM ", froms());
    if (where()) where()->stream(o << " WHERE ");
    if (!groups().empty()) stream_list(o << " GROUP BY ", groups());
    if (having()) having()->stream(o << " HAVING ");
    if (!windows().empty()) stream_list(o << " WINDOW ", windows());

    return o;
}

std::ostream& Select::Elem::stream(std::ostream& o) const {
    expr()->stream(o);
    switch (syms().size()) {
        case 0: break;
        case 1: stream_sym(o << " AS ", syms().front()); break;
        default: stream_syms(o << " AS ", syms());
    }
    return o;
}

std::ostream& Select::From::stream(std::ostream& o) const {
    if (lateral()) o << "LATERAL ";
    expr()->stream(o);
    if (ordinality()) o << " WITH ORDINALITY";
    if (as()) stream_sym(o << " AS ", as());
    if (!cols().empty()) stream_syms(o << ' ', cols());
    return o;
}

std::ostream& Select::WindowDef::stream(std::ostream& o) const {
    stream_sym(o, sym());
    return window()->stream(o << " AS ");
}

/// Like Select and Query - and unlike the self-parenthesizing Expr%essions - this prints *without*
/// parentheses: a ParenExprList preserves the ones that actually group in the source.
std::ostream& SetOp::stream(std::ostream& o) const {
    lhs()->stream(o);

    // clang-format off
    switch (tag()) {
        case Union:     o << " UNION";     break;
        case Intersect: o << " INTERSECT"; break;
        case Except:    o << " EXCEPT";    break;
    }
    // clang-format on
    if (all()) o << " ALL";

    return rhs()->stream(o << ' ');
}

std::ostream& Query::stream(std::ostream& o) const {
    if (!ctes().empty()) stream_list(o << "WITH " << (recursive() ? "RECURSIVE " : ""), ctes()) << ' ';

    body()->stream(o);

    if (!orders().empty()) stream_list(o << " ORDER BY ", orders());
    if (offset()) offset()->stream(o << " OFFSET ") << " ROWS";
    if (fetch()) fetch()->stream(o << " FETCH NEXT ") << " ROWS ONLY";
    if (limit()) limit()->stream(o << " LIMIT ");

    return o;
}

std::ostream& Query::Cte::stream(std::ostream& o) const {
    stream_sym(o, sym());
    if (!cols().empty()) stream_syms(o << ' ', cols());
    return query()->stream(o << " AS (") << ')';
}

/*
 * Insert / Update / Delete
 */

std::ostream& Insert::stream(std::ostream& o) const {
    stream_name(o << "INSERT INTO ", syms());
    if (!cols().empty()) stream_syms(o << ' ', cols());

    if (!query()) return o << " DEFAULT VALUES";
    return query()->stream(o << ' ');
}

std::ostream& Update::stream(std::ostream& o) const {
    stream_name(o << "UPDATE ", syms());
    if (as()) stream_sym(o << " AS ", as());

    stream_list(o << " SET ", assigns());
    if (where()) where()->stream(o << " WHERE ");
    return o;
}

std::ostream& Update::Assign::stream(std::ostream& o) const { return expr()->stream(stream_name(o, syms()) << " = "); }

std::ostream& Delete::stream(std::ostream& o) const {
    stream_name(o << "DELETE FROM ", syms());
    if (as()) stream_sym(o << " AS ", as());
    if (where()) where()->stream(o << " WHERE ");
    return o;
}

/*
 * Misc
 */

std::ostream& Prog::stream(std::ostream& o) const {
    for (auto sep = ""; auto&& expr : exprs()) {
        expr->stream(o << sep) << ';';
        sep = "\n";
    }
    return o;
}

} // namespace sql
