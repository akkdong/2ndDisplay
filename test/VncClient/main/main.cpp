// main.cpp
//

#include <Arduino.h>
#include <WiFi.h>
#include "board_init.h"
#include "lv_demos.h"

//
//
//

void setup()
{
    delay(500);
    Serial.begin(115200);
    //while(!Serial);
    delay(500);

    /*
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Wi-Fi 연결 중: ");
    Serial.println(ssid);

     while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi 연결 성공!");
    Serial.print("할당받은 IP 주소: ");
    Serial.println(WiFi.localIP());
    */

    bsp_init_gpio();
    bsp_init_lcd();
}

void loop()
{
    while(1)
    {
        Serial.println("Hello World!");
        delay(1000);
    }
}
