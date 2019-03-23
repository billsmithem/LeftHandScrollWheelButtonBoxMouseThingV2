/*
	Modified to support multiple modes of operation
		- Buttons + Mouse wheel
		- Buttons + Left & Right arrow keys (useful for video editing)
*/

#include <Keyboard.h>
#include <Mouse.h>

// #define	DEBUGGING
// #define DEBUG_MODES
//#define DEBUG_ENCODER
#define DEBUG_BUTTONS
// #define DEBUG_BLINK

// Arduino pin definitions
#define ENCODER_CLOCK		2		// Rotary encoder clock
#define ENCODER_DATA		A0		// Rotary encoder data 
#define ENCODER_SWITCH 		A1		// rotary encoder switch
#define LEFT_SWITCH			5		// left mouse button
#define	RIGHT_SWITCH		6		// right mouse button
#define THUMB_SWITCH		7		// pin for thumb switch

#define EXTERN_LED			16		// external LED

// keys
#define	THUMB_KEY			KEY_LEFT_CTRL	// thumb key value

// #define DEBOUNCE_TIME		100		// delay 100 milliseconds for switch transitions
#define DEBOUNCE_TIME		20		// delay 20 milliseconds for switch transitions

#define LED_PULSETIME		250
#define DEADTIME			1000	// delay between LED pulse streams

/*
	Setup mode
		Toggle setup mode by holding down buttons 1 & 2, then pressing button 3
*/
// what mode are we in? allows functionality changes
bool setupMode = false;		// changing modes
// available modes
enum hidMode {
	mouse,			// encoder sends mouse wheel events
	arrows			// encoder sends left and right cursor key events (useful for video editing)
} mode = mouse;

void nextMode() {
	switch (mode) {
		case mouse:
			mode = arrows;
			#if defined(DEBUGGING) && defined(DEBUG_MODES)
			Serial.println("mode == ARROWS");
			#endif
			break;
		case arrows:
		default:		// something is broken, switch back to mouse
			mode = mouse;
			#if defined(DEBUGGING) && defined(DEBUG_MODES)
			Serial.println("mode == MOUSE");
			#endif
			break;
	}
}

void displayMode() {	// blink LED to display mode we're in
	switch (mode) {
		case mouse:
			blink(1, LED_PULSETIME);
			break;
		case arrows:
			blink(2, LED_PULSETIME);
			break;
		default:		// error!!!
			blink(10, LED_PULSETIME);
	}
}

class Button {
	private:
		int pin;
		String buttonName;
		int state;
		int clickVal;
		bool isButton;

	// constructor
	public:
		Button(int pinNum, String name, int click, bool buttonMode = true ) {
			pin = pinNum;
			state = HIGH;
			buttonName = name;
			clickVal = click;
			isButton = buttonMode;
			pinMode(pin, INPUT_PULLUP);
		}

		int State() {
			return state;
		}

		bool Update() {
			int buttonState = digitalRead(pin);
			if (buttonState != state) {			// state change?
				delay(DEBOUNCE_TIME);
				buttonState = digitalRead(pin);
				if (buttonState != state) {		// if really changed
					state = buttonState;
					#if defined(DEBUGGING) && defined(DEBUG_BUTTONS)
					Serial.print(buttonName);
					#endif
					if (!setupMode) {
						if (state == LOW) {
							#if defined(DEBUGGING) && defined(DEBUG_BUTTONS)
							Serial.println(" down");
							#endif
							if (isButton)
								Mouse.press(clickVal);
							else {
								#if defined(DEBUGGING) && defined(DEBUG_BUTTONS)
								Serial.println(" key pressed");
								#endif
								Keyboard.press(clickVal);
							}
						} else {
							#if defined(DEBUGGING) && defined(DEBUG_BUTTONS)
							Serial.println(" up");
							#endif
							if (isButton)
								Mouse.release(clickVal);
							else {
								#if defined(DEBUGGING) && defined(DEBUG_BUTTONS)
								Serial.println(" key released");
								#endif
								Keyboard.release(clickVal);
							}
						}
					}
				}
			}
		}
};

// define mouse buttons - this will need to be enhanced to support multiple modes
Button midButton(ENCODER_SWITCH, "middle button", MOUSE_MIDDLE);
Button leftButton(LEFT_SWITCH, "left button", MOUSE_LEFT);
Button rightButton(RIGHT_SWITCH, "right button", MOUSE_RIGHT);
Button thumbButton(THUMB_SWITCH, "thumb button", THUMB_KEY, false);		// sends keyboard modifier

//------------------------------------- encoder -------------------------------
int encoderCount = 0;
bool stateChanged = false;
bool clockWise = true;		// which way do we spin the mouse wheel

void encoderInterrupt() {
	stateChanged = true;
	if (digitalRead(ENCODER_DATA) == HIGH) {
		if (clockWise)
			encoderCount--;
		else
			encoderCount++;
	} else {
		if (clockWise)
			encoderCount++;
		else
			encoderCount--;
	}
}

