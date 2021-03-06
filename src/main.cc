#include "main.h"

bool NodeNmea2000::CANSendFrame(unsigned long id, unsigned char len,
                            const unsigned char *buf, bool wait_sent) {
  return sendFrame(id, len, buf, wait_sent);
}

bool NodeNmea2000::CANOpen() {
  return true;
}

bool NodeNmea2000::CANGetFrame(unsigned long &id,
                           unsigned char &len, unsigned char *buf) {
  if (packets_.size() == 0) {
    return false;
  }
  id = packets_[0].id;
  len = packets_[0].length;
  memcpy(buf, packets_[0].data, len);
  packets_.pop_front();
  return true;
}

Nan::Persistent<v8::Function> NodeNmea2000::constructor;

#define CHECK_CONDITION(expr, str) if(!(expr)) return Nan::ThrowError(str);
#define SYMBOL(aString) Nan::New((aString)).ToLocalChecked()

void NodeNmea2000::Init(v8::Local<v8::Object> exports) {
  Nan::HandleScope scope;

  // Prepare constructor template
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("NMEA2000").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);        // for storing (this)

  // Prototype
  Nan::SetPrototypeMethod(tpl, "pushCanFrame",    pushCanFrame);
  Nan::SetPrototypeMethod(tpl, "setSendCanFrame", setSendCanFrame);
  Nan::SetPrototypeMethod(tpl, "open",           open);
  Nan::SetPrototypeMethod(tpl, "parseMessages",   parseMessages);

  // constructor
  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("NMEA2000").ToLocalChecked(), tpl->GetFunction());
}

int32_t intOrDefault(v8::Local<v8::Object> obj, const char* key, int def) {
  v8::Local<v8::Value> val = Nan::Get(obj, Nan::New(key).ToLocalChecked())
    .ToLocalChecked();
  if (val->IsNumber()) {
    return val->Int32Value();
  }
  return def;
}

std::string strOrDefault(v8::Local<v8::Object> obj, const char* key,
                         const char* def) {
  v8::Local<v8::Value> val =
    Nan::Get(obj, Nan::New<v8::String>(key).ToLocalChecked()).ToLocalChecked();
  if (!val->IsString()) {
    return def;
  }

  v8::String::Utf8Value utf8(val->ToString());
  return std::string(*utf8);
}

NAN_METHOD(NodeNmea2000::New) {
  CHECK_CONDITION(info.IsConstructCall(), "Must be called with new");
  CHECK_CONDITION(info.Length() >= 1, "Invalid arguments");
  CHECK_CONDITION(info[0]->IsArray(), "First argument must be an array");

  NodeNmea2000* zis = new NodeNmea2000();
  zis->Wrap(info.This());

  int source = 77;  // the default address

  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(info[0]); 
  if (array->Length() > 0) {
    zis->SetDeviceCount(array->Length());

    for (int i = 0; i < array->Length(); ++i) {
      auto entry = array->Get(i);
      CHECK_CONDITION(entry->IsObject(), "array entries must be objects");
      v8::Local<v8::Object> obj(entry->ToObject());

      std::string serialCode(strOrDefault(obj, "serialCode", ""));
      std::string model(strOrDefault(obj, "model", ""));
      std::string softwareVersion(strOrDefault(obj, "softwareVersion", ""));
      std::string modelVersion(strOrDefault(obj, "modelVersion", ""));

      zis->SetProductInformation(
          serialCode.c_str(),
          intOrDefault(obj, "productCode", 0),
          model.c_str(),
          softwareVersion.c_str(),
          modelVersion.c_str(),
          intOrDefault(obj, "loadEquivalency", 0xff),
          intOrDefault(obj, "nmea2000Version", 0xffff),
          intOrDefault(obj, "certificationLevel", 0xff),
          i
        );
      zis->SetDeviceInformation(
          intOrDefault(obj, "uniqueNumber",  0),
          intOrDefault(obj, "deviceFunction", 0xff),
          intOrDefault(obj, "deviceClass", 0xff),
          intOrDefault(obj, "manufacturerCode", 0xffff),
          intOrDefault(obj, "industryGroup", 4),
          i);

      source = intOrDefault(obj, "source", source);
    }
  }
  // For now NMEA2000 lib only support specifying the source
  // for the 1set device... TODO: set it for all devices.
  zis->SetMode(tNMEA2000::N2km_ListenAndNode, source);

  info.GetReturnValue().Set(info.This());
}

