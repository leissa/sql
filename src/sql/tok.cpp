#include "sql/tok.h"

#include <ankerl/unordered_dense.h>
#include <fe/assert.h>

using namespace std::literals;

namespace sql {

namespace {
struct Hash {
    using is_transparent = void;
    using is_avalanching = void;
    uint64_t operator()(std::string_view s) const noexcept {
        return ankerl::unordered_dense::hash<std::string_view>()(s);
    }
};
} // namespace

std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (auto c : sv)
        res += tolower(c);
    return res;
}

std::string_view Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str##sv;
        SQL_KEY(CODE)
        SQL_TOK(CODE)
#undef CODE
        case Tok::Tag::K_ILIKE: return "ILIKE"sv;
        case Tok::Tag::K_IS_NOT: return "IS NOT"sv;
        case Tok::Tag::K_IS_DISTINCT_FROM: return "IS DISTINCT FROM"sv;
        case Tok::Tag::K_IS_NOT_DISTINCT_FROM: return "IS NOT DISTINCT FROM"sv;
        case Tok::Tag::Nil: return "<nil>"sv;
    }

    fe::unreachable();
}

bool Tok::isa_key(std::string_view lower) {
    // Not a fe::SymSet: Sym%bols are interned per SymPool, and the printer - the only caller -
    // has none at hand. Hashing the spelling instead is pool-independent and allocates nothing.
    static const auto keys = [] {
        ankerl::unordered_dense::set<std::string, Hash, std::equal_to<>> res;
#define CODE(t, str) res.emplace(to_lower(str##sv));
        SQL_KEY(CODE)
#undef CODE
        return res;
    }();

    return keys.contains(lower);
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
        case Tok::Tag::T_pow: return Prec::Pow;
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
