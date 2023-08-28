/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
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
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#pragma once

#include <type_traits>

#include "tit/core/mat.hpp"
#include "tit/core/meta.hpp"
#include "tit/core/misc.hpp"
#include "tit/core/types.hpp"
#include "tit/core/vec.hpp"

namespace tit {

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

template<class PV>
concept _has_fields =
    requires { std::remove_cvref_t<PV>::fields; } && //
    meta::is_set_v<decltype(auto(std::remove_cvref_t<PV>::fields))>;

template<class PV>
concept _has_constants =
    requires { std::remove_cvref_t<PV>::constants; } && //
    meta::is_set_v<decltype(auto(std::remove_cvref_t<PV>::constants))>;

/** Check particle fields presense. */
/** @{ */
template<_has_fields PV, meta::type... Fields>
consteval bool has(meta::Set<Fields...> fields) {
  return std::remove_cvref_t<PV>::fields.includes(fields);
}
template<_has_fields PV, meta::type... Fields>
consteval bool has(Fields... fields) {
  return has<PV>(meta::Set{fields...});
}
template<_has_fields PV, meta::type... Fields>
consteval bool has() {
  return has<PV>(Fields{}...);
}
/** @} */

/** Check particle constant presense. */
/** @{ */
template<_has_fields PV, meta::type... Consts>
consteval bool has_const(meta::Set<Consts...> consts) {
  if constexpr (!_has_constants<PV>) return false;
  else {
    return has<PV>(consts) &&
           std::remove_cvref_t<PV>::constants.includes(consts);
  }
}
template<_has_fields PV, meta::type... Consts>
consteval bool has_const(Consts... consts) {
  return has_const<PV>(meta::Set{consts...});
}
template<_has_fields PV, meta::type... Consts>
consteval bool has_const() {
  return has_const<PV>(Consts{}...);
}
/** @} */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/** Declare a particle field. */
#define TIT_DEFINE_FIELD(type, name, ...)                                      \
  class name##_t {                                                             \
  public:                                                                      \
                                                                               \
    /** Field name. */                                                         \
    static constexpr const char* field_name = #name;                           \
                                                                               \
    /** Field type. */                                                         \
    template<class Real, size_t Dim>                                           \
    using field_value_type = type;                                             \
                                                                               \
    /** Field value for the specified particle view. */                        \
    template<_has_fields PV>                                                   \
      requires (has<PV, name##_t>())                                           \
    constexpr auto operator[](PV&& a) const noexcept -> decltype(auto) {       \
      return a[*this];                                                         \
    }                                                                          \
    /** Field value delta for the specified particle view. */                  \
    template<_has_fields PV>                                                   \
      requires (has<PV, name##_t>())                                           \
    constexpr auto operator[](PV&& a, PV&& b) const noexcept {                 \
      return a[*this] - b[*this];                                              \
    }                                                                          \
                                                                               \
    /** Field value for the specified particle view or default value. */       \
    template<_has_fields PV>                                                   \
    constexpr auto get(PV&& a, auto def) const noexcept {                      \
      if constexpr (has<PV>(*this)) return a[name##_t{}];                      \
      else return def;                                                         \
    }                                                                          \
                                                                               \
  }; /* class name##_t */                                                      \
  inline constexpr name##_t name __VA_OPT__(, __VA_ARGS__);

/** Declare a scalar particle field. */
#define TIT_DEFINE_SCALAR_FIELD(name, ...)                                     \
  TIT_DEFINE_FIELD(Real, name __VA_OPT__(, __VA_ARGS__))

/** Declare a vector particle field. */
#define TIT_DEFINE_VECTOR_FIELD(name, ...)                                     \
  TIT_DEFINE_FIELD(TIT_PASS(Vec<Real, Dim>), name __VA_OPT__(, __VA_ARGS__))

/** Declare a matrix particle field. */
#define TIT_DEFINE_MATRIX_FIELD(name, ...)                                     \
  TIT_DEFINE_FIELD(TIT_PASS(Mat<Real, Dim>), name __VA_OPT__(, __VA_ARGS__))

/** Field name. */
template<meta::type Field>
inline constexpr auto field_name_v = std::remove_cvref_t<Field>::field_name;

/** Field type. */
template<meta::type Field, class Real, size_t Dim>
using field_value_type_t =
    typename std::remove_cvref_t<Field>::template field_value_type<Real, Dim>;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum class ParState {
  /** Particle is far from subdomain boundary. */
  INNER,
  /** Particle is near subdomain boundary, and is in halo of some other
   ** subdomain. Fields of such particles are send to the corresponding
   ** processes during syncronization. */
  NEAR_HALO,
  /** Particle is on the subdomain boundary. Fields of such particles are
   ** recieved from the corresponding processes during syncronization. */
  HALO
};
struct ParInfo {
  size_t part;
  size_t global_index;
  ParState state;
};
template<class Stream>
constexpr auto operator<<(Stream& stream, ParInfo p) -> Stream& {
  stream << p.part;
  return stream;
}

/** Is particle fixed? For the fixed particles,
 ** no variables are updated during the simulation. */
TIT_DEFINE_FIELD(bool, fixed);
TIT_DEFINE_FIELD(ParInfo, parinfo);

/** Particle position. */
TIT_DEFINE_VECTOR_FIELD(r);

/** Particle velocity. */
TIT_DEFINE_VECTOR_FIELD(v);
/** Particle velocity (XSPH model). */
TIT_DEFINE_VECTOR_FIELD(v_xsph);
/** Particle acceleration. */
TIT_DEFINE_VECTOR_FIELD(dv_dt);
/** Particle velocity divergence. */
TIT_DEFINE_SCALAR_FIELD(div_v);
/** Particle velocity curl (always 3D). */
TIT_DEFINE_FIELD(TIT_PASS(Vec<Real, 3>), curl_v);

/** Particle mass. */
TIT_DEFINE_SCALAR_FIELD(m);
/** Particle density. */
TIT_DEFINE_SCALAR_FIELD(rho);
/** Particle density gradient. */
TIT_DEFINE_VECTOR_FIELD(grad_rho);
/** Particle density time derivative. */
TIT_DEFINE_SCALAR_FIELD(drho_dt);

/** Particle width. */
TIT_DEFINE_SCALAR_FIELD(h);
/** Particle "Omega" variable (Grad-H model) */
TIT_DEFINE_SCALAR_FIELD(Omega);

/** Particle pressure. */
TIT_DEFINE_SCALAR_FIELD(p);
/** Particle sound speed. */
TIT_DEFINE_SCALAR_FIELD(cs);

/** Particle thermal energy. */
TIT_DEFINE_SCALAR_FIELD(eps);
/** Particle thermal energy time derivative. */
TIT_DEFINE_SCALAR_FIELD(deps_dt);

/** Particle molecular viscosity. */
TIT_DEFINE_SCALAR_FIELD(mu);
/** Particle molecular turbulent viscosity. */
TIT_DEFINE_SCALAR_FIELD(mu_T);
/** Particle second viscosity. */
TIT_DEFINE_SCALAR_FIELD(lambda);

/** Particle artificial viscosity switch. */
TIT_DEFINE_SCALAR_FIELD(alpha);
/** Particle artificial viscosity switch time derivative. */
TIT_DEFINE_SCALAR_FIELD(dalpha_dt);

/** Kernel renormalization coefficient (Shepard filter) */
TIT_DEFINE_SCALAR_FIELD(S);
/** Kernel gradient renormalization matrix. */
TIT_DEFINE_MATRIX_FIELD(L);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

} // namespace tit
