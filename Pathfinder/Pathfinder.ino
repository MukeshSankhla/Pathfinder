#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>

#include "DFRobot_GNSS.h"
#include "DFRobot_BMM350.h"
#include "DFRobot_GDL.h"

#include "WebUI.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define TFT_DC  D2
#define TFT_CS  D6
#define TFT_RST D3

#define BUZZER_PIN 4

#define ENCODER_A 6
#define ENCODER_B 7
#define ENCODER_BTN 2

// ==========================================
// CUSTOM COLORS - Modern UI Palette
// ==========================================
// #define COLOR_BACKGROUND   0x0841    // Deep blue-black
// #define COLOR_PRIMARY      0x07FF    // Cyan
// #define COLOR_SUCCESS      0x07E0    // Bright green
// #define COLOR_WARNING      0xFD20    // Orange
// #define COLOR_DANGER       0xF800    // Red
// #define COLOR_TEXT_PRIMARY 0xFFFF    // White
// #define COLOR_TEXT_SECONDARY 0x8410  // Light gray
// #define COLOR_SURFACE      0x2124    // Dark gray surface
// #define COLOR_ACCENT       0x051D    // Dark cyan
// #define COLOR_NORTH        0xF800    // Red for North
// #define COLOR_COMPASS_RING 0x4208    // Medium gray

#define COLOR_BACKGROUND     0x0000    // Pure Black
#define COLOR_PRIMARY        0xFD20    // Phosphor Orange
#define COLOR_SUCCESS        0xFD20    // Phosphor Orange
#define COLOR_WARNING        0xFFE0    // Yellow
#define COLOR_DANGER         0xF800    // Red
#define COLOR_TEXT_PRIMARY   0xFD20    // Phosphor Orange
#define COLOR_TEXT_SECONDARY 0xFB00    // Dimmer Orange
#define COLOR_SURFACE        0x7800    // Very Dark Orange
#define COLOR_ACCENT         0xFD20    // Phosphor Orange
#define COLOR_NORTH          0xF800    // Red for North
#define COLOR_COMPASS_RING   0xFB00    // Dimmer Orange

// ==========================================
// HARDWARE OBJECTS
// ==========================================
DFRobot_ST7789_240x240_HW_SPI screen(/*dc=*/TFT_DC,/*cs=*/TFT_CS,/*rst=*/TFT_RST);
DFRobot_GNSS_I2C gnss(&Wire ,GNSS_DEVICE_ADDR);
DFRobot_BMM350_I2C bmm350(&Wire, I2C_ADDRESS);

WebServer server(80);
Preferences preferences;

// ==========================================
// CALIBRATION OFFSETS
// PASTE OUTPUT FROM CALIBRATION SCRIPT HERE
// ==========================================
float magXoffset = 21.000;
float magYoffset = -11.850;
float magZoffset = -395.250;
float magXscale = 0.683;
float magYscale = 0.710;
float magZscale = 7.810;

// ==========================================
// COMPASS ORIENTATION SETTINGS
// ==========================================
int heading_offset = 180;
bool reverse_compass = true;

// ==========================================
// CONSTANTS & STRUCTURES
// ==========================================
#define MAX_WAYPOINTS 20
#define ARRIVAL_THRESHOLD 5.0 // Meters

// Buzzer thresholds for off-course warning
#define OFF_COURSE_ANGLE     45.0   // Degrees off-target to start buzzing
#define SEVERE_OFF_ANGLE     100.0  // Degrees off-target for faster buzzing
#define BUZZER_FREQ          800    // Hz - low, soft tone for off-course warning
#define BUZZER_BEEP_MS       60     // Short beep duration (ms)
#define BUZZER_INTERVAL_MILD 2000   // Interval between beeps when mildly off-course
#define BUZZER_INTERVAL_BAD  800    // Interval between beeps when severely off-course

// UI sound frequencies & durations
#define SND_CLICK_FREQ       1200   // Hz - quick, subtle tick
#define SND_CLICK_MS         30     // Very short click
#define SND_SCROLL_FREQ      600    // Hz - soft low tick
#define SND_SCROLL_MS        15     // Ultra-short scroll tick
#define SND_MUTE_ON_FREQ     400    // Hz - descending tone (muted)
#define SND_MUTE_OFF_FREQ    1000   // Hz - ascending tone (unmuted)
#define SND_TOGGLE_MS        80     // Duration per note in mute jingle

struct Waypoint {
  char name[20];
  double lat;
  double lon;
  bool active;
};

Waypoint waypoints[MAX_WAYPOINTS];
int waypointCount = 0;

enum SystemState {
  STATE_GNSS_STARTUP,
  STATE_COMPASS,
  STATE_DETAILED,
  STATE_MENU,
  STATE_SETTINGS,
  STATE_CALIBRATION
};
SystemState currentState = STATE_GNSS_STARTUP;

// ==========================================
// ENCODER VARIABLES
// ==========================================
volatile int encoderPos = 0;
int lastEncoded = 0;
int virtualEncoderPos = 0;
int lastVirtualEncoderPos = 0;

bool buttonSingleClick = false;
bool buttonDoubleClick = false;
bool buttonTripleClick = false;

int selectedWaypointIndex = -1;
int menuLastScroll = -1;
int currentMenuScroll = 0;

// Buzzer State
unsigned long lastBuzzerTime = 0;
bool buzzerActive = false;
unsigned long buzzerOnTime = 0;
bool buzzerMuted = false;  // Global mute flag (persisted in NVS)

// Calibration State Variables
int calPhase = 0;              // 0=positioning, 1=computing, 2=confirm
int calDirIndex = 0;           // Current direction index (0-7)
#define CAL_NUM_DIRS 8
const char* calDirNames[CAL_NUM_DIRS] = {"N","NE","E","SE","S","SW","W","NW"};
float calDirAngles[CAL_NUM_DIRS] = {0, 45, 90, 135, 180, 225, 270, 315};
float calCapturedX[CAL_NUM_DIRS];
float calCapturedY[CAL_NUM_DIRS];
float calCapturedZ[CAL_NUM_DIRS];
bool  calCaptured[CAL_NUM_DIRS];
float calNewXoff, calNewYoff, calNewZoff;
float calNewXscl, calNewYscl, calNewZscl;

// Settings menu variables
int settingsMenuScroll = 0;
int settingsMenuLast = -1;

// Navigation Data
double currentLat = 0;
double currentLon = 0;
double currentSog = 0;
double currentAlt = 0;
int satCount = 0;
float currentHeading = 0;

// ==========================================
// ISR FOR ENCODER - VERIFIED LOGIC
// ==========================================
void IRAM_ATTR readEncoder() {
  int MSB = digitalRead(ENCODER_A);
  int LSB = digitalRead(ENCODER_B);
  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  // Standard quadrature encoder state machine
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;
  lastEncoded = encoded;
}

// ==========================================
// ISR FOR BUTTON - DEBOUNCED
// ==========================================
volatile unsigned long lastBtnIsrTime = 0;
volatile int isrClickCount = 0;

