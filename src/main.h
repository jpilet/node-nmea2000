#ifndef __MAIN_H_
#define __MAIN_H_

#include <nan.h>

#include <iostream>
#include <time.h>
#include <unistd.h>
#include <deque>

#include <NMEA2000.h> 
#include <N2kMsg.h>

class NodeNmea2000;

struct Packet {
  unsigned long id;
  unsigned char data[8];
  unsigned char length;
};

class Nmea2000 : public tNMEA2000 {
 public:
  Nmea2000(NodeNmea2000* nodeNmea2000) : nodeNmea2000_(nodeNmea2000) { }
  virtual ~Nmea2000() { }

  void pushPacket(const Packet& p) { packets_.push_back(p); ParseMessages(); }

 protected:
  // Virtual functions for different interfaces.
  bool CANSendFrame(unsigned long id, unsigned char len,
                    const unsigned char *buf, bool wait_sent) override;
  bool CANOpen() override;
  bool CANGetFrame(unsigned long &id,
                           unsigned char &len, unsigned char *buf) override;

 private:
  NodeNmea2000* nodeNmea2000_;
  std::deque<Packet> packets_;
};

class NodeNmea2000 : public Nan::ObjectWrap {
 public:
  NodeNmea2000(): nmea2000_(this) { }

  static void Init(v8::Local<v8::Object> exports);

  static NAN_METHOD(New);
  static NAN_METHOD(pushCanFrame);
  static NAN_METHOD(setSendCanFrame);
  static NAN_METHOD(open);
  static NAN_METHOD(parseMessages);

  bool sendFrame(unsigned long id, unsigned char len,
                 const unsigned char *buf, bool wait_sent);
 private:
  static Nan::Persistent<v8::Function> constructor;

  Nan::Persistent<v8::Function> sendPacketCb_;
  Nan::Persistent<v8::Object> sendPacketHandle_;
  Nmea2000 nmea2000_;
};

#endif  // __MAIN_H_
