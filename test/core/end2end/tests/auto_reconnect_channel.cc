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

#include "test/core/end2end/end2end_tests.h"

#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <grpc/byte_buffer.h>
#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/time.h>
#include "test/core/end2end/cq_verifier.h"

static void* tag(intptr_t t) { return (void*)t; }

static gpr_timespec n_seconds_from_now(int n) {
  return grpc_timeout_seconds_to_deadline(n);
}

static gpr_timespec five_seconds_from_now(void) {
  return n_seconds_from_now(5);
}

static void drain_cq(grpc_completion_queue* cq) {
  grpc_event ev;
  do {
    ev = grpc_completion_queue_next(cq, five_seconds_from_now(), NULL);
  } while (ev.type != GRPC_QUEUE_SHUTDOWN);
}

static void shutdown_client(grpc_end2end_test_fixture* f) {
  if (!f->client) return;
  grpc_channel_destroy(f->client);
  f->client = NULL;
}

static void do_one_request_and_shutdown_server(grpc_end2end_test_config config,
                                               grpc_end2end_test_fixture* f,
                                               cq_verifier* cqv) {
  grpc_call* c;
  grpc_call* s;
  grpc_op ops[6];
  grpc_op* op;
  grpc_metadata_array initial_metadata_recv;
  grpc_metadata_array trailing_metadata_recv;
  grpc_metadata_array request_metadata_recv;
  grpc_call_details call_details;
  grpc_status_code status;
  grpc_call_error error;
  grpc_slice details;
  int was_cancelled = 2;

  gpr_timespec deadline = five_seconds_from_now();
  c = grpc_channel_create_call(
      f->client, NULL, GRPC_PROPAGATE_DEFAULTS, f->cq,
      grpc_slice_from_static_string("/foo"),
      get_host_override_slice("foo.test.google.fr:1234", config), deadline,
      NULL);
  GPR_ASSERT(c);

  grpc_metadata_array_init(&initial_metadata_recv);
  grpc_metadata_array_init(&trailing_metadata_recv);
  grpc_metadata_array_init(&request_metadata_recv);
  grpc_call_details_init(&call_details);

  memset(ops, 0, sizeof(ops));
  op = ops;
  op->op = GRPC_OP_SEND_INITIAL_METADATA;
  op->data.send_initial_metadata.count = 0;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_RECV_INITIAL_METADATA;
  op->data.recv_initial_metadata.recv_initial_metadata = &initial_metadata_recv;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_RECV_STATUS_ON_CLIENT;
  op->data.recv_status_on_client.trailing_metadata = &trailing_metadata_recv;
  op->data.recv_status_on_client.status = &status;
  op->data.recv_status_on_client.status_details = &details;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  error = grpc_call_start_batch(c, ops, (size_t)(op - ops), tag(1), NULL);
  GPR_ASSERT(GRPC_CALL_OK == error);

  error =
      grpc_server_request_call(f->server, &s, &call_details,
                               &request_metadata_recv, f->cq, f->cq, tag(101));
  GPR_ASSERT(GRPC_CALL_OK == error);
  CQ_EXPECT_COMPLETION(cqv, tag(101), 1);
  cq_verify(cqv);

  memset(ops, 0, sizeof(ops));
  op = ops;
  op->op = GRPC_OP_SEND_INITIAL_METADATA;
  op->data.send_initial_metadata.count = 0;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_SEND_STATUS_FROM_SERVER;
  op->data.send_status_from_server.trailing_metadata_count = 0;
  op->data.send_status_from_server.status = GRPC_STATUS_UNIMPLEMENTED;
  grpc_slice status_details = grpc_slice_from_static_string("xyz");
  op->data.send_status_from_server.status_details = &status_details;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_RECV_CLOSE_ON_SERVER;
  op->data.recv_close_on_server.cancelled = &was_cancelled;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  error = grpc_call_start_batch(s, ops, (size_t)(op - ops), tag(102), NULL);
  GPR_ASSERT(GRPC_CALL_OK == error);

  CQ_EXPECT_COMPLETION(cqv, tag(102), 1);
  CQ_EXPECT_COMPLETION(cqv, tag(1), 1);
  cq_verify(cqv);

  GPR_ASSERT(status == GRPC_STATUS_UNIMPLEMENTED);
  GPR_ASSERT(0 == grpc_slice_str_cmp(details, "xyz"));
  GPR_ASSERT(0 == grpc_slice_str_cmp(call_details.method, "/foo"));
  validate_host_override_string("foo.test.google.fr:1234", call_details.host,
                                config);
  GPR_ASSERT(was_cancelled == 1);

  grpc_slice_unref(details);
  grpc_metadata_array_destroy(&initial_metadata_recv);
  grpc_metadata_array_destroy(&trailing_metadata_recv);
  grpc_metadata_array_destroy(&request_metadata_recv);
  grpc_call_details_destroy(&call_details);

  grpc_call_unref(c);
  grpc_call_unref(s);

  grpc_server_shutdown_and_notify(f->server, f->cq, tag(1000));
  CQ_EXPECT_COMPLETION(cqv, tag(1000), 1);
  cq_verify(cqv);
}

