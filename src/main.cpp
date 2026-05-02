#include <Arduino.h>
#include <math.h>

// ---------------- CONFIG ----------------

const int IMPORT_LED_PIN = 5;
const int EXPORT_LED_PIN = 6;

const float WH_PER_PULSE = 1.0;
const unsigned long PULSE_WIDTH_MS = 20;

// ---------------- STATE ----------------

float import_Wh = 0.0;
float export_Wh = 0.0;

int import_pulses_pending = 0;
int export_pulses_pending = 0;

unsigned long last_update_ms = 0;

// ---------------- LED STATE ----------------

struct PulseLED {
    int pin;
    bool active;
    unsigned long off_time_ms;
};

PulseLED importLED = {IMPORT_LED_PIN, false, 0};
PulseLED exportLED = {EXPORT_LED_PIN, false, 0};

// ---------------- SIMULATION ----------------

float simulateLoadWatts(unsigned long t_ms) {
    float base = 200.0;

    if (random(0, 1000) < 5) {
        base += random(500, 2000);
    }

    return base;
}

float simulateSolarWatts(unsigned long t_ms) {
    float hours = (t_ms / 1000.0) / 3600.0;
    float dayTime = fmod(hours, 24.0);

    float solar = 0.0;

    if (dayTime > 6 && dayTime < 18) {
        float x = (dayTime - 12.0) / 6.0;
        solar = 3000.0 * exp(-x * x * 2);

        solar *= random(80, 100) / 100.0;
    }

    return solar;
}

// ---------------- ENERGY ----------------

void updateEnergy(unsigned long now) {
    if (last_update_ms == 0) {
        last_update_ms = now;
        return;
    }

    float dt = (now - last_update_ms) / 1000.0;
    last_update_ms = now;

    float load = simulateLoadWatts(now);
    float solar = simulateSolarWatts(now);
    float net = load - solar;

    if (net >= 0)
        import_Wh += net * dt / 3600.0;
    else
        export_Wh += (-net) * dt / 3600.0;
}

// ---------------- PULSES ----------------

void accumulatePulses() {
    while (import_Wh >= WH_PER_PULSE) {
        import_Wh -= WH_PER_PULSE;
        import_pulses_pending++;
    }

    while (export_Wh >= WH_PER_PULSE) {
        export_Wh -= WH_PER_PULSE;
        export_pulses_pending++;
    }
}

void triggerPulse(PulseLED &led, unsigned long now) {
    digitalWrite(led.pin, HIGH);
    led.active = true;
    led.off_time_ms = now + PULSE_WIDTH_MS;
}

void updateLED(PulseLED &led, unsigned long now) {
    if (led.active && now >= led.off_time_ms) {
        digitalWrite(led.pin, LOW);
        led.active = false;
    }
}

void servicePulseQueue(unsigned long now) {
    if (!importLED.active && import_pulses_pending > 0) {
        triggerPulse(importLED, now);
        import_pulses_pending--;
    }

    if (!exportLED.active && export_pulses_pending > 0) {
        triggerPulse(exportLED, now);
        export_pulses_pending--;
    }
}

// ---------------- SETUP ----------------

void setup() {
    pinMode(IMPORT_LED_PIN, OUTPUT);
    pinMode(EXPORT_LED_PIN, OUTPUT);

    digitalWrite(IMPORT_LED_PIN, LOW);
    digitalWrite(EXPORT_LED_PIN, LOW);

    Serial.begin(115200);
    randomSeed(analogRead(0));
}

// ---------------- LOOP ----------------

void loop() {
    unsigned long now = millis();

    updateEnergy(now);
    accumulatePulses();
    servicePulseQueue(now);

    updateLED(importLED, now);
    updateLED(exportLED, now);

    static unsigned long lastPrint = 0;
    if (now - lastPrint > 1000) {
        lastPrint = now;

        Serial.print("Import Wh: ");
        Serial.print(import_Wh);
        Serial.print(" | Export Wh: ");
        Serial.print(export_Wh);
        Serial.print(" | Pending I:");
        Serial.print(import_pulses_pending);
        Serial.print(" E:");
        Serial.println(export_pulses_pending);
    }
}
