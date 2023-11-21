/*
 *
 * Copyright 2015 gRPC authors.
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

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;

using namespace std::chrono_literals;

class CallDataBase {
  protected:
    virtual void WaitForRequest() = 0;
    virtual void HandleRequest() = 0;

  public:
    virtual void Proceed() = 0;
    CallDataBase() {}
    virtual ~CallDataBase() {}
};

template <class RequestType, class ReplyType>
class CallDataT : CallDataBase {
  protected:
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;

    Greeter::AsyncService* service_;
    ServerCompletionQueue* completionQueue_;
    RequestType request_;
    ReplyType reply_;
    ServerAsyncResponseWriter<ReplyType> responder_;
    ServerContext serverContext_;

    // When we handle a request of this type, we need to tell
    // the completion queue to wait for new requests of the same type.
    virtual void AddNextToCompletionQueue() = 0;

  public:
    CallDataT(Greeter::AsyncService* service,
              ServerCompletionQueue* completionQueue)
        : status_(CREATE),
          service_(service),
          completionQueue_(completionQueue),
          responder_(&serverContext_) {}
    virtual ~CallDataT() {}

  public:
    virtual void Proceed() override {
        if (status_ == CREATE) {
            std::cout << "C: " << this << std::endl;
            status_ = PROCESS;
            WaitForRequest();
        } else if (status_ == PROCESS) {
            AddNextToCompletionQueue();
            HandleRequest();
        } else {
            // We're done! Self-destruct!
            if (status_ != FINISH) {
                // Log some error message
            }
            delete this;
        }
    }
};

class CallDataHello : CallDataT<HelloRequest, HelloReply> {
  public:
    CallDataHello(Greeter::AsyncService* service,
                  ServerCompletionQueue* completionQueue)
        : CallDataT(service, completionQueue) {
        Proceed();
    }

  protected:
    virtual void AddNextToCompletionQueue() override {
        new CallDataHello(service_, completionQueue_);
    }

    virtual void WaitForRequest() override {
        service_->RequestSayHello(&serverContext_, &request_, &responder_,
                                  completionQueue_, completionQueue_, this);
    }

    virtual void HandleRequest() override {
        std::thread t([this]() {
            std::this_thread::sleep_for(5000ms);
            reply_.set_message(std::string("Hello ") + request_.name());
            status_ = FINISH;
            responder_.Finish(reply_, Status::OK, this);
        });
        t.detach();
    }
};

class CallDataHelloAgain : CallDataT<HelloRequest, HelloReply> {
  public:
    CallDataHelloAgain(Greeter::AsyncService* service,
                       ServerCompletionQueue* completionQueue)
        : CallDataT(service, completionQueue) {
        Proceed();
    }

  protected:
    virtual void AddNextToCompletionQueue() override {
        new CallDataHelloAgain(service_, completionQueue_);
    }

    virtual void WaitForRequest() override {
        service_->RequestSayHelloAgain(&serverContext_, &request_, &responder_,
                                       completionQueue_, completionQueue_,
                                       this);
    }

    virtual void HandleRequest() override {
        reply_.set_message(std::string("Hello again ") + request_.name());
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);
    }
};

class ServerImpl final {
  public:
    ~ServerImpl() {
        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();
    }

    // There is no shutdown handling in this code.
    void Run(uint16_t port) {
        std::string server_address = "0.0.0.0:" + std::to_string(port);

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address,
                                 grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate
        // with clients. In this case it corresponds to an *asynchronous*
        // service.
        builder.RegisterService(&service_);
        // Get hold of the completion queue used for the asynchronous
        // communication with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        // Proceed to the server's main loop.
        HandleRpcs();
    }

  private:
    // Class encompasing the state and logic needed to serve a request.

    // This can be run in multiple threads if needed.
    void HandleRpcs() {
        // Spawn a new CallData instance to serve new clients.
        new CallDataHello(&service_, cq_.get());
        new CallDataHelloAgain(&service_, cq_.get());
        void* tag;  // uniquely identifies a request.
        bool ok;
        while (true) {
            // Block waiting to read the next event from the completion queue.
            // The event is uniquely identified by its tag, which in this case
            // is the memory address of a CallData instance. The return value of
            // Next should always be checked. This return value tells us whether
            // there is any kind of event or cq_ is shutting down.
            bool ret = cq_->Next(&tag, &ok);
            if (ok == false || ret == false) {
                return;
            }
            static_cast<CallDataBase*>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    Greeter::AsyncService service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv) {
    ServerImpl server;
    server.Run(50051);

    return 0;
}