NodeNmea2000::Packet::Packet(
    unsigned long id, const char* data, unsigned char length) {
  this->id = id;
  this->length = length;
  memcpy(this->data, data, length);
}

NAN_METHOD(NodeNmea2000::pushCanFrame) {
  NodeNmea2000* zis = ObjectWrap::Unwrap<NodeNmea2000>(info.Holder());

  CHECK_CONDITION(info.Length() >= 1, "Invalid arguments");
  CHECK_CONDITION(info[0]->IsObject(), "First argument must be an Object");

  v8::Local<v8::Object> obj =  info[0]->ToObject();

  v8::Local<v8::Value> dataArg = obj->Get(SYMBOL("data"));
  CHECK_CONDITION(node::Buffer::HasInstance(dataArg), "Data field must be a Buffer");

  zis->packets_.emplace_back(
    Packet(
      obj->Get(SYMBOL("id"))->Uint32Value(),
      node::Buffer::Data(dataArg->ToObject()),
      node::Buffer::Length(dataArg->ToObject())));

  zis->ParseMessages();
}

NAN_METHOD(NodeNmea2000::setSendCanFrame) {
  NodeNmea2000* zis = ObjectWrap::Unwrap<NodeNmea2000>(info.Holder());

  CHECK_CONDITION(zis != nullptr, "this must be a native NMEA2000 object");
  CHECK_CONDITION(info.Length() >= 1, "Invalid arguments");
  CHECK_CONDITION(info[0]->IsFunction(), "First argument must be a function");

  zis->sendPacketCb_.Reset(info[0].As<v8::Function>());
  if (info.Length() >= 2 && info[1]->IsObject())
      zis->sendPacketHandle_.Reset(info[1]->ToObject());
}

bool NodeNmea2000::sendFrame(unsigned long id, unsigned char len,
                             const unsigned char *buf, bool wait_sent) {
  if (sendPacketCb_.IsEmpty()) {
    std::cerr << "Warning: trying to send NMEA2000 packet, but no sendPacket "
      "function registered. Please call setSendCanFrame().\n";
    return false;
  } else {
    v8::Local<v8::Value> argv[2] = {
      Nan::New(static_cast<unsigned int>(id)),
      Nan::CopyBuffer(reinterpret_cast<const char *>(buf), len).ToLocalChecked()
    };

    Nan::Callback callback(Nan::New(sendPacketCb_));
    v8::Local<v8::Value> ret;
    
    if (sendPacketHandle_.IsEmpty()) {
      ret = callback.Call(2, argv);
    } else {
      ret = callback.Call(Nan::New(sendPacketHandle_), 2, argv);
    }

    return ret->IsTrue();
  }
}

NAN_METHOD(NodeNmea2000::open) {
  NodeNmea2000* zis = ObjectWrap::Unwrap<NodeNmea2000>(info.Holder());
  CHECK_CONDITION(zis != nullptr, "this must be a native NMEA2000 object");
  zis->Open();
}

NAN_METHOD(NodeNmea2000::parseMessages) {
  NodeNmea2000* zis = ObjectWrap::Unwrap<NodeNmea2000>(info.Holder());
  CHECK_CONDITION(zis != nullptr, "this must be a native NMEA2000 object");
  zis->ParseMessages();
}

void InitAll(v8::Local<v8::Object> exports)
{
  NodeNmea2000::Init(exports);
}


void delay(const uint32_t ms) {
  usleep(ms*1000);
};


uint32_t millis(void) {
  struct timespec ticker;
  
  clock_gettime(CLOCK_MONOTONIC, &ticker);
  return ((uint32_t) ((ticker.tv_sec * 1000) + (ticker.tv_nsec / 1000000)));
};

NODE_MODULE(node_nmea2000, InitAll)
