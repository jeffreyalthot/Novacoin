#include "transaction.hpp"

#include <sstream>

std::string Transaction::serialize() const {
    std::ostringstream out;
    out << from << '|' << to << '|' << amount << '|' << timestamp;
    return out.str();
}
