# 集成V8

V8是C++库，用于解析JavaScript 脚本。宏观来看，他的原理部分可以通过学习[C++ lispy](https://github.com/akerdi/lval) / [JavaScript lispy](https://github.com/akerdi/jslispy)，其内部会对Ast之后的代码生成字节码，部分优化后生成机器码，这部分知识待日后分解源码。本文讲解学习V8 过程。

## 1.环境

```
Platform: VSCode(Windows) + WSL(Ubuntu)
Language: C++
```

## 2.编译V8

首先从V8 官网查看[编译文档](https://v8.dev/docs/build), 他要求下载相关工具: Follow the instructions in our guide on [checking out the V8 source code](https://v8.dev/docs/source-code).

### 2.1下载depot_tools

首先下载depot_tools:

    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

将depot_tools 作为工具目录:

    export PATH=/path/to/depot_tools:$PATH

对于我，我将其放到了 /home/aker/cpp_test/v8/depot_tools, 所以我的语句是: `export PATH=/home/aker/cpp_test/v8/depot_tools:$PATH`.

关于gclient 工具需要科学上网获取相关工具的问题，可以[参考这篇文章](https://zinglix.xyz/2020/04/18/wsl2-proxy/), 只为了学习为目的哈。

我这里的做法是: 首先使用curl https://google.com 是能联网的，然后gclient 调用时就发生错误。这时设置git:

    git config --global http.proxy $IP:8001
    git config --global https.proxy $IP:8001

这里的$IP 值是: `cat /etc/resolv.conf` 中的namespace 项目。(注意http 和socks5 端口对应)

接下来下载相关工具: `gclient`.

gclient 完成如图:

![depot_tools_list](./assets/depot_tools_list.jpg)

### 2.2获取源码

    mkdir v8 && cd v8
    fetch v8

切到对应的版本:

    git branch -a && git checkout -b branch-heads/12.0 remotes/branch-heads/12.0

### 2.3编译d8

d8 是V8引擎用于展示V8功能的executable app，编译他是为了尝试功能是否得以验证，并体验Javascript引擎:

    tools/dev/gm.py x64.release

编译时间很长，如果想直接集成V8，则跳过此章节。

完成后可以查看体验d8:

![exec_d8](./assets/exec_d8.jpg)

### 2.4编译静态库

查看可以指定编译的平台:

    alias v8gen=./tools/dev/v8gen.py
    v8gen list

![v8_gen_list](./assets/v8_gen_list.jpg)

比如我们要编译x64.release 架构，则执行:

    ninja -C out/x64.release

最终结果在:

![compiled_static_libraries](./assets/compiled_static_libraries.jpg)

## 3.集成

这里展示CMake 集成过程:

CMakeLists.txt
```
cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 17)

project(app)

# -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX
# 解决编译时报错: `Embedder-vs-V8 build configuration mismatch.`
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX")

set(SOURCE_FILES ./main.cpp)

add_executable(app ${SOURCE_FILES})

# 加载静态库
target_include_directories(app PUBLIC /home/aker/cpp_test/v8/v8_git/v8/include)
target_link_directories(app PUBLIC /home/aker/cpp_test/v8/v8_git/v8/out.gn/x64.release.sample/obj)
target_link_libraries(app -lpthread -lv8_monolith -lv8_libbase -lv8_libplatform)
```

重点是指定将头文件和静态库加入到项目中。(项目外可能导致编译问题)。

## 4.测试

测试使用V8自带的3个例子: /samples/[hello-world.cc|shell.cc|process.cc].

## 5.学习使用

刚开始入门学习hello-world.cc 的例子，他指导如何V8使用的流程。

当大致能默写hello-world.cc(了解下大致流程，并且能手写默写)，就可以开始真正的学习了。

### 5.1.了解基础

+ An isolate is a VM instance with its own heap.
    - 一个isolate就是VM实例，他有独立的堆数据。
+ A local handle is a pointer to an object. All V8 objects are accessed using handles. They are necessary because of the way the V8 garbage collector works.
    - 一个local handle 代表一个对象包含着一个指针。所有V8对象都通过handle来获取。这是非常有必要的，因为这样数据就可以使用V8的资源(不可用)回收工作。
+ A handle scope can be thought of as a container for any number of handles. When you've finished with your handles, instead of deleting each one individually you can simply delete their scope.
    - 一个handle scope可以把他想象成一个容器包含着很多的handles。当你结束你对这些数据的操作时，你可以简单的删除这个scope而一起把附属的handle资源都回收，而不用一个一个的操作删除。
+ A context is an execution environment that allows separate, unrelated, JavaScript code to run in a single instance of V8. You must explicitly specify the context in which you want any JavaScript code to be run.
    - 一个context(上下文)形成一个可执行的环境，他允许分割和不相关联，JavaScript 代码运行再一个V8实例。你必须精确指定一个context去执行JavaScript代码。

### 5.2.CPP2JS映射对象

首先提供一个HelloWorld 执行Script的环境，或者启动Shell的示例。我们这里采用启动Shell的示例，版本是12.0.0:

```cpp
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
using namespace v8;

Local<ObjectTemplate> initializeGlobal(Isolate* isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
  return handle_scope.Escape(global);
}
Local<Context> CreateShellContext(Isolate* isolate, Local<ObjectTemplate> global) {
  return Context::New(isolate, NULL, global);
}

int main(int argc, char* argv[]) {
  V8::InitializeICUDefaultLocation(argv[0]);
  V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  {
    // 设定Isolate 和Handle的上下文scope
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    // 生成global的ObjectTemplate
    Local<ObjectTemplate> global = initializeGlobal(isolate);
    // 生成context
    Local<Context> context(CreateShellContext(isolate, global));
    // 设定当前context上下文scope
    Context::Scope context_scope(context);
    RunShell(isolate, platform.get());
  }
  isolate->Dispose();
  V8::Dispose();
  V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}
```

上面示例中缺少RunShell方法的实现，其他代码就是V8运行模板。如果要运行一个简单Shell，则可以实现:

```cpp
void RunShell(Isolate* isolate, Platform* platform) {
  // 通过isolate获取上下文context
  Local<Context> context = isolate->GetCurrentContext();

  Local<String> source;
  if (!String::NewFromUtf8(isolate, "1+3").ToLocal(&source)) {
    exit(1);
  }
  // 通过在当前context上下文环境编译字符串，然后在当前context中运行
  Local<Script> script = Script::Compile(context, source).ToLocalChecked();
  // Local对象重写了operate-> 方法，其中包括各个类型的转换方法，最后ToChecked即可转换成功
  Local<Value> result = script->Run(context).ToLocalChecked();
  int32_t a = result->Int32Value(context).ToChecked();
  printf("result is %d\n", a);
}
```

以上是直接编译一个JS执行字符串的例子，[完整在代码](./tech/1.helloworld.cpp).

而如果要形成REPL(Read/Eval/Print/Loop), 则可以通过下方示例，替换RunShell方法即可实现:

```cpp
const char* ToCString(const String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}
bool ExecuteString(Isolate* isolate, Local<String> source, Local<Value> name) {
  HandleScope handle_scope(isolate);
  Local<Context> context(isolate->GetCurrentContext());
  ScriptOrigin origin(name);
  Local<Script> script;
  if (!Script::Compile(context, source, &origin).ToLocal(&script)) {
    return false;
  }
  Local<Value> result;
  if (!script->Run(context).ToLocal(&result)) {
    return false;
  }
  String::Utf8Value str_value(isolate, result);
  const char* str = ToCString(str_value);
  printf("%s\n", str);
}
void RunShell(Isolate* isolate, Platform* platform) {
  fprintf(stdout, "V8 version %s [sample shell]\n", V8::GetVersion());
  Local<Context> context(isolate->GetCurrentContext());
  Context::Scope context_scope(context);

  static const int kBufferSize = 256;
  Local<String> name(String::NewFromUtf8Literal(isolate, "(shell)"));
  while (true) {
    char buffer[kBufferSize];
    fprintf(stdout, "> ");
    char* str = fgets(buffer,  kBufferSize, stdin);
    if (str == NULL) break;
    HandleScope handle_scope(isolate);
    ExecuteString(isolate, String::NewFromUtf8(isolate, str).ToLocalChecked(), name);
    // v8::platform::PumpMessageLoop 是V8引擎得平台类中得一个方法。它用于在V8引擎得消息循环中处理待处理得消息。
    // V8引擎得消息循环是一个事件处理机制，用于处理异步操作和事件驱动的任务。通常情况下，PumpMessageLoop方法会在V8引擎的主线程中被调用，用于保持消息循环的运行。
    while (platform::PumpMessageLoop(platform, isolate))
      continue;
  }
  fprintf(stdout, "\n");
}
```

以上即可以运行Shell了: [shell](./tech/2.shell.cpp)。接下来我们映射JS映射对象:

```diff
+int x;
+void XGetter(Local<String> property, const PropertyCallbackInfo<Value>& info) {
+   info.GetReturnValue().Set(x);
+}
+void XSetter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
+   x = value->Int32Value(info.GetIsolate()->GetCurrentContext()).FromMaybe(0);
+}
Local<ObjectTemplate> initializeGlobal(Isolate* isolate) {
    EscapableHandleScope handle_scope(isolate);
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
+   global->SetAccessor(String::NewFromUtf8(isolate, "x").ToLocalChecked(), XGetter, XSetter);
    return handle_scope.Escape(global);
}
```

要指定全局参数x给到JS调用.

测试: 运行Shell后，查看`x` 不是undefined，则代表映射成功.

[shell_x](./tech/3.shell_x.cpp)

### 5.3.CPP2JS映射全局对象

映射全局对象，我们假设生成了一个Point对象，最终将该对象暴露到JS中:

```diff
+class Point {
+public:
+  Point(int x): x_(x) {}
+  int x_;
+};
+void GetPointX(Local<String> property, const PropertyCallbackInfo<Value>& info) {
+  Local<Object> self = info.Holder();
+  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
+  void* ptr = wrap->Value();
+  int value = static_cast<Point*>(ptr)->x_;
+  info.GetReturnValue().Set(value);
+}
+void SetPointX(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
+  Local<Object> self = info.Holder();
+  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
+  void* ptr = wrap->Value();
+  static_cast<Point*>(ptr)->x_ = value->Int32Value(info.GetIsolate()->GetCurrentContext()).ToChecked();
+}
+void localGenerateX(Isolate* isolate) {
+  Local<Context> context(isolate->GetCurrentContext());
+
+  Local<ObjectTemplate> result = ObjectTemplate::New(isolate);
+  result->SetInternalFieldCount(1);
+  result->SetAccessor(String::NewFromUtf8(isolate, "x").ToLocalChecked(), GetPointX, SetPointX);
+  Point* p = new Point(11);
+  Local<Object> obj = result->NewInstance(context).ToLocalChecked();
+  obj->SetInternalField(0, External::New(isolate, p));
+  context->Global()->Set(context, String::NewFromUtf8(isolate, "p").ToLocalChecked(), obj).ToChecked();
+}
int main(int argc, char* argv[]) {
  ...
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    Local<ObjectTemplate> global = initializeGlobal(isolate);
    Local<Context> context(CreateShellContext(isolate, global));
    Context::Scope context_scope(context);
+    localGenerateX(isolate);
    RunShell(isolate, platform.get());
  }
}
```

通过向context->Global()->Set 设置了键`p`, 值为Local<ObjectTemplate> result 的NewInstance 对象。这样就将p 对象绑定到了JS对象中。

> External 是什么类型？答: void*，通过取名External 知晓是无类型指针。

测试: 启动后测试 p 和 p.x:

![export_p_instance_test](./assets/export_p_instance_test.jpg)

### 5.4.CPP2JS映射Class

映射Class 和上篇[#5.3映射全局对象](#53cpp2js映射全局对象) 基本一样。不同之处: 1. 对象映射可以在任何时刻，类型只能在global创建时处理；2. 对象映射生成的是ObjectTemplate, 类型是FunctionTemplate, 并且提供方法创建回调的方式来约定创建细节；等等...

```diff
+class Point {
+public:
+  Point(int x, int y): x_(x), y_(y) {}
+  int multi() {
+    return x_ * y_;
+  }
+
+  int x_, y_;
+};
+void GetPointX(Local<String> property, const PropertyCallbackInfo<Value>& info) {
+  printf("GetPointX is calling\n");
+  Local<Object> self = info.This();
+  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
+  void* ptr = wrap->Value();
+  int value = static_cast<Point*>(ptr)->x_;
+  info.GetReturnValue().Set(value);
+}
+void SetPointX(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
+  printf("SetPointX is calling\n");
+  Local<Object> self = info.This();
+  Local<External> wrap = self->GetInternalField(0).As<Value>().As<External>();
+  void* ptr = wrap->Value();
+  static_cast<Point*>(ptr)->x_ = value->Int32Value(info.GetIsolate()->GetCurrentContext()).ToChecked();
+}
+void PointMulti(const FunctionCallbackInfo<Value>& args) {
+  Isolate* isolate = Isolate::GetCurrent();
+  HandleScope handle_scope(isolate);
+
+  Local<Object> self = args.Holder();
+  Local<External> wrap = self->GetInternalField(0).As<Value>().As<External>();
+  void* ptr = wrap->Value();
+  int value = static_cast<Point*>(ptr)->multi();
+  args.GetReturnValue().Set(value);
+}
+void ConstructorCallback(const FunctionCallbackInfo<Value>& args) {
+  auto isolate = args.GetIsolate();
+  Local<Context> context(isolate->GetCurrentContext());
+  double x = args[0]->NumberValue(context).ToChecked();
+  double y = args[1]->NumberValue(context).ToChecked();
+  Local<External> external = External::New(isolate, new Point(x, y));
+  args.This()->SetInternalField(0, external);
+}
+void localNewPoint(Isolate* isolate, Local<ObjectTemplate>& global) {
+  Local<FunctionTemplate> pointTemplate = FunctionTemplate::New(isolate, ConstructorCallback);
+  pointTemplate->InstanceTemplate()->SetInternalFieldCount(1);
+  pointTemplate->SetClassName(String::NewFromUtf8(isolate, "Point", NewStringType::kInternalized).ToLocalChecked());
+  auto prototype_t = pointTemplate->PrototypeTemplate();
+  prototype_t->SetAccessor(String::NewFromUtf8(isolate, "x", NewStringType::kInternalized).ToLocalChecked(), GetPointX, SetPointX);
+  pointTemplate->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "multi").ToLocalChecked(), FunctionTemplate::New(isolate, PointMulti));
+
+  global->Set(isolate, "Point", pointTemplate);
+}
Local<ObjectTemplate> initializeGlobal(Isolate* isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);

+  localNewPoint(isolate, global);
  return handle_scope.Escape(global);
}
```

以上实现了Class 注册到JS办法。测试:

![class_test](./assets/class_test.jpg)

### 5.5.JS2CPP映射方法

待续...

## Ref

[v8 build](https://v8.dev/docs/build)

[v8 build-gn](https://v8.dev/docs/build-gn)

[v8 embed](https://v8.dev/docs/embed)

[V8Android](https://github.com/cstsinghua/V8Android)

[V8 基础使用介绍](https://www.iteye.com/blog/lcgg110-1115012)

[V8配置与常用调试](https://www.sec4.fun/2020/06/12/v8Config/)

[如何正确地使用v8嵌入到我们的C++应用中](https://juejin.cn/post/6844903956125057031)
