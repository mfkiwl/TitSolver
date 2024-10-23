/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *\
 * Part of the Tit Solver project, under the MIT License.
 * See /LICENSE.md for license information. SPDX-License-Identifier: MIT
\* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

#include "tit/core/basic_types.hpp"
#include "tit/core/checks.hpp"
#include "tit/core/multivector.hpp"
#include "tit/core/par.hpp"
#include "tit/core/profiler.hpp"
#include "tit/core/stats.hpp"
#include "tit/core/type_traits.hpp"
#include "tit/core/utils.hpp"
#include "tit/core/vec.hpp"

#include "tit/geom/bbox.hpp"
#include "tit/geom/partition.hpp"
#include "tit/geom/search.hpp"

#include "tit/graph/graph.hpp"

#include "tit/sph/field.hpp"
#include "tit/sph/particle_array.hpp"

namespace tit::sph {

// TODO: move it to an appropriate place!
#if COMPRESSIBLE_SOD_PROBLEM
inline constexpr auto Domain = geom::BBox{Vec{0.0}, Vec{2.0}};
#elif HARD_DAM_BREAKING
inline constexpr auto Domain = geom::BBox{Vec{0.0, 0.0}, Vec{4.0, 3.0}};
#elif EASY_DAM_BREAKING
inline constexpr auto Domain = geom::BBox{Vec{0.0, 0.0}, Vec{3.2196, 1.5}};
#else
inline constexpr auto Domain = geom::BBox{Vec{0.0, 0.0}, Vec{0.0, 0.0}};
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Particle adjacency graph.
template<geom::search_func SearchFunc = geom::GridSearch,
         geom::partition_func PartitionFunc = geom::RecursiveInertialBisection,
         geom::partition_func SecondaryPartitionFunc = PartitionFunc>
class ParticleMesh final {
public:

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  /// Construct a particle adjacency graph.
  ///
  /// @param search_indexing_func Nearest-neighbors search indexing function.
  /// @param partition_func Geometry partitioning function.
  constexpr explicit ParticleMesh(
      SearchFunc search_func = {},
      PartitionFunc partition_func = {},
      SecondaryPartitionFunc secondary_partition_func = {}) noexcept
      : search_func_{std::move(search_func)},
        partition_func_{std::move(partition_func)},
        secondary_partition_func_{std::move(secondary_partition_func)} {}

  /// Adjacent particles.
  template<particle_view PV>
  constexpr auto operator[](PV a) const noexcept {
    auto& particles = a.array();
    return adjacency_[a.index()] |
           std::views::transform(
               [&particles](size_t b) { return particles[b]; });
  }

  /// Particles used for interpolation for the fixed particles.
  template<particle_view PV>
  constexpr auto fixed_interp(PV a) const noexcept {
    TIT_ASSERT(a.has_type(ParticleType::fixed),
               "Particle must be of the fixed type!");
    auto& particles = a.array();
    const size_t i = a - *particles.fixed().begin();
    return interp_adjacency_[i] | //
           std::views::transform(
               [&particles](size_t b) { return particles[b]; });
  }

  /// Unique pairs of the adjacent particles.
  template<particle_array ParticleArray>
  constexpr auto pairs(ParticleArray& particles) const noexcept {
    return adjacency_.edges() | std::views::transform([&particles](auto ab) {
             const auto [a, b] = ab;
             return std::pair{particles[a], particles[b]};
           });
  }

