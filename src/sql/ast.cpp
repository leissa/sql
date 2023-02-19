#include "sql/ast.h"

#include <iostream>

namespace sql {

// stream

void Expr::dump() const { stream(std::cout) << std::endl; }

std::ostream& UnExpr::stream(std::ostream& o) const {
    // return o << name();
}

std::ostream& BinExpr::stream(std::ostream& o) const {}

std::ostream& ErrExpr::stream(std::ostream& o) const { return o << "<error>"; }

} // namespace sql
