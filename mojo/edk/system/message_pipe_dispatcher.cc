// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_dispatcher.h"

#include "base/logging.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_endpoint_id.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/local_message_pipe_endpoint.h"
#include "mojo/edk/system/memory.h"
#include "mojo/edk/system/message_pipe.h"
#include "mojo/edk/system/options_validation.h"
#include "mojo/edk/system/proxy_message_pipe_endpoint.h"

namespace mojo {
namespace system {

namespace {

const unsigned kInvalidPort = static_cast<unsigned>(-1);

struct SerializedMessagePipeDispatcher {
  // This is the endpoint ID on the receiving side, and should be a "remote ID".
  // (The receiving side should have already have an endpoint attached and run
  // via the |Channel|s. This endpoint will have both IDs assigned, so this ID
  // is only needed to associated that endpoint with a particular dispatcher.)
  ChannelEndpointId receiver_endpoint_id;
};

}  // namespace

// MessagePipeDispatcher -------------------------------------------------------

// static
const MojoCreateMessagePipeOptions
    MessagePipeDispatcher::kDefaultCreateOptions = {
        static_cast<uint32_t>(sizeof(MojoCreateMessagePipeOptions)),
        MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE};

MessagePipeDispatcher::MessagePipeDispatcher(
    const MojoCreateMessagePipeOptions& /*validated_options*/)
    : port_(kInvalidPort) {
}

// static
MojoResult MessagePipeDispatcher::ValidateCreateOptions(
    UserPointer<const MojoCreateMessagePipeOptions> in_options,
    MojoCreateMessagePipeOptions* out_options) {
  const MojoCreateMessagePipeOptionsFlags kKnownFlags =
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

  *out_options = kDefaultCreateOptions;
  if (in_options.IsNull())
    return MOJO_RESULT_OK;

  UserOptionsReader<MojoCreateMessagePipeOptions> reader(in_options);
  if (!reader.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateMessagePipeOptions, flags, reader))
    return MOJO_RESULT_OK;
  if ((reader.options().flags & ~kKnownFlags))
    return MOJO_RESULT_UNIMPLEMENTED;
  out_options->flags = reader.options().flags;

  // Checks for fields beyond |flags|:

  // (Nothing here yet.)

  return MOJO_RESULT_OK;
}

void MessagePipeDispatcher::Init(scoped_refptr<MessagePipe> message_pipe,
                                 unsigned port) {
  DCHECK(message_pipe.get());
  DCHECK(port == 0 || port == 1);

  message_pipe_ = message_pipe;
  port_ = port;
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return kTypeMessagePipe;
}

// static
scoped_refptr<MessagePipeDispatcher>
MessagePipeDispatcher::CreateRemoteMessagePipe(
    scoped_refptr<ChannelEndpoint>* channel_endpoint) {
  scoped_refptr<MessagePipe> message_pipe(
      MessagePipe::CreateLocalProxy(channel_endpoint));
  scoped_refptr<MessagePipeDispatcher> dispatcher(
      new MessagePipeDispatcher(MessagePipeDispatcher::kDefaultCreateOptions));
  dispatcher->Init(message_pipe, 0);
  return dispatcher;
}

// static
scoped_refptr<MessagePipeDispatcher> MessagePipeDispatcher::Deserialize(
    Channel* channel,
    const void* source,
    size_t size) {
  if (size != sizeof(SerializedMessagePipeDispatcher)) {
    LOG(ERROR) << "Invalid serialized message pipe dispatcher";
    return scoped_refptr<MessagePipeDispatcher>();
  }

  const SerializedMessagePipeDispatcher* s =
      static_cast<const SerializedMessagePipeDispatcher*>(source);
  scoped_refptr<MessagePipe> message_pipe =
      channel->PassIncomingMessagePipe(s->receiver_endpoint_id);
  if (!message_pipe.get()) {
    LOG(ERROR) << "Failed to deserialize message pipe dispatcher (ID = "
               << s->receiver_endpoint_id << ")";
    return scoped_refptr<MessagePipeDispatcher>();
  }

  DVLOG(2) << "Deserializing message pipe dispatcher (new local ID = "
           << s->receiver_endpoint_id << ")";
  scoped_refptr<MessagePipeDispatcher> dispatcher(
      new MessagePipeDispatcher(MessagePipeDispatcher::kDefaultCreateOptions));
  dispatcher->Init(message_pipe, 0);
  return dispatcher;
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!message_pipe_.get());
}

