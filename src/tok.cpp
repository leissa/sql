#include "sql/tok.h"

namespace sql {

const char* Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) case Tok::Tag::t: return str;
        SQL_KEYWORDS(CODE)
        SQL_PUNCTUATORS(CODE)
        SQL_MISC(CODE)
#undef CODE
    }

    return nullptr; // shutup warning
}

std::ostream& operator<<(std::ostream& o, const Tok& tok) {
    if (tok.isa(Tok::Tag::M_id)) return o << *tok.sym();
    return o << Tok::tag2str(tok.tag());
}

}
