#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <WebServer.h>

#include "./vec2.h"

// have to put this up here due to a bug in the build process
// see https://arduino.stackexchange.com/questions/66530/class-enum-was-not-declared-in-this-scope
enum GameState {
	PLAY,
	OVER,
	NAME
};
GameState gameState = PLAY;

// ## Hardware stuff ##

#define DATA_PIN 23
#define CS_PIN   5
#define CLK_PIN  18
#define MAX_DEVICES 4
#define MATRIX_TYPE MD_MAX72XX::FC16_HW

MD_MAX72XX display = MD_MAX72XX(MATRIX_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

const int joystickSW = 34;   // Pin for SW
const int joystickVRx = 32;  // Pin for VRx
const int joystickVRy = 33;  // Pin for VRy
const vec2 deadzones = vec2(600, 700); // Deadzone thresholds for ignoring small movements

int centerVRx, centerVRy; // Variables to hold the joystick's resting values
void calibrateJoystick() {
	// **Joystick Center Calibration**: Take an average reading to find the resting position
	long avgVRx = 0, avgVRy = 0;
	const int calibrationReads = 20; // Number of reads to average for calibration
	for (int i = 0; i < calibrationReads; i++) {
		avgVRx += analogRead(joystickVRx);
		avgVRy += analogRead(joystickVRy);
		delay(10);
	}
	centerVRx = avgVRx / calibrationReads;
	centerVRy = avgVRy / calibrationReads;

	Serial.print("Calibrated Center VRx: ");
	Serial.println(centerVRx);
	Serial.print("Calibrated Center VRy: ");
	Serial.println(centerVRy);
}

const int button1Pin = 12;

// input functions

vec2 readInputDir() {
	// Read joystick analog values
	int vrxValue = analogRead(joystickVRx);
	int vryValue = analogRead(joystickVRy);

	// Adjusted joystick values based on calibrated center position
	int adjustedVRx = vrxValue - centerVRx;
	int adjustedVRy = vryValue - centerVRy;

	/*
	// Display raw and adjusted values for debugging
	Serial.print("VRx Raw: "); Serial.print(vrxValue);
	Serial.print(" Adjusted: "); Serial.print(adjustedVRx);
	Serial.print(" | VRy Raw: "); Serial.print(vryValue);
	Serial.print(" Adjusted: "); Serial.println(adjustedVRy);
	*/

	int xDelta = 0, yDelta = 0;

	// Prioritize the larger movement to avoid diagonals
	if (abs(adjustedVRx) >= abs(adjustedVRy)) {
		// check against respective deadzone
		if (abs(adjustedVRx) > deadzones.x)
			xDelta = -adjustedVRx / abs(adjustedVRx); // inverted; leftward input means increasing column index
	} else {
		if (abs(adjustedVRy) > deadzones.y)
			yDelta = adjustedVRy / abs(adjustedVRy);
	}

	return vec2(xDelta, yDelta);
}
bool readInputButton() {
	//return digitalRead(joystickSW) == LOW;
	return digitalRead(button1Pin) == LOW;
}

// ## Scoreboard stuff ##

const int MAX_ENTRIES = 20;
int populatedEntries = 0;
char entryNames[MAX_ENTRIES*4];
int entryScores[MAX_ENTRIES];

void submitEntry(char* entryName, int entryScore) {
	if(populatedEntries < MAX_ENTRIES)
		populatedEntries++;

	int entryIndex = populatedEntries;
	for(int i = 0; i < populatedEntries; i++)
		if(entryScores[i] < entryScore) {
			entryIndex = i;
			break;
		}

	if(entryIndex >= MAX_ENTRIES) return;

	// shift all score entries below the new score
	for(int i = populatedEntries-1; i > entryIndex; i--) {
		strncpy(&entryNames[i*4], &entryNames[(i-1)*4], 4);
		entryScores[i] = entryScores[i-1];
	}

	strncpy(&entryNames[entryIndex*4], entryName, 4);
	entryScores[entryIndex] = entryScore;
}
void prefillScoreboard() {
	submitEntry("RB[[", 32);
	submitEntry("ONE[", 1);
	submitEntry("DOS[", 2);
	submitEntry("SAN[", 3);
	submitEntry("TUTU", 4);
	submitEntry("PIEC", 5);
	submitEntry("SEIS", 6);
	submitEntry("NANA", 7);
	submitEntry("OTTE", 8);
	submitEntry("KYUU", 9);
	submitEntry("TEN[", 10);
}

void debugPrintScoreboard() {
	Serial.println("| NAME | SCORE |");
	Serial.println("| ------------ |");
	for(int i = 0; i < populatedEntries; i++) {
		Serial.print("| ");
		Serial.print(entryNames[i*4]);
		Serial.print(entryNames[i*4+1]);
		Serial.print(entryNames[i*4+2]);
		Serial.print(entryNames[i*4+3]);
		Serial.print(" | ");
		Serial.print(entryScores[i]);
		Serial.println(" |");
	}
}

String scoreboardJson() {
	String out = "{ \"entries\": [ ";

	for(int i = 0; i < populatedEntries; i++) {
		out += "[\"";
		out += entryNames[i*4];
		out += entryNames[i*4+1];
		out += entryNames[i*4+2];
		out += entryNames[i*4+3];
		out += "\", ";
		out += entryScores[i];
		out += " ]";
		if(i != populatedEntries - 1) out += ',';
	}

	out += " ] }";
	return out;
}

// ## Server stuff ##

// reference https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html

const char* SSID = "snake scoreboard server";
WiFiServer server(80);
#include "./scoreboard.h" // the webpage. header generated with xxd -i

void initServer() {
	if(!WiFi.softAP(SSID)) {
		Serial.println("Failed to initialise access point.");
		return;
	}

	IPAddress ip = WiFi.softAPIP();
	Serial.print("AP IP: ");
	Serial.println(ip);

	server.begin();
	Serial.print("Server started");
}
void serverUpdate() {
	while(true) {
		NetworkClient client = server.accept();
		if(!client) break;
		if(!client.connected()) continue;

		String clientRequest = "";
		while(client.available())
			clientRequest += (char)client.read();

		if(clientRequest.startsWith("GET / ")) {
			client.println("HTTP/1.1 200 ok");
			client.println("Content-type:text/html");
			client.println();
			for(char c : scoreboard_html)
				client.print(c);
			client.println();
		} else if(clientRequest.startsWith("GET /data.json")) {
			client.println("HTTP/1.1 200 ok");
			client.println("Content-type:application/json");
			client.println();
			client.println(scoreboardJson());
		} else {
			client.println("HTTP/1.1 404 not found");
			client.println("Content-type:text/html");
			client.println();
			client.print("received request:");
			client.print(clientRequest);
			client.println();
		}

		client.stop(); // close connection
	}
}

// ## Game stuff ##

vec2 pos; // head position, x = column, y = bounds.x-row
vec2 dir = vec2(0, -1); // head direction
int length; // set in the reset function
const int MAX_LENGTH = 32;
vec2 tail[MAX_LENGTH]; // tail segments

vec2 fruit; // fruit position

int score = 0;

const vec2 bounds = vec2(32, 8); // game board/display bounds

bool tailContains(vec2 p) {
	for(int i = 0; i < length; i++)
		if(tail[i] == p) return true;
	return false;
}
void extendTail() {
	if(length >= MAX_LENGTH) return;
	length++;
	tail[length-1] = tail[length-2];
}
void updateTail() {
	for(int i = length-1; i > 0; i--)
		tail[i] = tail[i-1];
	tail[0] = pos;
}
void moveBy(vec2 delta) {
	updateTail();
	pos = pos + delta;

	pos.x = pos.x % bounds.x;
	pos.y = pos.y % bounds.y;

	while(pos.x < 0) pos.x += bounds.x;
	while(pos.y < 0) pos.y += bounds.y;
}

void repositionFruit() {
	// keep randomising until fruit doesn't intersect the head nor body
	do {
		int row = random(0, 8);      // Random row (0-7)
		int col = random(0, 32);     // Random column (0-31)
		fruit = vec2(col, row);
	} while(fruit == pos || tailContains(fruit));
}

// display functions

void plotPoint(vec2 v, bool s = true) {
	display.setPoint(v.y, v.x, s);
}
void fillDisplay() {
	for(int c = 0; c < 32; c++)
		display.setColumn(c, 255);
}
void drawSnake(bool negative = false) {
	for(int i = 0; i < length; i++)
		plotPoint(tail[i], !negative);
	plotPoint(pos, !negative);
}

void displayChar(char c, unsigned int screenIndex, bool invert = false) {
	if(c == '[') c = ' '; // replace placeholder with blank space
	screenIndex = 3 - screenIndex; // column indices go right to left

	uint8_t bitmap[8]; // allocate buffer for character bitmap
	uint8_t col_width = display.getChar(c, 8, bitmap); // get bitmap from library built-in font

	// disable automatic refresh to prevent flickering
	display.control(screenIndex, screenIndex, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

	// 255 means all 8 bits are set
	// please read up on bitmaps if you're confused
	for(int col = 0; col < 8; col++) // wipe columns
		display.setColumn((7-col) + 8*screenIndex, invert ? 255 : 0);
	// draw character column-by-column
	for(int col = 0; col < col_width; col++) {
		uint8_t column_data = bitmap[col];
		if(invert) column_data = column_data xor 255;
		display.setColumn((col_width-col) + 8*screenIndex, column_data);
	}

	// re-enable refresh
	display.control(screenIndex, screenIndex, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
void displayString(const char* s, unsigned int length = 4, unsigned int offset = 0, bool invert = false) {
	for(int i = 0; i < length; i++)
		displayChar(s[(i+offset) % length], i, invert);
}

// game loop functions

unsigned int stateTicks = 0;
void setGameState(GameState state) {
	gameState = state;
	stateTicks = 0;
	display.clear();
}
void gameOver() {
	Serial.print("GAME OVER");
	setGameState(OVER);
	fillDisplay();
	drawSnake(true);
}
void gameUpdate() {
	// TODO make this sample more often
	auto input = readInputDir();

	// set dir to input if it's not directly opposite to current direction
	if (!input.isZero() && (input != (dir*-1)))
		dir = input;

	/*
	Serial.print("dir: "); printVec(dir);
	Serial.print(" | pos: "); printVec(pos);
	Serial.print(" | length: "); Serial.print(length);
	Serial.print(" | button: "); Serial.print(readInputButton());
	Serial.println();
	*/

	moveBy(dir);

	// if head overlaps fruit
	if(pos == fruit) {
		repositionFruit();
		extendTail();
		score++;
	}
	// if head overlaps tail
	if(tailContains(pos)) {
		gameOver();
		return;
	}

	display.clear();
	drawSnake();
	plotPoint(fruit);
}
void deadUpdate() {
	// after 20 ticks, display score
	if(stateTicks > 20) {
		if(score > 9999) // won't fit on screen
			displayString("WHOA");
		else {
			int decimatedScore = score;
			// print each digit
			for(int digit = 0; digit < 4; digit++) {
				if(decimatedScore)
				// ASCII numbers are sequential, all we need to do is add the right offset
				displayChar(decimatedScore % 10 + '0', 3-digit);
				decimatedScore /= 10;
			}
		}
	}

	// after 40 ticks, go to name entry
	if(stateTicks > 40)
		setGameState(NAME);
}

unsigned int nameCharIndex;
char nameChars[4];
void nameUpdate() {
	vec2 inDir = readInputDir();
	char nameChar = nameChars[nameCharIndex];
	nameChar += inDir.y; // decrement if <0, increment if >0, no change if =0
	// [ is directly after Z in ASCII, it's our stand-in for a blank character
	if(nameChar < 'A') nameChar = '[';
	if(nameChar > '[') nameChar = 'A';
	nameChars[nameCharIndex] = nameChar;

	// display characters
	// invert every few ticks to create a blinking animation
	bool invertChar = (stateTicks / 5) % 2;
	for(int i = 0; i < 4; i++)
		displayChar(nameChars[i], i, i==nameCharIndex ? invertChar : false);

	if(readInputButton()) nameCharIndex++;
	if(nameCharIndex >= 4) {
		submitEntry(nameChars, score);
		Serial.print("NAME: ");
		Serial.print(nameChars);
		Serial.print(" SCORE: ");
		Serial.println(score);
		debugPrintScoreboard();
		gameReset();
	}
}

void gameReset() {
	// seed rng
	randomSeed(analogRead(joystickVRx));

	// reset name entry settings
	nameCharIndex = 0;
	for(int i = 0; i < 4; i++) nameChars[i] = 'A';
	// initialise score & length
	score = 0;
	length = 3;

	pos = bounds / 2; // initialise head position
	for(int i = 0; i < length; i++) // initialise tail
		tail[i] = pos;
	repositionFruit(); // initialise fruit position
	setGameState(PLAY);
}

void setup() {
	Serial.begin(9600);
	display.begin();
	display.clear();

	pinMode(joystickSW, INPUT_PULLUP);  // Enable internal pull-up resistor
	pinMode(joystickVRx, INPUT);
	pinMode(joystickVRy, INPUT);
	pinMode(button1Pin, INPUT_PULLUP);

	calibrateJoystick();

	prefillScoreboard();
	initServer();

	gameReset();
}

void loop() {
	switch(gameState) {
		case PLAY: gameUpdate(); break;
		case OVER: deadUpdate(); break;
		case NAME: nameUpdate(); break;
	}
	stateTicks++;

	serverUpdate();
	delay(150);
}
