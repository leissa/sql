#pragma once

#include <cstdint>
#include <ostream>

namespace sql {

// I don't recommend global variables in production code but for our toy project it's fine.
extern int num_errors;

struct Pos {
    Pos() = default;
    Pos(uint16_t row, uint16_t col)
        : row(row)
        , col(col)
    {}

    operator bool() const { return row; }

    uint16_t row = 0;
    uint16_t col = 0;
};

struct Loc {
    Loc() = default;
    Loc(const char* file, Pos begin, Pos finis)
        : file(file)
        , begin(begin)
        , finis(finis)
    {}
    Loc(const char* file, Pos pos)
        : Loc(file, pos, pos)
    {}

    const char* file = nullptr;
    Pos begin;
    Pos finis;

    std::ostream& err() const;
};

std::ostream& operator<<(std::ostream&, const Pos&);
std::ostream& operator<<(std::ostream&, const Loc&);

}
