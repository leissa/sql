#include "sql/tok.h"

#include "sql/assert.h"

using namespace std::literals;

namespace sql {

std::string_view Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, _, str) \
    case Tok::Tag::t: return str##sv;
        SQL_KEY(CODE)
#undef CODE
#define CODE(t, str) \
    case Tok::Tag::t: return str##sv;
        SQL_LIT(CODE)
        SQL_TOK(CODE)
#undef CODE
    }

    unreachable();
}

std::ostream& operator<<(std::ostream& o, const Tok& tok) {
    if (tok.isa(Tok::Tag::M_id)) return o << *tok.sym();
    if (tok.isa(Tok::Tag::L_i)) return o << tok.u64();
    return o << Tok::tag2str(tok.tag());
}

} // namespace sql
