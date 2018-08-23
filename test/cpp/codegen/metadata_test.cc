/*
 *
 * Copyright 2017 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <grpc/impl/codegen/byte_buffer.h>
#include <grpc/slice.h>
#include <grpcpp/impl/codegen/grpc_library.h>
#include <grpcpp/impl/codegen/metadata_map.h>
#include <grpcpp/impl/codegen/proto_utils.h>
#include <grpcpp/impl/grpc_library.h>
#include <gtest/gtest.h>

#include "src/proto/grpc/testing/echo.grpc.pb.h"

namespace grpc {

namespace {

class MetadataTest : public ::testing::Test {};

TEST_F(MetadataTest, TestAddStringValue) {
  MetadataContainer container;
  container.AddMetadata("foo", StringMetadataValue("bar"));
}

TEST_F(MetadataTest, TestAddProtoValue) {
  MetadataContainer container;
  testing::EchoRequest request;
  request.set_message("bar");
  container.AddMetadata("foo",
                        ProtoMetadataValue<testing::EchoRequest>(&request));
}

TEST_F(MetadataTest, TestGetAfterAddString) {
  MetadataContainer container;
  container.AddMetadata("foo", StringMetadataValue("bar"));
  const StringMetadataValue* gotten = nullptr;
  EXPECT_TRUE(container.GetMetadata<StringMetadataValue>("foo", &gotten));
  ASSERT_NE(gotten, nullptr);
  EXPECT_EQ(gotten->str_, "bar");
}

TEST_F(MetadataTest, TestGetMutableAfterAddString) {
  MetadataContainer container;
  container.AddMetadata("foo", StringMetadataValue("bar"));
  StringMetadataValue* gotten = nullptr;
  EXPECT_TRUE(
      container.GetMutableMetadata<StringMetadataValue>("foo", &gotten));
  ASSERT_NE(gotten, nullptr);
  EXPECT_EQ(gotten->str_, "bar");
  gotten->str_ = "quaz";
  const StringMetadataValue* gotten_again = nullptr;
  EXPECT_TRUE(container.GetMetadata<StringMetadataValue>("foo", &gotten_again));
  ASSERT_NE(gotten_again, nullptr);
  EXPECT_EQ(gotten_again->str_, "quaz");
}

TEST_F(MetadataTest, TestGetNotThereString) {
  MetadataContainer container;
  container.AddMetadata("foo", StringMetadataValue("bar"));
  const StringMetadataValue* gotten = nullptr;
  EXPECT_FALSE(
      container.GetMetadata<StringMetadataValue>("not_there", &gotten));
}

TEST_F(MetadataTest, TestGetAfterAddProto) {
  MetadataContainer container;
  testing::EchoRequest request;
  request.set_message("bar");
  container.AddMetadata("foo",
                        ProtoMetadataValue<testing::EchoRequest>(&request));
  const ProtoMetadataValue<testing::EchoRequest>* gotten = nullptr;
  EXPECT_TRUE(container.GetMetadata<ProtoMetadataValue<testing::EchoRequest>>(
      "foo", &gotten));
  ASSERT_NE(gotten, nullptr);
  EXPECT_EQ(gotten->msg_->message(), "bar");
}

TEST_F(MetadataTest, TestGetMutableAfterAddProto) {
  MetadataContainer container;
  testing::EchoRequest request;
  request.set_message("bar");
  container.AddMetadata("foo",
                        ProtoMetadataValue<testing::EchoRequest>(&request));
  ProtoMetadataValue<testing::EchoRequest>* gotten = nullptr;
  EXPECT_TRUE(
      container.GetMutableMetadata<ProtoMetadataValue<testing::EchoRequest>>(
          "foo", &gotten));
  ASSERT_NE(gotten, nullptr);
  EXPECT_EQ(gotten->msg_->message(), "bar");
  gotten->msg_->set_message("reset");
  const ProtoMetadataValue<testing::EchoRequest>* gotten_again = nullptr;
  EXPECT_TRUE(container.GetMetadata<ProtoMetadataValue<testing::EchoRequest>>(
      "foo", &gotten_again));
  ASSERT_NE(gotten_again, nullptr);
  EXPECT_EQ(gotten_again->msg_->message(), "reset");
}

TEST_F(MetadataTest, TestGetNotThereProto) {
  MetadataContainer container;
  testing::EchoRequest request;
  request.set_message("bar");
  container.AddMetadata("foo",
                        ProtoMetadataValue<testing::EchoRequest>(&request));
  const ProtoMetadataValue<testing::EchoRequest>* gotten = nullptr;
  EXPECT_FALSE(container.GetMetadata<ProtoMetadataValue<testing::EchoRequest>>(
      "not_there", &gotten));
}

}  // namespace
}  // namespace grpc

int main(int argc, char** argv) {
  grpc::internal::GrpcLibraryInitializer init;
  init.summon();
  grpc::GrpcLibraryCodegen lib;

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
