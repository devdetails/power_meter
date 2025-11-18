#include "webinterface.h"

#include "ina_values.h"
#include "value_format.h"
#include <math.h>

WebInterface::WebInterface()
    : m_server(80)
    , m_lastMeasurementHtml(F("<h1>Power Meter</h1><p>No measurements yet.</p>"))
    , m_webReady(false)
    , m_connected(false)
    , m_localIp()
    , m_lastEnergyWs(NAN)
{
}

String WebInterface::buildPage() const
{
  String html;
  html.reserve(384);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='1'>"
            "<title>Power Meter</title>");
  html += F("<style>body{font-family:sans-serif;margin:1.5em;}h1{font-size:1.5em;}table{border-collapse:collapse;margin-bottom:1em;}td,th{padding:0.25em 0.5em;border:1px solid #ccc;}th{text-align:left;background:#f7f7f7;}td:last-child{text-align:right;}</style>");
  html += F("</head><body>");
  html += m_lastMeasurementHtml;

  if (WiFi.status() == WL_CONNECTED)
  {
    html += F("<p>IP: ");
    html += WiFi.localIP().toString();
    html += F("</p>");
  }

  html += F("</body></html>");
  return html;
}

bool WebInterface::begin(const char *ssid, const char *password)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print(F("Connecting to WiFi"));
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000UL)
  {
    delay(250);
    Serial.print(F("."));
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("WiFi connection failed."));
    m_connected = false;
    m_webReady  = false;
    m_localIp   = IPAddress();
    return false;
  }

  m_localIp = WiFi.localIP();

  Serial.print(F("Connected! IP: "));
  Serial.println(m_localIp);

  m_server.on("/",
              [this]()
              {
                m_server.send(200, "text/html", buildPage());
              });
  m_server.begin();
  m_webReady  = true;
  m_connected = true;
  return true;
}

void WebInterface::updateMeasurements(const InaValues &values)
{
  const float deltaWh = max((!isnan(m_lastEnergyWs)) ? (values.energyWs - m_lastEnergyWs) / 3600.0f : 0.0f, 0.0f);
  const float totalWh = values.energyWs / 3600.0f;

  const String currentStr     = formatValue(values.current_mA / 1000.0f, "A", 5);
  const String deltaEnergyStr = formatValue(deltaWh, "Wh", 5);
  const String totalEnergyStr = formatValue(totalWh, "Wh", 5);
  const String vbusStr        = formatValue(values.vBus, "V", 3);

  String tempStr;
  tempStr.reserve(12);
  tempStr += String(values.temperature, 1);
  tempStr += F(" C");

  m_lastMeasurementHtml = F("<h1>Power Meter</h1>");
  m_lastMeasurementHtml += F("<table><tr><th colspan='2'>Last measurement</th></tr>");

  auto addRow =
      [&](const __FlashStringHelper *label, const String &value)
      {
        String row;
        row.reserve(64);
        row += F("<tr><td>");
        row += label;
        row += F("</td><td>");
        row += value;
        row += F("</td></tr>");
        m_lastMeasurementHtml += row;
      };

  addRow(F("Current"), currentStr);
  addRow(F("Energy"), deltaEnergyStr);
  addRow(F("Vbus"), vbusStr);
  addRow(F("Temp"), tempStr);

  m_lastMeasurementHtml += F("</table>");

  m_lastMeasurementHtml += F("<table><tr><th colspan='2'>Total energy</th></tr>");
  addRow(F("Energy"), totalEnergyStr);
  m_lastMeasurementHtml += F("</table>");

  m_lastEnergyWs = values.energyWs;
}

void WebInterface::loop()
{
  if (!m_webReady)
  {
    return;
  }

  m_server.handleClient();
  yield();
}

IPAddress WebInterface::localIp() const
{
  return m_localIp;
}

bool WebInterface::isConnected() const
{
  return m_connected;
}
