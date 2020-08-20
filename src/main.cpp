#include <Arduino.h>
#include <string.h>

//#include <LCDKeypad.h>
//#include <LiquidCrystal.h>
//#include <SoftwareSerial.h>
#include <NeoSWSerial.h>
#include <morse.h>
//#include <DmxSimple.h>
#include <EEPROMex.h>

#define PIN_MORSE 10
#define PIN_LIGHT 5
#define PIN_SIM900_ON 9
#define DMX_CH_FAN 100
#define DMX_CH_INTENSITY 101
#define PIN_DMX_JDI 3
#define PIN_DMX_JDO 4
#define PIN_DMX_JRDE 2
#define PIN_DMX_LED1 7
#define PIN_DMX_LED2 8

static unsigned long SOS_AUTO_TIME_MS = 60000*10;

#define EEPROM_MORSE_SPEED_ADDR 10
#define EEPROM_MORSE_PAUSE_ADDR 20



// Configure software serial port
NeoSWSerial SIM900(7, 8);
//NeoSWSerial SIM900(11, 12);
//Variable to save incoming SMS characters
char in_str[200];
auto ts = millis();
unsigned long sos_ts = millis();

// LCD keypad
//LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//LCDKeypad lcdKeypad(PIN_A0, &lcd);
LEDMorseSender morse_smoke(PIN_MORSE);
LEDMorseSender morse_light(PIN_LIGHT);



//void printLcd(char* text);
void read_line();
char get_char();
uint16_t check_msg(char const *buf, const String &phone_number, char *return_ptr = 0, uint16_t *ret_val = 0);
bool process_morse();
void send_sms(const String &number, const String &text);
bool process_ss(unsigned long timout = 0);
void sos_auto();
String get_number(const char* buf);
bool is_hex_notation(const String &str);
String utf_convert(const String &str);
void morse_filter_str(const String str, char* dest);

void setup() {
    morse_smoke.set_pause(0.0f);
    morse_light.set_pause(0.0f);
    Serial.println(F("Starting setup"));

    if (true) {
        digitalWrite(PIN_SIM900_ON, HIGH);
        delay(1000);
        digitalWrite(9, LOW);
        delay(5000);
    }


    morse_smoke.setup();
    morse_light.setup();

    SIM900.begin(9600);
    Serial.begin(115200);

    //lcd.begin(16, 6);


    // Give time to your GSM shield log on to network
    //Serial.println("Waiting 2s");
    delay(2000);

    // AT command to set SIM900 to SMS mode
    SIM900.print(F("AT+CMGF=1\r"));
    delay(100);

    // Set module to send SMS data to serial out upon receipt
    SIM900.print(F("AT+CNMI=2,2,0,0,0\r"));
    delay(100);
    SIM900.print(F("AT+CCID\r"));
    delay(100);
    //SIM900.print("AT+IPR=9600\r");
    //SIM900.print("AT&W\r");


    uint16_t morse_speed = EEPROM.readInt(EEPROM_MORSE_SPEED_ADDR);
    if (morse_speed > 1 && morse_speed < 20000) {
        morse_smoke.setSpeed((morseTiming_t)morse_speed);
        morse_light.setSpeed((morseTiming_t)morse_speed);
    }
    else {
        morse_speed = 500;
        EEPROM.writeInt(EEPROM_MORSE_SPEED_ADDR, morse_speed);
    }

    uint16_t morse_pause = EEPROM.readInt(EEPROM_MORSE_PAUSE_ADDR);
    morse_smoke.set_pause((morseTiming_t)morse_pause);
    morse_light.set_pause((morseTiming_t)morse_pause);

    Serial.print(F("PAUSE set to: "));
    Serial.println(morse_pause);
    Serial.print(F("SPEED set to: "));
    Serial.println(morse_speed);
    Serial.println(F("Setup complete"));

    // morse_smoke.setMessage("sos");
    // morse_smoke.startSending();
    // morse_light.setMessage("sos");
    // morse_light.startSending();
}


void loop() {
    // Display any text that the GSM shield sends out on the serial monitor
    process_ss();
    if (!process_morse()) {
        sos_auto();
    }
    else {
        sos_ts = millis();
    }
    delay(50);
}



//SOS_AUTO_TIME_MS

void sos_auto() 
{
    unsigned long diff = millis() - sos_ts;
    //Serial.println(diff);
    //Serial.println(SOS_AUTO_TIME_MS);

    if (diff > SOS_AUTO_TIME_MS) {
        sos_ts = millis();
        morse_smoke.setMessage("sos");
        morse_smoke.startSending();
        morse_light.setMessage("sos");
        morse_light.startSending();
    }
}

