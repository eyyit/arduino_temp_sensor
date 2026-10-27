// Real pinned receiver methods; simulated sockets, time, update backend and reboot.
// Uses fixture-only credentials. Never reads secrets.yaml or opens a device.
#include "office_ota_interlock.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#define USE_OTA_PASSWORD
#define USE_OTA_STATE_LISTENER
#define USE_OTA_VERSION 2
#define ESPHOME_LOG_LEVEL 0
#define ESPHOME_LOG_LEVEL_VERBOSE 6
#define ESP_LOGW(...) do {} while (0)
#define ESP_LOGV(...) do {} while (0)
#define ESP_LOGD(...) do {} while (0)
#define ESP_LOGI(...) do {} while (0)
#define LOG_STR(x) x
using LogString = char;
using esphome::wifi::OfficeOTAInterlock;
using esphome::wifi::OfficeScanWatchdog;
static constexpr size_t OTA_BUFFER_SIZE=1040, OTA_SIZE_FIELD_BYTES=4;
static constexpr uint32_t OTA_SOCKET_TIMEOUT_DATA=105000, OTA_PROGRESS_INTERVAL_MS=1000;
static constexpr uint16_t OTA_BLOCK_SIZE=8192;
static constexpr uint8_t CLIENT_FEATURE_SUPPORTS_SHA256_AUTH=0x02;
// SHA256 oracle uses the container's Python standard library, not a fake match bit.
// Inputs are fixture strings only; this is a host crypto adapter, not MCU SHA code.
std::string digest(const std::string &input) {
  { std::ofstream f("/config/tests/ota_hash_input.tmp", std::ios::binary); f << input; }
  FILE *pipe=popen("python3 -c \"import hashlib;print(hashlib.sha256(open('/config/tests/ota_hash_input.tmp','rb').read()).hexdigest())\"", "r");
  assert(pipe); char output[66]{}; assert(fgets(output,sizeof(output),pipe)); assert(pclose(pipe)==0);
  return std::string(output,64);
}
namespace esphome::sha256 {
class SHA256 {
  std::string input_, hex_;
 public:
  void init() { input_.clear(); }
  void add(const char *data,size_t len) { input_.append(data,len); }
  void calculate() { hex_=digest(input_); }
  bool equals_hex(const char *response) { return std::memcmp(hex_.data(),response,64)==0; }
};
}
namespace esphome::ota {
enum OTAResponseTypes { OTA_RESPONSE_OK=0, OTA_RESPONSE_REQUEST_SHA256_AUTH=2,
 OTA_RESPONSE_AUTH_OK=0x41, OTA_RESPONSE_UPDATE_PREPARE_OK=0x42, OTA_RESPONSE_BIN_MD5_OK=0x43,
 OTA_RESPONSE_RECEIVE_OK=0x44, OTA_RESPONSE_UPDATE_END_OK=0x45, OTA_RESPONSE_CHUNK_OK=0x47,
 OTA_RESPONSE_ERROR_AUTH_INVALID=0x82, OTA_RESPONSE_ERROR_UPDATE_PREPARE=0x81,
 OTA_RESPONSE_ERROR_WRITING_FLASH=0x83, OTA_RESPONSE_ERROR_MD5_MISMATCH=0x8b,
 OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE=0x8e, OTA_RESPONSE_ERROR_UNKNOWN=0xff };
enum OTAType { OTA_TYPE_UPDATE_APP=0 };
enum OTAState { OTA_COMPLETED, OTA_STARTED, OTA_IN_PROGRESS, OTA_ABORT, OTA_ERROR };
}
struct SimulatedReboot {};
struct FakeApp { unsigned reboots=0; void feed_wdt() {} void safe_reboot() { ++reboots; throw SimulatedReboot{}; } } App;
struct FakeSocket {
  std::vector<uint8_t> input,output;
  size_t pos=0, max_read=4096;
  bool stall=false, hard_error=false, closed=false;
  int setsockopt(int,int,const void*,size_t) { return 0; }
  int setblocking(bool) { return 0; }
  ssize_t read(uint8_t *dst,size_t len) {
    if (pos<input.size()) {
      size_t n=std::min({len,max_read,input.size()-pos}); std::memcpy(dst,input.data()+pos,n); pos+=n; return n;
    }
    if (hard_error) { errno=ECONNRESET; return -1; }
    if (stall) { fake_millis+=2000; errno=EAGAIN; return -1; }
    return 0;
  }
  ssize_t write(const uint8_t *src,size_t len) { output.insert(output.end(),src,src+len); return len; }
  void close() { closed=true; }
};
struct FakeBackend {
  unsigned begins=0,writes=0,ends=0,aborts=0; size_t received=0;
  bool fail_begin=false,fail_write=false,fail_end=false,activation=false;
  OfficeOTAInterlock &guard;
  explicit FakeBackend(OfficeOTAInterlock &g):guard(g) {}
  esphome::ota::OTAResponseTypes begin(size_t,esphome::ota::OTAType) {
    assert(guard.active()); ++begins;
    return fail_begin?esphome::ota::OTA_RESPONSE_ERROR_UPDATE_PREPARE:esphome::ota::OTA_RESPONSE_OK;
  }
  void set_update_md5(const char*) {}
  esphome::ota::OTAResponseTypes write(uint8_t*,size_t n) {
    assert(guard.active()); ++writes; received+=n;
    return fail_write?esphome::ota::OTA_RESPONSE_ERROR_WRITING_FLASH:esphome::ota::OTA_RESPONSE_OK;
  }
  esphome::ota::OTAResponseTypes end() {
    ++ends; if (fail_end) return esphome::ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
    activation=true; return esphome::ota::OTA_RESPONSE_OK;
  }
  void abort() { ++aborts; }
};
namespace esphome {
inline uint32_t encode_uint32(uint8_t a,uint8_t b,uint8_t c,uint8_t d) { return (uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<<8)|d; }
class ESPHomeOTAComponent {
 public:
  enum class OTAState { IDLE,AUTH_READ,DATA };
  struct DataTransfer { size_t ota_size=0,total=0,acknowledged=0; uint32_t last_data_ms=0,last_progress=0; };
  static constexpr size_t SHA256_HEX_SIZE=64;
  FakeSocket *client_=nullptr; FakeBackend *backend_=nullptr;
  OfficeOTAInterlock guard; OfficeScanWatchdog scan;
  std::string password_="fixture-correct-password";
  std::unique_ptr<uint8_t[]> auth_buf_;
  size_t auth_buf_pos_=0,handshake_buf_pos_=0;
  uint32_t client_connect_time_=0;
  uint8_t auth_type_=0,ota_features_=0;
  bool remote_closed_=false,warning=false;
  OTAState ota_state_=OTAState::IDLE;
  std::vector<ota::OTAState> events;
  void status_set_warning() { warning=true; }
  void status_clear_warning() { warning=false; }
  void status_momentary_error(const char*,int) { warning=true; }
  void notify_state_(ota::OTAState state,float,uint8_t) {
    events.push_back(state);
    if(state==ota::OTA_STARTED) guard.set_active(true);
    if(state==ota::OTA_ERROR || state==ota::OTA_COMPLETED) guard.set_active(false);
    if(state==ota::OTA_STARTED || state==ota::OTA_IN_PROGRESS) {
      assert(!guard.may_scan(scan)); assert(!guard.may_recover_radio()); assert(!guard.may_use_scan_result());
    }
  }
  bool extended_proto_() const { return false; }
  bool would_block_(int err) const { return err==EAGAIN || err==EWOULDBLOCK; }
  bool data_readall_(uint8_t *buf,size_t n) { return readall_(buf,n); }
  bool data_write_byte_(uint8_t b) { return writeall_(&b,1); }
  void ack_received_(DataTransfer&) {} // ESP8266 ACK_AFTER_WRITE
  void ack_written_(DataTransfer &x) { send_chunk_acks_(x); }
  void yield_and_feed_watchdog_() { App.feed_wdt(); delay(1); }
  void log_socket_error_(const LogString*) {} void log_read_error_(const LogString*) {}
  void log_auth_warning_(const LogString*) {} void log_start_(const LogString*) {}
  void log_remote_closed_(const LogString*) {}
  void send_error_and_cleanup_(ota::OTAResponseTypes error) { uint8_t b=error; client_->write(&b,1); cleanup_connection_(); }
  void handle_data_(); bool readall_(uint8_t*,size_t); bool writeall_(const uint8_t*,size_t);
  bool client_left_before_start_(); bool handle_read_error_(ssize_t,const LogString*);
  bool read_size_(uint8_t*,size_t&,const LogString*); ota::OTAResponseTypes write_flash_(uint8_t*,size_t);
  ssize_t receive_data_(uint8_t*,DataTransfer&); void send_chunk_acks_(DataTransfer&);
  void cleanup_connection_(); bool select_auth_type_(); bool handle_auth_read_(); void cleanup_auth_();
};
#include "ota_receiver_methods.inc"
}
using esphome::ESPHomeOTAComponent;
using namespace esphome::ota;
void auth_input(ESPHomeOTAComponent &ota,FakeSocket &socket,const std::string &password) {
  std::string nonce(64,'a'), cnonce(64,'b');
  ota.auth_buf_=std::make_unique<uint8_t[]>(193); std::memcpy(ota.auth_buf_.get()+1,nonce.data(),64);
  std::string wire=cnonce+digest(password+nonce+cnonce);
  socket.input.assign(wire.begin(),wire.end()); ota.client_=&socket;
}
void payload(FakeSocket &socket,size_t bytes,bool checksum=true,uint32_t advertised=16) {
  socket.input={uint8_t(advertised>>24),uint8_t(advertised>>16),uint8_t(advertised>>8),uint8_t(advertised)};
  if(checksum) socket.input.insert(socket.input.end(),32,'0');
  socket.input.insert(socket.input.end(),bytes,0xe9);
}
int main() {
  for(const auto &password : {std::string("wrong-password"),std::string("")}) {
    ESPHomeOTAComponent ota; FakeSocket socket; FakeBackend backend(ota.guard); ota.backend_=&backend;
    auth_input(ota,socket,password); assert(!ota.handle_auth_read_());
    assert(socket.closed && ota.client_==nullptr && ota.backend_==nullptr);
    assert(socket.output.back()==OTA_RESPONSE_ERROR_AUTH_INVALID);
    assert(backend.begins==0 && backend.writes==0 && !ota.guard.active() && App.reboots==0);
  }
  { ESPHomeOTAComponent ota; FakeSocket socket; auth_input(ota,socket,ota.password_);
    socket.max_read=7; bool ok=false; while(!ok && ota.client_) ok=ota.handle_auth_read_();
    assert(ok && !socket.closed && !ota.auth_buf_); }
  { ESPHomeOTAComponent ota; FakeSocket socket; auth_input(ota,socket,ota.password_);
    socket.input.resize(20); assert(!ota.handle_auth_read_()); assert(!ota.handle_auth_read_());
    assert(socket.closed && ota.client_==nullptr && !ota.guard.active()); }
  { ESPHomeOTAComponent ota; FakeSocket socket; auth_input(ota,socket,ota.password_);
    // A response to an older nonce must not authenticate the new challenge.
    std::memset(ota.auth_buf_.get()+1,'c',64);
    assert(!ota.handle_auth_read_()); assert(socket.output.back()==OTA_RESPONSE_ERROR_AUTH_INVALID); }
  { ESPHomeOTAComponent ota; FakeSocket socket; ota.client_=&socket;
    assert(!ota.select_auth_type_()); assert(socket.output.back()==OTA_RESPONSE_ERROR_AUTH_INVALID); }
  { ESPHomeOTAComponent ota; FakeSocket socket; ota.client_=&socket; ota.ota_features_=CLIENT_FEATURE_SUPPORTS_SHA256_AUTH;
    assert(ota.select_auth_type_()); }
  // FIN at each byte boundary of size, checksum and image; no activation/reboot.
  for(size_t cut=0;cut<52;++cut) {
    ESPHomeOTAComponent ota; FakeSocket socket; FakeBackend backend(ota.guard);
    payload(socket,16); socket.input.resize(cut); ota.client_=&socket; ota.backend_=&backend;
    ota.handle_data_(); assert(!backend.activation && backend.ends==0 && App.reboots==0);
    assert(socket.closed && !ota.guard.active() && ota.client_==nullptr);
    assert(ota.guard.may_scan(ota.scan));
    if(backend.begins) { assert(backend.aborts==1); assert(ota.events.back()==OTA_ERROR); }
  }
  for(int failure=0;failure<6;++failure) {
    ESPHomeOTAComponent ota; FakeSocket socket; FakeBackend backend(ota.guard); payload(socket,16);
    if(failure==0) { socket.input.resize(40); socket.stall=true; }
    if(failure==1) { socket.input.resize(40); socket.hard_error=true; }
    if(failure==2) backend.fail_begin=true;
    if(failure==3) backend.fail_write=true;
    if(failure==4) backend.fail_end=true;
    if(failure==5) { socket.input.resize(10); socket.stall=true; }
    ota.client_=&socket; ota.backend_=&backend; uint32_t before=fake_millis;
    ota.handle_data_();
    assert(!backend.activation && backend.aborts==1 && App.reboots==0);
    assert(ota.events.back()==OTA_ERROR && !ota.guard.active() && socket.closed);
    if(failure==0 || failure==5) assert(uint32_t(fake_millis-before)>105000);
  }
  for(size_t bytes : {size_t(1040),size_t(1560),size_t(8192),size_t(9500)}) {
    ESPHomeOTAComponent ota; FakeSocket socket; FakeBackend backend(ota.guard);
    payload(socket,bytes,true,10000); ota.client_=&socket; ota.backend_=&backend;
    ota.handle_data_();
    assert(backend.received==bytes && backend.writes>0 && backend.ends==0);
    assert(backend.aborts==1 && !backend.activation && App.reboots==0 && !ota.guard.active());
  }
  { ESPHomeOTAComponent ota; ota.scan.started(100,true); ota.guard.set_active(true);
    assert(ota.scan.expired(5100)); assert(!ota.guard.may_recover_radio());
    assert(!ota.guard.may_use_scan_result()); assert(!ota.guard.may_scan(ota.scan));
    ota.guard.set_active(false); assert(ota.guard.may_recover_radio());
    ota.scan.resetting(5100); assert(!ota.guard.may_scan(ota.scan));
    ota.scan.reset_complete(); assert(ota.guard.may_scan(ota.scan)); }
  // Success control: all bytes/end succeed. Missing final client ACK is explicitly
  // nonfatal upstream: activation and reboot still occur after successful end().
  { ESPHomeOTAComponent ota; FakeSocket socket; FakeBackend backend(ota.guard); payload(socket,16);
    ota.client_=&socket; ota.backend_=&backend;
    try { ota.handle_data_(); assert(false); } catch(const SimulatedReboot&) {}
    assert(backend.activation && backend.ends==1 && backend.aborts==0 && App.reboots==1);
    assert(ota.events.back()==OTA_COMPLETED && !ota.guard.active()); }
  puts("PASS: real OTA methods: wrong/empty password, fragmented correct auth, truncated auth, SHA256 requirement");
  puts("PASS: every truncated upload boundary, vanished/stalled peer, socket reset, backend begin/write/end failures");
  puts("PASS: OTA roaming interlock, deferred recovery, post-error resume, successful-end/lost-ACK control");
}