MessagePipe* MessagePipeDispatcher::GetMessagePipeNoLock() const {
  lock().AssertAcquired();
  return message_pipe_.get();
}

unsigned MessagePipeDispatcher::GetPortNoLock() const {
  lock().AssertAcquired();
  return port_;
}

void MessagePipeDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  message_pipe_->CancelAllWaiters(port_);
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  message_pipe_->Close(port_);
  message_pipe_ = nullptr;
  port_ = kInvalidPort;
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  // TODO(vtl): Currently, there are no options, so we just use
  // |kDefaultCreateOptions|. Eventually, we'll have to duplicate the options
  // too.
  scoped_refptr<MessagePipeDispatcher> rv =
      new MessagePipeDispatcher(kDefaultCreateOptions);
  rv->Init(message_pipe_, port_);
  message_pipe_ = nullptr;
  port_ = kInvalidPort;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    UserPointer<const void> bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports ||
         (transports->size() > 0 &&
          transports->size() <= GetConfiguration().max_message_num_handles));

  lock().AssertAcquired();

  if (num_bytes > GetConfiguration().max_message_num_bytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return message_pipe_->WriteMessage(port_, bytes, num_bytes, transports,
                                     flags);
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    UserPointer<void> bytes,
    UserPointer<uint32_t> num_bytes,
    DispatcherVector* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();
  return message_pipe_->ReadMessage(port_, bytes, num_bytes, dispatchers,
                                    num_dispatchers, flags);
}

HandleSignalsState MessagePipeDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();
  return message_pipe_->GetHandleSignalsState(port_);
}

MojoResult MessagePipeDispatcher::AddWaiterImplNoLock(
    Waiter* waiter,
    MojoHandleSignals signals,
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  return message_pipe_->AddWaiter(port_, waiter, signals, context,
                                  signals_state);
}

void MessagePipeDispatcher::RemoveWaiterImplNoLock(
    Waiter* waiter,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  message_pipe_->RemoveWaiter(port_, waiter, signals_state);
}

void MessagePipeDispatcher::StartSerializeImplNoLock(
    Channel* /*channel*/,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  *max_size = sizeof(SerializedMessagePipeDispatcher);
  *max_platform_handles = 0;
}

bool MessagePipeDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* /*platform_handles*/) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  SerializedMessagePipeDispatcher* s =
      static_cast<SerializedMessagePipeDispatcher*>(destination);

  // Convert the local endpoint to a proxy endpoint (moving the message queue)
  // and attach it to the channel.
  s->receiver_endpoint_id = channel->AttachAndRunEndpoint(
      message_pipe_->ConvertLocalToProxy(port_), false);
  DVLOG(2) << "Serializing message pipe dispatcher (remote ID = "
           << s->receiver_endpoint_id << ")";

  message_pipe_ = nullptr;
  port_ = kInvalidPort;

  *actual_size = sizeof(SerializedMessagePipeDispatcher);
  return true;
}

// MessagePipeDispatcherTransport ----------------------------------------------

MessagePipeDispatcherTransport::MessagePipeDispatcherTransport(
    DispatcherTransport transport)
    : DispatcherTransport(transport) {
  DCHECK_EQ(message_pipe_dispatcher()->GetType(), Dispatcher::kTypeMessagePipe);
}

}  // namespace system
}  // namespace mojo
