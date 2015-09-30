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

This will create a simple c++ program. The first thing you can notice is we are importing bunch of `v8` libreries. As you know, v8 is the engine that powers node behind the sceans and we will talk more about better approaches to this later. 
`NODE_MODULE(addon, init)` is your entry point of this file. The addon will be under the `addon` namespace and will first call the `init` function. 

```cpp
void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "hello", Method);
}
```

here we are getting a `local` object as param and declaring a new method called `hello`. A [local](http://izs.me/v8-docs/classv8_1_1Local.html) type is managed by the v8 Engine. We bind the `hello` command to the `Method` method. 

```cpp
void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
}
```

This method will be called when the node will call `hello`. Here we are getting the current `Isolate` data and seting the return value to `world`.


## binding.gyp
Now let's create a file if instructions for the node-gyp library:


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

Here we can set the name and source of the addon. There're many other params [you can configure in this library](https://github.com/nodejs/node-gyp#the-bindinggyp-file). 

## Makefile

```
//Makefile

all: build
build: build_node
build_node: node-gyp rebuild
```

In the makefile we specify the build instructions. 

## JS code
Now all we have left is to do is to compile the code and import it to node. In order to compile a simple `npm install` can do. This will call `node-gyp rebuild` on our native code (we can also call it ourself manually) and output the result as a binary compiled node file to `build/Release/addon.node`. Now let's call it from our app:

```javascript
var addon = require('./build/Release/addon');
console.log(addon.hello()); // will print 'world'
```
And that's it! We just made our first native call from node. 

# Advance use

Node is moving fast, also the underlyne v8 engine. This is why it is best to get some use of the [nan](https://github.com/nodejs/nan) npm library. The idea behind is to support a unify wrapper on top of the v8 engine so your native calls will be v8 version egnostic. Most of the calls will have similar signiture and this way we won't need to change the compiled version any time there's a new one. 
Let's look on a slightly more advance sample. The first change we will need is to add this code to the `binding.gyp` file under `targets`:

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

That's it, this is the basic of working with c++ and node together using addons.  
Once again, The code can sample can be found at my [GitHub repo](https://github.com/sagivo/nodejs-addons).