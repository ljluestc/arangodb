////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResultT.h"
#include "Cluster/ActionBase.h"
#include "Cluster/ActionDescription.h"
#include "Replication/utilities.h"
#include "VocBase/voc-types.h"

#include <chrono>
#include <memory>

struct TRI_vocbase_t;

namespace arangodb {
namespace network {
class ConnectionPool;
}

class DatabaseTailingSyncer;
class LogicalCollection;
struct SyncerId;

namespace maintenance {

class SynchronizeShard : public ActionBase, public ShardDefinition {
 public:
  SynchronizeShard(MaintenanceFeature&, ActionDescription const& d);

  virtual ~SynchronizeShard();

  bool first() override final;

  void setState(ActionState state) override final;

  std::string const& clientInfoString() const;

 private:
  arangodb::Result collectionCountOnLeader(std::string const& endpoint,
                                           uint64_t& c);

  arangodb::Result getReadLock(network::ConnectionPool* pool,
                               std::string const& endpoint,
                               std::string const& collection,
                               std::string const& clientId, uint64_t rlid,
                               bool soft, double timeout);

  arangodb::Result startReadLockOnLeader(std::string const& endpoint,
                                         std::string const& collection,
                                         std::string const& clientId,
                                         uint64_t& rlid, bool soft,
                                         double timeout = 300.0);

  arangodb::ResultT<TRI_voc_tick_t> catchupWithReadLock(
      std::string const& ep, LogicalCollection const& collection,
      std::string const& clientId, std::string const& leader,
      TRI_voc_tick_t lastLogTick,
      std::shared_ptr<DatabaseTailingSyncer> tailingSyncer);

  arangodb::Result catchupWithExclusiveLock(
      std::string const& ep, LogicalCollection& collection,
      std::string const& clientId, std::string const& leader, SyncerId syncerId,
      TRI_voc_tick_t lastLogTick,
      std::shared_ptr<DatabaseTailingSyncer> tailingSyncer);

  std::shared_ptr<DatabaseTailingSyncer> buildTailingSyncer(
      TRI_vocbase_t& vocbase, std::string const& endpoint);

  /// @brief Short, informative description of the replication client, passed to
  /// the server
  std::string _clientInfoString;

  uint64_t _followingTermId;

  /// @brief maximum tick until which we need to run WAL tailing for. 0 means
  /// "no restriction"
  uint64_t _tailingUpperBoundTick;

  /// @brief initial number of documents on leader
  uint64_t _initialDocCountOnLeader;
  /// @brief initial number of documents on follower
  uint64_t _initialDocCountOnFollower;
  /// @brief number of documents on follower at end of (successful)
  /// synchronization
  uint64_t _docCountAtEnd;

  /// @brief end time (timestamp in seconds)
  std::chrono::time_point<std::chrono::steady_clock> _endTimeForAttempt;
};

}  // namespace maintenance
}  // namespace arangodb
