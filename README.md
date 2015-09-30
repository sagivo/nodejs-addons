Sometimes you want to use your existing c++ code directly from your node app.&nbsp;

You can do it using Node.js addons, [nan](https://github.com/nodejs/nan) and [node-gym](https://github.com/nodejs/node-gyp) library. 

## C++ code
```cpp
// hello.cc
#include <node.h>

namespace demo {
  using v8::FunctionCallbackInfo;
  using v8::Isolate;
  using v8::Local;
  using v8::Object;
  using v8::String;
  using v8::Value;

  void Method(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
  }

  void init(Local<Object> exports) {
    NODE_SET_METHOD(exports, "hello", Method);
  }

  NODE_MODULE(addon, init)
}
```

This will create a simple c++ program. The first thing you can notice is that we are importing bunch of `v8` libreries. As you know, v8 is the engine that powers node behind the sceans and we will talk more about better approaches to this later.  
`NODE_MODULE(addon, init)` is your entry point. The addon will be under the `addon` namespace and will first call the `init` function. 

```cpp
void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "hello", Method);
}
```

here we are getting a `Local` object as param and declaring a new method called `hello`.  
A [Local](http://izs.me/v8-docs/classv8_1_1Local.html) type is managed by the v8 Engine.  
Next, we bind the `hello` command to the `Method` method:  

```cpp
void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
}
```

This method will be called when the node will call `hello`. Here we are getting the current `Isolate` data and setting the return value to be `world`.


## binding.gyp
Next, let's create a `binding.gyp` file with instructions to be used by the node-gyp library:


```json
{
  "targets": [
    {
      "target_name": "addon",
      "sources": [ "hello.cc" ]
    }
  ]
}
```

Here we can set the name and source of the addon we have.  
There are many other params [you can configure in this file](https://github.com/nodejs/node-gyp#the-bindinggyp-file). 

## Makefile
Create a `Makefile` file that specify the build instructions:  

```
all: build
build: build_node
build_node: node-gyp rebuild
```

## JS code
Now, all we have left is to do is to compile the code and import it to node.  
We can just use `npm install` to do it. This will call `node-gyp rebuild` on our native code (we can also call it ourself manually) and output the result as a binary compiled node file to `build/Release/addon.node`.  
The next step is to import it from our node app:

```javascript
var addon = require('./build/Release/addon');
console.log(addon.hello()); // will print 'world'
```
And that's it! We just made our first native call from node. 

# Advance use

Node is moving fast, also the underline v8 engine. This is why it is best to get some use of the [nan](https://github.com/nodejs/nan) npm library. The idea behind is to support a unify wrapper on top of the v8 engine so your native calls will be v8 version agnostic.  
Most of the calls will have similar signiture and this way we won't need to change the compiled version any time there's a new one.  
Let's look on a slightly more advance sample- the first change we will need is to add this code to the `binding.gyp` file under `targets`:

```json
"include_dirs" : ["<!(node -e \"require('nan')\")"]
```

After installing nan, we need to import it by adding this line at the head of `hello.cc`:  

```cpp
#include <nan.h>
```

Now, let's change our `Method` function a bit:

```cpp
void Method(const FunctionCallbackInfo<Value>& args) {
  v8::String::Utf8Value nameFromArgs(args[0]->ToString());
  std::string name = std::string(*nameFromArgs);
  std::string response = "hello " + name;

  args.GetReturnValue().Set(Nan::New(response).ToLocalChecked());
}
```

This time we will send a param from node.js to the c++ code. The way we do it is using the `args` in the functions' signiture.  
Notice that this time we're using [`Nan::New`](https://github.com/nodejs/nan/blob/master/doc/new.md#nannew) that returns `v8::Local` object.

From the node side, let's call it with a parameter this time:  

```javascript
var addon = require('./build/Release/addon');
console.log(addon.hello('Sam')); // will print "Hello Sam"
```

# Memory leaks 
Yes, once we write in c++ we can forget about gc and need to do everything ourselves. Nan (or v8) can handle it for us in case we declare new object of type `Local` but in case we allocate new objects we have to remember to free the memory. 

## Buffers
A good example of bug that may happen is the use of buffer. Let's assume you have a function that generate a buffer and returns it to node:  

```cpp
void GetBuffer(const FunctionCallbackInfo<Value>& args) {
  char *data;
  size_t length;
  GetSomeBufferData(data, length);
  MaybeLocal<Object> buffer = Nan::NewBuffer(data, length);
  args.GetReturnValue().Set(buffer.ToLocalChecked());
}
```

This code can create a memory leak as explained [here](https://github.com/nodejs/nan/blob/master/doc/buffers.md#api_nan_new_buffer):
> Note that when creating a Buffer using Nan::NewBuffer() and an existing char*, it is assumed that the ownership of the pointer is being transferred to the new Buffer for management. When a node::Buffer instance is garbage collected and a FreeCallback has not been specified, data will be disposed of via a call to free(). You must not free the memory space manually once you have created a Buffer in this way.  

Using `Nan::NewBuffer` will not free the char* from memory so you will have to do it yourself. The problem is that by adding `delete []data` you're getting into a race condition - the data can be deleted from the buffer before returning to node.  
The solution in this case will be to use [`Nan::CopyBuffer`](https://github.com/nodejs/nan/blob/master/doc/buffers.md#nancopybuffer) instead:  

```cpp
void GetBuffer(const FunctionCallbackInfo<Value>& args) {
  char *data;
  size_t length;
  GetSomeBufferData(data, length);
  MaybeLocal<Object> buffer = Nan::CopyBuffer(data, length);
  delete []data;
  args.GetReturnValue().Set(buffer.ToLocalChecked());
}
```

It might be a bit slower since we call `memcpy()` to create a new instance of the buffer but this way you can remove your `char*` and avoid memory leaks or race conditions.  

## Upgrading to onde 4.x
We at [Brewster](https://brewster.com) needed to upgrade an old codebase to a newer node version.  
This upgrade was not as trivial as we wanted since the new 4.x node uses the new v8 engine that introduced a lot of [api changes](https://docs.google.com/document/d/1g8JFi8T_oAE_7uAri7Njtig7fKaPDfotU6huOa1alds/edit):  
- Introduction of `MaybeLocal<>` and `Maybe<>` APIs
- Force explicit Isolate* parameter on all external APIs
- Deprecate unused Map/Set FromArray factory methods
- Deprecate `v8::Handle`
- `NanNew` -> `Nan::New`

And [much more changes](https://nodesource.com/blog/cpp-addons-for-nodejs-v4). Also, we needed to upgrade our gcc compiler to version 4.8 and up in order to compile the new v8. 
I suggest reading through the v8 and nan docs in order to see what are the main point you need to address once upgrading. 


That's it, this is the basic of working with c++ and node together using addons.  
Once again, The code can sample can be found at my [GitHub repo](https://github.com/sagivo/nodejs-addons).