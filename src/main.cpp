#include <Arduino.h>
#include <string.h>

//#include <LCDKeypad.h>
//#include <LiquidCrystal.h>
//#include <SoftwareSerial.h>
#include <NeoSWSerial.h>
#include <morse.h>
#include <DmxSimple.h>
#include <EEPROMex.h>

#define PIN_MORSE 13
#define PIN_LIGHT 10
#define DMX_CH_FAN 100
#define DMX_CH_INTENSITY 101
#define PIN_DMX_JDI 3
#define PIN_DMX_JDO 4
#define PIN_DMX_JRDE 2
#define PIN_DMX_LED1 7
#define PIN_DMX_LED2 8

#define EEPROM_MORSE_SPEED_ADDR 10



// Configure software serial port
//SoftwareSerial SIM900(7, 8);
NeoSWSerial SIM900(11, 12);
//Variable to save incoming SMS characters
char incoming_char = 0;
char in_str[200];
auto ts = millis();
String msg;

// LCD keypad
//LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//LCDKeypad lcdKeypad(PIN_A0, &lcd);
LEDMorseSender morse(PIN_MORSE);


void printLcd(char* text);
void read_line();
char get_char();
bool check_msg(char const *buf, char* return_ptr = 0, uint16_t *ret_val = 0);


void setup() {
    Serial.println("Starting setup");

    morse.setup();

    SIM900.begin(9600);
    Serial.begin(115200);

    //lcd.begin(16, 6);


    // Give time to your GSM shield log on to network
    //Serial.println("Waiting 2s");
    delay(2000);

    // AT command to set SIM900 to SMS mode
    SIM900.print("AT+CMGF=1\r");
    delay(100);

    // Set module to send SMS data to serial out upon receipt
    SIM900.print("AT+CNMI=2,2,0,0,0\r");
    delay(100);
    SIM900.print("AT+CCID\r");
    delay(100);
    SIM900.print("AT+IPR=19200\r");
    SIM900.print("AT&W\r");


    uint16_t morse_speed = EEPROM.readInt(EEPROM_MORSE_SPEED_ADDR);
    if (morse_speed > 1 && morse_speed < 2000) {
        morse.setSpeed((morseTiming_t)morse_speed);
    }
    else {
        morse_speed = 500;
        EEPROM.writeInt(EEPROM_MORSE_SPEED_ADDR, morse_speed);
    }
    
    Serial.print("Morse speed set to: ");
    Serial.println(morse_speed);



    DmxSimple.maxChannel(2);

    Serial.println("Setup complete");


    //check_msg((char const *)"TIME=19.9");
}


void loop() {
    // Display any text that the GSM shield sends out on the serial monitor
    if (SIM900.available() > 0) {
        delay(100);

        read_line();

        // Read SMS
        if (in_str[0] == '+' && in_str[1] == 'C' && in_str[2] == 'M' && in_str[3] == 'T' && in_str[4] == ':') {
            delay(100);
            read_line();
            Serial.print("SMS received: ");
            Serial.println(in_str);

            //if (check_msg(in_str)) {
            //    return;
            //}

            if (check_msg(in_str)) {
                return;
            }

            printLcd(in_str);


            // Start

            morse.setMessage(String(in_str));
            morse.startSending();

            Serial.println("Morse start");
            //DmxSimple.usePin(A0);

            DmxSimple.write(DMX_CH_FAN, 255);

            bool lastState = 0;
            while (morse.continueSending()) {
                bool currentState = morse.getState();
                if (lastState != currentState) {
                    digitalWrite(PIN_LIGHT, currentState);
                    lastState = currentState;
                }
            }
            DmxSimple.write(DMX_CH_FAN, 0);
            dmxEnd();

            Serial.println("Morse stop");
        }
    }
}




void printLcd(char* text) {

    //lcd.clear();
    //lcd.home();


    uint8_t pos = 0;
    uint8_t line = 0;
    while (text[pos] != 0) {

        if (pos % 16 == 0 && pos != 0) {
            //Serial.print("LCD new line: ");
            //Serial.println(pos);
            line++;
            //lcd.println("");
            //lcd.setCursor(0, line);
            //lcd.print("\n\r");
        }
        

        //lcd.printc((char)text[pos]);

        pos++;
    }
}


bool check_msg(char const *buf, char *return_ptr = 0, uint16_t *ret_val = 0) {
    bool ret = false;

    Serial.print("Checking: ");
    Serial.println(buf);

    if (buf[0] == 'T' && buf[1] == 'I' && buf[2] == 'M' && buf[3] == 'E' && buf[4] == '=') {
        char data[10];
        int pos = 5;
        ret = false;
        for (int i = 0; i < 10; i++) {
            char c = buf[i + pos];

            if ((c >= 48 && c <= 57)) {
                data[i] = c;
                Serial.print("Is n: ");
                Serial.printc(c);
                Serial.println("");
            } 
            else {

                data[i] = '\0';
                return_ptr = &data[0];
                
                
                uint16_t wpm_int = atoi(return_ptr);
                ret_val = &wpm_int;
                Serial.print("WPM: ");
                Serial.println(wpm_int);

                //morse.setWPM((morseTiming_t)wpm_int);

                morse.setSpeed((morseTiming_t)wpm_int);

                EEPROM.write(EEPROM_MORSE_SPEED_ADDR, wpm_int);
                ret = true;
                return ret;
                
            }
        }
    }

    return ret;
}

char get_char() {
    incoming_char = SIM900.read();
    return incoming_char;
}

void read_line() {
    in_str[0] = 0;
    int i = 0;
    while (SIM900.available()) {

        in_str[i] = get_char();

        Serial.printc(in_str[i]);

        if (in_str[i] == '\n') {
            in_str[i + 1] = 0;
            break;
        }
        i++;
    }

    //Serial.print("Msg line is: ");
    //Serial.println(in_str);
}


