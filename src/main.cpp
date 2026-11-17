// JCV Karaoke Pro — ESP32 Bluetooth button pad ("Karaoke Pad").
// SPDX-License-Identifier: MIT — Copyright (c) 2026 JCV Softwares
//
// A Bluetooth LE controller for a JCV Karaoke Pro Host (Android): the pad
// is a BLE peripheral with the Room API's service; the Host scans, connects,
// asks once to allow it, then every button press arrives as the same JSON
// actions as POST /api/control. Protocol: cloud/site/api.html#bluetooth
// (source of truth: lib/remote/ble_protocol.dart).
//
// Works three ways:
//   1. Mock / no hardware: type keys in the Serial Monitor (115200), see
//      printHelp() — e.g. "026693" + Enter keys in a song and presses OK.
//   2. 4x4 membrane keypad: 0-9, * = Back, # = OK, A = Next, B = Stop,
//      C = Melody, D = Mic.
//   3. Four extra buttons (to GND): Vol+, Vol-, Tempo+, Tempo- (repeat
//      while held).
// The on-board LED shows the padLink: slow blink = advertising, fast blink =
// waiting for the Host to allow it, on = ready, off flash = key sent.

#include <Arduino.h>
#include <NimBLEDevice.h>

// ——— Room API (BLE) ———————————————————————————————————————————————
static const char *SVC_UUID = "6b617261-6f6b-11ef-9b5e-0242ac120001";
static const char *IDENTITY_UUID = "6b617261-6f6b-11ef-9b5e-0242ac120002";
static const char *SESSION_UUID = "6b617261-6f6b-11ef-9b5e-0242ac120003";
static const char *CONTROL_UUID = "6b617261-6f6b-11ef-9b5e-0242ac120004";

// ——— Your pad ————————————————————————————————————————————————————
static const char *PAD_NAME = "Karaoke Pad";  // shown on the Host (≤ 48)

// 4x4 membrane keypad (rows = outputs, cols = inputs with pull-ups).
static const uint8_t ROW_PINS[4] = {13, 12, 14, 27};
static const uint8_t COL_PINS[4] = {26, 25, 33, 32};
static const char KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};
// Leave the keypad unplugged and it simply reads nothing.
static const bool USE_KEYPAD = true;

// Extra buttons: one leg to the pin, the other to GND.
struct DirectButton {
  uint8_t pin;
  const char *action;  // Room API action
  bool repeat;         // repeat while held
};
static const DirectButton BUTTONS[] = {
    {23, "vol+", true},
    {22, "vol-", true},
    {21, "tempo+", true},
    {19, "tempo-", true},
};

static const uint8_t LED_PIN = 2;  // on-board LED on most ESP32 dev boards
static const bool LED_ACTIVE_HIGH = true;

static const uint32_t DEBOUNCE_MS = 25;
static const uint32_t REPEAT_DELAY_MS = 450;
static const uint32_t REPEAT_EVERY_MS = 180;

// ——— State ————————————————————————————————————————————————————————
enum class Link { Advertising, Waiting, Ready, Refused };
static volatile Link padLink = Link::Advertising;
static volatile bool subscribed = false;
static NimBLECharacteristic *controlChr = nullptr;
static NimBLECharacteristic *sessionChr = nullptr;
static uint32_t flashUntil = 0;
static String lastError;

static void led(bool on) { digitalWrite(LED_PIN, (on == LED_ACTIVE_HIGH) ? HIGH : LOW); }

static const char *linkName(Link l) {
  switch (l) {
    case Link::Advertising: return "advertising (on the Host: Settings > Room > Bluetooth pads > scan)";
    case Link::Waiting: return "connected - waiting for the Host to allow this pad";
    case Link::Ready: return "ready - keys go to the Host";
    case Link::Refused: return "refused by the Host";
  }
  return "?";
}

// ——— Sending ———————————————————————————————————————————————————————
// One small JSON per press (the Host asks for MTU 185; these are < 60 B).
static void sendJson(const String &json) {
  if (controlChr == nullptr) return;
  if (padLink != Link::Ready) {
    Serial.printf("  (not sent: %s)\n", linkName(padLink));
    return;
  }
  if (!subscribed) {
    Serial.println("  (not sent: the Host hasn't subscribed yet)");
    return;
  }
  controlChr->setValue((const uint8_t *)json.c_str(), json.length());
  controlChr->notify();
  flashUntil = millis() + 60;
  Serial.printf("  -> %s\n", json.c_str());
}

static void sendAction(const char *action) {
  sendJson(String("{\"action\":\"") + action + "\"}");
}

static void sendDigit(char d) {
  sendJson(String("{\"action\":\"digit\",\"digit\":\"") + d + "\"}");
}

