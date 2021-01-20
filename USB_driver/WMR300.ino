#include <SPI.h>
#include <WiFi.h>
#include <ThingsBoard.h>

#include "WMR300.h"

#define uS_TO_S_FACTOR 1000000		// Conversion factor for micro seconds to seconds
#define SLEEP_TIME 1 * 60			// Time to sleep in seconds

#define WIFI_AP_NAME ""             // WiFi SSID
#define WIFI_PASSWORD ""            // WiFi password

#define THINGSBOARD_SERVER  ""      // ThingsBoard server
#define TOKEN ""                    // ThingsBoard token
#define TB_SLEEP 500

WiFiClient espClient;
ThingsBoard tb(espClient);

int status = WL_IDLE_STATUS;        // WIFI status

USB Usb;
WMR300 wmr300(&Usb);

struct temphumdew_packet thd_in_packet_tmp;
struct temphumdew_packet thd_out_packet_tmp;
struct wind_packet w_packet_tmp;
struct rain_packet r_packet_tmp;
struct pressure_packet p_packet_tmp;

void InitWiFi() {
    Serial.print("\n[*] Connecting to AP ...");
    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("."); 
    }
    Serial.println("");
    Serial.println("[+] Connected successfully to AP");
    Serial.print("[+] Ethernet started on IP: ");
    Serial.println(WiFi.localIP());  
}

void reconnect() {
    status = WiFi.status();
    if(status != WL_CONNECTED) {
        Serial.print("[*] Reconnecting to AP ...");
        WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
        while(WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("[+] Reconnected to AP");
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_AP_NAME, WIFI_PASSWORD);
    InitWiFi();

    if(Usb.Init() == -1) {
        Serial.println("[!] Cannot initialize USB device");
        while(1);
    }

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);     // Disable fast memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);     // Disable slow memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);		// Disable RTC peripherals
    esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR * SLEEP_TIME);			    // Just the internal Real Time Clock must be enabled

    Serial.println("[+] WMR300 started successfully");
}

void loop() {

    Usb.Task();

    if(wmr300.isConnected()) {
        Serial.println(F("\r\n[+] WMR300 connected!"));

        while(!wmr300.getFirstResponse()) {
            wmr300.sendHeartbeat();
            Usb.Task();
        }

        while(!wmr300.getSecondResponse()) {
            wmr300.sendEstablishComm();
            Usb.Task();
        }

        while(!wmr300.isAllParsed())
            Usb.Task();

        if (WiFi.status() != WL_CONNECTED)
            reconnect();

        if (!tb.connected()) {      // Connect to ThingsBoard
            Serial.print("[*] Connecting to: ");
            Serial.print(THINGSBOARD_SERVER);
            Serial.print(" with token ");
            Serial.println(TOKEN);
            while(!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
                Serial.println("[!] Failed to connect. Retrying...");
                delay(5000);
            }
            Serial.println("[+] Successfully connected to Thingsboard");
        }

        thd_in_packet_tmp = wmr300.getTHDindoor();
        tb.sendTelemetryFloat("temp_in", thd_in_packet_tmp.temperature);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("humi_in", thd_in_packet_tmp.humidity);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("dewp_in", thd_in_packet_tmp.dewpoint);
        delay(TB_SLEEP);

        thd_out_packet_tmp = wmr300.getTHDoutdoor();
        tb.sendTelemetryFloat("temp_out", thd_out_packet_tmp.temperature);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("humi_out", thd_out_packet_tmp.humidity);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("dewp_out", thd_out_packet_tmp.dewpoint);
        delay(TB_SLEEP);

        w_packet_tmp = wmr300.getWind();
        tb.sendTelemetryFloat("wgsp", w_packet_tmp.gust_speed);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("wgdi", w_packet_tmp.gust_direction);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("wasp", w_packet_tmp.speed_ave);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("wadi", w_packet_tmp.direction_ave);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("cmps", w_packet_tmp.compass_dir);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("wdch", w_packet_tmp.windchill);
        delay(TB_SLEEP);

        r_packet_tmp = wmr300.getRain();
        tb.sendTelemetryFloat("rain", r_packet_tmp.rain_hour);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("rfac", r_packet_tmp.rain_accum);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("rfrt", r_packet_tmp.rain_rate);
        delay(TB_SLEEP);

        p_packet_tmp = wmr300.getPressure();
        tb.sendTelemetryFloat("press", p_packet_tmp.station_press);
        delay(TB_SLEEP);
        tb.sendTelemetryFloat("pslv", p_packet_tmp.sea_press);
        delay(TB_SLEEP);
        tb.sendTelemetryInt("alti", p_packet_tmp.altitude);
        delay(TB_SLEEP);

        tb.loop();
        delay(TB_SLEEP);

        tb.disconnect();

        Serial.println("[+] Values have been sent. Sleeping now...");

        esp_deep_sleep_start();		// Hibernate

    }

}
