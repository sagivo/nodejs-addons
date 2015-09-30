// hello.cc
#include <nan.h>
#include <node.h>
#include <iostream>

namespace demo {
  using namespace v8;

  void Method(const FunctionCallbackInfo<Value>& args) {
    v8::String::Utf8Value nameFromArgs(args[0]->ToString());
    std::string name = std::string(*nameFromArgs);
    std::string response = "hello " + name;

    args.GetReturnValue().Set(Nan::New(response).ToLocalChecked());
  }

  void init(Local<Object> exports) {
    NODE_SET_METHOD(exports, "hello", Method);
  }

  NODE_MODULE(addon, init)
}