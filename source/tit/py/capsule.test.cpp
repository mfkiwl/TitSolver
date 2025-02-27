/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *|
 * Part of BlueTit Solver, licensed under Apache 2.0 with Commons Clause.
 * Commercial use, including SaaS, requires a separate license, see /LICENSE.md
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include <memory>

#include "tit/core/utils.hpp"

#include "tit/py/capsule.hpp"
#include "tit/py/number.hpp"

#include "tit/testing/test.hpp"

namespace tit {
namespace {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("py::Capsule") {
  SUBCASE("typing") {
    CHECK(py::Capsule::type().fully_qualified_name() == "PyCapsule");
    CHECK(py::Capsule::isinstance(py::Capsule{std::make_unique<int>(123)}));
    CHECK_FALSE(py::Capsule::isinstance(py::Int{}));
  }
  SUBCASE("data") {
    static bool destroyed;
    struct Data {
      TIT_NOT_COPYABLE_OR_MOVABLE(Data);
      Data() {
        destroyed = false;
      }
      ~Data() {
        destroyed = true;
      }
    };
    {
      auto capsule = py::Capsule{std::make_unique<Data>()};
      CHECK(capsule.data() != nullptr);
    }
    CHECK(destroyed);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

} // namespace
} // namespace tit
