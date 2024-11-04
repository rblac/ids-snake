#include <MD_MAX72xx.h>

#include "./vec2.h"

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
	return digitalRead(joystickSW) == LOW;
}


// ## Game stuff ##

vec2 pos; // head position, x = column, y = bounds.x-row
vec2 dir = vec2(0, -1); // head direction
int length = 3;
const int MAX_LENGTH = 32;
vec2 tail[MAX_LENGTH]; // tail segments

vec2 fruit; // fruit position

int score = 0;

enum GameState {
	PLAY,
	OVER
};
GameState gameState = PLAY;

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

// game loop functions

void gameOver() {
	gameState = OVER;
	fillDisplay();
	drawSnake(true);
}
void gameUpdate() {
	// TODO make this sample more often
	auto input = readInputDir();

	// set dir to input if it's not directly opposite to current direction
	if (!input.isZero() && (input != (dir*-1)))
		dir = input;

	Serial.print("dir: "); printVec(dir);
	Serial.print(" | pos: "); printVec(pos);
	Serial.print(" | length: "); Serial.print(length);
	Serial.print(" | button: "); Serial.print(readInputButton());
	Serial.println();

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
	/*
	for(int x = 0; x < bounds.x; x++)
		for(int y = 0; y < bounds.y; y++)
			plotPoint(vec2(x,y), (x+y)%2);
	*/
}

void setup() {
	Serial.begin(9600);
	display.begin();
	display.clear();

	pinMode(joystickSW, INPUT_PULLUP);  // Enable internal pull-up resistor
	pinMode(joystickVRx, INPUT);
	pinMode(joystickVRy, INPUT);

	calibrateJoystick();

	// seed rng
	randomSeed(analogRead(joystickVRx));

	pos = bounds / 2; // initialise head position
	for(int i = 0; i < length; i++) // initialise tail
		tail[i] = pos;
	repositionFruit(); // initialise fruit position
}

void loop() {
	switch(gameState) {
		case PLAY: gameUpdate(); break;
		case OVER: deadUpdate(); break;
	}
	delay(150);
}