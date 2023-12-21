#include "../include/libplatform/libplatform.h"

#include "../include/v8.h"

using namespace v8;

class Point {
public:
  Point(int x): x_(x) {}
  int x_;
};
void GetPointX(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void* ptr = wrap->Value();
  int value = static_cast<Point*>(ptr)->x_;
  info.GetReturnValue().Set(value);
}
void SetPointX(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void* ptr = wrap->Value();
  static_cast<Point*>(ptr)->x_ = value->Int32Value(info.GetIsolate()->GetCurrentContext()).ToChecked();
}

void localGenerateX(Isolate* isolate) {
  Local<Context> context(isolate->GetCurrentContext());

  Local<ObjectTemplate> result = ObjectTemplate::New(isolate);
  result->SetInternalFieldCount(1);
  result->SetAccessor(String::NewFromUtf8(isolate, "x").ToLocalChecked(), GetPointX, SetPointX);
  Point* p = new Point(11);
  Local<Object> obj = result->NewInstance(context).ToLocalChecked();
  obj->SetInternalField(0, External::New(isolate, p));
  context->Global()->Set(context, String::NewFromUtf8(isolate, "p").ToLocalChecked(), obj).ToChecked();
}

Local<ObjectTemplate> initializeGlobal(Isolate* isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
  return handle_scope.Escape(global);
}
Local<Context> CreateShellContext(Isolate* isolate, Local<ObjectTemplate> global) {
  return Context::New(isolate, NULL, global);
}

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
    while (platform::PumpMessageLoop(platform, isolate))
      continue;
  }
  fprintf(stdout, "\n");
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
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    Local<ObjectTemplate> global = initializeGlobal(isolate);
    Local<Context> context(CreateShellContext(isolate, global));
    Context::Scope context_scope(context);
    localGenerateX(isolate);
    RunShell(isolate, platform.get());
  }
  isolate->Dispose();
  V8::Dispose();
  V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}