void IRAM_ATTR readButton() {
  unsigned long now = millis();
  if (now - lastBtnIsrTime > 50) {  // 50ms debounce
    isrClickCount++;
    lastBtnIsrTime = now;
  }
}

void processClicks() {
  if (isrClickCount > 0) {
    if (millis() - lastBtnIsrTime > 400) {  // Increased to 400ms for easier double-clicking
      if (isrClickCount == 1) {
        buttonSingleClick = true;
        Serial.println("Single click detected");
      }
      else if (isrClickCount == 2) {
        buttonDoubleClick = true;
        Serial.println("Double click detected");
      }
      else if (isrClickCount >= 3) {
        buttonTripleClick = true;
        Serial.println("Triple click detected");
      }
      isrClickCount = 0;
    }
  }
}

// ==========================================
// NVS STORAGE (WAYPOINTS)
// ==========================================
void loadWaypoints() {
  preferences.begin("waypoints", false);
  waypointCount = 0;
  for (int i = 0; i < MAX_WAYPOINTS; i++) {
    String key = "wp_" + String(i);
    if (preferences.getBytesLength(key.c_str()) == sizeof(Waypoint)) {
      preferences.getBytes(key.c_str(), &waypoints[i], sizeof(Waypoint));
      if (waypoints[i].active) waypointCount++;
    } else {
      waypoints[i].active = false;
    }
  }
  preferences.end();
}

void saveWaypoint(int index) {
  preferences.begin("waypoints", false);
  String key = "wp_" + String(index);
  preferences.putBytes(key.c_str(), &waypoints[index], sizeof(Waypoint));
  preferences.end();
}

// ==========================================
// WEB SERVER HANDLERS
// ==========================================
void handleRoot() { server.send(200, "text/html", htmlPage); }

void handleGetWaypoints() {
  String json = "["; 
  bool first = true;
  for (int i = 0; i < MAX_WAYPOINTS; i++) {
    if (waypoints[i].active) {
      if (!first) json += ",";
      json += "{\"id\":" + String(i) + ",\"n\":\"" + String(waypoints[i].name) + "\",\"lat\":" + String(waypoints[i].lat, 6) + ",\"lon\":" + String(waypoints[i].lon, 6) + "}";
      first = false;
    }
  }
  json += "]"; 
  server.send(200, "application/json", json);
}

void handlePostWaypoint() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    int nStart = body.indexOf("\"n\":\"") + 5; 
    int nEnd = body.indexOf("\"", nStart);
    String n = body.substring(nStart, nEnd);
    int latStart = body.indexOf("\"lat\":") + 6; 
    int latEnd = body.indexOf(",", latStart);
    double lat = body.substring(latStart, latEnd).toDouble();
    int lonStart = body.indexOf("\"lon\":") + 6; 
    int lonEnd = body.indexOf("}", lonStart);
    double lon = body.substring(lonStart, lonEnd).toDouble();
    
    for (int i = 0; i < MAX_WAYPOINTS; i++) {
      if (!waypoints[i].active) {
        n.toCharArray(waypoints[i].name, 20); 
        waypoints[i].lat = lat; 
        waypoints[i].lon = lon;
        waypoints[i].active = true; 
        saveWaypoint(i); 
        waypointCount++;
        server.send(200, "text/plain", "OK"); 
        return;
      }
    }
    server.send(500, "text/plain", "Full");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleDeleteWaypoint() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if (id >= 0 && id < MAX_WAYPOINTS) {
      waypoints[id].active = false; 
      saveWaypoint(id); 
      waypointCount--;
      if(selectedWaypointIndex == id) selectedWaypointIndex = -1;
      server.send(200, "text/plain", "OK"); 
      return;
    }
  }
  server.send(400, "text/plain", "Bad ID");
}

// ==========================================
// MATH FUNCTIONS
// ==========================================
float calculate_distance(double lat1, double lon1, double lat2, double lon2) {
  float dlat = (lat2 - lat1) * M_PI / 180.0;
  float dlon = (lon2 - lon1) * M_PI / 180.0;
  lat1 = lat1 * M_PI / 180.0;
  lat2 = lat2 * M_PI / 180.0;
  float a = sin(dlat/2) * sin(dlat/2) + cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  return c * 6371000.0; 
}

float calculate_bearing(double lat1, double lon1, double lat2, double lon2) {
  lat1 = lat1 * M_PI / 180.0; 
  lon1 = lon1 * M_PI / 180.0;
  lat2 = lat2 * M_PI / 180.0; 
  lon2 = lon2 * M_PI / 180.0;
  float dlon = lon2 - lon1;
  float y = sin(dlon) * cos(lat2);
  float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
  float initial_bearing = atan2(y, x) * 180.0 / M_PI;
  return fmod((initial_bearing + 360.0), 360.0);
}

float calculate_heading(sBmm350MagData_t magData) {
  float x = magData.x - magXoffset;
  float y = magData.y - magYoffset;
  float z = magData.z - magZoffset;
  x *= magXscale; 
  y *= magYscale; 
  z *= magZscale;
  
  float heading = (atan2(y, x) * 180.0) / M_PI;
  if (heading < 0) heading += 360.0;
  
  if (reverse_compass) {
    heading = 360.0 - heading;
  }
  heading = fmod(heading + heading_offset + 360.0, 360.0);
  
  return heading;
}

// ==========================================
// BUZZER FUNCTIONS
// ==========================================

// Raw tone helpers (ignore mute — used internally)
void buzzerToneRaw(int freq, int ms) {
  ledcWriteTone(BUZZER_PIN, freq);
  delay(ms);
  ledcWriteTone(BUZZER_PIN, 0);
}

// Start a soft, short beep (for off-course warning timer)
void buzzerBeep() {
  if (buzzerMuted) return;
  ledcWriteTone(BUZZER_PIN, BUZZER_FREQ);
  buzzerActive = true;
  buzzerOnTime = millis();
}

// Stop the beep
void buzzerOff() {
  ledcWriteTone(BUZZER_PIN, 0);
  buzzerActive = false;
}

// --- UI Sound Effects ---

// Quick click sound (button press)
void buzzerClick() {
  if (buzzerMuted) return;
  ledcWriteTone(BUZZER_PIN, SND_CLICK_FREQ);
  delay(SND_CLICK_MS);
  ledcWriteTone(BUZZER_PIN, 0);
}

// Subtle scroll tick (menu hover/rotate)
void buzzerScroll() {
  if (buzzerMuted) return;
  ledcWriteTone(BUZZER_PIN, SND_SCROLL_FREQ);
  delay(SND_SCROLL_MS);
  ledcWriteTone(BUZZER_PIN, 0);
}

// Mute toggle jingle (always plays, even when muting)
void buzzerMuteToggle(bool nowMuted) {
  if (nowMuted) {
    // Descending two-note: "going silent"
    buzzerToneRaw(SND_MUTE_OFF_FREQ, SND_TOGGLE_MS);
    delay(40);
    buzzerToneRaw(SND_MUTE_ON_FREQ, SND_TOGGLE_MS);
  } else {
    // Ascending two-note: "sound on"
    buzzerToneRaw(SND_MUTE_ON_FREQ, SND_TOGGLE_MS);
    delay(40);
    buzzerToneRaw(SND_MUTE_OFF_FREQ, SND_TOGGLE_MS);
  }
}

