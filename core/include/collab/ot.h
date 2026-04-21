#pragma once
#include "collab/operation.h"
#include <utility>
#include <string>

namespace collab {

std::pair<Operation, Operation> transform(const Operation& a, const Operation& b);

void apply(std::string& document, const Operation& op);

}
