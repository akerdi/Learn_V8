// ./processon_maptemplate verbose=true map.js

#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include "../include/v8.h"
#include "../include/libplatform/libplatform.h"

using namespace std;
using namespace v8;

typedef map<string, string> StringStringMap;

class HttpRequestProcessor {
public:
  virtual ~HttpRequestProcessor() {}
  virtual bool Initialize(StringStringMap* options, StringStringMap* output) = 0;
  static void Log(const char* event);
};

class JsHttpRequestProcessor: public HttpRequestProcessor {
public:
  JsHttpRequestProcessor(Isolate* isolate, Local<String> script):isolate_(isolate), script_(script) {}
  virtual ~JsHttpRequestProcessor();
  virtual bool Initialize(StringStringMap* opts, StringStringMap* output);

private:
  bool InstallMaps(StringStringMap* opts, StringStringMap* output);
  bool ExecuteScript(Local<String> script);

  Local<Object> WrapMap(StringStringMap* obj);
  static StringStringMap* UnwrapMap(Local<Object> obj);
  static void MapGet(Local<Name> name, const PropertyCallbackInfo<Value>& info);
  static void MapSet(Local<Name> name, Local<Value> value, const PropertyCallbackInfo<Value>& info);
  static Local<ObjectTemplate> MakeMapTemplate(Isolate* isolate);

  Isolate* GetIsolate() { return isolate_; }

  Isolate* isolate_;
  Local<String> script_;
  Global<Context> context_;
  static Global<ObjectTemplate> map_template_;
};

StringStringMap* JsHttpRequestProcessor::UnwrapMap(Local<Object> obj) {
  Local<External> field = obj->GetInternalField(0).As<Value>().As<External>();
  void* ptr = field->Value();
  return static_cast<StringStringMap*>(ptr);
}
string ObjectToString(Isolate* isolate, Local<Value> value) {
  String::Utf8Value str_value(isolate, value);
  return string(*str_value);
}
static void LogCallback(const v8::FunctionCallbackInfo<Value>& info) {
  if (info.Length() < 1) return;

  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);
  Local<Value> arg = info[0];
  if (arg->IsMap()) {
    Local<Map> aMap = arg.As<Map>();
    Local<Array> array = aMap->AsArray();
    uint32_t length = array->Length();
    uint32_t halfLength = length / 2;
    for (int32_t i = 0; i < halfLength; i++) {
      Local<Value> key = array->Get(isolate->GetCurrentContext(), i).ToLocalChecked();
      Local<Value> value = array->Get(isolate->GetCurrentContext(), i+halfLength).ToLocalChecked();
      String::Utf8Value keyStr(isolate, key);
      String::Utf8Value valueStr(isolate, value);
      string str(*keyStr);
      str.append(":");
      str.append(*valueStr);
      HttpRequestProcessor::Log(str.c_str());
    }
  } else if (arg->IsString()) {
    String::Utf8Value value(isolate, arg);
    HttpRequestProcessor::Log(*value);
  } else if (arg->IsObject()) {
    Local<Object> argObj = arg->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
    Local<External> field = argObj->GetInternalField(0).As<Value>().As<External>();
    void* ptr = field->Value();
    StringStringMap* obj = static_cast<StringStringMap*>(ptr);
    for (const auto& pair : *obj) {
      string str(pair.first);
      str.append(pair.second);
      HttpRequestProcessor::Log(str.c_str());
    }
  }
}
bool JsHttpRequestProcessor::Initialize(StringStringMap* opts, StringStringMap* output) {
  HandleScope handle_scope(GetIsolate());
  Local<ObjectTemplate> global = ObjectTemplate::New(GetIsolate());
  global->Set(GetIsolate(), "log", FunctionTemplate::New(GetIsolate(), LogCallback));

  Local<Context> context = Context::New(GetIsolate(), NULL, global);
  context_.Reset(GetIsolate(), context);
  Context::Scope context_scope(context);

  if (!InstallMaps(opts, output)) return false;
  if (!ExecuteScript(script_)) return false;

  return true;
}
bool JsHttpRequestProcessor::ExecuteScript(Local<String> script) {
  HandleScope handle_scope(GetIsolate());

  TryCatch try_catch(GetIsolate());
  Local<Context> context(GetIsolate()->GetCurrentContext());
  Local<Script> compiled_script;
  if (!Script::Compile(context, script).ToLocal(&compiled_script)) {
    String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }
  Local<Value> result;
  if (!compiled_script->Run(context).ToLocal(&result)) {
    String::Utf8Value error(GetIsolate(), try_catch.Exception());
    Log(*error);
    return false;
  }
  return true;
}

