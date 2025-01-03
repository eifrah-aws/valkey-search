/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VALKEYSEARCH_SRC_INDEXES_VECTOR_BASE_H_
#define VALKEYSEARCH_SRC_INDEXES_VECTOR_BASE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "third_party/hnswlib/hnswlib.h"
#include "third_party/hnswlib/iostream.h"
#include "src/attribute_data_type.h"
#include "src/index_schema.pb.h"
#include "src/indexes/index_base.h"
#include "src/query/predicate.h"
#include "src/rdb_io_stream.h"
#include "src/utils/allocator.h"
#include "src/utils/intrusive_ref_count.h"
#include "src/utils/string_interning.h"
#include "vmsdk/src/managed_pointers.h"
#include "vmsdk/src/redismodule.h"

namespace valkey_search::indexes {

inline constexpr uint32_t kHNSWBlockSize = 1024 * 10;

std::vector<char> NormalizeEmbedding(absl::string_view record, size_t type_size,
                                     float* magnitude = nullptr);

struct Neighbor {
  InternedStringPtr external_id;
  float distance;
  std::optional<RecordsMap> attribute_contents;
  Neighbor(const InternedStringPtr& external_id, float distance)
      : external_id(external_id), distance(distance) {}
  Neighbor(const InternedStringPtr& external_id, float distance,
           std::optional<RecordsMap>&& attribute_contents)
      : external_id(external_id),
        distance(distance),
        attribute_contents(std::move(attribute_contents)) {}
  Neighbor(Neighbor&& other) noexcept
      : external_id(std::move(other.external_id)),
        distance(other.distance),
        attribute_contents(std::move(other.attribute_contents)) {}
  Neighbor& operator=(Neighbor&& other) noexcept {
    if (this != &other) {
      external_id = std::move(other.external_id);
      distance = other.distance;
      attribute_contents = std::move(other.attribute_contents);
    }
    return *this;
  }
};

const absl::NoDestructor<absl::flat_hash_map<
    absl::string_view, data_model::VectorIndex::AlgorithmCase>>
    kVectorAlgoByStr({
        {"HNSW", data_model::VectorIndex::AlgorithmCase::kHnswAlgorithm},
        {"FLAT", data_model::VectorIndex::AlgorithmCase::kFlatAlgorithm},
    });

const absl::NoDestructor<
    absl::flat_hash_map<absl::string_view, data_model::DistanceMetric>>
    kDistanceMetricByStr(
        {{"L2", data_model::DistanceMetric::DISTANCE_METRIC_L2},
         {"IP", data_model::DistanceMetric::DISTANCE_METRIC_IP},
         {"COSINE", data_model::DistanceMetric::DISTANCE_METRIC_COSINE}});

const absl::NoDestructor<
    absl::flat_hash_map<absl::string_view, data_model::VectorDataType>>
    kVectorDataTypeByStr({{"FLOAT32", data_model::VECTOR_DATA_TYPE_FLOAT32}});

template <typename V>
absl::string_view LookupKeyByValue(
    const absl::flat_hash_map<absl::string_view, V>& map, const V& value) {
  auto it = std::find_if(map.begin(), map.end(), [&value](const auto& pair) {
    return pair.second == value;
  });
  if (it != map.end()) {
    return it->first;  // Return the key
  } else {
    return "";
  }
}

class VectorBase : public IndexBase, public hnswlib::VectorTracker {
 public:
  absl::StatusOr<bool> AddRecord(const InternedStringPtr& key,
                                 absl::string_view record) override;
  absl::StatusOr<bool> RemoveRecord(const InternedStringPtr& key,
                                    indexes::DeletionType deletion_type =
                                        indexes::DeletionType::kNone) override;
  absl::StatusOr<bool> ModifyRecord(const InternedStringPtr& key,
                                    absl::string_view record) override;
  bool IsTracked(const InternedStringPtr& key) const override
      ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  virtual size_t GetCapacity() const = 0;
  bool GetNormalize() const { return normalize_; }
  std::unique_ptr<data_model::Index> ToProto() const override;
  virtual absl::Status SaveIndex(RDBOutputStream& rdb_stream) const override
      ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  void ForEachTrackedKey(
      absl::AnyInvocable<void(const InternedStringPtr&)> fn) const override {
    absl::MutexLock lock(&key_to_metadata_mutex_);
    for (const auto& [key, _] : tracked_metadata_by_key_) {
      fn(key);
    }
  }
  absl::StatusOr<InternedStringPtr> GetKeyDuringSearch(
      uint64_t internal_id) const ABSL_NO_THREAD_SAFETY_ANALYSIS;
  void AddPrefilteredKey(
      absl::string_view query, uint64_t count, const InternedStringPtr& key,
      std::priority_queue<std::pair<float, hnswlib::labeltype>>& results,
      absl::flat_hash_set<hnswlib::labeltype>& top_keys) const;
  vmsdk::UniqueRedisString NormalizeStringRecord(
      vmsdk::UniqueRedisString input) const override;
  uint64_t GetRecordCount() const override;
  template <typename T>
  absl::StatusOr<std::deque<Neighbor>> CreateReply(
      std::priority_queue<std::pair<T, hnswlib::labeltype>>& knn_res);
  absl::StatusOr<std::vector<char>> GetValue(const InternedStringPtr& key) const
      ABSL_NO_THREAD_SAFETY_ANALYSIS;
  int GetVectorDataSize() const { return GetDataTypeSize() * dimensions_; }
  char* TrackVector(uint64_t internal_id, char* vector, size_t len) override;
  std::shared_ptr<InternedString> InternVector(absl::string_view record,
                                               std::optional<float>& magnitude);