// Save mute state to NVS
void saveMuteState() {
  preferences.begin("buzzer", false);
  preferences.putBool("muted", buzzerMuted);
  preferences.end();
}

// Load mute state from NVS
void loadMuteState() {
  preferences.begin("buzzer", true);  // read-only
  buzzerMuted = preferences.getBool("muted", false);
  preferences.end();
}

// ==========================================
// NVS STORAGE (CALIBRATION)
// ==========================================
void loadCalibration() {
  preferences.begin("magcal", true);
  magXoffset = preferences.getFloat("xOff", 21.000);
  magYoffset = preferences.getFloat("yOff", -11.850);
  magZoffset = preferences.getFloat("zOff", -395.250);
  magXscale  = preferences.getFloat("xScl", 0.683);
  magYscale  = preferences.getFloat("yScl", 0.710);
  magZscale  = preferences.getFloat("zScl", 7.810);
  preferences.end();
  Serial.println("Calibration loaded from NVS:");
  Serial.printf("  Offsets: X=%.3f Y=%.3f Z=%.3f\n", magXoffset, magYoffset, magZoffset);
  Serial.printf("  Scales:  X=%.3f Y=%.3f Z=%.3f\n", magXscale, magYscale, magZscale);
}

void saveCalibration() {
  preferences.begin("magcal", false);
  preferences.putFloat("xOff", magXoffset);
  preferences.putFloat("yOff", magYoffset);
  preferences.putFloat("zOff", magZoffset);
  preferences.putFloat("xScl", magXscale);
  preferences.putFloat("yScl", magYscale);
  preferences.putFloat("zScl", magZscale);
  preferences.end();
  Serial.println("Calibration saved to NVS!");
}

// Call every loop iteration to manage off-course beep timing
// relAngle: 0 = on-course, 180 = completely wrong direction
void updateBuzzer(float relAngle) {
  // Turn off beep after its duration expires
  if (buzzerActive && (millis() - buzzerOnTime >= BUZZER_BEEP_MS)) {
    buzzerOff();
  }

  if (buzzerMuted) return;

  // Determine if off-course
  float deviation = relAngle;
  if (deviation > 180.0) deviation = 360.0 - deviation;

  if (deviation < OFF_COURSE_ANGLE) {
    if (buzzerActive) buzzerOff();
    return;
  }

  // Pick interval based on severity
  unsigned long interval = (deviation >= SEVERE_OFF_ANGLE)
                           ? BUZZER_INTERVAL_BAD
                           : BUZZER_INTERVAL_MILD;

  if (!buzzerActive && (millis() - lastBuzzerTime >= interval)) {
    buzzerBeep();
    lastBuzzerTime = millis();
  }
}

// ==========================================
// UI DRAWING FUNCTIONS
// ==========================================

// Draw modern arrow with better styling
void drawNavigationArrow(float angle, int cx, int cy, int size, uint16_t color, uint16_t outlineColor = COLOR_TEXT_PRIMARY) {
  float rad = angle * M_PI / 180.0;
  
  // Arrow tip
  int tx = cx + size * sin(rad);
  int ty = cy - size * cos(rad);
  
  // Arrow base points (wider for better visibility)
  float b1 = rad + 135.0 * M_PI / 180.0;
  float b2 = rad - 135.0 * M_PI / 180.0;
  int bx1 = cx + (size * 0.5) * sin(b1);
  int by1 = cy - (size * 0.5) * cos(b1);
  int bx2 = cx + (size * 0.5) * sin(b2);
  int by2 = cy - (size * 0.5) * cos(b2);
  
  // Draw filled arrow
  screen.fillTriangle(tx, ty, bx1, by1, bx2, by2, color);
  
  // Add outline for depth
  if (outlineColor != color) {
    screen.drawTriangle(tx, ty, bx1, by1, bx2, by2, outlineColor);
  }
}

// Draw compass cardinal points with rotation
void drawCardinalPoints(int cx, int cy, int radius, float heading, uint16_t overrideColor = 0xFFFF) {
  const char* cardinals[] = {"N", "E", "S", "W"};
  
  screen.setTextSize(2);
  
  for(int i = 0; i < 4; i++) {
    float angle = (-90.0 - heading + (i * 90.0));
    float rad = angle * M_PI / 180.0;
    
    int textX = cx + (radius + 14) * cos(rad);
    int textY = cy + (radius + 14) * sin(rad);
    
    // Center the text
    textX -= 6;
    textY -= 8;
    
    if (overrideColor != 0xFFFF) {
      screen.setTextColor(overrideColor);
    } else {
      if (i == 0) screen.setTextColor(COLOR_NORTH);
      else screen.setTextColor(COLOR_TEXT_PRIMARY);
    }
    
    screen.setCursor(textX, textY);
    screen.print(cardinals[i]);
  }
}

// Draw compass tick marks
void drawCompassTicks(int cx, int cy, int radius, float heading, uint16_t color1 = COLOR_TEXT_SECONDARY, uint16_t color2 = COLOR_COMPASS_RING) {
  for(int deg = 0; deg < 360; deg += 10) {
    float angle = (-90.0 - heading + deg);
    float rad = angle * M_PI / 180.0;
    
    int tickLength = (deg % 30 == 0) ? 8 : 4;
    int x1 = cx + (radius - tickLength) * cos(rad);
    int y1 = cy + (radius - tickLength) * sin(rad);
    int x2 = cx + radius * cos(rad);
    int y2 = cy + radius * sin(rad);
    
    uint16_t tickColor = (deg % 30 == 0) ? color1 : color2;
    screen.drawLine(x1, y1, x2, y2, tickColor);
  }
}

// Draw modern status bar
void drawStatusBar(const char* title, uint16_t bgColor = COLOR_SURFACE) {
  screen.fillRect(0, 0, 240, 32, bgColor);
  screen.setTextSize(2);
  if (bgColor == COLOR_ACCENT || bgColor == COLOR_SUCCESS || bgColor == COLOR_PRIMARY) {
    screen.setTextColor(COLOR_BACKGROUND);
  } else {
    screen.setTextColor(COLOR_TEXT_PRIMARY);
  }
  
  // Center the title
  int titleWidth = strlen(title) * 12;
  int titleX = (240 - titleWidth) / 2;
  screen.setCursor(titleX, 8);
  screen.print(title);
}

