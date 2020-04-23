#pragma once

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template<typename... Ts>
using void_t = void;

template <template <typename...> typename Trait, typename Void, typename... Args>
struct is_detected_helper : std::false_type { };

template <template <typename...> typename Trait, typename... Args>
struct is_detected_helper<Trait, void_t<Trait<Args...>>, Args...> : std::true_type { };

template <template <typename...> typename Trait, typename... Args>
constexpr bool is_detected_v = is_detected_helper<Trait, void, Args...>::value;

template<typename... Ts>
constexpr bool dependent_false_t = false;

} // namespace frg
