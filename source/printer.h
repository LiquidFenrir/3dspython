#pragma once

#include <string_view>

namespace Printer {

using payload_t = void*;
using callback_t = void (*)(payload_t, std::string_view);
inline payload_t payload = nullptr;
inline callback_t callback = nullptr;

void print(std::string_view str);

}