bool JsHttpRequestProcessor::InstallMaps(StringStringMap* opts, StringStringMap* output) {
  HandleScope handle_scope(GetIsolate());
  Local<Context> context = Local<Context>::New(GetIsolate(), context_);

  Local<Object> opts_obj = WrapMap(opts);
  Local<Object> output_obj = WrapMap(output);
  context->Global()
    ->Set(context, String::NewFromUtf8Literal(GetIsolate(), "options"), opts_obj).FromJust();
  context->Global()
    ->Set(context, String::NewFromUtf8Literal(GetIsolate(), "output"), output_obj).FromJust();
  return true;
}
Local<Object> JsHttpRequestProcessor::WrapMap(StringStringMap* obj) {
  EscapableHandleScope handle_scope(GetIsolate());
  if (map_template_.IsEmpty()) {
    Local<ObjectTemplate> raw_template = MakeMapTemplate(GetIsolate());
    map_template_.Reset(GetIsolate(), raw_template);
  }
  Local<ObjectTemplate> templ = Local<ObjectTemplate>::New(GetIsolate(), map_template_);
  Local<Object> result = templ->NewInstance(GetIsolate()->GetCurrentContext()).ToLocalChecked();
  Local<External> map_ptr = External::New(GetIsolate(), obj);
  result->SetInternalField(0, map_ptr);
  return handle_scope.Escape(result);
}
void JsHttpRequestProcessor::MapGet(Local<Name> name, const PropertyCallbackInfo<Value>& info) {
  if (name->IsSymbol()) return;

  StringStringMap* obj = UnwrapMap(info.Holder());
  string key = ObjectToString(info.GetIsolate(), name);
  StringStringMap::iterator iter = obj->find(key);
  if (iter == obj->end()) return;

  const string& value = (*iter).second;
  info.GetReturnValue().Set(
    String::NewFromUtf8(info.GetIsolate(), value.c_str(), NewStringType::kNormal, static_cast<int>(value.length())).ToLocalChecked()
  );
}
void JsHttpRequestProcessor::MapSet(Local<Name> name, Local<Value> value_obj, const PropertyCallbackInfo<Value>& info) {
  if (name->IsSymbol()) return;

  StringStringMap* obj = UnwrapMap(info.Holder());
  string key = ObjectToString(info.GetIsolate(), name.As<String>());
  string value = ObjectToString(info.GetIsolate(), value_obj.As<String>());
  (*obj)[key] = value;
  info.GetReturnValue().Set(value_obj);
}
Local<ObjectTemplate> JsHttpRequestProcessor::MakeMapTemplate(Isolate* isolate) {
  EscapableHandleScope handle_scope(isolate);
  Local<ObjectTemplate> result = ObjectTemplate::New(isolate);
  result->SetInternalFieldCount(1);
  result->SetHandler(NamedPropertyHandlerConfiguration(MapGet, MapSet));
  return handle_scope.Escape(result);
}

JsHttpRequestProcessor::~JsHttpRequestProcessor() {
  context_.Reset();
}

Global<ObjectTemplate> JsHttpRequestProcessor::map_template_;

void HttpRequestProcessor::Log(const char* event) {
  printf("Logged: %s\n", event);
}

void ParseOptions(int argc, char* argv[], StringStringMap* options, string* file) {
  for (int i = 0; i < argc; i++) {
    string str = argv[i];
    size_t s = str.find('=', 0);
    if (s == string::npos) {
      *file = str;
    } else {
      string key = str.substr(0, s);
      string value = str.substr(s+1);
      (*options)[key] = value;
    }
  }
}
MaybeLocal<String> Readfile(Isolate* isolate, const string& name) {
  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) {
    return MaybeLocal<String>();
  }
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  std::unique_ptr<char> chars(new char[size+1]);
  chars.get()[size] = '\0';
  for (int i = 0; i < size;) {
    i += fread(chars.get()+i, 1, size-i, file);
    if (ferror(file)) {
      fclose(file);
      return MaybeLocal<String>();
    }
  }
  fclose(file);
  MaybeLocal<String> result = String::NewFromUtf8(
    isolate, chars.get(), NewStringType::kNormal, static_cast<int>(size)
  );
  return result;
}
void PrintMap(StringStringMap* m) {
  for (StringStringMap::iterator it = m->begin(); it != m->end(); it++) {
    pair<string, string> entry = *it;
    printf("%s: %s\n", entry.first.c_str(), entry.second.c_str());
  }
}

int main(int argc, char* argv[]) {
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  StringStringMap options;
  string file;
  ParseOptions(argc, argv, &options, &file);
  if (file.empty()) {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<String> source;
  if (!Readfile(isolate, file).ToLocal(&source)) {
    fprintf(stderr, "Error reading '%s'\n", file.c_str());
    return 1;
  }
  JsHttpRequestProcessor processor(isolate, source);
  StringStringMap output;
  if (!processor.Initialize(&options, &output)) {
    fprintf(stderr, "Error initializing processor.\n");
    return 1;
  }
  PrintMap(&output);
}
