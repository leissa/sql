#include "sql/ast.h"

#include <iostream>

namespace sql {

int Lam::counter_ = 0;

// stream

void Exp::dump() const {
    stream(std::cout) << std::endl;
}

std::ostream& Var::stream(std::ostream& o) const {
    return o << name();
}

std::ostream& App::stream(std::ostream& o) const {
    if (auto lam = isa_let()) {
        o << "let " << lam->binder() << " = ";
        arg()->stream(o) << ";" << std::endl;
        return lam->body()->stream(o);
    }

    o << "(";
    callee()->stream(o) << " ";
    return arg()->stream(o) << ")";
}

std::ostream& Lam::stream(std::ostream& o) const {
    o << "(lam " << binder() << ". ";
    return body()->stream(o) << ")";
}

std::ostream& Err::stream(std::ostream& o) const {
    return o << "<error>";
}

}