bool process_ss(unsigned long timout = 0) 
{
    bool ret = false;
    unsigned long ts = millis();
    do {
        if (SIM900.available() > 0) {
            ret = true;
            delay(100);

            read_line();

            // Read SMS
            if (in_str[0] == '+' && in_str[1] == 'C' && in_str[2] == 'M' && in_str[3] == 'T' && in_str[4] == ':') {
                delay(100);
                String incoming_number = get_number(in_str);
                read_line();

                Serial.print(F("SMS received: "));
                Serial.println(in_str);
   
                if (check_msg(in_str, incoming_number)) {
                    return;
                }
                
                
                if (is_hex_notation(String(in_str))) {

                    Serial.println(F("Is HEX"));
                    String utf8_str = utf_convert(String(in_str));

                    Serial.print(F("UTF: "));
                    Serial.println(utf8_str);

                    morse_filter_str(utf8_str, in_str);
                    //char* filtered_c = filtered.c_str();

                    //strcpy(in_str, filtered.c_str());
                    //in_str[filtered.length()+1] = '\0';

                    //return;

                }
                else {
                    morse_filter_str(String(in_str), in_str);
                }

                if (morse_smoke.continueSending()) {
                    send_sms(incoming_number, F("Trenutno je dimnik zaseden, prosimo poskusite kasneje."));
                    return;
                }

                if (strlen(in_str) > 60) {
                    send_sms(incoming_number, F("Poslali ste predolgo sporocilo, max 60 znakov."));
                    return;
                }

                String morse_str(in_str);

                String return_sms;
                return_sms.concat(F("From:"));
                return_sms.concat(incoming_number);
                return_sms.concat(F(" SMS: "));
                return_sms.concat(in_str);
                send_sms(F("+38631882449"), return_sms);
                
                morse_str.toLowerCase();
                Serial.print(F("Setting morse to: "));
                Serial.println(morse_str);
                morse_smoke.setMessage(morse_str);
                morse_smoke.startSending();
                morse_light.setMessage(morse_str);
                morse_light.startSending();


            }
        }
    } while (millis() - ts < timout);

    return ret;

}

bool process_morse() 
{
    morse_light.continueSending();

    if (morse_smoke.continueSending()) {
        return true;
    }
    else {
        return false;
    }
} 


// Check for control messages
uint16_t check_msg(char const *buf, const String &phone_number, char *return_ptr = 0, uint16_t *ret_val = 0) {
    uint16_t ret = 0;

    Serial.print(F("Checking: "));
    Serial.println(buf);

    if (buf[0] == 'T' && buf[1] == 'I' && buf[2] == 'M' && buf[3] == 'E' && buf[4] == '=') {
        char data[10];
        int pos = 5;
        ret = 0;
        for (int i = 0; i < 10; i++) {
            char c = buf[i + pos];

            if ((c >= 48 && c <= 57)) {
                data[i] = c;
            } 
            else {

                data[i] = '\0';
                return_ptr = &data[0];
                
                uint16_t wpm_int = atoi(return_ptr);
                ret_val = &wpm_int;
                Serial.print(F("WPM: "));
                Serial.println(wpm_int);

                //morse.setWPM((morseTiming_t)wpm_int);
                morse_smoke.setSpeed((morseTiming_t)wpm_int);
                morse_light.setSpeed((morseTiming_t)wpm_int);

                String out = "Time is set to ";
                out.concat(String(wpm_int));

                //Serial.println(wpm_int);

                send_sms(phone_number, out);

                EEPROM.writeInt(EEPROM_MORSE_SPEED_ADDR, wpm_int);
                ret = wpm_int;
                return 1;
            }
        }  
    }

    if (buf[0] == 'P' && buf[1] == 'A' && buf[2] == 'U' && buf[3] == 'S' && buf[5] == '=') {
        char data[10];
        int pos = 6;
        ret = 0;
        for (int i = 0; i < 10; i++) {
            char c = buf[i + pos];

            if ((c >= 48 && c <= 57)) {
                data[i] = c;
            } 
            else {

                data[i] = '\0';
                return_ptr = &data[0];
                
                uint16_t wpm_int = atoi(return_ptr);
                ret_val = &wpm_int;
                Serial.print(F("Pause: "));
                Serial.println(wpm_int);

                //morse.setWPM((morseTiming_t)wpm_int);
                morse_smoke.set_pause((morseTiming_t)wpm_int);
                morse_light.set_pause((morseTiming_t)wpm_int);

                String out = F("Pause is set to ");
                out.concat(String(wpm_int));

                //Serial.println(wpm_int);

                send_sms(phone_number, out);

                EEPROM.writeInt(EEPROM_MORSE_PAUSE_ADDR, wpm_int);
                ret = wpm_int;
                return 1;
            }
        }
    }


    return ret;
}

