#pragma once
#include "math_service.pb.h"
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

class MathServiceImpl : public ::google::protobuf::Service
{
public:
    void Add(::google::protobuf::RpcController* controller,
             const ::math::AddRequest* request,
             ::math::AddResponse* response,
             ::google::protobuf::Closure* done)
    {
        response->set_sum(request->a() + request->b());
        if(done) done->Run();
    }

    void Sub(::google::protobuf::RpcController* controller,
             const ::math::SubRequest* request,
             ::math::SubResponse* response,
             ::google::protobuf::Closure* done)
    {
        response->set_diff(request->a() - request->b());
        if(done) done->Run();
    }

    const ::google::protobuf::ServiceDescriptor* GetDescriptor()
    {
	    const ::google::protobuf::FileDescriptor* file_desc = ::math::AddRequest::descriptor()->file();
	    return file_desc->FindServiceByName("MathService");
    }

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                    ::google::protobuf::RpcController* controller,
                    const ::google::protobuf::Message* request,
                    ::google::protobuf::Message* response,
                    ::google::protobuf::Closure* done)
    {
        if (method->name() == "Add")
            Add(controller, dynamic_cast<const math::AddRequest*>(request), dynamic_cast<math::AddResponse*>(response), done);
        else if (method->name() == "Sub")
            Sub(controller, dynamic_cast<const math::SubRequest*>(request), dynamic_cast<math::SubResponse*>(response), done);
    }

    const ::google::protobuf::Message& GetRequestPrototype(const ::google::protobuf::MethodDescriptor*) const
    {
        static math::AddRequest req;
        return req;
    }

    const ::google::protobuf::Message& GetResponsePrototype(const ::google::protobuf::MethodDescriptor*) const
    {
        static math::AddResponse rsp;
        return rsp;
    }
};
