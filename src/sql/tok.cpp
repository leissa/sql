#include "sql/tok.h"

#include <fe/assert.h>

using namespace std::literals;

namespace sql {

std::string_view Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str##sv;
        SQL_KEY(CODE)
        SQL_TOK(CODE)
#undef CODE
        case Tok::Tag::K_IS_NOT: return "IS NOT"sv;
        case Tok::Tag::K_IS_DISTINCT_FROM: return "IS DISTINCT FROM"sv;
        case Tok::Tag::K_IS_NOT_DISTINCT_FROM: return "IS NOT DISTINCT FROM"sv;
        case Tok::Tag::Nil: return "<nil>"sv;
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
        case Tok::Tag::K_BETWEEN: return Prec::Between;
        case Tok::Tag::K_IN:
        case Tok::Tag::K_LIKE:
        case Tok::Tag::K_SIMILAR:
        case Tok::Tag::K_IS:
        case Tok::Tag::T_eq:
        case Tok::Tag::T_ne:
        case Tok::Tag::T_ue:
        case Tok::Tag::T_l:
        case Tok::Tag::T_le:
        case Tok::Tag::T_g:
        case Tok::Tag::T_ge:  return Prec::Comp;
        case Tok::Tag::T_concat: return Prec::Concat;
        case Tok::Tag::T_add:
        case Tok::Tag::T_sub: return Prec::Add;
        case Tok::Tag::T_mul:
        case Tok::Tag::T_div:
        case Tok::Tag::T_mod: return Prec::Mul;
        default: return {};
    }
}
// clang-format on

std::ostream& operator<<(std::ostream& o, Tok::Tag tag) { return o << Tok::tag2str(tag); }

std::ostream& operator<<(std::ostream& o, Tok tok) {
    if (tok.isa(Tok::Tag::V_id)) return o << *tok.sym();
    if (tok.isa(Tok::Tag::V_str)) return o << '\'' << *tok.sym() << '\'';
    if (tok.isa(Tok::Tag::V_int)) return o << tok.u64();
    if (tok.isa(Tok::Tag::V_real) || tok.isa(Tok::Tag::V_param)) return o << *tok.sym();
    return o << Tok::tag2str(tok.tag());
}

} // namespace sql
