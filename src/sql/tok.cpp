#include "sql/tok.h"

#include <fe/assert.h>

using namespace std::literals;

namespace sql {

std::string_view Tok::str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str##sv;
        SQL_KEY(CODE)
        SQL_TOK(CODE)
#undef CODE
        case Tok::Tag::K_IS_NOT: return "IS NOT"sv;
    }

    fe::unreachable();
}

// clang-format off
std::optional<Tok::Prec> Tok::un_prec(Tok::Tag tag) {
    switch (tag) {
        case Tok::Tag::K_NOT: return Prec::Not;
        case Tok::Tag::T_add:
        case Tok::Tag::T_sub: return Prec::Unary;
        default: return {};
    }
}

std::optional<Tok::Prec> Tok::bin_prec(Tok::Tag tag) {
    switch (tag) {
        case Tok::Tag::K_OR:  return Prec::Or;
        case Tok::Tag::K_AND: return Prec::And;
        case Tok::Tag::T_eq:
        case Tok::Tag::T_ne:
        case Tok::Tag::T_l:
        case Tok::Tag::T_le:
        case Tok::Tag::T_g:
        case Tok::Tag::T_ge:  return Prec::Comp;
        case Tok::Tag::T_add:
        case Tok::Tag::T_sub: return Prec::Add;
        case Tok::Tag::T_mul:
        case Tok::Tag::T_div: return Prec::Mul;
        default: return {};
    }
}
// clang-format on

std::ostream& operator<<(std::ostream& o, Tok::Tag tag) { return o << Tok::str(tag); }

std::ostream& operator<<(std::ostream& o, Tok tok) {
    if (tok.isa(Tok::Tag::V_id)) return o << *tok.sym();
    if (tok.isa(Tok::Tag::V_int)) return o << tok.u64();
    return o << Tok::str(tok.tag());
}

} // namespace sql
