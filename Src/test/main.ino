#include "config.h"

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqClient(espClient);
DNSServer dns;
Ticker ledTicker;
Ticker mqTicker;

String hostname("AYH-");
boolean toconnect = false;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_SWITCH, OUTPUT);

  hostname += String(ESP.getChipId(), HEX);
  hostname.toUpperCase();

  readConfig();

  if (!connectWifi(mConfig.ssid, mConfig.pass))
    modeConfig();

  Serial.println(WiFi.localIP());
  int i = mConfig.host.indexOf(':');
  String host = mConfig.host.substring(0, i);
  int port = atoi(mConfig.host.substring(i + 1, mConfig.host.length()).c_str());
  Serial.println(host);
  Serial.println(port);
  // IPAddress ip(192, 168, 88, 20);
  // mqClient.setServer(host.c_str(), port);
  // mqClient.setServer("broker-cn.emqx.io", 1883);
  mqClient.setServer("broker.emqx.io", 1883);
  mqClient.setCallback(mqttCallback);

  // ledTicker.attach(1, blinkLED);
  mqTicker.attach(5, mqttLoop);
}

void loop()
{
  // mqttLoop();
}

void readConfig()
{
  if (LittleFS.begin())
  {
    if (LittleFS.exists("/config.json"))
    {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        int i = 0;
        while (configFile.available())
        {
          i++;
          String data = configFile.readStringUntil('\n');
          data.trim();
          switch (i)
          {
          case 1:
            mConfig.ssid = data;
            break;
          case 2:
            mConfig.pass = data;
            break;
          case 3:
            mConfig.host = data;
            break;
          case 4:
            mConfig.subscribe = data;
            break;
          }
        }
      }
      configFile.close();
      return;
    }
  }
}

void writeConfig(Config *c)
{
  File configFile = LittleFS.open("/config.json", "w");
  configFile.println(c->ssid);
  configFile.println(c->pass);
  configFile.println(c->host);
  configFile.println(c->subscribe);
  configFile.close();
}

void modeConfig()
{
  digitalWrite(LED_BUILTIN, LOW);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(hostname);

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", IPAddress(192, 168, 4, 1));

  httpUpdater.setup(&server, "/firmware");
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound(handleRoot);
  server.begin();

  while (true)
  {
    delay(1);
    dns.processNextRequest();
    server.handleClient();
    if (toconnect)
    {
      if (connectWifi(mConfig.ssid, mConfig.pass))
      {
        server.close();
        dns.stop();
        break;
      }
      else
      {
        digitalWrite(LED_BUILTIN, LOW);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(hostname);
      }
    }
  }
}

void handleRoot()
{
  String page = FPSTR(HTTP_HEADER);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEADER_END);
  page += FPSTR(HTTP_FORM_DATA);
  page.replace("{version}", VERSION);
  page.replace("{ssid}", mConfig.ssid);
  page.replace("{pass}", mConfig.pass);
  page.replace("{host}", mConfig.host);
  page.replace("{subscribe}", mConfig.subscribe);
  page += FPSTR(HTTP_END);

  server.sendHeader("Content-Length", String(page.length()));
  server.send(200, "text/html", page);
}

void handleSave()
{
  mConfig.ssid = server.arg("ssid");
  mConfig.pass = server.arg("pwd");
  mConfig.host = server.arg("host");
  mConfig.subscribe = server.arg("sub");

  writeConfig(&mConfig);

  String page = FPSTR(HTTP_HEADER);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEADER_END);
  page += FPSTR(HTTP_SAVED);
  page += FPSTR(HTTP_END);
  server.sendHeader("Content-Length", String(page.length()));
  server.send(200, "text/html", page);

  toconnect = true;
}

unsigned long connectTime = 0;
bool connectWifi(String ssid, String pass)
{
  ledTicker.attach_ms(200, blinkLED);
  delay(1500);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);

  connectTime = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if ((millis() - connectTime) > 20000)
      break;

    delay(10);
  }
  ledTicker.detach();
  digitalWrite(LED_BUILTIN, HIGH);
  toconnect = WiFi.status() == WL_CONNECTED;
  return toconnect;
}

void mqttConnect()
{
  if (mqClient.connect(hostname.c_str()))
    mqClient.subscribe(mConfig.subscribe.c_str());
  // mqClient.subscribe("myiot/hass/switch1");
  else
  {
    Serial.print("MQTT Failed, rc=");
    Serial.println(mqClient.state());
  }
}

void mqttLoop()
{
  if (mqClient.connected())
    mqClient.loop();
  else
    mqttConnect();
}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length)
{
  Serial.println(payload[0]);
  if (length > 1)
    return;
  digitalWrite(PIN_SWITCH, payload[0]);
  digitalWrite(LED_BUILTIN, payload[0]);
}

void blinkLED()
{
  uint8_t state = digitalRead(LED_BUILTIN);
  digitalWrite(LED_BUILTIN, !state);
}