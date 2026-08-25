#pragma once

#include <fe/driver.h>

namespace sql {

class Driver : public fe::Driver {
public:
    template<class T, class... Args>
    auto ast(Args&&... args) {
        return arena_.mk<const T>(std::forward<Args&&>(args)...);
    }

private:
    fe::Arena arena_;
};

} // namespace sql
