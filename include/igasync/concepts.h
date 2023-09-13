#ifndef IGASYNC_CONCEPTS_H
#define IGASYNC_CONCEPTS_H

#include <concepts>
#include <tuple>

namespace igasync {

template <typename T>
concept Any = true;

template <typename ValT, typename F, typename... Args>
concept HasAppropriateFunctor = requires(F&& f, Args&&... args) {
  requires(std::same_as<std::invoke_result_t<F, Args...>, ValT>);
};

template <typename F, typename... Args>
concept CanApplyFunctor =
    requires(F&& f, Args&&... args) { requires(std::invocable<F, Args...>); };

template <typename ValT>
concept IsVoid = std::is_void_v<ValT>;

template <typename ValT, typename F,
          typename SValT = std::add_const_t<std::add_lvalue_reference_t<
              std::remove_const_t<std::remove_reference_t<ValT>>>>>
concept HasSingleConstRefParam = requires(F f, SValT val) {
  { f.operator()(val) } -> IsVoid;
};

template <typename ValT, typename F,
          typename SValT = std::remove_const_t<std::remove_reference_t<ValT>>>
concept HasSingleValueParam = requires(F f, SValT val) {
  { f.operator()(std::move(val)) } -> IsVoid;
};

template <typename F>
concept HasNoParamsOperator = requires(F f) {
  { f.operator()() } -> IsVoid;
};

template <typename ValT, typename F>
concept NonVoidPromiseThenCb = requires(F f, ValT val) {
  { f(val) } -> IsVoid;
  requires(HasSingleConstRefParam<ValT, F>);
};

template <typename ValT, typename F>
concept NonVoidPromiseConsumeCb = requires(F f, ValT val) {
  { f(std::move(val)) } -> IsVoid;
  requires(HasSingleValueParam<ValT, F>);
  requires(std::is_move_constructible_v<ValT>);
};

template <typename F>
concept VoidPromiseThenCb = requires(F f) {
  { f() } -> IsVoid;
  requires(HasNoParamsOperator<F>);
};

}  // namespace igasync

#endif