String get_number(const char* buf) 
{
    String number;

    Serial.print(F("Getting number from: "));
    Serial.println(buf);

    if (buf[0] == '+' && buf[1] == 'C' && buf[2] == 'M' && buf[3] == 'T' && buf[4] == ':') {
        char data[10];
        int pos = 7;
        for (int i = 0; i < 15; i++) {
            char c = buf[i + pos];

            if ((c >= 48 && c <= 57) || c == 43) {
                number.concat(c);
            } 
            else {
                data[i] = '\0';
                return number;  
            }
        }
    }
    else {
        return number;
    }
}


void send_sms(const String &number, const String &text)
{
    Serial.print(F("Sending SMS: "));
    Serial.println(text);

    //SIM900.println("AT+CMGS=\"+ZZxxxxxxxxxx\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
    SIM900.print(F("AT+CMGS=\""));//change ZZ with country code and xxxxxxxxxxx with phone number to sms
    SIM900.print(number);
    SIM900.println(F("\""));//change ZZ with country code and xxxxxxxxxxx with phone number to sms
    process_ss(200);
    SIM900.print(text); //text content
    process_ss(200);
    SIM900.write(26);                //CTRL+Z key combination to send message
    process_ss();


}


char get_char() {
    return SIM900.read();
}

void read_line() {
    in_str[0] = '\0';
    int i = 0;
    ts = millis();
    static bool msg_r;

    if (SIM900.available()) {
        while (true) {
            msg_r = true;

            if (millis() > ts + 1000) {
                Serial.println(F("Error: serial poll timout"));
                break;
            }

            if (!SIM900.available()) {
                continue;
            }

            in_str[i] = get_char();
            //msg_line.concat(in_str[i]);

            //Serial.printc(in_str[i]);

            if (in_str[i] == '\n') {
                in_str[i] = '\0';
                break;
            }

            i++;
            in_str[i+1] = '\0';
        }
    }
    if (msg_r){
        Serial.print(F("Msg line is: "));
        Serial.println(in_str);
        msg_r = false;
    }
}

bool is_hex_notation(const String &str)
{
    if (str.length() < 4) {
        //Serial.println("Size < 4");
        return false;
    }

    // Serial.print(F("Testing string:"));
    // Serial.println(str);

    //Serial.println(str.length());

    for (int i = 0; i < str.length()-1; i += 4) {
        //Serial.println(i);
        String temp = str.substring(i, i+4);

        //Serial.print(temp);
        //Serial.print(F(": "));

        uint32_t val = strtol(temp.c_str(), NULL, 16);

        //Serial.print(val);
        //Serial.println(F(" "));

        if (val == 0) {
            //Serial.print(temp);
            //Serial.print(F(":::: "));
            //Serial.print(val);
            //Serial.println(" ");

            //Serial.print(F("Not hex: "));
            //Serial.println(temp);
            return false;
        }
    }
    //Serial.println(F("All hex"));

    return true;

}


String utf_convert(const String &str)
{

    String utf_str;

    if (str.length() < 4) {
        //Serial.println(F("Size < 4"));
        return utf_str;
    }

    for (int i = 0; i < str.length()-1; i += 4) {
        String temp = str.substring(i, i+4);

        uint32_t val = strtol(temp.c_str(), NULL, 16);

        if ((val >= 48 && val <= 57) || 
            (val >= 65 && val <= 90) ||
            (val >= 97 && val <= 122) ||
            val == 32) {
                utf_str.concat(char(val));
            }
    }

    return utf_str;
}

void morse_filter_str(const String str, char* dest) 
{
    unsigned int pos = 0;

    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        dest[pos] = '\0';
    
        if ((c >= 48 && c <= 57) || 
            (c >= 65 && c <= 90) ||
            (c >= 97 && c <= 122) ||
            c == ' ') {

            // Don't make double spaces
            if (dest[pos-1] == ' ' && c == ' ') {
                continue;
            }

            dest[pos] = c;
            pos++;
            continue;
        }
    }
    return;
}




































// void printLcd(char* text) {

//     //lcd.clear();
//     //lcd.home();


//     uint8_t pos = 0;
//     uint8_t line = 0;
//     while (text[pos] != 0) {

//         if (pos % 16 == 0 && pos != 0) {
//             //Serial.print("LCD new line: ");
//             //Serial.println(pos);
//             line++;
//             //lcd.println("");
//             //lcd.setCursor(0, line);
//             //lcd.print("\n\r");
//         }
        

//         //lcd.printc((char)text[pos]);

//         pos++;
//     }
// }
