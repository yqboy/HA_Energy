#include "config.h"

ESP8266WebServer httpServer(80);
WiFiClient espClient;
PubSubClient mqClient(espClient);

String hostname("AP-");
String DATA;
u32_t PFnum = 0;
u32_t oldPFreg = 0;
u8_t oldBit;
bool firstBit;

// 电压系数 = 150K/(49.9*1K)
float VF = 3.006;
// 电流系数 = 次级线圈匝数/(1000x采样电阻)*初级线圈匝数
// 电流系数 = 2000/(1000x10)*10
// 电流系数 = 最大量程/(2.5x采样电阻)
// 电流系数 = 50A/(2.5x10)
float CF = 2.0;

u32_t lastTime;

void setup()
{
    hostname += String(ESP.getChipId(), HEX);
    hostname.toUpperCase();
    fsRead();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    httpServer.on("/", handleRoot);
    httpServer.on("/save", handleSave);
    httpServer.on("/state", handleState);
    httpServer.on("/reset", handleReset);
    httpServer.on("/upload", HTTP_POST, handleFirmware, handleUpload);
    httpServer.onNotFound(handleRoot);
    httpServer.begin();

    if (mConfig.ssid == "")
        modeConfig();

    connectWifi(mConfig.ssid, mConfig.pwd);

    Serial.begin(4800, SERIAL_8E1, SERIAL_RX_ONLY);
    Serial.setTimeout(25);
}

void loop()
{
    httpServer.handleClient();
    mqttConnect();
    mqClient.loop();
    SerialEvent();
}

void SerialEvent()
{
    if (Serial.available() > 0)
    {
        char buf[24];
        size_t len = Serial.readBytes(buf, 24);
        if (len != 24)
        {
            while (Serial.available() > 0)
                Serial.read();
            return;
        }

        // if (buf[0] != 0x55 || buf[1] != 0x5A || !checkSum(buf))
        if (buf[1] != 0x5A || !checkSum(buf))
            return;

        String energy_voltage = "0", energy_current = "0", energy_power = "0";
        energy_voltage = String(float(buf[2] * 65536 + buf[3] * 256 + buf[4]) / float(buf[5] * 65536 + buf[6] * 256 + buf[7]) * VF);
        if (buf[0] == 0x55)
        {
            energy_current = String(float(buf[8] * 65536 + buf[9] * 256 + buf[10]) / float(buf[11] * 65536 + buf[12] * 256 + buf[13]) * CF);
            energy_power = String(float(buf[14] * 65536 + buf[15] * 256 + buf[16]) / float(buf[17] * 65536 + buf[18] * 256 + buf[19]) * VF * CF);
        }

        u8_t pf_reg = bitRead(buf[20], 7);

        if (!firstBit)
        {
            firstBit = true;
            oldBit = pf_reg;
            oldPFreg = buf[21] * 256 + buf[22];
            PFnum = 0;
        }

        if (pf_reg != oldBit)
        {
            PFnum++;
            oldBit = pf_reg;
        }

        uint32_t PFcnt = (PFnum * 65536 + (buf[21] * 256 + buf[22])) - oldPFreg;
        uint32_t PowerReg = buf[14] * 65536 + buf[15] * 256 + buf[16];
        String energy_total = String(PFcnt / (1E9 * 3600 / (PowerReg * VF * CF)));
        // String bufHex;
        // for (size_t i = 0; i < 24; i++)
        //     bufHex += String(buf[i], HEX);

        DATA = "电压:\t" + energy_voltage + "V<br/>电流:\t" + energy_current + "A<br/>功率:\t" + energy_power + "W<br/>电量:\t" + energy_total + "kWh<br/>";

        if (millis() - lastTime > 10 * 1000 || lastTime == 0)
        {
            String topic = "homeassistant/sensor/energy/" + hostname + "/total";
            mqClient.publish(topic.c_str(), energy_total.c_str());
            delay(100);
            topic = "homeassistant/sensor/energy/" + hostname + "/voltage";
            mqClient.publish(topic.c_str(), energy_voltage.c_str());
            delay(100);
            topic = "homeassistant/sensor/energy/" + hostname + "/current";
            mqClient.publish(topic.c_str(), energy_current.c_str());
            delay(100);
            topic = "homeassistant/sensor/energy/" + hostname + "/power";
            mqClient.publish(topic.c_str(), energy_power.c_str());
            lastTime = millis();
        }
    }
}

bool checkSum(char *data)
{
    byte check = 0;
    for (byte i = 2; i < 23; i++)
        check = check + data[i];
    return check == data[23];
}

void fsRead()
{
    if (!LittleFS.begin())
        return;
    if (!LittleFS.exists("/config"))
        return;

    File fs = LittleFS.open("/config", "r");

    int i = 0;
    while (fs.available())
    {
        i++;
        String data = fs.readStringUntil('\n');
        data.trim();
        switch (i)
        {
        case 1:
            mConfig.ssid = data;
            break;
        case 2:
            mConfig.pwd = data;
            break;
        case 3:
            mConfig.host = data;
            break;
        }
    }
    fs.close();
}

void fsWrite()
{
    File fs = LittleFS.open("/config", "w");
    fs.println(mConfig.ssid);
    fs.println(mConfig.pwd);
    fs.println(mConfig.host);
    delay(100);
    fs.close();
}

