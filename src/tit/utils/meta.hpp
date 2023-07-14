/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *\
 * Copyright (C) 2020-2023 Oleg Butakov                                       *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#pragma once

#include <concepts>
#include <type_traits>

namespace tit::meta {

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/** An empty object. */
template<class T>
concept meta_type = true; // std::is_object_v<T>; // && std::is_empty_v<T>;
template<class T>
concept type = std::is_object_v<T> && std::is_empty_v<T>;

/** Check that T is in Us. */
template<meta_type T, meta_type... Us>
inline constexpr bool contains_v = (... || std::same_as<T, Us>);

template<meta_type T, meta_type U, meta_type... Us>
  requires contains_v<T, U, Us...>
inline constexpr size_t index_of_v = [] {
  if constexpr (std::same_as<T, U>) return 0;
  else return (index_of_v<T, Us...> + 1);
}();

/** Check that all Ts are unique. */
template<class... Ts>
inline constexpr bool all_unique_v = true;
// clang-format off
template<class T, class... Ts>
inline constexpr bool all_unique_v<T, Ts...> =
    (!contains_v<T, Ts...>) && all_unique_v<Ts...>;
// clang-format on

template<meta_type... Ts>
  requires all_unique_v<Ts...>
class Set {
public:

  /** Initialize a meta-set. */
  Set() = default;
  consteval Set(Ts...)
    requires (sizeof...(Ts) != 0)
  {}

  static consteval size_t size() {
    return sizeof...(Ts);
  }

  /** Set union operation. */
  consteval auto operator|(Set<>) const {
    return Set<Ts...>{};
  }
  template<meta_type U, meta_type... Us>
  consteval auto operator|(Set<U, Us...>) const {
    if constexpr (contains_v<U, Ts...>) return Set<Ts...>{} | Set<Us...>{};
    else return Set<Ts..., U>{} | Set<Us...>{};
  }

  /** Set minus operation. */
  consteval auto operator-(Set<>) const {
    return Set<Ts...>{};
  }
  template<meta_type U, meta_type... Us>
  consteval auto operator-(Set<U, Us...>) const {
    if constexpr (contains_v<U, Ts...>) {
      return []<class X, class... Xs>(Set<X, Xs...>) {
        return Set<Xs...>{} - Set<Us...>{};
      }(Set<U>{} | Set<Ts...>{});
    } else return Set<Ts...>{} - Set<Us...>{};
  }

  template<meta_type U>
  static consteval bool contains(U) {
    return contains_v<U, Ts...>;
  }

  template<meta_type... Us>
  static consteval bool includes(Set<Us...>) {
    return std::is_same_v<Set<Ts...>, decltype(Set<Ts...>{} | Set<Us...>{})>;
  }
};

template<class T>
inline constexpr bool is_set_v = false;
template<class... Ts>
inline constexpr bool is_set_v<Set<Ts...>> = true;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

template<class T>
static consteval auto _type_name_impl() {
  return __PRETTY_FUNCTION__;
}

template<class T>
inline constexpr auto type_name = _type_name_impl<T>();

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

} // namespace tit::meta
