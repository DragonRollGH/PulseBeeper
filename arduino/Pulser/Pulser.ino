#include <ArduinoJson.h>
#include <base64.hpp>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <LittleFS.h>
#include <MQTT.h>
#include <NeoPixelBus.h>
#include <OneButton.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <Wire.h>

#include "DataStream.cpp"
#include "Pixel.cpp"

// const int MPU = 0x68;  //MPU-6050的I2C地址
// const int nValCnt = 4; //一次读取寄存器的数量

const byte PixelLen = 20;
const byte FrameRate = 17; // =1000ms/60fps
const byte PinTouch = 12;
const char *AP_SSID = "Rolls_Pulser";
const char *Name = "Roll_v1.0.04200019";

// const char *MQTTServer = "";
// const int   MQTTPort = 1883;
// const char *MQTTUsername = "";
// const char *MQTTPassword = "";
// const char *MQTTClientid = "";
// const char *MQTTSub1 = "";
// const char *MQTTSub2 = "";
// const char *MQTTPub = "";


//define in FS
byte H = 0;
byte S = 255;
byte L = 50;
byte A = 5;
byte B = 35;
byte sleepRun = 1;
byte sleepIdle = 100; // > 300 is useless
unsigned int flowCache = 1;
byte indicatorLightness = 10;
byte indicatorPin = 10;
bool indicatorToggleFlag = 0;
bool heartBeginFlag = 0;


byte sleep = sleepIdle;
unsigned int flowFrame;
unsigned long flowStart;

ESP8266WiFiMulti STA;
MQTTClient MQTT(512);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> heart(PixelLen);
OneButton button(PinTouch, false, false);
WiFiClient WLAN;
WiFiManager WM;
Ticker heartTicker;

DataStream stream;
Pixel pixels[PixelLen];

// class MenuSystem
// {
// public:
//     byte tapType[5];
//     void (*callback)();
//     MenuSystem()
//     {}
//     MenuSystem(byte tT[], void (*cb)())
//     {
//         for (byte i = 0; i < 5; ++i)
//         {
//             tapType[i] = tT[i];
//         }
//         callback = cb;
//     }
// };
// MenuSystem menu[6];
// byte menuCurrent = 0;

// void menuHome()
// {
//     Serial.println("Home");
// }
// void menuMain()
// {
//     Serial.println("Main");
// }
// void menuAck()
// {
//     Serial.println("Ack");
// }
// void menuBattery()
// {
//     Serial.println("Battery");
// }
// void menuPulse()
// {
//     Serial.println("Pulse");
// }
// void menuSubmenu()
// {
//     Serial.println("Submenu");
// }
ICACHE_RAM_ATTR void checkTicks()
{
    button.tick();
}
// void pressStop()
// {
//     Serial.println("pressStop");
// }
// void singleClick()
// {
//     Serial.println("singleClick");
// }
// void doubleClick()
// {
//     Serial.println("doubleClick");
// }
// void multiClick()
// {
//     Serial.println("multiClick");
// }
// void pressStart()
// {
//     Serial.println("pressStart");
// }
// void pressStop()
// {
//     menuCurrent = menu[menuCurrent].tapType[0];
//     menu[menuCurrent].callback();
// }
// void singleClick()
// {
//     menuCurrent = menu[menuCurrent].tapType[1];
//     menu[menuCurrent].callback();
// }
// void doubleClick()
// {
//     menuCurrent = menu[menuCurrent].tapType[2];
//     menu[menuCurrent].callback();
// }
// void multiClick()
// {
//     menuCurrent = menu[menuCurrent].tapType[3];
//     menu[menuCurrent].callback();
// }
// void pressStart()
// {
//     menuCurrent = menu[menuCurrent].tapType[4];
//     menu[menuCurrent].callback();
// }

void mqttMsg(String &topic, String &payload)
{
    // Serial.println("Message arrived [" + topic + "] " + payload);

    if (payload.length() > 1 && payload[0] == ':')
    {
        switch (payload[1])
        {
        case 'A':
            cmdACK();
            break;
        case 'B':
            cmdBattery();
            break;
        case 'D':
            cmdDefault(payload);
            break;
        case 'F':
            cmdFile(payload);
            break;
        case 'H':
            cmdHSL(payload);
            break;
        case 'P':
            cmdPulse(payload);
            break;
        case 'R':
            cmdRGB(payload);
            break;
        case 'U':
            cmdUpdate(payload);
            break;
        default:
            MQTT.publish(MQTTPub, "Unknown command");
            break;
        }
    }
}

