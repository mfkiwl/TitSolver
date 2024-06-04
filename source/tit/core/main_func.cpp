/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *\
 * Part of the Tit Solver project, under the MIT License.
 * See /LICENSE.md for license information. SPDX-License-Identifier: MIT
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include <cstdlib>

#include "tit/core/checks.hpp"
#include "tit/core/exception.hpp"
#include "tit/core/main_func.hpp"
#include "tit/core/profiler.hpp"
#include "tit/core/system.hpp"

#include "tit/par/control.hpp"

namespace tit {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

auto run_main(int argc, char** argv, const main_func_t& main_func) -> int {
  // Setup signal handler.
  const FatalSignalHandler handler{};
  // Setup terminate handler.
  const TerminateHandler terminate_handler{};
  // Enable profiling.
  if (std::getenv("TIT_ENABLE_PROFILER") != nullptr) { // NOLINT(*-mt-unsafe)
    Profiler::enable();
  }
  // Setup parallelism.
  par::set_num_threads(8);
  // Run the main function.
  TIT_ENSURE(main_func != nullptr, "Main function must be specified!");
  return main_func(argc, argv);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

} // namespace tit
