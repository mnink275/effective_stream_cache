#include <greeter.hpp>

namespace ink {

namespace {

#ifdef NDEBUG
constexpr bool kDebugEnabled = false;
#else
constexpr bool kDebugEnabled = true;
#endif

}  // namespace

std::string Greeter::MakeGreetingFor(std::string_view) const {
  return "Hello";
}

}  // namespace ink
