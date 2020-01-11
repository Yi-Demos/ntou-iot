#include <Wire.h>
#include <sfm3000wedo.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define MAX_COUNT 70
#define SDA_PIN 4
#define SCL_PIN 5

const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

static int cur_index = 0;
static int storedO2DB[MAX_COUNT] = { 0 };
static int storedflowsDB[MAX_COUNT] = { 0 };
static int storedTimeDB[MAX_COUNT] = { 0 };

const char* MY_SSID = "";
const char* MY_PWD = "";
const char *Tranquility = "http://api.tranquility.tech/ntou?";

SFM3000wedo measflows(64);

int offset = 32000; // offset for the sensor
float scale = 140.0; // scale factor for Air and N2 is 140.0, O2 is 142.8

const int doutPin = 14;
const int rangePin = 12;
const int ckPin = 0;
const int gatePin = 2;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN, I2C_SLAVE); // join i2c bus with address #8
    Wire.onReceive(receiveEvent); // register event
    connectWifi();

    // sesnor init
    measflows.init();
    Serial.println("Sensor initialized!");
    pinMode(doutPin, INPUT);
    pinMode(rangePin, OUTPUT);
    pinMode(ckPin, OUTPUT);
    pinMode(gatePin, OUTPUT);
    pinMode(15, OUTPUT);
}

void receiveEvent(size_t howMany) {
    (void)howMany;
    while (1 < Wire.available()) { // loop through all but the last
        char c = Wire.read(); // receive byte as a character
        Serial.print(c);
    }
    int x = Wire.read(); // receive byte as an integer
    Serial.println(x); // print the integer
}

void loop() {
    unsigned int result = measflows.getvalue();

    int red, green, blue;
    int integration = 150;
    int brightness = 60;

    float flows = ((float)result - offset) / scale;

    char s[64];  // variable to hold character string to be output to serial console

    if (result >= 0) Serial.print('\n');

    analogWrite(15, brightness);

    digitalWrite(gatePin, LOW);
    digitalWrite(ckPin, LOW);
    digitalWrite(rangePin, HIGH);
    digitalWrite(gatePin, HIGH);
    delay(integration);
    digitalWrite(gatePin, LOW);
    delayMicroseconds(4);
    red = shiftIn12(doutPin, ckPin);
    delayMicroseconds(3);
    green = shiftIn12(doutPin, ckPin);
    delayMicroseconds(3);
    blue = shiftIn12(doutPin, ckPin);

    float calculation = red / blue;
    float o2 = calculation * 1000;

    unsigned long time;
    time = millis();
    float curTime = time / 1000.0;

    Serial.print("o2=");
    Serial.print(o2);
    Serial.print(", ");
    Serial.print("flows=");
    Serial.print(flows);
    Serial.print(", ");
    Serial.print("time=");
    Serial.print(curTime);
    Serial.print("   \n");

    localDB(o2, flows, curTime);
}

// read 12-bit value (assuming data sent from LSB)
int shiftIn12(int dataPin, int clockPin) {
	int value = 0;

	for (int i = 0; i < 12; i++) {
		digitalWrite(clockPin, HIGH);
		value |= digitalRead(dataPin) << i;
		digitalWrite(clockPin, LOW);
	}

	return value;
}

void localDB(int o2, int flows, int curTime) {
    cur_index++;
    if (cur_index == MAX_COUNT) {
        cur_index = 0;
        storedO2DB[cur_index] = o2;
        storedflowsDB[cur_index] = flows;
        storedTimeDB[cur_index] = curTime;
        senddata(storedO2DB, storedflowsDB, storedTimeDB);
    }
    else {
        storedO2DB[cur_index] = o2;
        storedflowsDB[cur_index] = flows;
        storedTimeDB[cur_index] = curTime;
    }
}

void connectWifi() {
    Serial.print("Connecting to " + *MY_SSID);
    WiFi.begin(MY_SSID, MY_PWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("Connected");
    Serial.println("");
}

void senddata(int* o2, int* flows, int* curTime) {
    String query = "";
    int arrayLength;
    for (arrayLength = 0; arrayLength < 50; arrayLength++) {
        query += "o2=";
        query += o2[arrayLength];
        query += '&';
        query += "flows=";
        query += flows[arrayLength];
        query += '&';
        query += "t=";
        query += curTime[arrayLength];
        query += '&';
    }

    Serial.println(query);

    HTTPClient http;

    http.begin(Tranquility + query);
    int httpCode = http.GET(); // send the request

    http.end(); //close connection
    Serial.println("Upload !");
}
