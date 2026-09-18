#include "sql/sql.h"

#include <cerrno>

#include <fstream>

namespace sql {

Result parse(std::string sql, std::filesystem::path name) {
    auto res      = Result(std::make_unique<Driver>());
    auto [src, _] = res.driver_->src().add(std::move(name), std::move(sql));
    res.prog_     = Parser(*res.driver_, *src).parse_prog();
    return res;
}

Result parse(std::istream& is, std::filesystem::path name) { return parse(fe::SrcMap::slurp(is), std::move(name)); }

Result parse_file(const std::filesystem::path& path) {
    auto ifs = std::ifstream(path);
    if (!ifs) throw std::filesystem::filesystem_error("cannot read file", path, {errno, std::generic_category()});
    return parse(fe::SrcMap::slurp(ifs), path);
}

} // namespace sql
