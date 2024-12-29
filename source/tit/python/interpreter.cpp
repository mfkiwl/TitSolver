/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *|
 * Part of the Tit Solver project, under the MIT License.
 * See /LICENSE.md for license information. SPDX-License-Identifier: MIT
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <Python.h> // IWYU pragma: keep

#include "tit/core/basic_types.hpp"
#include "tit/core/checks.hpp"
#include "tit/core/exception.hpp"
#include "tit/core/main_func.hpp"
#include "tit/core/str_utils.hpp"
#include "tit/core/sys/utils.hpp"

#include "tit/python/interpreter.hpp"

namespace tit::python {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

namespace {

// Undent the text such that the first non-empty line is aligned with the
// first line of the text.
constexpr auto unindent(std::string_view text) -> std::string {
  std::string result;
  size_t indent = std::string::npos;
  size_t start = 0;
  while (start < text.size()) {
    auto end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();

    const auto line = text.substr(start, end - start);
    const auto line_indent = line.find_first_not_of(" \t");
    if (indent == std::string::npos) indent = line_indent;
    if (line_indent != std::string_view::npos) {
      result.append(line.substr(std::min(indent, line_indent)));
    }
    result.push_back('\n');

    start = end + 1;
  }
  return result;
}

} // namespace

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// NOLINTBEGIN(*-include-cleaner)

Config::Config() : config_{new PyConfig{}} {
  PyConfig_InitIsolatedConfig(config_.get());
}

void Config::Cleaner_::operator()(PyConfig* config) const {
  if (config != nullptr) PyConfig_Clear(config);
  std::default_delete<PyConfig>::operator()(config);
}

auto Config::base() const noexcept -> PyConfig* {
  TIT_ASSERT(config_ != nullptr, "Config is not initialized!");
  return config_.get();
}

void Config::set_home(CStrView home) const {
  const auto status =
      PyConfig_SetBytesString(base(), &base()->home, home.c_str());
  if (PyStatus_IsError(status) == 0) return;
  TIT_THROW("Failed to set Python home directory to '{}': {}: {}.",
            home,
            status.func,
            status.err_msg);
}

void Config::set_prog_name(CStrView name) const {
  const auto status =
      PyConfig_SetBytesString(base(), &base()->program_name, name.c_str());
  if (PyStatus_IsError(status) == 0) return;
  TIT_THROW("Failed to set Python program name to '{}': {}: {}.",
            name,
            status.func,
            status.err_msg);
}

void Config::set_cmd_args(CmdArgs args) const {
  const auto status = PyConfig_SetBytesArgv(
      base(),
      static_cast<Py_ssize_t>(args.size()),
      const_cast<char* const*>(args.data())); // NOLINT(*-const-cast)
  if (PyStatus_IsError(status) == 0) return;
  TIT_THROW("Failed to set Python command line arguments: {}: {}.",
            status.func,
            status.err_msg);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Interpreter::Interpreter(Config config) : config_{std::move(config)} {
  // Initialize the Python interpreter.
  const auto status = Py_InitializeFromConfig(config_.base());
  if (PyStatus_IsError(status) != 0) {
    TIT_THROW("Failed to initialize Python interpreter: {}: {}.",
              status.func,
              status.err_msg);
  }

  // Get the globals of the main module.
  auto* const main_module = PyImport_AddModule("__main__");
  if (main_module == nullptr) {
    PyErr_Print();
    TIT_THROW("Failed to import the main module.");
  }
  globals_ = PyModule_GetDict(main_module);
  if (globals_ == nullptr) {
    PyErr_Print();
    TIT_THROW("Failed to get the main module globals.");
  }

  // Initialize the coverage report.
#ifdef TIT_HAVE_GCOV
  exec(R"PY(
    import os
    import coverage

    # Start the coverage report.
    __cov = coverage.Coverage(
        config_file=os.path.join(os.environ["SOURCE_DIR"], "pyproject.toml"),
        branch=True,
    )
    __cov.start()
  )PY");
#endif
}

Interpreter::~Interpreter() {
  // Finalize the coverage report.
#ifdef TIT_HAVE_GCOV
  exec(R"PY(
    # Some of our tests will emit warnings for missing coverage data.
    # This is expected, and we can safely ignore them.
    import warnings
    warnings.filterwarnings("ignore")

    # Write the coverage report.
    __cov.stop()
    __cov.save()
  )PY");
#endif

  // Finalize the Python interpreter.
  Py_Finalize();
}

void Interpreter::append_path(CStrView path) const {
  exec(std::format("import sys; sys.path.append('{}')", path.c_str()));
}

auto Interpreter::exec_raw(CStrView statement) const -> bool {
  auto* const result = PyRun_StringFlags(statement.c_str(),
                                         /*start=*/Py_file_input,
                                         /*globals=*/globals_,
                                         /*locals=*/globals_,
                                         /*flags=*/nullptr);
  if (result != nullptr) {
    Py_DECREF(result);
    return true;
  }

  PyErr_Print();
  return false;
}

auto Interpreter::exec(std::string_view statement) const -> bool {
  return exec_raw(unindent(statement));
}

auto Interpreter::exec_file(CStrView file_name) const -> bool {
  const auto file = open_file(file_name, "r");
  auto* const result = PyRun_File(/*fp=*/file.get(),
                                  /*filename=*/file_name.c_str(),
                                  /*start=*/Py_file_input,
                                  /*globals=*/globals_,
                                  /*locals=*/globals_);
  if (result != nullptr) {
    Py_DECREF(result);
    return true;
  }

  PyErr_Print();
  return false;
}

// NOLINTEND(*-include-cleaner)

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

} // namespace tit::python
