// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_HTTP_SERVER_HTTP_SERVER_FACTORY_IMPL_H_
#define SERVICES_HTTP_SERVER_HTTP_SERVER_FACTORY_IMPL_H_

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/http_server/public/interfaces/http_server_factory.mojom.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"

namespace mojo {
class ApplicationImpl;
}  // namespace mojo

namespace http_server {

class HttpServerImpl;

class HttpServerFactoryImpl : public HttpServerFactory {
 public:
  HttpServerFactoryImpl(mojo::ApplicationImpl* app);
  ~HttpServerFactoryImpl() override;

  void AddBinding(mojo::InterfaceRequest<HttpServerFactory> request);

  // The server impl calls back here when the last of its clients disconnects.
  void DeleteServer(HttpServerImpl* server,
                    mojo::NetAddress* requested_address);

 private:
  // Servers that were requested with non-zero port are stored in a map and we
  // allow multiple clients to connect to them. We use a pair of the IP address
  // and the port as a lookup key.
  typedef std::pair<std::vector<uint8_t>, uint16_t> ServerKey;

  ServerKey GetServerKey(mojo::NetAddress* local_address);

  // HttpServerFactory:
  void CreateHttpServer(mojo::InterfaceRequest<HttpServer> server_request,
                        mojo::NetAddressPtr local_address) override;

  mojo::WeakBindingSet<HttpServerFactory> bindings_;

  std::map<ServerKey, HttpServerImpl*> port_indicated_servers_;

  // Servers that were requested with port = 0 (pick any) are not available to
  // other clients.
  std::set<HttpServerImpl*> port_any_servers_;

  mojo::ApplicationImpl* app_;
};

}  // namespace http_server

#endif  // SERVICES_HTTP_SERVER_HTTP_SERVER_FACTORY_IMPL_H_