static void do_first_rpc_after_restarting_server(
    grpc_end2end_test_config config, grpc_end2end_test_fixture* f,
    cq_verifier* cqv) {
  grpc_call* c;
  grpc_op ops[6];
  grpc_op* op;
  grpc_metadata_array initial_metadata_recv;
  grpc_metadata_array trailing_metadata_recv;
  grpc_status_code status;
  grpc_call_error error;
  grpc_slice details;
  const char* error_string;

  gpr_timespec deadline = five_seconds_from_now();
  c = grpc_channel_create_call(
      f->client, NULL, GRPC_PROPAGATE_DEFAULTS, f->cq,
      grpc_slice_from_static_string("/foo"),
      get_host_override_slice("foo.test.google.fr:1234", config), deadline,
      NULL);
  GPR_ASSERT(c);

  grpc_metadata_array_init(&initial_metadata_recv);
  grpc_metadata_array_init(&trailing_metadata_recv);

  memset(ops, 0, sizeof(ops));
  op = ops;
  op->op = GRPC_OP_SEND_INITIAL_METADATA;
  op->data.send_initial_metadata.count = 0;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_RECV_INITIAL_METADATA;
  op->data.recv_initial_metadata.recv_initial_metadata = &initial_metadata_recv;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  op->op = GRPC_OP_RECV_STATUS_ON_CLIENT;
  op->data.recv_status_on_client.trailing_metadata = &trailing_metadata_recv;
  op->data.recv_status_on_client.status = &status;
  op->data.recv_status_on_client.status_details = &details;
  op->data.recv_status_on_client.error_string = &error_string;
  op->flags = 0;
  op->reserved = NULL;
  op++;
  error = grpc_call_start_batch(c, ops, (size_t)(op - ops), tag(1), NULL);
  GPR_ASSERT(GRPC_CALL_OK == error);
  CQ_EXPECT_COMPLETION(cqv, tag(1), 1);
  cq_verify(cqv);

  GPR_ASSERT(status == GRPC_STATUS_UNAVAILABLE);
  GPR_ASSERT(0 == grpc_slice_str_cmp(details, "Request did not leave client"));
  // Ensure that the internal error propagated the correct bit that signals that
  // this RPC never actually made it to the wire.
  GPR_ASSERT(nullptr !=
             strstr(error_string, "\"grpc_error_int_server_visibility\":2"));

  grpc_slice_unref(details);
  grpc_metadata_array_destroy(&initial_metadata_recv);
  grpc_metadata_array_destroy(&trailing_metadata_recv);

  grpc_call_unref(c);
}

static void auto_reconnect_channel_test(grpc_end2end_test_config config) {
  grpc_end2end_test_fixture f = config.create_fixture(NULL, NULL);
  cq_verifier* cqv = cq_verifier_create(f.cq);

  gpr_log(GPR_INFO, "Running test: %s/%s", "auto_reconnect_channel_test",
          config.name);

  config.init_client(&f, NULL);
  config.init_server(&f, NULL);

  do_one_request_and_shutdown_server(config, &f, cqv);

  /* restart the server */
  config.init_server(&f, NULL);

  /* TODO(roth): Remove this test after transparent retries are finished,
     because at that point we will not be able to test this error bit from
     end2end tests since the client_channel will consume the error during the
     retry.

     I assume that when transparent retried are in, they will implicitly test
     this functionality */
  do_first_rpc_after_restarting_server(config, &f, cqv);
  grpc_server_shutdown_and_notify(f.server, f.cq, tag(1001));
  CQ_EXPECT_COMPLETION(cqv, tag(1001), 1);
  cq_verify(cqv);

  cq_verifier_destroy(cqv);

  shutdown_client(&f);
  grpc_completion_queue_shutdown(f.cq);
  drain_cq(f.cq);
  grpc_completion_queue_destroy(f.cq);
  grpc_completion_queue_destroy(f.shutdown_cq);

  config.tear_down_data(&f);
}

void auto_reconnect_channel(grpc_end2end_test_config config) {
  GPR_ASSERT(config.feature_mask & FEATURE_MASK_SUPPORTS_DELAYED_CONNECTION);
  auto_reconnect_channel_test(config);
}

void auto_reconnect_channel_pre_init(void) {}
