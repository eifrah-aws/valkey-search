/*
 * Copyright (c) 2025, ValkeySearch contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include "src/attribute_data_type.h"
#include "src/index_schema.pb.h"
#include "src/indexes/index_base.h"
#include "src/query/predicate.h"
#include "src/rdb_io_stream.h"
#include "src/utils/allocator.h"
#include "src/utils/intrusive_ref_count.h"
#include "src/utils/string_interning.h"
#include "third_party/hnswlib/hnswlib.h"
#include "third_party/hnswlib/iostream.h"
#include "vmsdk/src/managed_pointers.h"
#include "vmsdk/src/valkey_module_api/valkey_module.h"

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
  absl::Status SaveIndex(RDBOutputStream& rdb_stream) const override
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
      vmsdk::UniqueRedisString record) const override;
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
        vector_allocator_(CREATE_UNIQUE_PTR(
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
  virtual absl::Status AddRecordImpl(uint64_t internal_id,
                                     absl::string_view record) = 0;

  virtual absl::Status RemoveRecordImpl(uint64_t internal_id) = 0;
  virtual absl::StatusOr<bool> ModifyRecordImpl(uint64_t internal_id,
                                                absl::string_view record) = 0;
  virtual int RespondWithInfoImpl(RedisModuleCtx* ctx) const = 0;

  virtual size_t GetDataTypeSize() const = 0;
  virtual void ToProtoImpl(
      data_model::VectorIndex* vector_index_proto) const = 0;
  virtual absl::Status SaveIndexImpl(RDBOutputStream& rdb_stream) const = 0;
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
  virtual char* GetValueImpl(uint64_t internal_id) const = 0;

  int dimensions_;
  std::string attribute_identifier_;
  bool normalize_{false};
  data_model::AttributeDataType attribute_data_type_;
  data_model::DistanceMetric distance_metric_;
  virtual absl::StatusOr<std::pair<float, hnswlib::labeltype>>
  ComputeDistanceFromRecordImpl(uint64_t internal_id,
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
