#include "spoke/status.h"

namespace spoke {

std::string Status::toString() const {
  if (ok()) return "OK";
  return "Error(" + std::to_string(static_cast<int>(code)) + "): " + message;
}

}  // namespace spoke
