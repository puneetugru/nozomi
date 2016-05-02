#pragma once

#include <utility>

#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/mpl/at.hpp>

namespace sakura {

template <typename Container>
inline void push_back_move(Container& container) {}

template <typename Container, typename Value, typename... Values>
inline void push_back_move(Container& container,
                           Value&& value,
                           Values&&... values) {
  container.push_back(std::move(value));
  push_back_move(container, std::move(values)...);
}

template <typename... Types>
struct type_sequence {};

template <typename Callable, size_t N>
struct parameter_type {
  using args_t = typename boost::function_types::parameter_types<decltype(
      &Callable::operator())>::type;
  using type = typename boost::mpl::at_c<args_t, N>::type;
};

template <typename Callable, size_t Zero, size_t... N>
auto make_type_sequence(const Callable&, std::index_sequence<Zero, N...>) {
  return type_sequence<typename parameter_type<Callable, N>::type...>();
}

template <typename Callable>
auto make_type_sequence(const Callable& handler) {
  constexpr int N = boost::function_types::function_arity<decltype(
      &Callable::operator())>::value;
  return make_type_sequence(handler, std::make_index_sequence<N>{});
}
}