void modeConfig()
{
    DNSServer dns;
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", IPAddress(192, 168, 4, 1));
    digitalWrite(LED_BUILTIN, LOW);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(hostname);
    while (true)
    {
        dns.processNextRequest();
        httpServer.handleClient();
    }
}

bool connectWifi(String ssid, String pass)
{
    delay(1500);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostname.c_str());

    WiFi.begin(ssid, pass);

    unsigned long connectTime = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if ((millis() - connectTime) > 20000)
            break;

        blinkLED();
        delay(200);
    }

    digitalWrite(LED_BUILTIN, HIGH);
    return WiFi.status() == WL_CONNECTED;
}

void handleRoot()
{
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_FORM_DATA);
    page.replace("{hostname}", hostname);
    page.replace("{version}", VERSION);
    page.replace("{DATA}", DATA);
    page.replace("{ssid}", mConfig.ssid);
    page.replace("{pwd}", mConfig.pwd);
    page.replace("{host}", mConfig.host);
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
}

void handleSave()
{
    mConfig.ssid = httpServer.arg("ssid");
    mConfig.pwd = httpServer.arg("pwd");
    mConfig.host = httpServer.arg("host");
    fsWrite();
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_SAVED);
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
    delay(100);
    ESP.restart();
}

void handleState()
{
    httpServer.send(200, "text/html", DATA);
}

void handleReset()
{
    LittleFS.format();
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200);
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_CLEAN);
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
    delay(100);
    ESP.restart();
}

void handleFirmware()
{
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_FIRMWARE);
    page.replace("{firmware}", (Update.hasError()) ? "失败" : "成功");
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
    delay(100);
    ESP.restart();
}

void handleUpload()
{
    HTTPUpload &upload = httpServer.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        Update.begin(maxSketchSpace, U_FLASH);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
        Update.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END)
        Update.end(true);
}

void mqPubConfig()
{

    delay(100);
    String topic = "homeassistant/sensor/energy_total/" + hostname + "/config";
    String payload = "{\"name\":\"电量\",\"device_class\":\"energy\",\"state_topic\":\"homeassistant/sensor/energy/" + hostname + "/total\",\"state_class\":\"total_increasing\",\"unit_of_measurement\":\"kWh\",\"unique_id\":\"" + hostname + "_total\",\"device\":{\"identifiers\":[\"" + hostname + "\"],\"name\":\"" + hostname + "\",\"manufacturer\":\"虤虎科技\",\"model\":\"能源采集\"}}";
    mqClient.publish(topic.c_str(), payload.c_str(), true);

    delay(100);
    topic = "homeassistant/sensor/energy_voltage/" + hostname + "/config";
    payload = "{\"name\":\"电压\",\"device_class\":\"voltage\",\"state_topic\":\"homeassistant/sensor/energy/" + hostname + "/voltage\",\"unit_of_measurement\":\"V\",\"unique_id\":\"" + hostname + "_voltage\",\"device\":{\"identifiers\":[\"" + hostname + "\"],\"name\":\"" + hostname + "\",\"manufacturer\":\"虤虎科技\",\"model\":\"能源采集\"}}";
    mqClient.publish(topic.c_str(), payload.c_str(), true);

    delay(100);
    topic = "homeassistant/sensor/energy_current/" + hostname + "/config";
    payload = "{\"name\":\"电流\",\"device_class\":\"current\",\"state_topic\":\"homeassistant/sensor/energy/" + hostname + "/current\",\"unit_of_measurement\":\"A\",\"unique_id\":\"" + hostname + "_current\",\"device\":{\"identifiers\":[\"" + hostname + "\"],\"name\":\"" + hostname + "\",\"manufacturer\":\"虤虎科技\",\"model\":\"能源采集\"}}";
    mqClient.publish(topic.c_str(), payload.c_str(), true);

    delay(100);
    topic = "homeassistant/sensor/energy_power/" + hostname + "/config";
    payload = "{\"name\":\"功率\",\"device_class\":\"power\",\"state_topic\":\"homeassistant/sensor/energy/" + hostname + "/power\",\"unit_of_measurement\":\"W\",\"unique_id\":\"" + hostname + "_power\",\"device\":{\"identifiers\":[\"" + hostname + "\"],\"name\":\"" + hostname + "\",\"manufacturer\":\"虤虎科技\",\"model\":\"能源采集\"}}";
    mqClient.publish(topic.c_str(), payload.c_str(), true);
}

void mqttConnect()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    if (!mqClient.connected())
    {
        if (mConfig.host == "")
            return;

        int i = mConfig.host.indexOf(':');
        if (i == -1)
            return;

        String host = mConfig.host.substring(0, i);
        int port = atoi(mConfig.host.substring(i + 1, mConfig.host.length()).c_str());
        mqClient.setServer(host.c_str(), port);
        mqClient.setCallback(mqttCallback);
        if (mqClient.connect(hostname.c_str()))
        {
            mqPubConfig();
            String sub = "homeassistant/sensor/" + hostname + "/reset";
            mqClient.subscribe(sub.c_str());
            delay(200);
        }
        else
            delay(5000);
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    firstBit = false;
}

void blinkLED()
{
    uint8_t state = digitalRead(LED_BUILTIN);
    digitalWrite(LED_BUILTIN, !state);
}