 protected:
  VectorBase(IndexerType indexer_type, int dimensions,
             data_model::AttributeDataType attribute_data_type,
             absl::string_view attribute_identifier)
      : IndexBase(indexer_type),
        dimensions_(dimensions),
        attribute_identifier_(attribute_identifier),
        attribute_data_type_(attribute_data_type),
        vector_allocator_(CreateUniquePtr(
            FixedSizeAllocator, dimensions * sizeof(float) + 1, true)) {}

  bool IsValidSizeVector(absl::string_view record) {
    const auto data_type_size = GetDataTypeSize();
    int32_t dim = record.size() / GetDataTypeSize();
    return dim == dimensions_ && (record.size() % data_type_size == 0);
  }
  int RespondWithInfo(RedisModuleCtx* ctx) const override;
  template <typename T>
  void Init(int dimensions, data_model::DistanceMetric distance_metric,
            std::unique_ptr<hnswlib::SpaceInterface<T>>& space);
  virtual absl::Status _AddRecord(uint64_t internal_id,
                                  absl::string_view record) = 0;

  virtual absl::Status _RemoveRecord(uint64_t internal_id) = 0;
  virtual absl::StatusOr<bool> _ModifyRecord(uint64_t internal_id,
                                             absl::string_view record) = 0;
  virtual int _RespondWithInfo(RedisModuleCtx* ctx) const = 0;

  virtual size_t GetDataTypeSize() const = 0;
  virtual void _ToProto(data_model::VectorIndex* vector_index_proto) const = 0;
  virtual absl::Status _SaveIndex(RDBOutputStream& rdb_stream) const = 0;
  void ExternalizeVector(RedisModuleCtx* ctx,
                         const AttributeDataType* attribute_data_type,
                         absl::string_view key_cstr,
                         absl::string_view attribute_identifier);
  absl::Status LoadTrackedKeys(RedisModuleCtx* ctx,
                               const AttributeDataType* attribute_data_type,
                               const data_model::TrackedKeys& tracked_keys);
  // Used for backwards compatibility.
  // TODO(b/) remove after rollout.
  absl::Status ConsumeKeysAndInternalIdsForBackCompat(
      RDBInputStream& rdb_stream);
  absl::Status LoadKeysAndInternalIds(
      RedisModuleCtx* ctx, const AttributeDataType* attribute_data_type,
      RDBInputStream& rdb_stream);
  virtual char* _GetValue(uint64_t internal_id) const = 0;

  int dimensions_;
  std::string attribute_identifier_;
  bool normalize_{false};
  data_model::AttributeDataType attribute_data_type_;
  data_model::DistanceMetric distance_metric_;
  virtual absl::StatusOr<std::pair<float, hnswlib::labeltype>>
  _ComputeDistanceFromRecord(uint64_t internal_id,
                             absl::string_view query) const = 0;
  virtual char* TrackVector(uint64_t internal_id,
                            const InternedStringPtr& vector) = 0;
  virtual void UnTrackVector(uint64_t internal_id) = 0;

 private:
  absl::StatusOr<uint64_t> TrackKey(const InternedStringPtr& key,
                                    float magnitude,
                                    const InternedStringPtr& vector)
      ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  absl::StatusOr<std::optional<uint64_t>> UnTrackKey(
      const InternedStringPtr& key) ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  absl::Status UpdateMetadata(const InternedStringPtr& key, float magnitude,
                              const InternedStringPtr& vector)
      ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  absl::StatusOr<uint64_t> GetInternalId(const InternedStringPtr& key) const
      ABSL_LOCKS_EXCLUDED(key_to_metadata_mutex_);
  absl::StatusOr<uint64_t> GetInternalIdDuringSearch(
      const InternedStringPtr& key) const ABSL_NO_THREAD_SAFETY_ANALYSIS;
  absl::flat_hash_map<uint64_t, InternedStringPtr> key_by_internal_id_
      ABSL_GUARDED_BY(key_to_metadata_mutex_);
  struct TrackedKeyMetadata {
    uint64_t internal_id;
    // If normalize_ is false, this will be -1.0f. Otherwise, it will be the
    // magnitude of the vector. If the magnitude is not initialized, it will be
    // -inf (this is an intermediate state during backfill when transitioning
    // from the old RDB format that didn't include magnitudes).
    float magnitude;
  };

  InternedStringMap<TrackedKeyMetadata> tracked_metadata_by_key_
      ABSL_GUARDED_BY(key_to_metadata_mutex_);
  uint64_t inc_id_ ABSL_GUARDED_BY(key_to_metadata_mutex_){0};
  mutable absl::Mutex key_to_metadata_mutex_;
  absl::StatusOr<std::pair<float, hnswlib::labeltype>>
  ComputeDistanceFromRecord(const InternedStringPtr& key,
                            absl::string_view query) const;
  UniqueFixedSizeAllocatorPtr vector_allocator_;
};

class InlineVectorEvaluator : public query::Evaluator {
 public:
  bool Evaluate(const query::Predicate& predicate,
                const InternedStringPtr& key);

 private:
  bool EvaluateTags(const query::TagPredicate& predicate) override;
  bool EvaluateNumeric(const query::NumericPredicate& predicate) override;
  const InternedStringPtr* key_{nullptr};
};

}  // namespace valkey_search::indexes

#endif  // VALKEYSEARCH_SRC_INDEXES_VECTOR_BASE_H_
