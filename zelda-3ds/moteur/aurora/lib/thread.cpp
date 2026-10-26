// Version 3DS : les options des fils (nom, priorité, cœur) sont gérées par la plateforme.
#include "thread.hpp"

namespace aurora::thread {
void set_current(const Options&) noexcept {}
} // namespace aurora::thread