static void sendPlay(long code) {
  sendJson(String("{\"action\":\"play\",\"code\":") + code + "}");
}

// Keypad keys → Room API actions.
static void pressKey(char k) {
  if (k >= '0' && k <= '9') return sendDigit(k);
  switch (k) {
    case '*': return sendAction("back");
    case '#': return sendAction("ok");
    case 'A': return sendAction("next");
    case 'B': return sendAction("stop");
    case 'C': return sendAction("melody");
    case 'D': return sendAction("mic");
  }
}

// ——— BLE callbacks ———————————————————————————————————————————————————
class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, ble_gap_conn_desc *desc) override {
    padLink = Link::Waiting;
    Serial.printf("[ble] Host connected (%s)\n",
                  NimBLEAddress(desc->peer_ota_addr).toString().c_str());
  }
  void onDisconnect(NimBLEServer *) override {
    padLink = Link::Advertising;
    subscribed = false;
    Serial.println("[ble] Host disconnected - advertising again");
    NimBLEDevice::startAdvertising();
  }
  void onMTUChange(uint16_t mtu, ble_gap_conn_desc *) override {
    Serial.printf("[ble] MTU %u\n", mtu);
  }
};

// The Host writes the pad's status here, e.g.
// {"v":1,"ok":true,"role":"controller","pendingController":false,...}
class SessionCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) override {
    std::string v = c->getValue();
    String s(v.c_str());
    Serial.printf("[host] %s\n", s.c_str());
    if (s.indexOf("\"pendingController\":true") >= 0) {
      padLink = Link::Waiting;
    } else if (s.indexOf("\"ok\":true") >= 0 && s.indexOf("\"role\":\"controller\"") >= 0) {
      padLink = Link::Ready;
    } else if (s.indexOf("\"ok\":false") >= 0) {
      padLink = Link::Refused;
      int e = s.indexOf("\"error\":\"");
      lastError = e >= 0 ? s.substring(e + 9, s.indexOf('"', e + 9)) : "";
      if (lastError == "controllers_full") {
        Serial.println("[host] no free place for another remote (Host: max remotes)");
      }
    }
    Serial.printf("[pad] %s\n", linkName(padLink));
  }
};

class ControlCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *, ble_gap_conn_desc *, uint16_t subValue) override {
    subscribed = subValue != 0;
    Serial.printf("[ble] Host %s key presses\n", subscribed ? "listens to" : "stopped listening to");
  }
};

static void startBle() {
  NimBLEDevice::init(PAD_NAME);
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCb());
  NimBLEService *svc = server->createService(SVC_UUID);

  // Identity: the id is informational — the Host trusts the radio address.
  String mac = NimBLEDevice::getAddress().toString().c_str();
  mac.replace(":", "");
  String identity = String("{\"v\":1,\"deviceId\":\"esp32-") + mac +
                    "\",\"name\":\"" + PAD_NAME + "\"}";
  NimBLECharacteristic *idChr = svc->createCharacteristic(IDENTITY_UUID, NIMBLE_PROPERTY::READ);
  idChr->setValue((const uint8_t *)identity.c_str(), identity.length());

  sessionChr = svc->createCharacteristic(
      SESSION_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  sessionChr->setCallbacks(new SessionCb());

  controlChr = svc->createCharacteristic(CONTROL_UUID, NIMBLE_PROPERTY::NOTIFY);
  controlChr->setCallbacks(new ControlCb());

  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);  // the name goes in the scan response
  adv->start();
  Serial.printf("[ble] %s %s - %s\n", PAD_NAME, identity.c_str(), linkName(padLink));
}

// ——— Buttons ———————————————————————————————————————————————————————
static char scanKeypad() {
  if (!USE_KEYPAD) return 0;
  char found = 0;
  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(5);
    for (uint8_t c = 0; c < 4; c++) {
      if (digitalRead(COL_PINS[c]) == LOW) found = KEYS[r][c];
    }
    digitalWrite(ROW_PINS[r], HIGH);
  }
  return found;
}

static void pollKeypad() {
  static char stable = 0, lastRead = 0;
  static uint32_t changedAt = 0;
  char k = scanKeypad();
  uint32_t now = millis();
  if (k != lastRead) {
    lastRead = k;
    changedAt = now;
  }
  if (now - changedAt < DEBOUNCE_MS || k == stable) return;
  stable = k;
  if (k != 0) {
    Serial.printf("[key] %c\n", k);
    pressKey(k);
  }
}

