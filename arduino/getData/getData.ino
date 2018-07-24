#include <Process.h>
#include "SoftwareSerial.h"
#include "U8glib.h"
#include <avr/wdt.h>

SoftwareSerial xbee(8, 9); // Declaramos la variable xbee donde 8 es RX y 9 es TX 
                           // y así simulamos un puerto serie mediante la librería SoftwareSerial

struct XBeeMessage {
  float data[7];
  uint8_t crc;
};
struct XBeeMessage message;

U8GLIB_DOGM128 u8g(5, 4); // CS = 5, A0 = 4

const uint8_t logo_bitmap[] PROGMEM = {
  0xFF, 0xE0, 0x00, 0x03, 0xFF, 0x80,                 
  0xFF, 0xE0, 0x00, 0x03, 0xFF, 0xC0,                 
  0xFF, 0xE0, 0x00, 0x03, 0xFF, 0xFC,                 
  0xFF, 0xFE, 0x00, 0x00, 0x7F, 0xFE,                 
  0xFF, 0xFF, 0xE0, 0x00, 0x07, 0xFE,                 
  0xFF, 0xFF, 0xF0, 0x00, 0x07, 0xFE,                 
  0xFF, 0xFF, 0xF0, 0x00, 0x07, 0xFE,                 
  0xFF, 0xE7, 0xE0, 0x07, 0x83, 0xFE,                 
  0xFF, 0xE1, 0xE0, 0x07, 0xC3, 0xFE,                 
  0xFF, 0xC0, 0xC0, 0x07, 0xC3, 0xFE,                 
  0xFF, 0xC0, 0x00, 0x07, 0xC3, 0xFE,                 
  0xFF, 0x80, 0x00, 0x07, 0xFF, 0xFE,                 
  0xFF, 0x8E, 0x00, 0x03, 0xFF, 0xFE,                 
  0xFF, 0xFE, 0x1F, 0x00, 0xFF, 0xFE,                 
  0xFF, 0xFE, 0x1F, 0xE0, 0xFF, 0xFC,                 
  0x7F, 0xFE, 0x1F, 0xE3, 0xFE, 0x30,                 
  0x3F, 0xFC, 0x07, 0xFF, 0xFE, 0x00,                 
  0x03, 0xF0, 0x07, 0xFF, 0xFC, 0x00,                 
  0x03, 0xF0, 0x07, 0xFF, 0xF8, 0x00,                 
  0x03, 0xFF, 0x87, 0xFF, 0x00, 0x00,                 
  0x03, 0xFF, 0x87, 0xE0, 0x00, 0x00,                 
  0x01, 0xFF, 0x87, 0xC0, 0x00, 0x00,                 
  0x00, 0xFF, 0xE7, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0xC0, 0x00, 0x00,                 
  0x00, 0x0F, 0xFF, 0x80, 0x00, 0x00,                 
};

float hume;
float temp;
float pressure;
float tension;
float corriente;

Process p1;   

void setup() { 
  wdt_disable();
   
  Bridge.begin(); // Initialize Bridge
  Serial.begin(115200); // Inicializamos el puerto serie
  Serial.println(F("setup() begin"));
  xbee.begin(9600); // Inicializamos el puerto serie del XBee
  
  do {} while( u8g.nextPage() );
  pantalla_inicio();
  do {} while( u8g.nextPage() );

  String p_output = "255.255.255.255";
  exec_cmd(F("ifconfig wlan0 | grep 'inet addr:' | cut -d: -f2 | awk '{ print $1}'"), &p_output);
  draw(&p_output);

  wdt_enable(WDTO_8S);
  
  Serial.println(F("setup() exit"));
}

void pantalla_inicio() {
  // picture loop
  u8g.firstPage();
  do {
    u8g.drawBitmapP(41, 10, 6, 29, logo_bitmap);
    u8g.setFont(u8g_font_04b_03);
    u8g.drawStr(14, 49, F("Universidad"));
    u8g.drawStr(1, 59, F("del Pais Vasco"));
    u8g.drawStr(67, 49, F("Euskal Herriko"));
    u8g.drawStr(67, 59, F("Unibertsitatea"));
  } while( u8g.nextPage() );
  delay(3000);
}

