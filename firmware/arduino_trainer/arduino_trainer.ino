// firmware/arduino_trainer/arduino_trainer.ino

#include <Arduino.h>

// board information
static const char* FW_NAME = "pocketsat-trainer-mk0";
static const char* FW_VERSION = "0.1.0";
static const uint32_t HEARTBEAT_MS = 1000;
static uint32_t last_heartbeat = 0;
static bool led_state = false;

// comm
static constexpr size_t CMD_BUF_SIZE = 64;
static char cmd_buf[CMD_BUF_SIZE];
static size_t cmd_len = 0;

// state machine management
enum class Mode { BOOT, SAFE, NOMINAL, PAYLOAD };
Mode state = Mode::BOOT;

// utils
template<typename T>
void print_kv(const char* key, T value) {
  Serial.print(key);
  Serial.print('=');
  Serial.print(value);
}

const char* mode_name(Mode mode){
  switch (mode){
    case Mode::BOOT: return "BOOT";
    case Mode::SAFE: return "SAFE";
    case Mode::NOMINAL: return "NOMINAL";
    case Mode::PAYLOAD: return "PAYLOAD";
    default: return "UNKNOWN";
  }
}

void print_boot_banner() {
  Serial.println("EVT,BOOT_BEGIN");
  Serial.print("TLM,BOOT,fw="); Serial.print(FW_NAME);
  Serial.print(",version="); Serial.print(FW_VERSION);
  Serial.print(",mode=BOOT");
  Serial.print(",tick="); Serial.println(millis());
  Serial.println("EVT,BOOT_COMPLETE");
}

void tlm_health(uint32_t tick, Mode mode, const char* status) {
  Serial.print("TLM,HEALTH,");
  print_kv("tick", tick);
  Serial.print(',');
  print_kv("mode", mode_name(mode));
  Serial.print(',');
  print_kv("status", status);
  Serial.println();
}

void poll_serial_commands() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    
    if (c == '\r') continue;
    
    if (c == '\n') {
      cmd_buf[cmd_len] = '\0';
      handle_command(cmd_buf);
      cmd_len = 0;
      return;    
    }
    
    if (cmd_len + 1 >= CMD_BUF_SIZE) {
      cmd_len = 0;
      Serial.println("REJECT,LINE_TOO_LONG");
      return;    
    }
    
    cmd_buf[cmd_len++] = c;
  }
}

void request_mode(Mode mode, const char* reason){
  state = mode;
  return;
}

void handle_command(const char* cmd) {
  if (strcmp(cmd, "PING") == 0) {
    Serial.println("ACK,PING");
    return;
  }
  
  if (strcmp(cmd, "GET_STATUS") == 0) {
    Serial.println("ACK,GET_STATUS");
    tlm_health(millis(), state, "OK");
    return;
  }

  if (strncmp(cmd, "SET_MODE ", 9) == 0) {
    const char* arg = cmd + 9;
    if (strcmp(arg, "SAFE") == 0) {
      request_mode(Mode::SAFE, "command");
      Serial.println("ACK,SET_MODE,SAFE");
      return;
    }
    if (strcmp(arg, "NOMINAL") == 0) {
      request_mode(Mode::NOMINAL, "command");
      Serial.println("ACK,SET_MODE,NOMINAL");
      return;
    }
    if (strcmp(arg, "PAYLOAD") == 0) {
      request_mode(Mode::PAYLOAD, "command");
      Serial.println("ACK,SET_MODE,PAYLOAD");
      return;
    }
    Serial.println("REJECT,SET_MODE,BAD_ARGUMENT");
    return;
  }
  Serial.println("REJECT,UNKNOWN_COMMAND");
}

// Main logic
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to attach during early bring-up.
  state = Mode::BOOT;
  print_boot_banner();
}

void loop() {
  //const uint32_t now = millis();
  //if (now - last_heartbeat >= HEARTBEAT_MS) {
    //last_heartbeat = now;
    //led_state = !led_state;
    //digitalWrite(LED_BUILTIN, led_state ? HIGH : LOW);
    
    //Mode status = Mode::SAFE;
    //tlm_health(now, status, "OK");
  //}
  poll_serial_commands();
}
