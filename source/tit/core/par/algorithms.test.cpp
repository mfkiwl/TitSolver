/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *|
 * Part of BlueTit Solver, licensed under Apache 2.0 with Commons Clause.
 * Commercial use, including SaaS, requires a separate license, see /LICENSE.md
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#include <algorithm>
#include <chrono>
#include <random>
#include <ranges>
#include <thread>
#include <vector>

#include "tit/core/basic_types.hpp"
#include "tit/core/par/algorithms.hpp"
#include "tit/core/par/control.hpp"

#include "tit/testing/test.hpp"

namespace tit {
namespace {

// Disclaimer: Since this submodule is no more that a simple wrapper around the
// Intel TBB library, there is no need to test it in detail. The only thing we
// need to test is that our wrappers are working correctly.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::for_each") {
  par::set_num_threads(4);
  std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SUBCASE("basic") {
    // Ensure the loop is executed.
    par::for_each(data, [](int& i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      i += 1;
    });
    CHECK(data == std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from the worker threads are caught.
    const auto loop = [&data] {
      par::for_each(data, [](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        if (i == 7) throw std::runtime_error{"Loop failed!"};
      });
      FAIL("Loop should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(loop(), "Loop failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::static_for_each") {
  par::set_num_threads(4);
  std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<size_t> indices(data.size());
  SUBCASE("basic") {
    // Ensure the loop is executed and the thread distribution is correct.
    par::static_for_each( //
        std::views::zip(data, indices),
        [](size_t thread_index, auto out_pair) {
          std::this_thread::sleep_for(std::chrono::milliseconds{10});
          auto& [i, out_thread_index] = out_pair;
          out_thread_index = thread_index, i += 1;
        });
    CHECK(data == std::vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    CHECK(indices == std::vector<size_t>{0, 0, 0, 1, 1, 1, 2, 2, 3, 3});
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from worker threads are caught.
    const auto loop = [&data] {
      par::static_for_each(data, [](size_t /*thread_index*/, int i) {
        if (i == 7) throw std::runtime_error{"Loop failed!"};
      });
      FAIL("Loop should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(loop(), "Loop failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::block_for_each") {
  par::set_num_threads(4);
  using VectorOfVectors = std::vector<std::vector<int>>;
  VectorOfVectors data{{0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}};
  SUBCASE("basic") {
    // Ensure the loop is executed.
    /// @todo This test does not really test that the iterations are done in
    /// in chunks, it is hard to test that without exposing the internals.
    par::block_for_each(data, [](int& i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      i += 1;
    });
    CHECK(data == VectorOfVectors{{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}});
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from the worker threads are caught.
    const auto loop = [&data] {
      par::block_for_each(data, [](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        if (i == 7) throw std::runtime_error{"Loop failed!"};
      });
      FAIL("Loop should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(loop(), "Loop failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::copy_if") {
  par::set_num_threads(4);
  std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<int> out(data.size());
  SUBCASE("basic") {
    // Ensure the loop is executed.
    const auto iter = par::copy_if(data, out.begin(), [](int i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      return i % 2 == 0;
    });
    CHECK(iter == out.begin() + 5);
    const auto out_range = std::ranges::subrange(out.begin(), iter);
    std::ranges::sort(out_range);
    CHECK_RANGE_EQ(out_range, {0, 2, 4, 6, 8});
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from the worker threads are caught.
    const auto algorithm = [&data, &out] {
      par::copy_if(data, out.begin(), [](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        if (i == 7) throw std::runtime_error{"Algorithm failed!"};
        return i % 2 == 0;
      });
      FAIL("Algorithm should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(algorithm(), "Algorithm failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::transform") {
  par::set_num_threads(4);
  std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<int> out(data.size());
  SUBCASE("basic") {
    // Ensure the loop is executed.
    const auto iter = par::transform(data, out.begin(), [](int i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      return 2 * i + 1;
    });
    CHECK(iter == out.end());
    CHECK_RANGE_EQ(out, {1, 3, 5, 7, 9, 11, 13, 15, 17, 19});
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from the worker threads are caught.
    const auto algorithm = [&data, &out] {
      par::transform(data, out.begin(), [](int i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        if (i == 7) throw std::runtime_error{"Algorithm failed!"};
        return 2 * i + 1;
      });
      FAIL("Algorithm should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(algorithm(), "Algorithm failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TEST_CASE("par::sort") {
  par::set_num_threads(4);
  constexpr auto sorted = std::views::iota(0, 1000);
  auto data = sorted | std::ranges::to<std::vector>();
  std::ranges::shuffle(data, std::mt19937{123});
  SUBCASE("basic") {
    // Ensure the loop is executed.
    par::sort(data);
    CHECK_RANGE_EQ(data, sorted);
  }
  SUBCASE("exceptions") {
    // Ensure the exceptions from the worker threads are caught.
    const auto algorithm = [&data] {
      par::sort(data, [](int a, int b) {
        if (a == 123) throw std::runtime_error{"Algorithm failed!"};
        return a < b;
      });
      FAIL("Algorithm should have thrown an exception!");
    };
    CHECK_THROWS_WITH_AS(algorithm(), "Algorithm failed!", std::runtime_error);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

} // namespace
} // namespace tit
