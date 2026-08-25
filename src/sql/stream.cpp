#include <cctype>

#include <iostream>

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

/*
 * Type
 */

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

std::ostream& SimpleType::stream(std::ostream& o) const {
    // Bare `DOUBLE` is not a type name of its own - the standard spells it `DOUBLE PRECISION`.
    o << tag() << (tag() == Tok::Tag::K_DOUBLE ? " PRECISION" : "");
    if (varying()) o << " VARYING";
    stream_args(o, args());
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
 * Constraint
 */

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
            stream_sym(o << "REFERENCES ", table());
            if (!ref_cols().empty()) stream_syms(o << ' ', ref_cols());
            break;
        case Foreign_Key:
            stream_syms(o << "FOREIGN KEY ", cols());
            stream_sym(o << " REFERENCES ", table());
            if (!ref_cols().empty()) stream_syms(o << ' ', ref_cols());
            break;
    }

    return o;
}

/*
 * Expr
 */

// clang-format off
std::ostream& ErrExpr  ::stream(std::ostream& o) const { return o << "<error expression>"; }
std::ostream& SimpleVal::stream(std::ostream& o) const { return o << tag(); }
std::ostream& IntVal   ::stream(std::ostream& o) const { return o << u64(); }
std::ostream& StrVal   ::stream(std::ostream& o) const { return stream_str(o, sym()); }
std::ostream& Drop     ::stream(std::ostream& o) const { return stream_sym(o << "DROP TABLE ", sym()); }
// clang-format on

std::ostream& Id::stream(std::ostream& o) const {
    for (auto sep = ""; auto&& sym : syms()) {
        stream_sym(o << sep, sym);
        sep = ".";
    }

    if (asterisk()) o << ".*";
    return o;
}

std::ostream& UnExpr::stream(std::ostream& o) const {
    o << '(' << tag() << ' ';
    rhs()->stream(o);
    return o << ')';
}

std::ostream& Func::stream(std::ostream& o) const {
    stream_sym(o, sym()) << '(';
    if (distinct()) o << "DISTINCT ";
    for (auto sep = ""; const auto& arg : args()) {
        o << sep;
        arg->stream(o);
        sep = ", ";
    }
    return o << ')';
}

std::ostream& Between::stream(std::ostream& o) const {
    o << '(';
    expr()->stream(o);
    if (negated()) o << " NOT";
    lo()->stream(o << " BETWEEN ");
    hi()->stream(o << " AND ");
    return o << ')';
}

std::ostream& Cast::stream(std::ostream& o) const {
    expr()->stream(o << "CAST(");
    type()->stream(o << " AS ");
    return o << ')';
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

std::ostream& ParenExprList::stream(std::ostream& o) const {
    o << '(';
    for (auto sep = ""; const auto& arg : args()) {
        o << sep;
        arg->stream(o);
        sep = ", ";
    }
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

std::ostream& Create::stream(std::ostream& o) const {
    stream_sym(o << "CREATE TABLE ", sym()) << " (";
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

std::ostream& Select::stream(std::ostream& o) const {
    o << "SELECT ";
    if (distinct()) o << "DISTINCT ";

    if (elems().empty()) {
        o << "*";
    } else {
        for (auto sep = ""; auto&& elem : elems()) {
            o << sep;
            elem->stream(o);
            sep = ", ";
        }
    }

    o << " FROM ";
    for (auto sep = ""; auto&& from : froms()) {
        from->stream(o << sep);
        sep = ", ";
    }

    if (where()) where()->stream(o << " WHERE ");

    if (!groups().empty()) {
        o << " GROUP BY ";
        for (auto sep = ""; auto&& group : groups()) {
            group->stream(o << sep);
            sep = ", ";
        }
    }

    if (having()) having()->stream(o << " HAVING ");

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
    expr()->stream(o);
    if (as()) stream_sym(o << " AS ", as());
    if (!cols().empty()) stream_syms(o << ' ', cols());
    return o;
}

/*
 * Query
 */

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
    body()->stream(o);

    if (!orders().empty()) {
        o << " ORDER BY ";
        for (auto sep = ""; auto&& order : orders()) {
            order->stream(o << sep);
            sep = ", ";
        }
    }

    if (offset()) offset()->stream(o << " OFFSET ") << " ROWS";
    if (fetch()) fetch()->stream(o << " FETCH NEXT ") << " ROWS ONLY";

    return o;
}

std::ostream& Query::Order::stream(std::ostream& o) const {
    expr()->stream(o);
    return o << (desc() ? " DESC" : " ASC");
}

/*
 * Insert / Update / Delete
 */

std::ostream& Insert::stream(std::ostream& o) const {
    stream_sym(o << "INSERT INTO ", sym());
    if (!cols().empty()) stream_syms(o << ' ', cols());

    if (query()) return query()->stream(o << ' ');

    o << " VALUES ";
    for (auto sep = ""; auto&& row : rows()) {
        row->stream(o << sep);
        sep = ", ";
    }
    return o;
}

std::ostream& Update::stream(std::ostream& o) const {
    stream_sym(o << "UPDATE ", sym());
    if (as()) stream_sym(o << " AS ", as());

    o << " SET ";
    for (auto sep = ""; auto&& assign : assigns()) {
        assign->stream(o << sep);
        sep = ", ";
    }

    if (where()) where()->stream(o << " WHERE ");
    return o;
}

std::ostream& Update::Assign::stream(std::ostream& o) const { return expr()->stream(stream_sym(o, sym()) << " = "); }

std::ostream& Delete::stream(std::ostream& o) const {
    stream_sym(o << "DELETE FROM ", sym());
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