static void pollButtons() {
  static bool down[sizeof(BUTTONS) / sizeof(BUTTONS[0])] = {};
  static uint32_t since[sizeof(BUTTONS) / sizeof(BUTTONS[0])] = {};
  static uint32_t nextRepeat[sizeof(BUTTONS) / sizeof(BUTTONS[0])] = {};
  uint32_t now = millis();
  for (size_t i = 0; i < sizeof(BUTTONS) / sizeof(BUTTONS[0]); i++) {
    bool pressed = digitalRead(BUTTONS[i].pin) == LOW;
    if (pressed && !down[i]) {
      if (since[i] == 0) since[i] = now;
      if (now - since[i] >= DEBOUNCE_MS) {
        down[i] = true;
        nextRepeat[i] = now + REPEAT_DELAY_MS;
        Serial.printf("[btn] %s\n", BUTTONS[i].action);
        sendAction(BUTTONS[i].action);
      }
    } else if (pressed && down[i] && BUTTONS[i].repeat && now >= nextRepeat[i]) {
      nextRepeat[i] = now + REPEAT_EVERY_MS;
      sendAction(BUTTONS[i].action);
    } else if (!pressed) {
      down[i] = false;
      since[i] = 0;
    }
  }
}

// ——— Serial mock ———————————————————————————————————————————————————
static void printHelp() {
  Serial.println();
  Serial.println("Karaoke Pad - type keys, then Enter:");
  Serial.println("  0-9        key in digits            026693  digits + OK");
  Serial.println("  o / b / x  OK / Back / Cancel       n / s   Next / Stop");
  Serial.println("  m / i      Melody / Mic             + / -   Volume up / down");
  Serial.println("  ] / [      Tempo up / down          u d l r  TV navigation");
  Serial.println("  F / f      lyrics bigger / smaller  G / g   next / previous background");
  Serial.println("  p026693    play song 026693 now     ?       this help, status");
  Serial.println();
}

static void runCommand(String line) {
  line.trim();
  if (line.length() == 0) return;
  if (line == "?" || line == "h" || line == "help") {
    printHelp();
    Serial.printf("[pad] %s\n", linkName(padLink));
    return;
  }
  if (line[0] == 'p' && line.length() > 1) return sendPlay(line.substring(1).toInt());
  bool allDigits = true;
  for (size_t i = 0; i < line.length(); i++) allDigits &= isDigit(line[i]);
  if (allDigits) {
    for (size_t i = 0; i < line.length(); i++) {
      sendDigit(line[i]);
      delay(120);
    }
    if (line.length() >= 4) sendAction("ok");  // a whole song number: confirm it
    return;
  }
  for (size_t i = 0; i < line.length(); i++) {
    switch (line[i]) {
      case 'o': sendAction("ok"); break;
      case 'b': sendAction("back"); break;
      case 'x': sendAction("cancel"); break;
      case 'n': sendAction("next"); break;
      case 's': sendAction("stop"); break;
      case 'm': sendAction("melody"); break;
      case 'i': sendAction("mic"); break;
      case '+': sendAction("vol+"); break;
      case '-': sendAction("vol-"); break;
      case ']': sendAction("tempo+"); break;
      case '[': sendAction("tempo-"); break;
      case 'u': sendAction("up"); break;
      case 'd': sendAction("down"); break;
      case 'l': sendAction("left"); break;
      case 'r': sendAction("right"); break;
      case 'F': sendAction("font+"); break;
      case 'f': sendAction("font-"); break;
      case 'G': sendAction("bg_next"); break;
      case 'g': sendAction("bg_prev"); break;
      default: Serial.printf("  unknown key '%c' (? for help)\n", line[i]);
    }
    delay(80);
  }
}

static void pollSerial() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      runCommand(line);
      line = "";
    } else if (line.length() < 32) {
      line += c;
    }
  }
}

// ——— LED ———————————————————————————————————————————————————————————
static void updateLed() {
  uint32_t now = millis();
  if (now < flashUntil) return led(false);
  switch (padLink) {
    case Link::Advertising: return led((now / 600) % 2 == 0);
    case Link::Waiting: return led((now / 150) % 2 == 0);
    case Link::Ready: return led(true);
    case Link::Refused: return led((now / 100) % 8 == 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED_PIN, OUTPUT);
  if (USE_KEYPAD) {
    for (uint8_t r = 0; r < 4; r++) {
      pinMode(ROW_PINS[r], OUTPUT);
      digitalWrite(ROW_PINS[r], HIGH);
    }
    for (uint8_t c = 0; c < 4; c++) pinMode(COL_PINS[c], INPUT_PULLUP);
  }
  for (const auto &b : BUTTONS) pinMode(b.pin, INPUT_PULLUP);
  startBle();
  printHelp();
}

void loop() {
  pollSerial();
  pollKeypad();
  pollButtons();
  updateLed();
  delay(2);
}
