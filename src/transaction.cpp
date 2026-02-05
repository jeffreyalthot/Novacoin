#include "transaction.hpp"

#include <functional>
#include <iomanip>
#include <sstream>

namespace {
std::string toHex(std::size_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}
} // namespace

std::string Transaction::serialize() const {
    std::ostringstream out;
    out << from << '|' << to << '|' << amount << '|' << timestamp << '|' << fee;
    return out.str();
}

std::string Transaction::id() const {
    return toHex(std::hash<std::string>{}(serialize()));
}
