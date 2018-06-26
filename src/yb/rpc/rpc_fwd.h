//
// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//
//

#ifndef YB_RPC_RPC_FWD_H
#define YB_RPC_RPC_FWD_H

#include <chrono>
#include <functional>

#include <boost/version.hpp>

#include "yb/gutil/ref_counted.h"

#include "yb/rpc/rpc_introspection.pb.h"

#include "yb/util/strongly_typed_bool.h"

namespace boost {
namespace asio {

#if BOOST_VERSION >= 106600
class io_context;
typedef io_context io_service;
#else
class io_service;
#endif

} // namespace asio
} // namespace boost

namespace yb {
namespace rpc {

class Acceptor;
class AcceptorPool;
class ConnectionContext;
class GrowableBufferAllocator;
class Messenger;
class MessengerBuilder;
class Proxy;
class ProxyCache;
class Reactor;
class ReactorTask;
class RpcConnectionPB;
class RpcContext;
class RpcController;
class RpcService;
class Rpcs;
class Protocol;
class ServicePoolImpl;
class Stream;
class ThreadPool;

struct RpcMethodMetrics;

class RpcCommand;
typedef std::shared_ptr<RpcCommand> RpcCommandPtr;

class Connection;
class ConnectionContext;
typedef std::shared_ptr<Connection> ConnectionPtr;
typedef std::weak_ptr<Connection> ConnectionWeakPtr;

class InboundCall;
typedef std::shared_ptr<InboundCall> InboundCallPtr;

class OutboundCall;
typedef std::shared_ptr<OutboundCall> OutboundCallPtr;

class OutboundData;
typedef std::shared_ptr<OutboundData> OutboundDataPtr;

class ServerEventList;
typedef std::shared_ptr<ServerEventList> ServerEventListPtr;

class ServiceIf;
typedef std::shared_ptr<ServiceIf> ServiceIfPtr;

class ErrorStatusPB;

typedef boost::asio::io_service IoService;

// SteadyTimePoint is something like MonoTime, but 3rd party libraries know it and don't know about
// our private MonoTime.
typedef std::chrono::steady_clock::time_point SteadyTimePoint;

class ConnectionContextFactory;
typedef std::shared_ptr<ConnectionContextFactory> ConnectionContextFactoryPtr;

class StreamFactory;
typedef std::shared_ptr<StreamFactory> StreamFactoryPtr;

YB_STRONGLY_TYPED_BOOL(ReadBufferFull);

} // namespace rpc
} // namespace yb

#endif // YB_RPC_RPC_FWD_H
