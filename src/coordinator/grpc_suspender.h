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

#ifndef VALKEYSEARCH_SRC_COORDINATOR_GRPC_SUSPENDER_H_
#define VALKEYSEARCH_SRC_COORDINATOR_GRPC_SUSPENDER_H_

#include <cstdint>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"

namespace valkey_search::coordinator {

// GRPCSuspender is used to suspend and resume gRPC callbacks in combination
// with GRPCSuspensionGuard. This is used to ensure that gRPC callbacks do not
// access shared mutexes used by the child process during fork.
class GRPCSuspender {
 public:
  static GRPCSuspender& Instance() {
    static GRPCSuspender instance;
    return instance;
  }
  absl::Status Suspend();
  absl::Status Resume();
  void Increment();
  void Decrement();

 private:
  GRPCSuspender() = default;

  absl::Mutex mutex_;
  int64_t count_ ABSL_GUARDED_BY(mutex_) = 0;
  bool suspended_ ABSL_GUARDED_BY(mutex_) = false;
  absl::CondVar in_flight_tasks_completed_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar resume_ ABSL_GUARDED_BY(mutex_);
};

// gRPC runs server callbacks and client-provided callbacks on a background
// thread. This guard ensures that these threads do not access any shared
// mutexes used by the child process during fork. It should be acquired by each
// gRPC callback so that new callbacks can be suspended prior to forking.
class GRPCSuspensionGuard {
 public:
  explicit GRPCSuspensionGuard(GRPCSuspender& grpc_suspender)
      : grpc_suspender_(grpc_suspender) {
    grpc_suspender_.Increment();
  }
  ~GRPCSuspensionGuard() { grpc_suspender_.Decrement(); }

 private:
  GRPCSuspender& grpc_suspender_;
};

}  // namespace valkey_search::coordinator

#endif  // VALKEYSEARCH_SRC_COORDINATOR_GRPC_SUSPENDER_H_