// Draw info card
void drawInfoCard(int y, const char* label, const char* value, uint16_t valueColor = COLOR_TEXT_PRIMARY) {
  screen.setTextSize(1);
  screen.setTextColor(COLOR_TEXT_SECONDARY);
  screen.setCursor(10, y);
  screen.print(label);
  
  screen.setTextColor(valueColor);
  screen.setCursor(10, y + 12);
  screen.print(value);
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize screen with splash
  screen.begin();
  screen.fillScreen(COLOR_BACKGROUND);
  
  // Draw splash screen base text
  screen.setTextColor(COLOR_PRIMARY);
  screen.setTextSize(3);
  screen.setCursor(30, 148);
  screen.print("PATHFINDER");
  
  screen.setTextSize(2);
  screen.setTextColor(COLOR_TEXT_SECONDARY);
  screen.setCursor(6, 178);
  screen.print("www.makerbrains.com");

  // Load waypoints
  loadWaypoints();
  
  // Start WiFi AP
  WiFi.softAP("PATHFINDER", "X12342026");
  
  // Configure web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/waypoints", HTTP_GET, handleGetWaypoints);
  server.on("/api/waypoints", HTTP_POST, handlePostWaypoint);
  server.on("/api/waypoints", HTTP_DELETE, handleDeleteWaypoint);
  server.begin();

  // Configure encoder pins
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  
  // Initialize buzzer PWM (ESP32 core v3.x API)
  ledcAttach(BUZZER_PIN, BUZZER_FREQ, 8);  // 8-bit resolution
  ledcWrite(BUZZER_PIN, 0);  // Start silent

  // Load mute state from NVS
  loadMuteState();
  // Load calibration from NVS
  loadCalibration();

  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BTN), readButton, FALLING);

  // Initialize I2C sensors
  Wire.begin();
  
  if(gnss.begin()) {
    gnss.enablePower();
    gnss.setGnss(eGPS_BeiDou_GLONASS);
  }
  
  if(bmm350.begin() == 0){
    bmm350.setOperationMode(eBmm350NormalMode);
    bmm350.setPresetMode(BMM350_PRESETMODE_HIGHACCURACY, BMM350_DATA_RATE_25HZ);
    bmm350.setMeasurementXYZ();
  }

  // ==========================================
  // COOL BOOT ANIMATION
  // ==========================================
  int cx = 120, cy = 80, r = 56;
  screen.drawCircle(cx, cy, r, COLOR_ACCENT);
  
  for (int i = 0; i <= 360; i += 8) {
    float rad = i * M_PI / 180.0;
    float rad_tail = (i - 32) * M_PI / 180.0;
    
    // Draw radar sweep line
    screen.drawLine(cx, cy, cx + (r-2)*sin(rad), cy - (r-2)*cos(rad), COLOR_PRIMARY);
    // Erase tail
    screen.drawLine(cx, cy, cx + (r-2)*sin(rad_tail), cy - (r-2)*cos(rad_tail), COLOR_BACKGROUND);
    
    // Redraw crosshair & inner circle to repair erased pixels
    screen.drawLine(cx-r, cy, cx+r, cy, COLOR_ACCENT);
    screen.drawLine(cx, cy-r, cx, cy+r, COLOR_ACCENT);
    screen.drawCircle(cx, cy, r-12, COLOR_ACCENT);
    
    delay(25);
  }
  
  // Boot confirmation beep (only if not muted)
  if (!buzzerMuted) {
    ledcWriteTone(BUZZER_PIN, BUZZER_FREQ);
    delay(80);
    ledcWriteTone(BUZZER_PIN, 0);
  }

  currentState = STATE_GNSS_STARTUP;
}

