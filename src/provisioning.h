#pragma once
static String provHtmlPage()
{
  String html;
  html.reserve(7500);
  html += F("<!DOCTYPE html><html lang='id'><head>"
            "<meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<title>Device Setup - Attendance Machine</title>"
            "<style>"
            "*{box-sizing:border-box;margin:0;padding:0}"
            "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            "background:#0a0f1e;min-height:100vh;display:flex;flex-direction:column;"
            "align-items:center;justify-content:center;padding:16px}"
            ".card{background:#111827;border:1px solid #1f2937;border-radius:20px;"
            "width:100%;max-width:480px;overflow:hidden;box-shadow:0 25px 50px rgba(0,0,0,.5)}"
            ".header{padding:32px 32px 28px;border-bottom:1px solid #1f2937;"
            "background:linear-gradient(160deg,#111827 0%,#0f172a 100%)}"
            ".header-title h1{font-size:26px;font-weight:800;letter-spacing:-.5px;"
            "background:linear-gradient(90deg,#f8fafc,#94a3b8);"
            "-webkit-background-clip:text;-webkit-text-fill-color:transparent}"
            ".header-sub{color:#475569;font-size:12px;letter-spacing:2px;"
            "text-transform:uppercase;font-weight:500;margin-top:4px}"
            ".body{padding:28px 32px}"
            ".section{margin-bottom:22px}"
            ".section-label{color:#374151;font-size:10px;font-weight:700;"
            "letter-spacing:2px;text-transform:uppercase;margin-bottom:12px;"
            "display:flex;align-items:center;gap:10px}"
            ".section-label::after{content:'';flex:1;height:1px;background:#1f2937}"
            ".field{margin-bottom:10px}"
            "label{display:block;color:#6b7280;font-size:12px;font-weight:500;margin-bottom:5px}"
            "input,select{width:100%;background:#0a0f1e;border:1px solid #1f2937;border-radius:8px;"
            "color:#e5e7eb;font-size:14px;padding:10px 14px;outline:none;"
            "transition:border .2s,box-shadow .2s}"
            "input:focus,select:focus{border-color:#1d4ed8;box-shadow:0 0 0 3px rgba(29,78,216,.15)}"
            "input::placeholder{color:#374151}"
            "select option{background:#111827}"
            ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
            ".row4{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:8px}"
            ".hint{color:#4b5563;font-size:11px;margin-top:4px}"
            ".btn{width:100%;background:#1d4ed8;color:#fff;border:none;border-radius:10px;"
            "padding:13px;font-size:14px;font-weight:600;cursor:pointer;margin-top:4px;"
            "letter-spacing:.5px;transition:background .2s,transform .1s;"
            "display:flex;align-items:center;justify-content:center;gap:8px}"
            ".btn:hover{background:#2563eb}.btn:active{transform:scale(.99)}"
            ".footer{text-align:center;padding:16px 32px;border-top:1px solid #1f2937}"
            ".footer p{color:#374151;font-size:11px;line-height:1.8}"
            ".footer strong{color:#4b5563}"
            "</style></head><body>"
            "<div class='card'>"
            "<div class='header'>"
            "<div class='header-title'><h1>DEVICE SETUP</h1></div>"
            "<div class='header-sub'>Attendance Machine</div>"
            "</div>"
            "<div class='body'><form method='POST' action='/save'>"
            "<div class='section'><div class='section-label'>WiFi Utama</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid1' placeholder='Nama Jaringan' required></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass1' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>WiFi Cadangan 1</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid2' placeholder='Nama Jaringan'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass2' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>WiFi Cadangan 2</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid3' placeholder='Nama Jaringan'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass3' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>Konfigurasi Perangkat</div>"
            "<div class='field'><label>API URL Backend</label>"
            "<input name='apiurl' placeholder='https://domain.sch.id' value='" DEFAULT_API_URL "' required></div>"
            "<div class='hint'>Tanpa trailing slash. Contoh: https://presensi.sekolah.sch.id</div>"
            "<div class='field' style='margin-top:10px'><label>API Key</label>"
            "<input name='apikey' placeholder='Masukkan API Key' required></div>"
            "<div class='field'><label>Nama Perangkat</label>"
            "<input name='devname' placeholder='Contoh: GERBANG UTAMA' maxlength='19' "
            "pattern='[A-Za-z0-9 ._-]*' title='Huruf, angka, spasi, - _ .'></div>"
            "</div>"
            "<div class='section'><div class='section-label'>Jadwal Sleep Mode</div>"
            "<div class='row'>"
            "<div class='field'><label>Mulai Sleep (jam)</label>"
            "<select name='slp_s'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == SLEEP_START_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "<div class='field'><label>Selesai Sleep (jam)</label>"
            "<select name='slp_e'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == SLEEP_END_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "</div>"
            "<p class='hint'>Perangkat akan deep sleep dari jam Mulai hingga jam Selesai. "
            "Jika Mulai sama dengan Selesai, sleep dinonaktifkan.</p>"
            "</div>"
            "<div class='section'><div class='section-label'>Jadwal Dim OLED</div>"
            "<div class='row'>"
            "<div class='field'><label>Mulai Dim (jam)</label>"
            "<select name='dim_s'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == OLED_DIM_START_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "<div class='field'><label>Selesai Dim (jam)</label>"
            "<select name='dim_e'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == OLED_DIM_END_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "</div>"
            "<p class='hint'>OLED akan dimatikan sementara pada rentang jam tersebut. "
            "Jika Mulai sama dengan Selesai, dim dinonaktifkan.</p>"
            "</div>"
            "<button type='submit' class='btn'>Simpan &amp; Restart</button>"
            "</form></div>"
            "<div class='footer'>"
            "<p><strong>&copy; 2022 - <script>document.write(new Date().getFullYear())</script>"
            " ZEDLABS TEKNOLOGI INDONESIA</strong></p>"
            "<p>Attendance Machine <strong>v" FIRMWARE_VERSION "</strong></p>"
            "</div></div></body></html>");
  return html;
}

