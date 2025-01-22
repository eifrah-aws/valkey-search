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

#ifndef VALKEYSEARCH_SRC_INDEXES_VECTOR_FLAT_H_
#define VALKEYSEARCH_SRC_INDEXES_VECTOR_FLAT_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "src/attribute_data_type.h"
#include "src/indexes/vector_base.h"
#include "src/rdb_io_stream.h"
#include "src/utils/string_interning.h"
#include "third_party/hnswlib/bruteforce.h"
#include "third_party/hnswlib/hnswlib.h"
#include "vmsdk/src/valkey_module_api/valkey_module.h"

namespace valkey_search::indexes {

template <typename T>
class VectorFlat : public VectorBase {
 public:
  static absl::StatusOr<std::shared_ptr<VectorFlat<T>>> Create(
      const data_model::VectorIndex& vector_index_proto,
      absl::string_view attribute_identifier,
      data_model::AttributeDataType attribute_data_type)
      ABSL_NO_THREAD_SAFETY_ANALYSIS;
  static absl::StatusOr<std::shared_ptr<VectorFlat<T>>> LoadFromRDB(
      RedisModuleCtx* ctx, const AttributeDataType* attribute_data_type,
      const data_model::VectorIndex& vector_index_proto,
      RDBInputStream& rdb_stream,
      absl::string_view attribute_identifier) ABSL_NO_THREAD_SAFETY_ANALYSIS;
  ~VectorFlat() override = default;
  size_t GetDataTypeSize() const override { return sizeof(T); }

  const hnswlib::SpaceInterface<float>* GetSpace() const {
    return space_.get();
  }
  int GetDimensions() const { return dimensions_; }
  int GetBlockSize() const { return block_size_; }
  size_t GetCapacity() const override
      ABSL_SHARED_LOCKS_REQUIRED(resize_mutex_) {
    return algo_->data_->getCapacity();
  }
  absl::StatusOr<std::deque<Neighbor>> Search(
      absl::string_view query, uint64_t count,
      std::unique_ptr<hnswlib::BaseFilterFunctor> filter = nullptr)
      ABSL_LOCKS_EXCLUDED(resize_mutex_);

 protected:
  absl::Status ResizeIfFull() ABSL_LOCKS_EXCLUDED(resize_mutex_);
  absl::Status AddRecordImpl(uint64_t internal_id,
                             absl::string_view record) override
      ABSL_LOCKS_EXCLUDED(resize_mutex_);

  absl::Status RemoveRecordImpl(uint64_t internal_id) override
      ABSL_LOCKS_EXCLUDED(resize_mutex_);
  absl::StatusOr<bool> ModifyRecordImpl(uint64_t internal_id,
                                        absl::string_view record) override;
  void ToProtoImpl(data_model::VectorIndex* vector_index_proto) const override;
  int RespondWithInfoImpl(RedisModuleCtx* ctx) const override;
  absl::Status SaveIndexImpl(RDBOutputStream& rdb_stream) const override;
  absl::StatusOr<std::pair<float, hnswlib::labeltype>>
  ComputeDistanceFromRecordImpl(uint64_t internal_id,
                                absl::string_view query) const override;
  char* GetValueImpl(uint64_t internal_id) const override
      ABSL_NO_THREAD_SAFETY_ANALYSIS {
    return algo_->getPoint(internal_id);
  }
  char* TrackVector(uint64_t internal_id,
                    const InternedStringPtr& vector) override
      ABSL_LOCKS_EXCLUDED(tracked_vectors_mutex_);
  void UnTrackVector(uint64_t internal_id) override
      ABSL_LOCKS_EXCLUDED(tracked_vectors_mutex_);

 private:
  VectorFlat(int dimensions, data_model::DistanceMetric distance_metric,
             uint32_t block_size, absl::string_view attribute_identifier,
             data_model::AttributeDataType attribute_data_type);
  std::unique_ptr<hnswlib::BruteforceSearch<T>> algo_
      ABSL_GUARDED_BY(resize_mutex_);
  std::unique_ptr<hnswlib::SpaceInterface<T>> space_;
  uint32_t block_size_;
  mutable absl::Mutex resize_mutex_;
  mutable absl::Mutex tracked_vectors_mutex_;
  absl::flat_hash_map<uint64_t, InternedStringPtr> tracked_vectors_
      ABSL_GUARDED_BY(tracked_vectors_mutex_);
};
}  // namespace valkey_search::indexes

#endif  // VALKEYSEARCH_SRC_INDEXES_VECTOR_FLAT_H_
