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
static int storedRedDB[MAX_COUNT] = { 0 };
static int storedGreenDB[MAX_COUNT] = { 0 };
static int storedBlueDB[MAX_COUNT] = { 0 };
static int storedflowDB[MAX_COUNT] = { 0 };
static int storedTimeDB[MAX_COUNT] = { 0 };

const int doutPin = 14;
const int rangePin = 12;
const int ckPin = 0;
const int gatePin = 2;

const char *MY_SSID = "";
const char *MY_PWD = "";
const char *Tranquility = "https://api.tranquility.tech/ntou?=";

int offset = 32000;	// offset for the sensor
int sent = 0;

SFM3000wedo measflow(64);

float scale = 140.0; // scale factor for Air and N2 is 140.0, O2 is 142.8

void setup()
{
	Serial.begin(115200);
	Wire.begin(SDA_PIN, SCL_PIN, I2C_SLAVE);	// join i2c bus with address #8
	Wire.onReceive(receiveEvent);	// register event
	connectWifi();

	// initialize the sesnor
	measflow.init();
	Serial.println("Sensor initialized!");
	pinMode(doutPin, INPUT);
	pinMode(rangePin, OUTPUT);
	pinMode(ckPin, OUTPUT);
	pinMode(gatePin, OUTPUT);
	pinMode(15, OUTPUT);
}

void receiveEvent(size_t howMany) {
	(void) howMany;
	while (1 < Wire.available()) {
		// loop through all but the last
		char c = Wire.read();	// receive byte as a character
		Serial.print(c);	// print the character
	}
	int x = Wire.read();	// receive byte as an integer
	Serial.println(x);	// print the integer
}

void loop() {
    unsigned int result = measflow.getvalue();

    float flow = ((float) result - offset) / scale;
    if (result >= 0) Serial.print('\n');

    int integration = 150;
    int brightness = 60;
    analogWrite(14, brightness);

    char s[64]; // variable to hold character string to be output to serial console

    digitalWrite(gatePin, LOW);
    digitalWrite(ckPin, LOW);
    digitalWrite(rangePin, HIGH);

    digitalWrite(gatePin, HIGH);
    delay(integration);
    digitalWrite(gatePin, LOW);

    Serial.print("flow=");
    Serial.print(flow);
    Serial.print('\n');

    localDB(flow);
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

void localDB(int flow) {
	cur_index++;
	if (cur_index == MAX_COUNT) {
		cur_index = 0;
		storedflowDB[cur_index] = flow;
		senddata(storedflowDB);
	}
	else {
		storedflowDB[cur_index] = flow;
	}
}

void connectWifi() {
	Serial.print("Connecting to " + *MY_SSID);
	WiFi.begin(MY_SSID, MY_PWD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.print(".");
	}

	Serial.println("Connected");
}

void senddata(int *flow) {
	String query = "";
	int arrayLength;
	for (arrayLength = 0; arrayLength < MAX_COUNT; arrayLength++) {
		query += "flows=";
		query += flow[arrayLength];
	}

	HTTPClient http;
	http.begin(Tranquility + query); // specify request destination
	int httpCode = http.GET();

	http.end();	// close connection
	Serial.println("Upload !");
}
