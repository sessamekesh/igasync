#ifndef IGASYNC_CONCEPTS_H
#define IGASYNC_CONCEPTS_H

#include <concepts>

namespace igasync {
template <class F, class... ArgTypes>
concept IsVoidFn = std::is_void_v<std::invoke_result_t<F, ArgTypes...>>;
}

#endif
