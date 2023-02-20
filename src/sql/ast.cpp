#include "sql/ast.h"

#include <iostream>

namespace sql {

// stream

void Node::dump() const { stream(std::cout) << std::endl; }

std::ostream& UnExpr::stream(std::ostream& o) const {
    return o;
}

std::ostream& BinExpr::stream(std::ostream& o) const {
    return o;
}

std::ostream& ErrExpr::stream(std::ostream& o) const { return o << "<error>"; }

} // namespace sql
