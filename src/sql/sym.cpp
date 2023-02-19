#include "sql/sym.h"

namespace sql {

std::ostream& operator<<(std::ostream& o, Sym sym) { return o << *sym; }

} // namespace sql