void exec_cmd(String cmd, String* output) {
  Process p;        
  p.runShellCommand(cmd);
  *output = p.readString();
  Serial.println(*output);
  p.close();
}

void loop() {
  if(xbee.available() < sizeof(message)) {
    if(!p1.running()) {
      p1.close();
      while(p1.available() > 0) {
        char c = p1.read();
        Serial.print(c);
      }
    }
    wdt_reset();
  } else { 
    if(p1.running()) p1.close();
    
    xbee.readBytes((byte*)&message, sizeof(message)); // leemos los datos recibidos
    hume = message.data[0];
    temp = message.data[1];
    pressure = message.data[2];
    float latitud = message.data[3];
    float longitud = message.data[4];
    tension = message.data[5];
    corriente = message.data[6];
    
    Serial.println(F("Mensaje recibido. Datos: ")); 
    Serial.print(F("Humedad: ")); Serial.println(message.data[0], 2);
    Serial.print(F("Temperatura: ")); Serial.println(message.data[1], 2);
    Serial.print(F("Presion: ")); Serial.println(message.data[2], 2);
    Serial.print(F("Latitud: ")); Serial.println(message.data[3], 7);
    Serial.print(F("Longitud: ")); Serial.println(message.data[4], 7);
    Serial.print(F("Tension: ")); Serial.println(message.data[5], 2);
    Serial.print(F("Corriente: ")); Serial.println(message.data[6], 2);
    
    uint8_t crc_R = message.crc;
    Serial.print(F("CRC_R: ")); Serial.println(crc_R);
    uint8_t crc_L = XORchecksum8((byte*)message.data, sizeof(message.data)); // XOR con los datos que hemos recibido
    Serial.print(F("CRC_L: ")); Serial.println(crc_L);
  
    if(crc_R != crc_L) return; // si el CRC calculado y el obtenido son diferentes significa que algún dato se ha perdido o es diferente
    else {    
      do {} while( u8g.nextPage() );
      delay(100);
      pantalla_inicio();
      do {} while( u8g.nextPage() );
      delay(100);

      String p_output = "2018-02-13 23:59:59";
      exec_cmd(F("date '+%Y-%m-%d %H:%M:%S'"), &p_output);
      draw(&p_output);
     
      p1.begin(F("python"));  
      p1.addParameter(F("/root/showdataV7.py")); // ejecutamos el script y la pasamos como parametro los datos obtenidos
      p1.addParameter(String(temp,2));
      p1.addParameter(String(hume,2));
      p1.addParameter(String(pressure,2));
      p1.addParameter(String(latitud,7));
      p1.addParameter(String(longitud,7));
      p1.addParameter(String(tension,2));
      p1.addParameter(String(corriente,2));
      p1.runAsynchronously();     
    }
  }
}

uint8_t XORchecksum8(const byte *data, size_t dataLength) {
  uint8_t value = 0;
  for(size_t i = 0; i < dataLength; i++) {
    value ^= (uint8_t)data[i];
  }
  
  return ~value;
}

void draw(String* p_output) {
  // picture loop
  u8g.firstPage();  
  do {
    u8g.setFont(u8g_font_6x13);

    u8g.setPrintPos(1, 9);
    u8g.print(F("Temperature: ")); u8g.print(temp, 1); u8g.print(F("C"));
    
    u8g.setPrintPos(1, 22);
    u8g.print(F("Humidity: ")); u8g.print(hume, 1); u8g.print(F("%"));
    
    u8g.setPrintPos(1, 35);
    u8g.print(F("Pressure: ")); u8g.print(pressure, 1); u8g.print(F("mb"));

    u8g.setPrintPos(1, 48);
    u8g.print(F("Batt: ")); 
    u8g.print(tension, 2); u8g.print(F("V "));
    u8g.print(corriente, 2); u8g.print(F("mA"));
    
    u8g.setPrintPos(1, 62);
    u8g.print(*p_output);
  } while( u8g.nextPage() );
  delay(500);
}
