#include "sql/ast.h"

#include <iostream>

namespace sql {

// stream

void Node::dump() const { stream(std::cout) << std::endl; }

/*
 * Expr
 */

std::ostream& IdExpr::stream(std::ostream& o) const { return o << sym(); }

std::ostream& UnExpr::stream(std::ostream& o) const { return o; }

std::ostream& BinExpr::stream(std::ostream& o) const { return o; }

std::ostream& ErrExpr::stream(std::ostream& o) const { return o << "<error>"; }

/*
 * Stmt
 */

std::ostream& SelectStmt::stream(std::ostream& o) const {
    o << "SELECT ";
    if (distinct()) o << "DISTINCT ";
    select()->stream(o);
    from()->stream(o << " FROM ");
    if (where()) where()->stream(o << " WHERE ");
    if (group()) group()->stream(o << " GROUP BY ");
    if (having()) having()->stream(o << " HAVING ");
    return o;
}

} // namespace sql