void cmdACK(void)
{
    MQTT.publish(MQTTPub, Name);
}

void cmdBattery(void)
{
    MQTT.publish(MQTTPub, String(getBattery()));
}

void cmdDefault(String &payload) {}

void cmdFile(String &payload)
{
    LittleFS.begin();
    LittleFS.end();
}

void cmdHSL(String &payload)
{
    if (payload.length() > 2)
    {
        for (unsigned int i = 2; i < payload.length(); i++)
        {
            stream.write(payload[i]);
        }
    }
}

void cmdPulse(String &payload) {}

void cmdRGB(String &payload) {}

void cmdUpdate(String &paylaod)
{
    String url = "http://192.168.1.110:5500/arduino/Pulser/Pulser.ino.generic.bin";
    if (paylaod.length() > 2)
    {
        url = paylaod.substring(2);
    }
    // Serial.println("Starting update from " + url);
    MQTT.publish(MQTTPub, "Starting update from " + url);

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart([] { MQTT.publish(MQTTPub, "[httpUpdate] Started"); });
    ESPhttpUpdate.onError([](int err) { MQTT.publish(MQTTPub, String("[httpUpdate] Error: ") + ESPhttpUpdate.getLastErrorString().c_str()); });
    ESPhttpUpdate.update(url);
}

float getBattery()
{
    unsigned int adcs = 0;
    for (byte i = 0; i < 10; i++)
    {
        adcs += analogRead(A0);
        delay(10);
    }
    float voltage = (adcs / 10) * 247.0f / 1024 / 47 - 0.19;
    return voltage;
}

void setPixelsColor(void)
{
    ++flowFrame;
    bool running = 1;
    while (stream.avalible && running)
    {
        switch (stream.read())
        {
        case '&':
            switch (stream.read())
            {
            case 'D':
                useDefault();
                break;
            case 'H':
                H = parseHex(stream.read(), stream.read());
                break;
            case 'S':
                S = parseHex(stream.read(), stream.read());
                break;
            case 'L':
                L = parseHex(stream.read(), stream.read());
                break;
            case 'A':
                A = parseHex(stream.read(), stream.read());
                break;
            case 'B':
                B = parseHex(stream.read(), stream.read());
                break;
            case 'C':
                setHSL(stream.read(), stream.read(), stream.read(), stream.read());
                break;
            case 'N':
                runPixels(stream.read(), stream.read(), stream.read(), stream.read());
                break;
            default:
                break;
            }
            break;
        case ';':
            running = 0;
            break;
        default:
            break;
        }
    }
    bool anyActive = 0;
    for (byte i = 0; i < PixelLen; i++)
    {
        if (pixels[i].active)
        {
            heart.SetPixelColor(i, HslColor(pixels[i].H, pixels[i].S, pixels[i].L));
            anyActive = 1;
        }
        else
        {
            heart.SetPixelColor(i, RgbColor(0, 0, 0));
        }
        pixels[i].update();
    }
    heart.Show();
    if (!anyActive)
    {
        stopFlow();
    }
}

void useDefault(void) {}

byte parseHex(byte L)
{
    if ((L >= '0') && (L <= '9'))
        return L - '0';
    if ((L >= 'A') && (L <= 'F'))
        return L + 10 - 'A';
    if ((L >= 'a') && (L <= 'a'))
        return L + 10 - 'a';
    return -1;
}

byte parseHex(byte H, byte L)
{
    return parseHex(H) * 16 + parseHex(L);
}

void setHSL(byte b1, byte b2, byte b3, byte b4) {}

void runPixels(byte b1, byte b2, byte b3, byte b4)
{
    byte base[4] = {b1, b2, b3, b4};
    byte arry[3];
    decode_base64(base, arry);
    for (byte i = 0; i < PixelLen; i++)
    {
        if (arry[i / 8] & (byte)128)
        {
            pixels[i].run(H, S, L, A, B);
        }
        arry[i / 8] <<= 1;
    }
}

void runFlow(void)
{
    flowFrame = 1;
    flowStart = millis();
    sleep = sleepRun;
    heart.Begin();
    heartTicker.attach_ms(17, heartTick);
}

void stopFlow(void)
{
    flowStart = 0;
    sleep = sleepIdle;
    pinMode(3, INPUT);
    heartTicker.detach();
}

