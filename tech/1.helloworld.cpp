#include "../include/libplatform/libplatform.h"
#include "../include/v8.h"
using namespace v8;

Local<ObjectTemplate> initializeGlobal(Isolate* isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
  return handle_scope.Escape(global);
}
Local<Context> CreateShellContext(Isolate* isolate, Local<ObjectTemplate> global) {
  return Context::New(isolate, NULL, global);
}
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