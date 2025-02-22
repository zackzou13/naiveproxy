// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef QUICHE_QUIC_TOOLS_QUIC_CLIENT_H_
#define QUICHE_QUIC_TOOLS_QUIC_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "quiche/quic/core/http/quic_client_push_promise_index.h"
#include "quiche/quic/core/http/quic_spdy_client_session.h"
#include "quiche/quic/core/quic_config.h"
#include "quiche/quic/core/quic_packet_reader.h"
#include "quiche/quic/platform/api/quic_epoll.h"
#include "quiche/quic/tools/quic_client_epoll_network_helper.h"
#include "quiche/quic/tools/quic_name_lookup.h"
#include "quiche/quic/tools/quic_spdy_client_base.h"

namespace quic {

class QuicServerId;

namespace test {
class QuicClientPeer;
}  // namespace test

class QuicClient : public QuicSpdyClientBase {
 public:
  // These will create their own QuicClientEpollNetworkHelper.
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             QuicEpollServer* epoll_server,
             std::unique_ptr<ProofVerifier> proof_verifier);
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             QuicEpollServer* epoll_server,
             std::unique_ptr<ProofVerifier> proof_verifier,
             std::unique_ptr<SessionCache> session_cache);
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             const QuicConfig& config, QuicEpollServer* epoll_server,
             std::unique_ptr<ProofVerifier> proof_verifier,
             std::unique_ptr<SessionCache> session_cache);
  // This will take ownership of a passed in network primitive.
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             QuicEpollServer* epoll_server,
             std::unique_ptr<QuicClientEpollNetworkHelper> network_helper,
             std::unique_ptr<ProofVerifier> proof_verifier);
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             const QuicConfig& config, QuicEpollServer* epoll_server,
             std::unique_ptr<QuicClientEpollNetworkHelper> network_helper,
             std::unique_ptr<ProofVerifier> proof_verifier);
  QuicClient(QuicSocketAddress server_address, const QuicServerId& server_id,
             const ParsedQuicVersionVector& supported_versions,
             const QuicConfig& config, QuicEpollServer* epoll_server,
             std::unique_ptr<QuicClientEpollNetworkHelper> network_helper,
             std::unique_ptr<ProofVerifier> proof_verifier,
             std::unique_ptr<SessionCache> session_cache);
  QuicClient(const QuicClient&) = delete;
  QuicClient& operator=(const QuicClient&) = delete;

  ~QuicClient() override;

  std::unique_ptr<QuicSession> CreateQuicClientSession(
      const ParsedQuicVersionVector& supported_versions,
      QuicConnection* connection) override;

  // Exposed for the quic client test.
  int GetLatestFD() const { return epoll_network_helper()->GetLatestFD(); }

  QuicClientEpollNetworkHelper* epoll_network_helper();
  const QuicClientEpollNetworkHelper* epoll_network_helper() const;
};

}  // namespace quic

#endif  // QUICHE_QUIC_TOOLS_QUIC_CLIENT_H_
