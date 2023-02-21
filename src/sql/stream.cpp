#include <iostream>

#include "sql/ast.h"

namespace sql {

// stream

void Node::dump() const { stream(std::cout) << std::endl; }

/*
 * Expr
 */

// clang-format off
std::ostream& ErrExpr  ::stream(std::ostream& o) const { return o << "<error>"; }
std::ostream& SimpleVal::stream(std::ostream& o) const { return o << tag(); }
std::ostream& IntVal   ::stream(std::ostream& o) const { return o << u64(); }
// clang-format on

std::ostream& IdExpr::stream(std::ostream& o) const {
    const char* sep = "";
    for (auto&& sym : syms()) {
        o << sep << sym;
        sep = ".";
    }

    if (asterisk()) o << ".*";
    return o;
}

std::ostream& UnExpr::stream(std::ostream& o) const {
    o << '(' << tag();
    rhs()->stream(o);
    return o << ')';
}

std::ostream& BinExpr::stream(std::ostream& o) const {
    o << '(';
    lhs()->stream(o);
    o << ' ' << tag() << ' ';
    rhs()->stream(o);
    return o << ')';
}

/*
 * Stmt
 */

std::ostream& Select::stream(std::ostream& o) const {
    o << "SELECT ";
    if (distinct()) o << "DISTINCT ";

    if (target().empty()) {
        o << "*";
    } else {
        const char* sep = "";
        for (auto&& elem : target()) {
            o << sep;
            elem->stream(o);
            sep = ", ";
        }
    }
    from()->stream(o << " FROM ");
    if (where()) where()->stream(o << " WHERE ");
    if (group()) group()->stream(o << " GROUP BY ");
    return o;
}

std::ostream& Select::Elem::stream(std::ostream& o) const {
    expr()->stream(o);
    if (as()) o << " AS " << as();
    return o;
}

} // namespace sql