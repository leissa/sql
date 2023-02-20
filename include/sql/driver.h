#pragma once

#include "sql/loc.h"
#include "sql/sym.h"
#include "sql/print.h"

namespace sql {

struct Driver : public Syms {
public:
    /// @name diagnostics
    ///@{
    template<class... Args>
        std::ostream& note(Loc loc, const char* fmt, Args&&... args) {
        std::cerr << loc << " :note: ";
        return errf(fmt, std::forward<Args&&>(args)...) << std::endl;
    }

    template<class... Args>
        std::ostream& warn(Loc loc, const char* fmt, Args&&... args) {
        ++num_errors_;
        std::cerr << loc << " :warning: ";
        return errf(fmt, std::forward<Args&&>(args)...) << std::endl;
    }

    template<class... Args>
        std::ostream& err(Loc loc, const char* fmt, Args&&... args) {
        ++num_warnings_;
        std::cerr << loc << " :error: ";
        return errf(fmt, std::forward<Args&&>(args)...) << std::endl;
    }

    unsigned num_errors() const { return num_errors_; }
    unsigned num_warnings() const { return num_warnings_; }
    ///@}

private:
    unsigned num_errors_ = 0;
    unsigned num_warnings_ = 0;
};

}