// ==========================================
// STATE MACHINE
// ==========================================
void loop() {
  server.handleClient(); 
  processClicks();
  
  static int lastState = -1;
  bool stateChanged = ((int)currentState != lastState);
  lastState = currentState;

  // Read sensors
  satCount = gnss.getNumSatUsed();
  bmm350.getGeomagneticData();
  currentHeading = calculate_heading(bmm350.getGeomagneticData());
  
  if (satCount > 0) {
    sLonLat_t lat = gnss.getLat();
    sLonLat_t lon = gnss.getLon();
    currentLat = lat.latitudeDegree;
    currentLon = lon.lonitudeDegree;
    currentSog = gnss.getSog();
    currentAlt = gnss.getAlt();
  }

  virtualEncoderPos = encoderPos / 4;

  // TRIPLE CLICK - Toggle mute/unmute (works from any state)
  if (buttonTripleClick) {
    buttonTripleClick = false;
    buttonDoubleClick = false;
    buttonSingleClick = false;
    buzzerMuted = !buzzerMuted;
    saveMuteState();
    Serial.print("Buzzer ");
    Serial.println(buzzerMuted ? "MUTED" : "UNMUTED");
    buzzerMuteToggle(buzzerMuted);  // Always plays confirmation jingle
    return;
  }

  // DOUBLE CLICK - Opens menu from Compass or Detailed view only
  if (buttonDoubleClick && (currentState == STATE_COMPASS || currentState == STATE_DETAILED)) {
    Serial.println("Opening menu via double-click");
    buttonDoubleClick = false;
    buttonSingleClick = false;  // Clear single click flag to prevent conflicts
    buzzerClick();  // Click sound
    buzzerOff();    // Silence any active off-course beep
    currentState = STATE_MENU;
    lastVirtualEncoderPos = virtualEncoderPos;
    menuLastScroll = -1;
    screen.fillScreen(COLOR_BACKGROUND);
    return;  // Exit loop to prevent single click from triggering
  }

  // Debug: Print encoder position changes
  static int lastDebugPos = 0;
  if (virtualEncoderPos != lastDebugPos) {
    Serial.print("Encoder pos: ");
    Serial.println(virtualEncoderPos);
    lastDebugPos = virtualEncoderPos;
  }

  switch(currentState) {

    // ==========================================
    // GNSS STARTUP STATE
    // ==========================================
    case STATE_GNSS_STARTUP: {
      static unsigned long lastDraw = 0;
      static int scanAngle = 0;
      
      if (stateChanged) {
        screen.fillScreen(COLOR_BACKGROUND);
        drawStatusBar("ACQUIRING GNSS");
        
        // Draw static radar rings
        int cx = 120, cy = 100, r = 56;
        screen.drawCircle(cx, cy, r, COLOR_ACCENT);
        
        // Draw static texts
        screen.setTextSize(2);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(10, 200);
        screen.print("Need 3+ satellites");
      }
      
      if (millis() - lastDraw > 25) {
        lastDraw = millis();
        int cx = 120, cy = 100, r = 56;
        
        float rad = scanAngle * M_PI / 180.0;
        float rad_tail = (scanAngle - 32) * M_PI / 180.0;
        
        // Draw radar sweep line
        screen.drawLine(cx, cy, cx + (r-2)*sin(rad), cy - (r-2)*cos(rad), COLOR_PRIMARY);
        // Erase tail
        screen.drawLine(cx, cy, cx + (r-2)*sin(rad_tail), cy - (r-2)*cos(rad_tail), COLOR_BACKGROUND);
        
        // Redraw crosshair & inner circle to repair erased pixels
        screen.drawLine(cx-r, cy, cx+r, cy, COLOR_ACCENT);
        screen.drawLine(cx, cy-r, cx, cy+r, COLOR_ACCENT);
        screen.drawCircle(cx, cy, r-12, COLOR_ACCENT);
        
        scanAngle = (scanAngle + 8) % 360;
        
        // Satellite count
        screen.fillRect(50, 172, 170, 24, COLOR_BACKGROUND); // Erase old text
        screen.setTextSize(2);
        screen.setTextColor(COLOR_TEXT_PRIMARY);
        screen.setCursor(48, 174);
        screen.print("Satellites: ");
        
        if(satCount == 0) screen.setTextColor(COLOR_DANGER);
        else if(satCount < 3) screen.setTextColor(COLOR_WARNING);
        else screen.setTextColor(COLOR_SUCCESS);
        screen.print(satCount);

        // Transition when ready
        if (satCount >= 3) {
          delay(500);
          buttonSingleClick = false; 
          buttonDoubleClick = false;
          currentState = STATE_COMPASS;
        }
      }
      break;
    }

    // ==========================================
    // WAYPOINT MENU STATE
    // ==========================================
    case STATE_MENU: {
      // Handle encoder rotation
      if (virtualEncoderPos > lastVirtualEncoderPos) { 
        currentMenuScroll++; 
        lastVirtualEncoderPos = virtualEncoderPos;
        buzzerScroll();  // Menu hover sound
      }
      else if (virtualEncoderPos < lastVirtualEncoderPos) { 
        currentMenuScroll--; 
        lastVirtualEncoderPos = virtualEncoderPos;
        buzzerScroll();  // Menu hover sound
      }

      // Build active waypoint list
      int activeCount = 0; 
      int activeIndices[MAX_WAYPOINTS];
      for(int i = 0; i < MAX_WAYPOINTS; i++) { 
        if(waypoints[i].active) {
          activeIndices[activeCount++] = i; 
        }
      }

      // Wrap scroll position (includes Settings entry at the end)
      int totalMenuItems = activeCount + 1; // +1 for Settings
      if (currentMenuScroll < 0) currentMenuScroll = totalMenuItems - 1;
      if (currentMenuScroll >= totalMenuItems) currentMenuScroll = 0;

      static int lastActiveCount = -1;
      
      // Redraw menu when scroll position changes
      if (currentMenuScroll != menuLastScroll || activeCount != lastActiveCount) {
        menuLastScroll = currentMenuScroll; 
        lastActiveCount = activeCount;
        screen.fillScreen(COLOR_BACKGROUND);
        
        // Header
        drawStatusBar("SELECT TARGET", COLOR_PRIMARY);
        
        // Instructions
        screen.setTextSize(1);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(10, 220);
        screen.print("Rotate: scroll  Click: select");

        // Draw list items (up to 4 visible, includes waypoints + Settings)
        int visibleCount = min(4, totalMenuItems);
        
        for(int i = 0; i < visibleCount; i++) {
          int displayIdx = (currentMenuScroll + i) % totalMenuItems;
          int yPos = 42 + i * 42;
          
          if (i == 0) {
            // Selected item - highlighted
            screen.fillRoundRect(5, yPos, 230, 38, 6, COLOR_PRIMARY);
            screen.setTextColor(COLOR_BACKGROUND);
          } else {
            // Non-selected items
            screen.fillRoundRect(5, yPos, 230, 38, 6, COLOR_SURFACE);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
          }
          
          if (displayIdx < activeCount) {
            // --- Waypoint entry ---
            int actualIdx = activeIndices[displayIdx];
            float dist = calculate_distance(currentLat, currentLon, waypoints[actualIdx].lat, waypoints[actualIdx].lon);
            
            // Waypoint Name
            screen.setTextSize(2);
            screen.setCursor(15, yPos + 6);
            screen.print(waypoints[actualIdx].name);
            
            // Distance
            screen.setTextSize(1);
            screen.setCursor(15, yPos + 24);
            if(i != 0) screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.print("Dist: ");
            if (dist > 1000) {
              screen.print(dist/1000.0, 1);
              screen.print(" km");
            } else {
              screen.print((int)dist);
              screen.print(" m");
            }
          } else {
            // --- Settings entry ---
            screen.setTextSize(2);
            screen.setCursor(15, yPos + 6);
            screen.print("> Settings");
            screen.setTextSize(1);
            screen.setCursor(15, yPos + 24);
            if(i != 0) screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.print("Calibration & Config");
          }
          
          if (i == 0) {
            // Indicator arrow
            screen.fillTriangle(218, yPos + 12, 218, yPos + 26, 226, yPos + 19, COLOR_BACKGROUND);
          }
        }
        
        // Scroll indicator if more items than visible
        if(totalMenuItems > 4) {
          screen.setTextSize(1);
          screen.setTextColor(COLOR_TEXT_SECONDARY);
          screen.setCursor(180, 215);
          screen.print(currentMenuScroll + 1);
          screen.print("/");
          screen.print(totalMenuItems);
        }
      }

      // SINGLE CLICK - Select waypoint or enter Settings
      if (buttonSingleClick) {
        buttonSingleClick = false;
        buzzerClick();  // Click sound
        int selectedMenuIdx = currentMenuScroll % totalMenuItems;
        if (selectedMenuIdx < activeCount) {
          // Selected a waypoint
          selectedWaypointIndex = activeIndices[selectedMenuIdx];
          Serial.print("Selected waypoint: ");
          Serial.println(waypoints[selectedWaypointIndex].name);
          currentState = STATE_COMPASS;
        } else {
          // Selected Settings
          Serial.println("Entering Settings");
          settingsMenuScroll = 0;
          settingsMenuLast = -1;
          currentState = STATE_SETTINGS;
        }
        screen.fillScreen(COLOR_BACKGROUND);
      }
      
      // Note: Double click is handled globally before state machine
      break;
    }

    // ==========================================
    // COMPASS VIEW STATE
    // ==========================================
    case STATE_COMPASS: {
      static unsigned long lastNavDraw = 0;
      static float lastDrawnHeading = -999;
      static float lastRelAngle = -999;
      
      int cx = 120, cy = 80, r = 50;
      
      if (stateChanged) {
        screen.fillScreen(COLOR_BACKGROUND);
        
        // Draw static outer rings
        screen.drawCircle(cx, cy, r + 2, COLOR_PRIMARY);
        screen.drawCircle(cx, cy, r + 1, COLOR_PRIMARY);
        screen.drawCircle(cx, cy, r, COLOR_COMPASS_RING);
        screen.drawCircle(cx, cy, r - 15, COLOR_COMPASS_RING);
        screen.fillCircle(cx, cy, 3, COLOR_COMPASS_RING);
        
        lastDrawnHeading = -999;
        lastRelAngle = -999;
      }
      
      if (millis() - lastNavDraw > 100) {  // Smooth 10fps
        lastNavDraw = millis();
        
        // Erase old dynamic elements if heading changed
        if (lastDrawnHeading != -999 && abs(currentHeading - lastDrawnHeading) > 0.1) {
          drawCompassTicks(cx, cy, r, lastDrawnHeading, COLOR_BACKGROUND, COLOR_BACKGROUND);
          drawCardinalPoints(cx, cy, r, lastDrawnHeading, COLOR_BACKGROUND);
        }
        
        // Draw new dynamic elements
        if (lastDrawnHeading == -999 || abs(currentHeading - lastDrawnHeading) > 0.1) {
          drawCompassTicks(cx, cy, r, currentHeading);
          drawCardinalPoints(cx, cy, r, currentHeading, 0xFFFF);
          lastDrawnHeading = currentHeading;
        }
        
        // Target waypoint indicator
        if (selectedWaypointIndex != -1 && selectedWaypointIndex < MAX_WAYPOINTS) {
          Waypoint dest = waypoints[selectedWaypointIndex];
          float dist = calculate_distance(currentLat, currentLon, dest.lat, dest.lon);
          
          if (stateChanged) {
            // Draw Top info bar only on state change to avoid flicker
            screen.fillRoundRect(5, 160, 230, 38, 4, COLOR_SURFACE);
            screen.setTextSize(1);
            screen.setTextColor(COLOR_PRIMARY);
            screen.setCursor(10, 164);
            screen.print("TARGET:");
            screen.setTextSize(2);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(10, 176);
            screen.print(dest.name);
          }
          
          if (dist < ARRIVAL_THRESHOLD) {
            // ARRIVED — silence buzzer
            buzzerOff();
            
            screen.fillCircle(cx, cy, 50, COLOR_SUCCESS);
            screen.setTextSize(2);
            screen.setTextColor(COLOR_BACKGROUND);
            screen.setCursor(cx - 45, cy - 8);
            screen.print("ARRIVED");
            
            // Bottom info (redraw bg to prevent flicker over old text)
            screen.setTextSize(2);
            screen.setTextColor(COLOR_BACKGROUND);
            screen.setCursor(55, 170);
            screen.print("< D");
            screen.print((int)dist);
            screen.print("m >");
          } else {
            float bearing = calculate_bearing(currentLat, currentLon, dest.lat, dest.lon);
            float relAngle = fmod((bearing - currentHeading + 360.0), 360.0);
            
            // Update buzzer for off-course warning
            updateBuzzer(relAngle);
            
            // Erase old arrow if angle changed significantly
            if (lastRelAngle != -999 && abs(relAngle - lastRelAngle) > 0.5) {
              drawNavigationArrow(lastRelAngle, cx, cy, 30, COLOR_BACKGROUND, COLOR_BACKGROUND);
            }
            
            // Draw new arrow
            if (lastRelAngle == -999 || abs(relAngle - lastRelAngle) > 0.5) {
              drawNavigationArrow(relAngle, cx, cy, 30, COLOR_SUCCESS);
              lastRelAngle = relAngle;
            }
            
            // Distance info bar at bottom (redraw bg to prevent text overlap)
            screen.fillRoundRect(5, 204, 230, 38, 4, COLOR_SURFACE);
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(10, 208);
            screen.print("Target Distance:");
            screen.setTextSize(2);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(10, 220);
            if (dist > 1000) {
              screen.print(dist/1000.0, 1); 
              screen.print(" km"); 
            } else {
              screen.print((int)dist); 
              screen.print(" m"); 
            }
            
            // Bearing
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(124, 208);
            screen.print("Bearing:");
            screen.setTextSize(2);
            screen.setCursor(124, 220);
            screen.print((int)bearing);
            screen.print((char)247);
          }
        } else {
          if (stateChanged) {
            screen.setTextSize(2);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(5, 170);
            screen.print("Click for info");
            screen.setCursor(5, 200);
            screen.print("Double click to set target");
          }
        }
        
        // Current heading display (redraw bg to prevent text overlap)
        screen.fillRoundRect(170, 10, 56, 34, 4, COLOR_SURFACE);
        screen.setTextSize(1);
        screen.setTextColor(COLOR_TEXT_PRIMARY);
        screen.setCursor(174, 14);
        screen.print("Heading");
        screen.setTextSize(2);
        screen.setCursor(174, 24);
        screen.print((int)currentHeading);
        screen.print((char)247);
        
        // Satellite count display in compass
        screen.fillRoundRect(10, 10, 56, 34, 4, COLOR_SURFACE);
        screen.setTextSize(1);
        if(satCount == 0) screen.setTextColor(COLOR_DANGER);
        else if(satCount < 3) screen.setTextColor(COLOR_WARNING);
        else screen.setTextColor(COLOR_SUCCESS);
        screen.setCursor(15, 14);
        screen.print("No.Of S");
        screen.setTextSize(2);
        screen.setCursor(15, 24);
        screen.print(satCount);
      }

      // SINGLE CLICK - Switch to detailed info view
      if (buttonSingleClick) {
        Serial.println("Switching to detailed view");
        buttonSingleClick = false;
        buzzerClick();  // Click sound
        buzzerOff();    // Silence any active off-course beep
        currentState = STATE_DETAILED;
      }
      
      // Note: Double click handled globally to open menu
      break;
    }

    // ==========================================
    // DETAILED INFO STATE
    // ==========================================
    case STATE_DETAILED: {
      static unsigned long lastDetDraw = 0;
      
      if (stateChanged) {
        screen.fillScreen(COLOR_BACKGROUND);
        drawStatusBar("TELEMETRY", COLOR_ACCENT);
      }
      
      if (millis() - lastDetDraw > 500) {
        lastDetDraw = millis();
        // Removed global fillScreen to prevent flicker
        
        
        // GPS Status Card
        screen.fillRoundRect(5, 38, 230, 45, 6, COLOR_SURFACE);
        screen.setTextSize(1);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(12, 43);
        screen.print("GPS STATUS");
        
        screen.setTextColor(COLOR_TEXT_PRIMARY);
        screen.setCursor(12, 56);
        screen.print("Satellites: ");
        if(satCount >= 6) screen.setTextColor(COLOR_SUCCESS);
        else if(satCount >= 3) screen.setTextColor(COLOR_WARNING);
        else screen.setTextColor(COLOR_DANGER);
        screen.print(satCount);
        
        screen.setTextColor(COLOR_TEXT_PRIMARY);
        screen.setCursor(12, 68);
        screen.print("Speed: ");
        screen.print(currentSog, 1);
        screen.print(" kn");
        
        // Navigation Card
        screen.fillRoundRect(5, 88, 230, 45, 6, COLOR_SURFACE);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(12, 93);
        screen.print("NAVIGATION");
        
        screen.setTextColor(COLOR_TEXT_PRIMARY);
        screen.setCursor(12, 106);
        screen.print("Heading: ");
        screen.print(currentHeading, 1);
        screen.print((char)247);
        
        screen.setCursor(12, 118);
        screen.print("Altitude: ");
        screen.print(currentAlt, 1);
        screen.print(" m");
        
        // Position Card
        screen.fillRoundRect(5, 138, 230, 35, 6, COLOR_SURFACE);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(12, 143);
        screen.print("POSITION");
        
        screen.setTextColor(COLOR_PRIMARY);
        screen.setTextSize(1);
        screen.setCursor(12, 156);
        screen.print(currentLat, 5);
        screen.setCursor(124, 156);
        screen.print(currentLon, 5);

        // Target Card (if selected)
        if (selectedWaypointIndex != -1) {
          Waypoint dest = waypoints[selectedWaypointIndex];
          
          screen.fillRoundRect(5, 178, 230, 35, 6, COLOR_SURFACE);
          screen.setTextColor(COLOR_TEXT_SECONDARY);
          screen.setCursor(12, 183);
          screen.print("TARGET: ");
          screen.setTextColor(COLOR_SUCCESS);
          screen.print(dest.name);
          
          screen.setTextColor(COLOR_PRIMARY);
          screen.setCursor(12, 196);
          screen.print(dest.lat, 5);
          screen.setCursor(124, 196);
          screen.print(dest.lon, 5);
        }
        
        // Instructions
        screen.setTextSize(1);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(5, 225);
        screen.print("Click: compass  2x-Click: menu");
      }

      // SINGLE CLICK - Switch back to compass view
      if (buttonSingleClick) {
        Serial.println("Switching to compass view");
        buttonSingleClick = false;
        buzzerClick();  // Click sound
        currentState = STATE_COMPASS;
      }
      
      // Note: Double click handled globally to open menu
      break;
    }

    // ==========================================
    // SETTINGS MENU STATE
    // ==========================================
    case STATE_SETTINGS: {
      // Handle encoder rotation
      if (virtualEncoderPos > lastVirtualEncoderPos) { 
        settingsMenuScroll++; 
        lastVirtualEncoderPos = virtualEncoderPos;
        buzzerScroll();
      }
      else if (virtualEncoderPos < lastVirtualEncoderPos) { 
        settingsMenuScroll--; 
        lastVirtualEncoderPos = virtualEncoderPos;
        buzzerScroll();
      }

      // 2 menu items: Calibrate Compass, Back
      int settingsItemCount = 2;
      if (settingsMenuScroll < 0) settingsMenuScroll = settingsItemCount - 1;
      if (settingsMenuScroll >= settingsItemCount) settingsMenuScroll = 0;

      if (settingsMenuScroll != settingsMenuLast || stateChanged) {
        settingsMenuLast = settingsMenuScroll;
        screen.fillScreen(COLOR_BACKGROUND);
        
        drawStatusBar("SETTINGS", COLOR_WARNING);
        
        // Instructions
        screen.setTextSize(1);
        screen.setTextColor(COLOR_TEXT_SECONDARY);
        screen.setCursor(10, 220);
        screen.print("Rotate: scroll  Click: select");

        const char* settingsNames[] = {"Calibrate Compass", "Back"};
        const char* settingsDesc[]  = {"Magnetometer cal & save", "Return to navigation"};

        for(int i = 0; i < settingsItemCount; i++) {
          int displayIdx = (settingsMenuScroll + i) % settingsItemCount;
          int yPos = 50 + i * 50;
          
          if (i == 0) {
            screen.fillRoundRect(5, yPos, 230, 44, 6, COLOR_WARNING);
            screen.setTextColor(COLOR_BACKGROUND);
          } else {
            screen.fillRoundRect(5, yPos, 230, 44, 6, COLOR_SURFACE);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
          }
          
          screen.setTextSize(2);
          screen.setCursor(15, yPos + 8);
          screen.print(settingsNames[displayIdx]);
          
          screen.setTextSize(1);
          screen.setCursor(15, yPos + 28);
          if(i != 0) screen.setTextColor(COLOR_TEXT_SECONDARY);
          screen.print(settingsDesc[displayIdx]);
          
          if (i == 0) {
            screen.fillTriangle(218, yPos + 14, 218, yPos + 30, 226, yPos + 22, COLOR_BACKGROUND);
          }
        }
      }

      // SINGLE CLICK - Select settings item
      if (buttonSingleClick) {
        buttonSingleClick = false;
        buzzerClick();
        int sel = settingsMenuScroll % settingsItemCount;
        if (sel == 0) {
          // Calibrate Compass
          Serial.println("Starting calibration...");
          currentState = STATE_CALIBRATION;
        } else {
          // Back
          currentState = STATE_COMPASS;
        }
        screen.fillScreen(COLOR_BACKGROUND);
      }
      break;
    }

    // ==========================================
    // CALIBRATION STATE (guided 8-direction)
    // ==========================================
    case STATE_CALIBRATION: {
      static unsigned long lastCalDraw = 0;
      
      if (stateChanged) {
        calPhase = 0;
        calDirIndex = 0;
        for(int i = 0; i < CAL_NUM_DIRS; i++) calCaptured[i] = false;
        screen.fillScreen(COLOR_BACKGROUND);
        drawStatusBar("CALIBRATE", COLOR_WARNING);
        lastCalDraw = 0;
        Serial.println("Calibration: guided 8-direction mode");
      }
      
      switch (calPhase) {
        case 0: {
          sBmm350MagData_t rawMag = bmm350.getGeomagneticData();
          float rx = rawMag.x, ry = rawMag.y, rz = rawMag.z;
          
          if (millis() - lastCalDraw > 120) {
            lastCalDraw = millis();
            screen.fillRect(0, 34, 240, 206, COLOR_BACKGROUND);
            
            int cx = 120, cy = 100, r = 50;
            screen.drawCircle(cx, cy, r, COLOR_COMPASS_RING);
            screen.drawCircle(cx, cy, r+1, COLOR_COMPASS_RING);
            
            for(int i = 0; i < CAL_NUM_DIRS; i++) {
              float a = (calDirAngles[i] - 90.0) * M_PI / 180.0;
              int dx = cx + (r - 6) * cos(a);
              int dy = cy + (r - 6) * sin(a);
              if (calCaptured[i]) {
                screen.fillCircle(dx, dy, 4, COLOR_SUCCESS);
              } else if (i == calDirIndex) {
                screen.fillCircle(dx, dy, 5, COLOR_WARNING);
              } else {
                screen.fillCircle(dx, dy, 3, COLOR_SURFACE);
              }
            }
            
            float targetAngle = calDirAngles[calDirIndex];
            drawNavigationArrow(targetAngle, cx, cy, 30, COLOR_WARNING, COLOR_TEXT_PRIMARY);
            screen.fillCircle(cx, cy, 3, COLOR_TEXT_PRIMARY);
            
            screen.setTextSize(3);
            screen.setTextColor(COLOR_WARNING);
            int lblLen = strlen(calDirNames[calDirIndex]);
            screen.setCursor(cx - (lblLen * 9), cy - 60);
            screen.print(calDirNames[calDirIndex]);
            
            screen.setTextSize(2);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(190, 40);
            screen.print(calDirIndex);
            screen.print("/");
            screen.print(CAL_NUM_DIRS);
            
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(10, 165);
            screen.print("X="); screen.print(rx, 1);
            screen.print(" Y="); screen.print(ry, 1);
            screen.print(" Z="); screen.print(rz, 1);
            
            screen.setTextSize(2);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(10, 185);
            screen.print("Point to ");
            screen.setTextColor(COLOR_WARNING);
            screen.print(calDirNames[calDirIndex]);
            
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(10, 210);
            screen.print("Click: capture this direction");
            screen.setCursor(10, 225);
            screen.print("Double-click: cancel");
          }
          
          if (buttonSingleClick) {
            buttonSingleClick = false;
            buzzerClick();
            
            float sx = 0, sy = 0, sz = 0;
            for(int s = 0; s < 10; s++) {
              sBmm350MagData_t m = bmm350.getGeomagneticData();
              sx += m.x; sy += m.y; sz += m.z;
              delay(20);
            }
            calCapturedX[calDirIndex] = sx / 10.0;
            calCapturedY[calDirIndex] = sy / 10.0;
            calCapturedZ[calDirIndex] = sz / 10.0;
            calCaptured[calDirIndex] = true;
            
            Serial.printf("Captured %s: X=%.1f Y=%.1f Z=%.1f\n",
              calDirNames[calDirIndex],
              calCapturedX[calDirIndex],
              calCapturedY[calDirIndex],
              calCapturedZ[calDirIndex]);
            
            screen.fillRoundRect(30, 80, 180, 40, 8, COLOR_SUCCESS);
            screen.setTextSize(2);
            screen.setTextColor(COLOR_BACKGROUND);
            screen.setCursor(55, 88);
            screen.print(calDirNames[calDirIndex]);
            screen.print(" LOCKED");
            delay(400);
            
            calDirIndex++;
            if (calDirIndex >= CAL_NUM_DIRS) {
              calPhase = 1;
              screen.fillScreen(COLOR_BACKGROUND);
              drawStatusBar("COMPUTING", COLOR_WARNING);
            } else {
              lastCalDraw = 0;
            }
          }
          
          if (buttonDoubleClick) {
            buttonDoubleClick = false;
            buttonSingleClick = false;
            buzzerClick();
            Serial.println("Calibration cancelled");
            currentState = STATE_SETTINGS;
            screen.fillScreen(COLOR_BACKGROUND);
          }
          break;
        }
        
        case 1: {
          Serial.println("Computing from 8 directions...");
          float minX = calCapturedX[0], maxX = calCapturedX[0];
          float minY = calCapturedY[0], maxY = calCapturedY[0];
          float minZ = calCapturedZ[0], maxZ = calCapturedZ[0];
          for(int i = 1; i < CAL_NUM_DIRS; i++) {
            if (calCapturedX[i] < minX) minX = calCapturedX[i];
            if (calCapturedX[i] > maxX) maxX = calCapturedX[i];
            if (calCapturedY[i] < minY) minY = calCapturedY[i];
            if (calCapturedY[i] > maxY) maxY = calCapturedY[i];
            if (calCapturedZ[i] < minZ) minZ = calCapturedZ[i];
            if (calCapturedZ[i] > maxZ) maxZ = calCapturedZ[i];
          }
          
          calNewXoff = (maxX + minX) / 2.0;
          calNewYoff = (maxY + minY) / 2.0;
          calNewZoff = (maxZ + minZ) / 2.0;
          
          float rangeX = (maxX - minX) / 2.0;
          float rangeY = (maxY - minY) / 2.0;
          float rangeZ = (maxZ - minZ) / 2.0;
          float avgRange = (rangeX + rangeY + rangeZ) / 3.0;
          calNewXscl = (rangeX > 0) ? avgRange / rangeX : 1.0;
          calNewYscl = (rangeY > 0) ? avgRange / rangeY : 1.0;
          calNewZscl = (rangeZ > 0) ? avgRange / rangeZ : 1.0;
          
          Serial.printf("  Offsets: X=%.3f Y=%.3f Z=%.3f\n", calNewXoff, calNewYoff, calNewZoff);
          Serial.printf("  Scales:  X=%.3f Y=%.3f Z=%.3f\n", calNewXscl, calNewYscl, calNewZscl);
          
          screen.setTextSize(2);
          screen.setTextColor(COLOR_TEXT_PRIMARY);
          screen.setCursor(40, 80);
          screen.print("Computing...");
          screen.setTextSize(1);
          screen.setCursor(30, 110);
          screen.print("8 directions captured");
          
          delay(800);
          calPhase = 2;
          screen.fillScreen(COLOR_BACKGROUND);
          drawStatusBar("CONFIRM CAL", COLOR_SUCCESS);
          lastCalDraw = 0;
          break;
        }
        
        case 2: {
          if (millis() - lastCalDraw > 500) {
            lastCalDraw = millis();
            screen.fillRect(5, 38, 230, 140, COLOR_BACKGROUND);
            
            screen.fillRoundRect(5, 38, 230, 60, 6, COLOR_SURFACE);
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(12, 43);
            screen.print("OFFSETS (hard-iron)");
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(12, 56);
            screen.print("X: "); screen.print(calNewXoff, 2);
            screen.setCursor(90, 56);
            screen.print("Y: "); screen.print(calNewYoff, 2);
            screen.setCursor(168, 56);
            screen.print("Z: "); screen.print(calNewZoff, 1);
            screen.setCursor(12, 72);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.print("8-direction calibration");
            
            screen.fillRoundRect(5, 104, 230, 45, 6, COLOR_SURFACE);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(12, 109);
            screen.print("SCALES (soft-iron)");
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(12, 124);
            screen.print("X: "); screen.print(calNewXscl, 3);
            screen.setCursor(90, 124);
            screen.print("Y: "); screen.print(calNewYscl, 3);
            screen.setCursor(168, 124);
            screen.print("Z: "); screen.print(calNewZscl, 3);
            
            screen.fillRoundRect(5, 160, 112, 35, 6, COLOR_SUCCESS);
            screen.setTextSize(2);
            screen.setTextColor(COLOR_BACKGROUND);
            screen.setCursor(18, 168);
            screen.print("SAVE");
            
            screen.fillRoundRect(123, 160, 112, 35, 6, COLOR_DANGER);
            screen.setTextColor(COLOR_TEXT_PRIMARY);
            screen.setCursor(130, 168);
            screen.print("DISCARD");
            
            screen.setTextSize(1);
            screen.setTextColor(COLOR_TEXT_SECONDARY);
            screen.setCursor(10, 210);
            screen.print("Click: SAVE to NVS");
            screen.setCursor(10, 225);
            screen.print("Double-click: DISCARD changes");
          }
          
          if (buttonSingleClick) {
            buttonSingleClick = false;
            buzzerClick();
            magXoffset = calNewXoff;
            magYoffset = calNewYoff;
            magZoffset = calNewZoff;
            magXscale  = calNewXscl;
            magYscale  = calNewYscl;
            magZscale  = calNewZscl;
            saveCalibration();
            
            screen.fillScreen(COLOR_BACKGROUND);
            screen.fillRoundRect(20, 70, 200, 100, 10, COLOR_SUCCESS);
            screen.setTextSize(3);
            screen.setTextColor(COLOR_BACKGROUND);
            screen.setCursor(48, 90);
            screen.print("SAVED!");
            screen.setTextSize(1);
            screen.setCursor(40, 130);
            screen.print("Calibration stored in NVS");
            if (!buzzerMuted) {
              buzzerToneRaw(800, 60);
              delay(50);
              buzzerToneRaw(1200, 60);
            }
            delay(1500);
            currentState = STATE_SETTINGS;
            screen.fillScreen(COLOR_BACKGROUND);
          }
          
          if (buttonDoubleClick) {
            buttonDoubleClick = false;
            buttonSingleClick = false;
            buzzerClick();
            Serial.println("Calibration discarded");
            currentState = STATE_SETTINGS;
            screen.fillScreen(COLOR_BACKGROUND);
          }
          break;
        }
      }
      break;
    }
  }
}