  /// Unique pairs of the adjacent particles partitioned by the block.
  template<particle_array ParticleArray>
  constexpr auto block_pairs(ParticleArray& particles) const noexcept {
    return block_edges_.buckets() |
           std::views::transform([&particles](auto block) {
             return block | std::views::transform([&particles](auto ab) {
                      const auto [a, b] = ab;
                      /// @todo I have zero idea why, but using pair here
                      /// instead of a tuple causes a massive performance hit.
                      return std::tuple{particles[a], particles[b]};
                    });
           });
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  /// Update the adjacency graph.
  template<particle_array ParticleArray, class SearchRadiusFunc>
  constexpr void update(ParticleArray& particles,
                        const SearchRadiusFunc& radius_func) {
    TIT_PROFILE_SECTION("ParticleMesh::update()");

    // Update the adjacency graphs.
    search_(particles, radius_func);

    // Partition the adjacency graph by the block.
    partition_(particles);
  }

private:

  template<particle_array ParticleArray, class SearchRadiusFunc>
  constexpr void search_(ParticleArray& particles,
                         const SearchRadiusFunc& radius_func) {
    TIT_PROFILE_SECTION("ParticleMesh::search()");
    using PV = ParticleView<ParticleArray>;

    // Build the search index.
    const auto positions = r[particles];
    const auto search_index = search_func_(positions);

    // Search for the neighbors.
    static std::vector<std::vector<size_t>> adjacency{};
    adjacency.resize(particles.size());
    par::for_each(particles.all(), [&radius_func, &search_index](PV a) {
      const auto& search_point = r[a];
      const auto search_radius = radius_func(a);
      TIT_ASSERT(search_radius > 0.0, "Search radius must be positive.");

      // Search for the neighbors for the current particle and store the
      // sorted results.
      auto& search_results = adjacency[a.index()];
      search_results.clear();
      search_index.search(search_point,
                          search_radius,
                          std::back_inserter(search_results));
      std::ranges::sort(search_results);
    });

    // Compress the adjacency graph.
    adjacency_.clear();
    for (const auto& x : adjacency) adjacency_.append_bucket(x);

    // Search for the interpolation points for the fixed particles.
    static std::vector<std::vector<size_t>> interp_adjacency{};
    interp_adjacency.resize(particles.fixed().size());
    par::for_each( //
        std::views::enumerate(particles.fixed()),
        [&radius_func, &search_index, &particles](auto ia) {
          auto [i, a] = ia;
          /// @todo Once we have a proper geometry library, we should use
          ///       here and clean up the code.
          const auto& search_point = r[a];
          const auto search_radius = 3 * radius_func(a);
          const auto point_on_boundary = Domain.clamp(search_point);
          const auto interp_point = 2 * point_on_boundary - search_point;

          // Search for the neighbors for the interpolation point and
          // store the sorted results.
          auto& search_results = interp_adjacency[i];
          search_results.clear();
          search_index.search(interp_point,
                              search_radius,
                              std::back_inserter(search_results));
          std::erase_if(search_results, [&particles](size_t b) {
            return particles.has_type(b, ParticleType::fixed);
          });
          std::ranges::sort(search_results);
        });

    // Compress the interpolation graph.
    interp_adjacency_.clear();
    for (const auto& x : interp_adjacency) interp_adjacency_.append_bucket(x);
  }

  template<particle_array ParticleArray>
  constexpr void partition_(ParticleArray& particles, size_t num_levels = 2) {
    TIT_PROFILE_SECTION("ParticleMesh::partition()");
    TIT_ASSERT(num_levels < PartVec::MaxNumLevels,
               "Number of levels exceeds the predefined maximum!");

    // Initialize the partitioning.
    const auto num_threads = par::num_threads();
    const auto num_parts = num_levels * num_threads + 1;
    TIT_ENSURE(num_parts <= std::numeric_limits<PartIndex>::max(),
               "Number of parts is too large!");
    const auto parts = parinfo[particles];
    std::ranges::fill(parts, PartVec(static_cast<PartIndex>(num_parts - 1)));

    // Build the multi-level partitioning.
    const auto positions = r[particles];
    static std::vector<size_t> interface{};
    for (size_t level = 0; level < num_levels; ++level) {
      const auto is_first_level = level == 0;
      const auto is_last_level = level == (num_levels - 1);

      // Partition the particles.
      const auto level_parts =
          parts | std::views::transform(
                      [level](auto& part) -> auto& { return part[level]; });
      if (is_first_level) {
        partition_func_(positions, level_parts, num_threads);
      } else {
        secondary_partition_func_(permuted_view(positions, interface),
                                  permuted_view(level_parts, interface),
                                  num_threads,
                                  /*init_part=*/level * num_threads);
      }

      // Update the interface particles.
      if (is_last_level) break;
      const auto is_interface = [level_parts, this](size_t a) {
        return std::ranges::any_of(
            permuted_view(level_parts, adjacency_[a]),
            std::bind_front(std::not_equal_to{}, level_parts[a]));
      };
      if (is_first_level) {
        interface.resize(particles.size());
        const auto all_particles = iota_perm(particles.all());
        const auto not_interface_iter =
            par::copy_if(all_particles, interface.begin(), is_interface);
        interface.erase(not_interface_iter, interface.end());
      } else {
        // Note: `std::ranges::partition` is faster than `std::erase_if`
        //       because it does not preserve the order of the elements.
        /// @todo Parallelize this code.
        const auto non_interface =
            std::ranges::partition(interface, is_interface);
        interface.erase(std::begin(non_interface), interface.end());
      }
    }

    // Assemble the block adjacency graph.
    block_edges_.assign_pairs_par_wide(
        num_parts + 1,
        adjacency_.transform_edges([parts](const auto& ab) {
          const auto [a, b] = ab;
          const auto part_ab = PartVec::common(parts[a], parts[b]);
          return std::pair{part_ab, ab};
        }));

    // Report the block sizes.
    TIT_STATS("ParticleMesh::block_edges_", block_edges_.bucket_sizes());
  }

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  graph::Graph adjacency_;
  graph::Graph interp_adjacency_;
  Multivector<std::pair<size_t, size_t>> block_edges_;
  [[no_unique_address]] SearchFunc search_func_;
  [[no_unique_address]] PartitionFunc partition_func_;
  [[no_unique_address]] SecondaryPartitionFunc secondary_partition_func_;

}; // class ParticleMesh

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/// Particle mesh type.
template<class PM>
concept particle_mesh = specialization_of<PM, ParticleMesh>;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

} // namespace tit::sph