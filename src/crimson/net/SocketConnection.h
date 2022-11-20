// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2017 Red Hat, Inc
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#pragma once

#include <seastar/core/sharded.hh>

#include "msg/Policy.h"
#include "crimson/common/throttle.h"
#include "crimson/net/Connection.h"
#include "crimson/net/Socket.h"

namespace crimson::net {

class Protocol;
class SocketMessenger;
class SocketConnection;
using SocketConnectionRef = seastar::shared_ptr<SocketConnection>;

#ifdef UNIT_TESTS_BUILT
class Interceptor;
#endif

class SocketConnection : public Connection {
  const seastar::shard_id core;
  SocketMessenger& messenger;
  std::unique_ptr<Protocol> protocol;

  entity_name_t peer_name = {0, entity_name_t::NEW};

  entity_addr_t peer_addr;

  // which of the peer_addrs we're connecting to (as client)
  // or should reconnect to (as peer)
  entity_addr_t target_addr;

  clock_t::time_point last_keepalive;

  clock_t::time_point last_keepalive_ack;

  uint64_t features = 0;

  ceph::net::Policy<crimson::common::Throttle> policy;

  /// the seq num of the last transmitted message
  seq_num_t out_seq = 0;
  /// the seq num of the last received message
  seq_num_t in_seq = 0;
  /// update the seq num of last received message
  /// @returns true if the @c seq is valid, and @c in_seq is updated,
  ///          false otherwise.
  bool update_rx_seq(seq_num_t seq);

  // messages to be resent after connection gets reset
  std::deque<MessageURef> out_q;
  // messages sent, but not yet acked by peer
  std::deque<MessageURef> sent;

  uint64_t peer_global_id = 0;

  std::unique_ptr<user_private_t> user_private;

  seastar::shard_id shard_id() const;

 public:
  SocketConnection(SocketMessenger& messenger,
                   ChainedDispatchers& dispatchers);

  ~SocketConnection() override;

  const entity_name_t &get_peer_name() const override {
    return peer_name;
  }

  const entity_addr_t &get_peer_addr() const override {
    return peer_addr;
  }

  const entity_addr_t &get_peer_socket_addr() const override {
    return target_addr;
  }

  uint64_t get_features() const override {
    return features;
  }

  bool is_connected() const override;

  seastar::future<> send(MessageURef msg) override;

  seastar::future<> keepalive() override;

  clock_t::time_point get_last_keepalive() const override {
    return last_keepalive;
  }

  clock_t::time_point get_last_keepalive_ack() const override {
    return last_keepalive_ack;
  }

  void set_last_keepalive_ack(clock_t::time_point when) override {
    last_keepalive_ack = when;
  }

  void mark_down() override;

  bool has_user_private() const override {
    return user_private != nullptr;
  }

  user_private_t &get_user_private() override {
    assert(has_user_private());
    return *user_private;
  }

  void set_user_private(std::unique_ptr<user_private_t> new_user_private) override {
    assert(!has_user_private());
    user_private = std::move(new_user_private);
  }

  void print(std::ostream& out) const override;

 public:
  void set_peer_type(entity_type_t peer_type) {
    // it is not allowed to assign an unknown value when the current
    // value is known
    assert(!(peer_type == 0 &&
             peer_name.type() != 0));
    // it is not allowed to assign a different known value when the
    // current value is also known.
    assert(!(peer_type != 0 &&
             peer_name.type() != 0 &&
             peer_type != peer_name.type()));
    peer_name._type = peer_type;
  }

  void set_peer_id(int64_t peer_id) {
    // it is not allowed to assign an unknown value when the current
    // value is known
    assert(!(peer_id == entity_name_t::NEW &&
             peer_name.num() != entity_name_t::NEW));
    // it is not allowed to assign a different known value when the
    // current value is also known.
    assert(!(peer_id != entity_name_t::NEW &&
             peer_name.num() != entity_name_t::NEW &&
             peer_id != peer_name.num()));
    peer_name._num = peer_id;
  }

  void set_peer_name(entity_name_t name) {
    set_peer_type(name.type());
    set_peer_id(name.num());
  }

  void set_last_keepalive(clock_t::time_point when) {
    last_keepalive = when;
  }

  void set_features(uint64_t f) {
    features = f;
  }

  /// start a handshake from the client's perspective,
  /// only call when SocketConnection first construct
  void start_connect(const entity_addr_t& peer_addr,
                     const entity_name_t& peer_name);
  /// start a handshake from the server's perspective,
  /// only call when SocketConnection first construct
  void start_accept(SocketRef&& socket,
                    const entity_addr_t& peer_addr);

  seastar::future<> close_clean(bool dispatch_reset);

  bool is_server_side() const {
    return policy.server;
  }

  bool is_lossy() const {
    return policy.lossy;
  }

  seastar::socket_address get_local_address() const;

  SocketMessenger &get_messenger() const {
    return messenger;
  }

#ifdef UNIT_TESTS_BUILT
  bool is_closed_clean() const override;

  bool is_closed() const override;

  bool peer_wins() const override;

  Interceptor *interceptor = nullptr;
#else
  bool peer_wins() const;
#endif

  friend class Protocol;
  friend class ProtocolV2;
};

} // namespace crimson::net
