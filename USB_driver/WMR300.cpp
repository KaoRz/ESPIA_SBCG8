#include <iostream>
#include "WMR300.h"

WMR300::WMR300(USB *p) : HIDUniversal(p) {
    this->firstResponse = false;
    this->secondResponse = false;

    this->thd_in_parsed = false;
    this->thd_out_parsed = false;
    this->w_parsed = false;
    this->r_parsed = false;
    this->p_parsed = false;

    memset(&this->thd_in_packet, '\x00', sizeof(struct temphumdew_packet));
    memset(&this->thd_out_packet, '\x00', sizeof(struct temphumdew_packet));
    memset(&this->w_packet, '\x00', sizeof(struct wind_packet));
    memset(&this->r_packet, '\x00', sizeof(struct rain_packet));
    memset(&this->p_packet, '\x00', sizeof(struct pressure_packet));   
};

bool WMR300::getFirstResponse(void) {
    return this->firstResponse;
};

bool WMR300::getSecondResponse(void) {
    return this->secondResponse;
};

void WMR300::setFirstResponse(bool state) {
    this->firstResponse = state;
};

void WMR300::setSecondResponse(bool state) {
    this->secondResponse = state;
};

struct temphumdew_packet WMR300::getTHDindoor(void) {
    return this->thd_in_packet;
};

struct temphumdew_packet WMR300::getTHDoutdoor(void){
    return this->thd_out_packet;
};

struct wind_packet WMR300::getWind(void) {
    return this->w_packet;
};

struct rain_packet WMR300::getRain(void) {
    return this->r_packet;
};

struct pressure_packet WMR300::getPressure(void){
    return this->p_packet;
};

bool WMR300::isConnected(void) {
    return HIDUniversal::isReady() && HIDUniversal::VID == WMR300_VID && HIDUniversal::PID == WMR300_PID;
};

bool WMR300::isAllParsed(void) {
    return this->thd_in_parsed && this->thd_out_parsed && this->w_parsed && this->r_parsed && this->p_parsed;
};

uint8_t WMR300::OnInitSuccessful(void) {                                        // Called by the HIDUniversal library on success
    if (HIDUniversal::VID != WMR300_VID || HIDUniversal::PID != WMR300_PID) {   // Make sure the right device is actually connected
            return 1;
    }

    return 0;
};

void WMR300::ParseHIDData(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {

    uint16_t tmp_value;
    float tmp_convert;
  
    if(HIDUniversal::VID != WMR300_VID || HIDUniversal::PID != WMR300_PID)      // Make sure the right device is actually connected
        return;

#ifdef DEBUG
    char hex_str[3] = { 0 };
    if(len && buf)  {
        E_Notify(PSTR("[+] Packet received:\r\n"), 0);
        for (uint8_t i = 0; i < len; i++) {
            sprintf(hex_str, "%x", buf[i]);
            E_Notify(PSTR(hex_str), 0);
            E_Notify(PSTR(" "), 0);
            memset(hex_str, '\x00', sizeof(hex_str));
        }
        E_Notify(PSTR("\r\n"), 0);
    }
#endif

    if(len && buf) {
        if(buf[0] == STATIONINFO && !this->firstResponse) {    // Heartbeat response
            this->magic0 = buf[12];
            this->magic1 = buf[13];

            this->firstResponse = true;
        }

        else if(buf[0] == ACK && !this->secondResponse) {
            this->secondResponse = true;
        }

        else {
            switch(buf[0]) {
                case TEMPHUMDEW:
                    if(buf[7] == 0) {
                        tmp_value  = buf[8] << 8;
                        tmp_value += buf[9];
                        this->thd_in_packet.temperature = (float)tmp_value / 10;
                                        
                        this->thd_in_packet.humidity = buf[10];

                        tmp_value  = buf[11] << 8;
                        tmp_value += buf[12];
                        this->thd_in_packet.dewpoint = (float)tmp_value / 10;

                        this->thd_in_parsed = true;
                    }

                    else if(buf[7] == 1) {
                        tmp_value  = buf[8] << 8;
                        tmp_value += buf[9];
                        this->thd_out_packet.temperature = (float)tmp_value / 10;
                                        
                        this->thd_out_packet.humidity = buf[10];

                        tmp_value  = buf[11] << 8;
                        tmp_value += buf[12];
                        this->thd_out_packet.dewpoint = (float)tmp_value / 10;

                        this->thd_out_parsed = true;
                    }

                    break;

                case WIND:
                    tmp_value  = buf[8] << 8;
                    tmp_value += buf[9];
                    this->w_packet.gust_speed = (float)tmp_value / 10;

                    tmp_value  = buf[10] << 8;
                    tmp_value += buf[11];
                    this->w_packet.gust_direction = tmp_value;

                    tmp_value  = buf[12] << 8;
                    tmp_value += buf[13];
                    this->w_packet.speed_ave = (float)tmp_value / 10;

                    tmp_value  = buf[14] << 8;
                    tmp_value += buf[15];
                    this->w_packet.direction_ave = tmp_value;

                    tmp_value  = buf[16] << 8;
                    tmp_value += buf[17];
                    this->w_packet.compass_dir = tmp_value;

                    tmp_value  = buf[18] << 8;
                    tmp_value += buf[19];
                    this->w_packet.windchill = tmp_value;

                    this->w_parsed = true;

                    break;

                case RAIN:
                    tmp_value  = buf[8] << 8;
                    tmp_value += buf[9];
                    this->r_packet.rain_hour = (float)tmp_value / 100;

                    tmp_value  = buf[15] << 8;
                    tmp_value += buf[16];
                    this->r_packet.rain_accum = (float)tmp_value / 100;

                    tmp_value  = buf[17] << 8;
                    tmp_value += buf[18];
                    this->r_packet.rain_rate = (float)tmp_value / 100;

                    this->r_parsed = true;

                    break;

                case PRESSURE:
                    tmp_value  = buf[8] << 8;
                    tmp_value += buf[9];
                    this->p_packet.station_press = (float)tmp_value / 10;

                    tmp_value  = buf[10] << 8;
                    tmp_value += buf[11];
                    this->p_packet.sea_press = (float)tmp_value / 10;

                    tmp_value  = buf[12] << 8;
                    tmp_value += buf[13];
                    this->p_packet.altitude = tmp_value;

                    this->p_parsed = true;

                    break;

                default:
                    break;

            }

        }
                
	}

};

void WMR300::sendHeartbeat(void) {
    
    uint8_t buf[6] = { 0 };
        
    buf[0] = 0xA6;
    buf[1] = 0x91;
    buf[2] = 0xCA;
    buf[3] = 0x45;
    buf[4] = 0x52;
    buf[5] = 0x00;
        
    pUsb->outTransfer(bAddress, epInfo[epInterruptOutIndex].epAddr, sizeof(buf), buf);

};

void WMR300::sendEstablishComm(void) {

    uint8_t buf[7] = { 0 };

    buf[0] = 0x73;
    buf[1] = 0xe5;
    buf[2] = 0x0a;
    buf[3] = 0x26;
    buf[4] = this->magic0;
    buf[5] = this->magic1;
    buf[6] = 0x00;

    pUsb->outTransfer(bAddress, epInfo[epInterruptOutIndex].epAddr, sizeof(buf), buf);

};
