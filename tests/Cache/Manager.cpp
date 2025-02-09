////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Cache/BinaryKeyHasher.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/PlainCache.h"
#include "Random/RandomGenerator.h"
#include "RestServer/SharedPRNGFeature.h"

#include "Mocks/Servers.h"
#include "MockScheduler.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

TEST(CacheManagerTest, test_create_and_destroy_caches) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  Manager manager(sharedPRNG, postFn, requestLimit, true, 0.04, 0.25);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  std::vector<std::shared_ptr<Cache>> caches;

  for (size_t i = 0; i < 8; ++i) {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(i, beforeStats->activeTables);

    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_NE(nullptr, cache);
    ASSERT_GT(cache->size(), 80 * 1024);  // size of each cache is about 80kb

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation + cache->size(),
              afterStats->globalAllocation);
    ASSERT_EQ(i + 1, afterStats->activeTables);

    ASSERT_EQ(0, afterStats->spareAllocation);
    ASSERT_EQ(0, afterStats->spareTables);

    caches.emplace_back(std::move(cache));
  }

  std::uint64_t spareTables = 0;
  while (!caches.empty()) {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(spareTables, beforeStats->spareTables);

    auto cache = caches.back();
    std::uint64_t size = cache->size();
    ASSERT_GT(size, 80 * 1024);  // size of each cache is about 80kb
    manager.destroyCache(cache);

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    if (afterStats->spareTables == beforeStats->spareTables) {
      // table deleted
      ASSERT_EQ(beforeStats->globalAllocation,
                afterStats->globalAllocation + size);
      ASSERT_EQ(spareTables, afterStats->spareTables);
    } else {
      // table recycled
      ++spareTables;
      // ASSERT_EQ(spareTables * 80 * 1024, afterStats.spareAllocation);
      ASSERT_EQ(spareTables, afterStats->spareTables);
    }
    ASSERT_EQ(caches.size() - 1, afterStats->activeTables);

    caches.pop_back();
  }
}

TEST(CacheManagerTest, test_basic_constructor_function) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  Manager manager(sharedPRNG, postFn, requestLimit, true, 0.04, 0.25);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  std::uint64_t bigRequestLimit = 4ULL * 1024ULL * 1024ULL * 1024ULL;
  Manager bigManager(sharedPRNG, nullptr, bigRequestLimit, true, 0.04, 0.25);

  ASSERT_EQ(bigRequestLimit, bigManager.globalLimit());

  ASSERT_TRUE(1024ULL * 1024ULL < bigManager.globalAllocation());
  ASSERT_TRUE(bigRequestLimit > bigManager.globalAllocation());
}

TEST(CacheManagerTest, test_mixed_cache_types_under_mixed_load_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 1024ULL * 1024ULL * 1024ULL, true, 0.04,
                  0.25);
  std::size_t cacheCount = 4;
  std::size_t threadCount = 4;
  std::vector<std::shared_ptr<Cache>> caches;
  for (std::size_t i = 0; i < cacheCount; i++) {
    auto res = manager.createCache<BinaryKeyHasher>(
        (i % 2 == 0) ? CacheType::Plain : CacheType::Transactional);
    TRI_ASSERT(res);
    caches.emplace_back(res);
  }

  std::uint64_t chunkSize = 4 * 1024 * 1024;
  std::uint64_t initialInserts = 1 * 1024 * 1024;
  std::uint64_t operationCount = 4 * 1024 * 1024;
  std::atomic<std::uint64_t> hitCount(0);
  std::atomic<std::uint64_t> missCount(0);
  auto worker = [&caches, cacheCount, initialInserts, operationCount, &hitCount,
                 &missCount](std::uint64_t lower, std::uint64_t upper) -> void {
    // fill with some initial data
    for (std::uint64_t i = 0; i < initialInserts; i++) {
      std::uint64_t item = lower + i;
      std::size_t cacheIndex = item % cacheCount;
      CachedValue* value = CachedValue::construct(&item, sizeof(std::uint64_t),
                                                  &item, sizeof(std::uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = caches[cacheIndex]->insert(value);
      if (status != TRI_ERROR_NO_ERROR) {
        delete value;
      }
    }

    // initialize valid range for keys that *might* be in cache
    std::uint64_t validLower = lower;
    std::uint64_t validUpper = lower + initialInserts - 1;

    // commence mixed workload
    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint32_t r =
          RandomGenerator::interval(static_cast<std::uint32_t>(99));

      if (r >= 99) {  // remove something
        if (validLower == validUpper) {
          continue;  // removed too much
        }

        std::uint64_t item = validLower++;
        std::size_t cacheIndex = item % cacheCount;

        caches[cacheIndex]->remove(&item, sizeof(std::uint64_t));
      } else if (r >= 95) {  // insert something
        if (validUpper == upper) {
          continue;  // already maxed out range
        }

        std::uint64_t item = ++validUpper;
        std::size_t cacheIndex = item % cacheCount;
        CachedValue* value = CachedValue::construct(
            &item, sizeof(std::uint64_t), &item, sizeof(std::uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = caches[cacheIndex]->insert(value);
        if (status != TRI_ERROR_NO_ERROR) {
          delete value;
        }
      } else {  // lookup something
        std::uint64_t item = RandomGenerator::interval(
            static_cast<int64_t>(validLower), static_cast<int64_t>(validUpper));
        std::size_t cacheIndex = item % cacheCount;

        auto f = caches[cacheIndex]->find(&item, sizeof(std::uint64_t));
        if (f.found()) {
          hitCount++;
          TRI_ASSERT(f.value() != nullptr);
          TRI_ASSERT(BinaryKeyHasher::sameKey(f.value()->key(),
                                              f.value()->keySize(), &item,
                                              sizeof(std::uint64_t)));
        } else {
          missCount++;
          TRI_ASSERT(f.value() == nullptr);
        }
      }
    }
  };

  std::vector<std::thread*> threads;
  // dispatch threads
  for (std::size_t i = 0; i < threadCount; i++) {
    std::uint64_t lower = i * chunkSize;
    std::uint64_t upper = ((i + 1) * chunkSize) - 1;
    threads.push_back(new std::thread(worker, lower, upper));
  }

  // join threads
  for (auto t : threads) {
    t->join();
    delete t;
  }

  for (auto cache : caches) {
    manager.destroyCache(cache);
  }

  RandomGenerator::shutdown();
}

TEST(CacheManagerTest, test_manager_under_cache_lifecycle_chaos_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 1024ULL * 1024ULL * 1024ULL, true, 0.04,
                  0.25);
  std::size_t threadCount = 4;
  std::uint64_t operationCount = 4ULL * 1024ULL;

  auto worker = [&manager, operationCount]() -> void {
    std::queue<std::shared_ptr<Cache>> caches;

    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint32_t r =
          RandomGenerator::interval(static_cast<std::uint32_t>(1));
      switch (r) {
        case 0: {
          auto res = manager.createCache<BinaryKeyHasher>(
              (i % 2 == 0) ? CacheType::Plain : CacheType::Transactional);
          if (res) {
            caches.emplace(res);
          }
        }
        // intentionally falls through
        case 1:
        default: {
          if (!caches.empty()) {
            auto cache = caches.front();
            caches.pop();
            manager.destroyCache(cache);
          }
        }
      }
    }
  };

  std::vector<std::thread*> threads;
  // dispatch threads
  for (std::size_t i = 0; i < threadCount; i++) {
    threads.push_back(new std::thread(worker));
  }

  // join threads
  for (auto t : threads) {
    t->join();
    delete t;
  }

  RandomGenerator::shutdown();
}