static void deriveProvisioningPassword(char *out, size_t outSz)
{
  static const char cs[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const size_t n = 10;
  if (outSz < n + 1)
  {
    if (outSz > 0)
      out[0] = '\0';
    return;
  }
  for (size_t i = 0; i < n; i++)
    out[i] = cs[esp_random() % 32];
  out[n] = '\0';
}

void startProvisioningMode()
{
  deriveProvisioningPassword(provApPassword, sizeof(provApPassword));
  showOLED(F("PROVISIONING"), PROV_AP_SSID);
  delay(1500);
  showOLED(F("AP PASSWORD"), provApPassword);
  delay(2500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(PROV_AP_SSID, provApPassword);
  dnsServer.start(PROV_DNS_PORT, "*", WiFi.softAPIP());
  provServer.on("/", HTTP_GET, []()
                { provServer.send(200, "text/html", provHtmlPage()); });
  provServer.on("/save", HTTP_POST, []()
                {
    String s1 = provServer.arg("ssid1"), p1 = provServer.arg("pass1");
    String s2 = provServer.arg("ssid2"), p2 = provServer.arg("pass2");
    String s3 = provServer.arg("ssid3"), p3 = provServer.arg("pass3");
    String ak = provServer.arg("apikey");
    String dn = provServer.arg("devname");
    String aurl = provServer.arg("apiurl");
    String slpS = provServer.arg("slp_s");
    String slpE = provServer.arg("slp_e");
    String dimS = provServer.arg("dim_s");
    String dimE = provServer.arg("dim_e");
    if (s1.length() == 0 || ak.length() == 0 || aurl.length() == 0) {
      provServer.send(400, "text/plain", "SSID 1, API Key, dan API URL wajib diisi.");
      return;
    }
    if (!aurl.startsWith("https://"))
    {
      provServer.send(400, "text/plain", "API URL harus diawali https://");
      return;
    }
    while (aurl.endsWith("/")) aurl.remove(aurl.length() - 1);
    if (aurl.length() > 79) {
      provServer.send(400, "text/plain", "API URL terlalu panjang (max 79 karakter).");
      return;
    }
    if (s1.length() > 32 || s2.length() > 32 || s3.length() > 32)
    {
      provServer.send(400, "text/plain", "SSID maksimal 32 karakter.");
      return;
    }
    if (p1.length() > 63 || p2.length() > 63 || p3.length() > 63)
    {
      provServer.send(400, "text/plain", "Password WiFi maksimal 63 karakter.");
      return;
    }
    if (ak.length() > CRED_PLAIN_MAX)
    {
      provServer.send(400, "text/plain", "API Key terlalu panjang.");
      return;
    }
    if (dn.length() > DEVICE_NAME_MAX_LEN)
    {
      provServer.send(400, "text/plain", "Nama perangkat maksimal 19 karakter.");
      return;
    }
    for (size_t i = 0; i < dn.length(); i++)
    {
      if (!isSafeDeviceNameChar(dn[i]))
      {
        provServer.send(400, "text/plain", "Nama perangkat hanya boleh huruf, angka, spasi, '-', '_' dan '.'.");
        return;
      }
    }

    bool hourErr = false;
    auto parseHour = [&hourErr](const String &s, int def) -> int
    {
      if (s.length() == 0)
        return def;
      for (size_t i = 0; i < s.length(); i++)
      {
        if (!isdigit((unsigned char)s[i]))
        {
          hourErr = true;
          return def;
        }
      }
      int v = s.toInt();
      if (v < 0 || v > 23)
      {
        hourErr = true;
        return def;
      }
      return v;
    };
    int iSlpS = parseHour(slpS, SLEEP_START_HOUR_DEFAULT);
    int iSlpE = parseHour(slpE, SLEEP_END_HOUR_DEFAULT);
    int iDimS = parseHour(dimS, OLED_DIM_START_HOUR_DEFAULT);
    int iDimE = parseHour(dimE, OLED_DIM_END_HOUR_DEFAULT);
    if (hourErr)
    {
      provServer.send(400, "text/plain", "Jam sleep/dim harus angka 0-23.");
      return;
    }
    bool saved = saveCredential(NVS_KEY_SSID1, s1.c_str());
    saved = saveCredential(NVS_KEY_PASS1, p1.c_str()) && saved;
    saved = saveCredential(NVS_KEY_SSID2, s2.c_str()) && saved;
    saved = saveCredential(NVS_KEY_PASS2, p2.c_str()) && saved;
    saved = saveCredential(NVS_KEY_SSID3, s3.c_str()) && saved;
    saved = saveCredential(NVS_KEY_PASS3, p3.c_str()) && saved;
    saved = saveCredential(NVS_KEY_APIKEY, ak.c_str()) && saved;
    saved = saveCredential(NVS_KEY_DEVNAME, dn.c_str()) && saved;
    saved = saveCredential(NVS_KEY_APIURL, aurl.c_str()) && saved;
    if (!saved)
    {
      provServer.send(500, "text/plain", "Gagal menyimpan konfigurasi. Periksa panjang input lalu coba lagi.");
      return;
    }
    prefs.begin(NVS_NS_CONFIG, false);
    prefs.putInt(NVS_KEY_CFG_SLP_S, iSlpS);
    prefs.putInt(NVS_KEY_CFG_SLP_E, iSlpE);
    prefs.putInt(NVS_KEY_CFG_DIM_S, iDimS);
    prefs.putInt(NVS_KEY_CFG_DIM_E, iDimE);
    prefs.end();
    markProvisioned();
    provServer.send(200, "text/html",
                    "<html><body style='font-family:sans-serif;text-align:center;padding:40px'>"
                    "<h2>&#10003; Tersimpan!</h2><p>Device akan restart dalam 2 detik...</p>"
                    "</body></html>");
    delay(2000);
    ESP.restart(); });
  provServer.onNotFound([]()
                        { provServer.send(200, "text/html", provHtmlPage()); });
  provServer.begin();
  unsigned long t0 = millis();
  while (millis() - t0 < PROVISIONING_TIMEOUT_MS)
  {
    dnsServer.processNextRequest();
    provServer.handleClient();
    esp_task_wdt_reset();
    delay(10);
  }
  showOLED(F("TIMEOUT"), "RESTART...");
  delay(2000);
  ESP.restart();
}
