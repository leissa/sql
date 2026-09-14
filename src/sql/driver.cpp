#include "sql/driver.h"

using namespace std::literals;

namespace sql {

Driver::Driver() {
#define CODE(t, str) keys_[sym(to_lower(str##sv))] = Tok::Tag::t;
    SQL_KEY(CODE)
#undef CODE

    size_t i = 0;
#define CODE(t, str) non_keys_[i++] = sym(to_lower(str##sv));
    SQL_NON_KEY(CODE)
#undef CODE

    sym_error_ = sym("<error>"s);
}

} // namespace sql
