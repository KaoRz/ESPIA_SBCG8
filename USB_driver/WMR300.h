#ifndef __WMR300_h__
#define __WMR300_h__

#include <hiduniversal.h>

#define WMR300_VID 0x0fde
#define WMR300_PID 0xca08

#define DEBUG 1

enum RESPEnum {
    ACK = 0x41,
    STATIONINFO = 0x57,
    TEMPHUMDEW = 0xd3,
    WIND = 0xd4,
    RAIN = 0xd5,
    PRESSURE = 0xd6,
};

struct temphumdew_packet {
    float temperature;
    uint8_t humidity;
    float dewpoint;
};

struct wind_packet {
    float gust_speed;
    uint16_t gust_direction;
    float speed_ave;
    uint16_t direction_ave;
    uint16_t compass_dir;
    uint16_t windchill;
};

struct rain_packet {
    float rain_hour;
    float rain_accum;
    float rain_rate;
};

struct pressure_packet {
    float station_press;
    float sea_press;
    uint16_t altitude;
};

class WMR300 : public HIDUniversal {

    uint8_t magic0;
    uint8_t magic1;

    bool firstResponse;
    bool secondResponse;

    bool thd_in_parsed;
    bool thd_out_parsed;
    bool w_parsed;
    bool r_parsed;
    bool p_parsed;

    struct temphumdew_packet thd_in_packet;
    struct temphumdew_packet thd_out_packet;
    struct wind_packet w_packet;
    struct rain_packet r_packet;
    struct pressure_packet p_packet;
        
    public:
        WMR300(USB *p);

        bool getFirstResponse(void);
        bool getSecondResponse(void);
        void setFirstResponse(bool);
        void setSecondResponse(bool);

        struct temphumdew_packet getTHDindoor(void);
        struct temphumdew_packet getTHDoutdoor(void);
        struct wind_packet getWind(void);
        struct rain_packet getRain(void);
        struct pressure_packet getPressure(void);

        bool isAllParsed(void);

        bool isConnected(void);
        void sendPacket(void);
        void sendHeartbeat(void);
        void sendEstablishComm(void);

    protected:
        void ParseHIDData(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf); // Called by the HIDUniversal library
        uint8_t OnInitSuccessful();

};

#endif
