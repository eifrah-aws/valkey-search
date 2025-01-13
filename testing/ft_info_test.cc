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

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "src/commands/commands.h"
#include "src/index_schema.pb.h"
#include "src/schema_manager.h"
#include "testing/common.h"
#include "vmsdk/src/module.h"
#include "vmsdk/src/valkey_module_api/valkey_module.h"
#include "vmsdk/src/testing_infra/module.h"
#include "vmsdk/src/thread_pool.h"

namespace valkey_search {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;

struct SingleFtInfoTestCase {
  std::vector<std::string> argv;
  std::optional<std::string> index_schema_pbtxt;
  bool expect_return_failure;
  absl::string_view expected_output;
};

struct MultiFtInfoTestCase {
  std::string test_name;
  std::vector<SingleFtInfoTestCase> test_cases;
};

class FTInfoTest : public ValkeySearchTestWithParam<MultiFtInfoTestCase> {};

TEST_P(FTInfoTest, FTInfoTests) {
  const MultiFtInfoTestCase& test_cases = GetParam();

  for (bool use_thread_pool : {true, false}) {
    for (const auto& test_case : test_cases.test_cases) {
      fake_ctx_ = RedisModuleCtx{};
      // Setup the data structures for the test case.
      vmsdk::ThreadPool mutations_thread_pool("writer-thread-pool-", 5);
      SchemaManager::InitInstance(std::make_unique<TestableSchemaManager>(
          &fake_ctx_, []() {},
          use_thread_pool ? &mutations_thread_pool : nullptr, false));
      data_model::IndexSchema index_schema_proto;
      if (test_case.index_schema_pbtxt.has_value()) {
        ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
            test_case.index_schema_pbtxt.value(), &index_schema_proto));
        VMSDK_EXPECT_OK(SchemaManager::Instance().CreateIndexSchema(
            &fake_ctx_, index_schema_proto));
        EXPECT_EQ(SchemaManager::Instance().GetNumberOfIndexSchemas(), 1);
      }

      if (test_case.expect_return_failure) {
        EXPECT_CALL(*kMockRedisModule, ReplyWithError(&fake_ctx_, _))
            .WillOnce(Return(REDISMODULE_OK));
      }

      // Run the command.
      std::vector<RedisModuleString*> cmd_argv;
      std::transform(test_case.argv.begin(), test_case.argv.end(),
                     std::back_inserter(cmd_argv), [&](std::string val) {
                       return TestRedisModule_CreateStringPrintf(
                           &fake_ctx_, "%s", val.data());
                     });
      EXPECT_EQ(vmsdk::CreateCommand<FTInfoCmd>(&fake_ctx_, cmd_argv.data(),
                                                cmd_argv.size()),
                REDISMODULE_OK);
      EXPECT_EQ(fake_ctx_.reply_capture.GetReply(), test_case.expected_output);

      if (test_case.index_schema_pbtxt.has_value()) {
        VMSDK_EXPECT_OK(SchemaManager::Instance().RemoveIndexSchema(
            index_schema_proto.db_num(), index_schema_proto.name()));
        EXPECT_EQ(SchemaManager::Instance().GetNumberOfIndexSchemas(), 0);
      }

      // Clean up
      for (auto cmd_arg : cmd_argv) {
        TestRedisModule_FreeString(&fake_ctx_, cmd_arg);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    FTInfoTests, FTInfoTest,
    ValuesIn<MultiFtInfoTestCase>({
        {
            .test_name = "happy_path_hnsw",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 0
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              vector_index: {
                                dimension_count: 10
                                normalize: true
                                distance_metric: DISTANCE_METRIC_COSINE
                                vector_data_type: VECTOR_DATA_TYPE_FLOAT32
                                initial_cap: 100
                                hnsw_algorithm {
                                  m: 240
                                  ef_construction: 400
                                  ef_runtime: 30
                                }
                              }
                            }
                          }
                        )",
                        .expect_return_failure = false,
                        .expected_output =
                            "*24\r\n+index_name\r\n+test_name\r\n+index_"
                            "options\r\n*0\r\n+index_definition\r\n*6\r\n+key_"
                            "type\r\n+HASH\r\n+prefixes\r\n*1\r\n+prefix_1\r\n+"
                            "default_score\r\n$1\r\n1\r\n+attributes\r\n*1\r\n*"
                            "8\r\n+identifier\r\n+test_identifier_1\r\n+"
                            "attribute\r\n+test_attribute_1\r\n+type\r\n+"
                            "VECTOR\r\n+index\r\n*12\r\n+capacity\r\n:100\r\n+"
                            "dimensions\r\n:10\r\n+distance_metric\r\n+"
                            "COSINE\r\n+size\r\n$1\r\n0\r\n+data_type\r\n+"
                            "FLOAT32\r\n+algorithm\r\n*8\r\n+name\r\n+HNSW\r\n+"
                            "m\r\n:240\r\n+ef_construction\r\n:400\r\n+ef_"
                            "runtime\r\n:30\r\n+num_docs\r\n$1\r\n0\r\n+num_"
                            "terms\r\n$1\r\n0\r\n+num_records\r\n$1\r\n0\r\n+"
                            "hash_indexing_failures\r\n$1\r\n0\r\n+backfill_in_"
                            "progress\r\n$1\r\n0\r\n+backfill_complete_"
                            "percent\r\n$8\r\n1.000000\r\n+mutation_queue_"
                            "size\r\n$1\r\n0\r\n+recent_mutations_queue_"
                            "delay\r\n$5\r\n0 sec\r\n",
                    },
                },
        },
        {
            .test_name = "happy_path_flat",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 0
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              vector_index: {
                                dimension_count: 10
                                normalize: true
                                distance_metric: DISTANCE_METRIC_COSINE
                                vector_data_type: VECTOR_DATA_TYPE_FLOAT32
                                initial_cap: 100
                                flat_algorithm {
                                  block_size: 1024
                                }
                              }
                            }
                          }
                        )",
                        .expect_return_failure = false,
                        .expected_output =
                            "*24\r\n+index_name\r\n+test_name\r\n+index_"
                            "options\r\n*0\r\n+index_definition\r\n*6\r\n+key_"
                            "type\r\n+HASH\r\n+prefixes\r\n*1\r\n+prefix_1\r\n+"
                            "default_score\r\n$1\r\n1\r\n+attributes\r\n*1\r\n*"
                            "8\r\n+identifier\r\n+test_identifier_1\r\n+"
                            "attribute\r\n+test_attribute_1\r\n+type\r\n+"
                            "VECTOR\r\n+index\r\n*12\r\n+capacity\r\n:100\r\n+"
                            "dimensions\r\n:10\r\n+distance_metric\r\n+"
                            "COSINE\r\n+size\r\n$1\r\n0\r\n+data_type\r\n+"
                            "FLOAT32\r\n+algorithm\r\n*4\r\n+name\r\n+FLAT\r\n+"
                            "block_size\r\n:1024\r\n+num_docs\r\n$1\r\n0\r\n+"
                            "num_terms\r\n$1\r\n0\r\n+num_records\r\n$"
                            "1\r\n0\r\n+hash_indexing_failures\r\n$1\r\n0\r\n+"
                            "backfill_in_progress\r\n$1\r\n0\r\n+backfill_"
                            "complete_percent\r\n$8\r\n1.000000\r\n+mutation_"
                            "queue_size\r\n$1\r\n0\r\n+recent_mutations_queue_"
                            "delay\r\n$5\r\n0 sec\r\n",
                    },
                },
        },
        {
            .test_name = "happy_path_tag_no_flags",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 0
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              tag_index: {
                                separator: "@"
                              }
                            }
                          }
                        )",
                        .expect_return_failure = false,
                        .expected_output =
                            "*24\r\n+index_name\r\n+test_name\r\n+index_"
                            "options\r\n*0\r\n+index_definition\r\n*6\r\n+key_"
                            "type\r\n+HASH\r\n+prefixes\r\n*1\r\n+prefix_1\r\n+"
                            "default_score\r\n$1\r\n1\r\n+attributes\r\n*1\r\n*"
                            "10\r\n+identifier\r\n+test_identifier_1\r\n+"
                            "attribute\r\n+test_attribute_1\r\n+type\r\n+"
                            "TAG\r\n+SEPARATOR\r\n+@\r\n+size\r\n$1\r\n0\r\n+"
                            "num_docs\r\n$1\r\n0\r\n+num_terms\r\n$1\r\n0\r\n+"
                            "num_records\r\n$1\r\n0\r\n+hash_indexing_"
                            "failures\r\n$1\r\n0\r\n+backfill_in_progress\r\n$"
                            "1\r\n0\r\n+backfill_complete_percent\r\n$8\r\n1."
                            "000000\r\n+mutation_queue_size\r\n$1\r\n0\r\n+"
                            "recent_mutations_queue_delay\r\n$5\r\n0 sec\r\n",
                    },
                },
        },
        {
            .test_name = "happy_path_tag_case_sensitive_true",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 0
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              tag_index: {
                                separator: "@"
                                case_sensitive: true
                              }
                            }
                          }
                        )",
                        .expect_return_failure = false,
                        .expected_output =
                            "*24\r\n+index_name\r\n+test_name\r\n+index_"
                            "options\r\n*0\r\n+index_definition\r\n*6\r\n+key_"
                            "type\r\n+HASH\r\n+prefixes\r\n*1\r\n+prefix_1\r\n+"
                            "default_score\r\n$1\r\n1\r\n+attributes\r\n*1\r\n*"
                            "11\r\n+identifier\r\n+test_identifier_1\r\n+"
                            "attribute\r\n+test_attribute_1\r\n+type\r\n+"
                            "TAG\r\n+SEPARATOR\r\n+@\r\n+CASESENSITIVE\r\n+"
                            "size\r\n$1\r\n0\r\n+num_docs\r\n$1\r\n0\r\n+num_"
                            "terms\r\n$1\r\n0\r\n+num_records\r\n$1\r\n0\r\n+"
                            "hash_indexing_failures\r\n$1\r\n0\r\n+backfill_in_"
                            "progress\r\n$1\r\n0\r\n+backfill_complete_"
                            "percent\r\n$8\r\n1.000000\r\n+mutation_queue_"
                            "size\r\n$1\r\n0\r\n+recent_mutations_queue_"
                            "delay\r\n$5\r\n0 sec\r\n",
                    },
                },
        },
        {
            .test_name = "happy_path_numeric",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 0
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              numeric_index: {}
                            }
                          }
                        )",
                        .expect_return_failure = false,
                        .expected_output =
                            "*24\r\n+index_name\r\n+test_name\r\n+index_"
                            "options\r\n*0\r\n+index_definition\r\n*6\r\n+key_"
                            "type\r\n+HASH\r\n+prefixes\r\n*1\r\n+prefix_1\r\n+"
                            "default_score\r\n$1\r\n1\r\n+attributes\r\n*1\r\n*"
                            "8\r\n+identifier\r\n+test_identifier_1\r\n+"
                            "attribute\r\n+test_attribute_1\r\n+type\r\n+"
                            "NUMERIC\r\n+size\r\n$1\r\n0\r\n+num_docs\r\n$"
                            "1\r\n0\r\n+num_terms\r\n$1\r\n0\r\n+num_"
                            "records\r\n$1\r\n0\r\n+hash_indexing_failures\r\n$"
                            "1\r\n0\r\n+backfill_in_progress\r\n$1\r\n0\r\n+"
                            "backfill_complete_percent\r\n$8\r\n1.000000\r\n+"
                            "mutation_queue_size\r\n$1\r\n0\r\n+recent_"
                            "mutations_queue_delay\r\n$5\r\n0 sec\r\n",
                    },
                },
        },
        {
            .test_name = "incorrect_param_count",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info"},
                        .expect_return_failure = true,
                        .expected_output = "$49\r\nERR wrong number of "
                                           "arguments for FT.INFO command\r\n",
                    },
                },
        },
        {
            .test_name = "index_schema_does_not_exist",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "non_exist_test_name"},
                        .index_schema_pbtxt = std::nullopt,
                        .expect_return_failure = true,
                        .expected_output =
                            "$47\r\nIndex with name 'non_exist_test_name' not "
                            "found\r\n",
                    },
                },
        },
        {
            .test_name = "index_schema_exists_in_different_db",
            .test_cases =
                {
                    {
                        .argv = {"FT.Info", "non_exist_test_name"},
                        .index_schema_pbtxt = R"(
                          name: "test_name"
                          db_num: 1
                          subscribed_key_prefixes: "prefix_1"
                          attribute_data_type: ATTRIBUTE_DATA_TYPE_HASH
                          attributes: {
                            alias: "test_attribute_1"
                            identifier: "test_identifier_1"
                            index: {
                              vector_index: {
                                dimension_count: 10
                                normalize: true
                                distance_metric: DISTANCE_METRIC_COSINE
                                vector_data_type: VECTOR_DATA_TYPE_FLOAT32
                                initial_cap: 100
                                flat_algorithm {
                                  block_size: 1024
                                }
                              }
                            }
                          }
                        )",
                        .expect_return_failure = true,
                        .expected_output =
                            "$47\r\nIndex with name 'non_exist_test_name' not "
                            "found\r\n",
                    },
                },
        },
    }),
    [](const TestParamInfo<MultiFtInfoTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace valkey_search
