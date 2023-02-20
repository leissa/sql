#include "sql/ast.h"

#include <iostream>

namespace sql {

// stream

void Node::dump() const { stream(std::cout) << std::endl; }

/*
 * Expr
 */

// clang-format off
std::ostream& IdExpr ::stream(std::ostream& o) const { return o << sym(); }
std::ostream& LitExpr::stream(std::ostream& o) const { return o << u64(); }
// clang-format on

std::ostream& UnExpr::stream(std::ostream& o) const { return o; }

std::ostream& BinExpr::stream(std::ostream& o) const { return o; }

std::ostream& ErrExpr::stream(std::ostream& o) const { return o << "<error>"; }

/*
 * Query
 */

std::ostream& TableQuery::stream(std::ostream& o) const {
    from()->stream(o << "FROM ");
    if (where()) where()->stream(o << " WHERE ");
    if (group()) group()->stream(o << " GROUP BY ");
    return o;
}

/*
 * Stmt
 */

std::ostream& SelectStmt::stream(std::ostream& o) const {
    o << "SELECT ";
    if (distinct()) o << "DISTINCT ";
    return table()->stream(o << " FROM ");
}

} // namespace sql