void checkEncoder() {
	if (stateChanged) {
		#if defined(DEBUGGING) && defined(DEBUG_ENCODER)
		Serial.print("encoder count = "); Serial.println(encoderCount);
		#endif
		if (setupMode) {
			// switch to next mode
			nextMode();
		} else {
			switch (mode) {
				case mouse:
					Mouse.move(0, 0, encoderCount);
					break;
				case arrows:
					if (encoderCount > 0) {
						Keyboard.write(KEY_LEFT_ARROW);
						#if defined(DEBUGGING) && defined(DEBUG_ENCODER)
						Serial.println("LEFT ARROW");
						#endif
					} else {
						Keyboard.write(KEY_RIGHT_ARROW);
						#if defined(DEBUGGING) && defined(DEBUG_ENCODER)
						Serial.println("RIGHT ARROW");
						#endif
					}
					break;
			}
		}
		encoderCount = 0;
		stateChanged = false;
	}
}

//------------------------------------- support functions ---------------------
void updateButtons() {
	leftButton.Update();
	rightButton.Update();
	midButton.Update();
	thumbButton.Update();
}

void toggleSetup() {
	if (!setupMode) {
		if (leftButton.State() == LOW && rightButton.State() == LOW && midButton.State() == LOW) {
			while (midButton.State() == LOW)	// wait for middle button to be released or we'll switch states again
				midButton.Update();
			setupMode = true;
			Mouse.release(MOUSE_LEFT);		// release the buttons so computer isn't locked up
			Mouse.release(MOUSE_RIGHT);
			Mouse.release(MOUSE_MIDDLE);
		}
	} else {
		if (midButton.State() == LOW) {
			setupMode = false;
		}
	}
}

//------------------------------------- start ---------------------------------
void setup() {
	// use falling edge because capacitor on input will cause a slow rising edge
	attachInterrupt(digitalPinToInterrupt(ENCODER_CLOCK), encoderInterrupt, FALLING);
	Mouse.begin();
	Keyboard.begin();
	Serial.begin(9600);

	// enable use of built-in LED
	pinMode(LED_BUILTIN, OUTPUT);

	// configure pin for external LED
	pinMode(EXTERN_LED, OUTPUT);
	// and turn it off (testing)
	externLed(HIGH);

	blink(4, LED_PULSETIME);
}

//-----------------------------------------------------------------------------
// blink LED without stopping processing
bool running = false;
unsigned long endTime;
int blinkCount = 0;
int blinkDuration = 0;
bool ledOn = false;

void blink(int count, int blinkTime) {
	// Serial.print("bink: count = "); Serial.print(count); Serial.print(" blinkTime = "); Serial.println(blinkTime);
	if (!running) {							// start new cycle, if already running, too bad, can't stack them up
		running = true;
		blinkCount = count + 1;					// remember pulses and their length
		blinkDuration = blinkTime;
		endTime = millis() + blinkDuration;	// set the timer
		// digitalWrite(LED_BUILTIN, HIGH);	// and turn on the LED
		externLed(HIGH);
		ledOn = true;
	}
}

void doBlink() {
	if (running) {
		if (endTime < millis()) {				// has delay elapsed?
			#if defined(DEBUGGING) && defined(DEBUG_BLINK)
			Serial.print("ledOn = "); Serial.println(ledOn);
			#endif
			if (ledOn) {						// toggle LED
				#if defined(DEBUGGING) && defined(DEBUG_BLINK)
				Serial.print("blinkCount = "); Serial.print(blinkCount); Serial.println(" turning off LED");
				#endif
				// digitalWrite(LED_BUILTIN, LOW);	// turn off
				externLed(HIGH);
				ledOn = false;
				blinkCount--;					// decrement count every time cycle completes
				if (blinkCount > 0) {			// if more to do
					endTime = millis() + blinkDuration;		// restart timer
				} else if (blinkCount == 0) {				// finished, delay a bit before allowing turn on again
					endTime = millis() + DEADTIME;	// long delay after last one cycle
					ledOn = true;				// say it's on so we hit the off code again
				} else {						// we're done
					running = false;
				}
			} else {
				#if defined(DEBUGGING) && defined(DEBUG_BLINK)
				Serial.print("blinkCount = "); Serial.print(blinkCount); Serial.println(" turning on LED");
				#endif
				// digitalWrite(LED_BUILTIN, HIGH);
				externLed(LOW);
				ledOn = true;
				endTime = millis() + blinkDuration;		// restart timer
			}
		}
	}
}

// external LED
void externLed(int state) {
	digitalWrite(EXTERN_LED, state);
}
//------------------------------------- main loop -----------------------------
void loop() {
	if (setupMode) {
		// display current mode (blink extern LED)
		displayMode();
	}
	checkEncoder();
	updateButtons();
	toggleSetup();		// go to setup mode?
	doBlink();			// keep the LED running
}
