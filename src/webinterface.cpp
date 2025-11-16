#include "webinterface.h"

WebInterface::WebInterface()
    : m_server(80)
    , m_lastMeasurementHtml(F("<h1>Power Meter</h1><p>No measurements yet.</p>"))
    , m_webReady(false)
    , m_connected(false)
    , m_localIp()
{
}

String WebInterface::buildPage() const
{
  String html;
  html.reserve(256);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Power Meter</title>");
  html += F("<style>body{font-family:sans-serif;margin:1.5em;}h1{font-size:1.5em;}table{border-collapse:collapse;}td{padding:0.25em 0.5em;border:1px solid #ccc;}</style>");
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

void WebInterface::updateMeasurements(float vBus, float vShunt, float temperature, float current_mA)
{
  m_lastMeasurementHtml = F("<h1>Power Meter</h1>");
  m_lastMeasurementHtml += F("<table>");

  auto addRow =
      [&](const __FlashStringHelper *label, float value, uint8_t decimals, const __FlashStringHelper *unit)
      {
        String row;
        row.reserve(64);
        row += F("<tr><td>");
        row += label;
        row += F("</td><td>");
        row += String(value, decimals);
        row += F("</td><td>");
        row += unit;
        row += F("</td></tr>");
        m_lastMeasurementHtml += row;
      };

  addRow(F("Vbus"), vBus, 3, F("V"));
  addRow(F("Vshunt"), vShunt, 6, F("V"));
  addRow(F("Temp"), temperature, 2, F("C"));
  addRow(F("Current"), current_mA, 2, F("mA"));

  m_lastMeasurementHtml += F("</table>");
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