void heartBegin()
{
    if (!heartBeginFlag)
    {
        heartBeginFlag = 1;
        heart.Begin();
    }
}

void heartClear()
{
    heart.ClearTo(RgbColor(0, 0, 0));
    heart.Show();
}

void heartClear(byte i)
{
    heart.SetPixelColor(i, RgbColor(0, 0, 0));
    heart.Show();
}

void heartEnd()
{
    pinMode(3, INPUT);
}

void heartIndicator(byte i, char s)
{
    // IndicatorBlink = !IndicatorBlink;
    // heartIndicator(i, s, IndicatorBlink);
}

void heartIndicator(byte i, char s, bool b)
{
    RgbColor color;
    if (b)
    {
        switch (s)
        {
        case 'r':
            color = RgbColor(indicatorLightness, 0, 0);
            break;
        case 'g':
            color = RgbColor(0, indicatorLightness, 0);
            break;
        case 'b':
            color = RgbColor(0, 0, indicatorLightness);
            break;
        case 'k':
            color = RgbColor(0, 0, 0);
            break;
        default:
            color = RgbColor(0, 0, 0);
            break;
        }
    }
    else
    {
        color = RgbColor(0, 0, 0);
    }
    heart.SetPixelColor(i, color);
    heart.Show();
}

void heartTick()
{
    setPixelsColor();
}

void indicatorClear()
{
    heartBegin();
    heart.SetPixelColor(indicatorPin, RgbColor(0, 0, 0));
    heart.Show();
}

void indicatorSet(char c)
{
    heartBegin();
    RgbColor color;
    switch (c)
    {
    case 'r':
        color = RgbColor(indicatorLightness, 0, 0);
        break;
    case 'g':
        color = RgbColor(0, indicatorLightness, 0);
        break;
    case 'b':
        color = RgbColor(0, 0, indicatorLightness);
        break;
    default:
        color = RgbColor(0, 0, 0);
        break;
    }
    heart.SetPixelColor(indicatorPin, color);
    heart.Show();
}

void indicatorToggle(char c)
{
    indicatorToggleFlag = !indicatorToggleFlag;
    if (indicatorToggleFlag)
    {
        indicatorSet(c);
    }
    else
    {
        indicatorClear();
    }
}

void MQTTConnect()
{
    for (byte i = 0; i < 120; ++i)
    {
        indicatorToggle('g');
        if (WiFi.status() != WL_CONNECTED)
        {
            WiFiConnect();
        }
        if (MQTT.connect(MQTTClientid, MQTTUsername, MQTTPassword))
        {
            break;
        }
        delay(500);
    }
    MQTT.subscribe(MQTTSub1);
    delay(10);
    MQTT.subscribe(MQTTSub2);
    cmdACK();
    heartClear();
}

void MQTTInitialize()
{
    MQTT.begin(MQTTServer, MQTTPort, WLAN);
    MQTT.onMessage(mqttMsg);
}

void MQTTLoop()
{
    if (!MQTT.connected())
    {
        MQTTConnect();
    }
    MQTT.loop();
}

int WiFiConnect()
{
    //stopTicker!
    for (byte i = 0; i < 30; ++i)
    {
        indicatorToggle('r');
        if (STA.run() == WL_CONNECTED)
        {
            return 1;
        }
        delay(500);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        indicatorSet('b');
        WM.setConfigPortalTimeout(180);
        WM.startConfigPortal(AP_SSID);
        if (WiFi.status() == WL_CONNECTED)
        {
            return 1;
        }
        else
        {
            ESP.deepSleepMax();
        }
    }
}

void WiFiInitialize()
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
    STA.addAP("iTongji-manul", "YOUYUAN4411");
    STA.addAP("DragonRoll", "1234567890");
}

void setup()
{
    heart.Begin();
    WiFiInitialize();
    MQTTInitialize();

    attachInterrupt(digitalPinToInterrupt(PinTouch), checkTicks, CHANGE);
    button.attachClick([]() { stream.write("&NgAAA;"); });
    button.attachDoubleClick([]() { stream.write("&NwAAA;"); });
    button.attachMultiClick([]() { stream.write("&N4AAA;"); });
    button.attachLongPressStart([]() { stream.write("&N8AAA;"); });
    button.attachLongPressStop([]() { stream.write("&N+AAA;"); });
}

void loop()
{
    MQTTLoop();

    button.tick();

    if (!flowStart && stream.avalible >= flowCache)
    {
        runFlow();
    }

    delay(sleep);
}
