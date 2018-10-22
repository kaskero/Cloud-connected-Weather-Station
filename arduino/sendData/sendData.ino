#include "DHT.h"
#include <SFE_BMP180.h>
#include "SoftwareSerial.h"
#include <TinyGPS++.h>  
#include <avr/sleep.h>
#include <avr/power.h>
#include <DS2745_lib.h>

SoftwareSerial ss(5, 6); // La conexión serie del dispositivo GPS
TinyGPSPlus gps; // Se declara una instancia de la librería

#define DHTPIN 3 // El pin 3 sera en punto de comunicación entre el sensor y el micro
#define DHTTYPE DHT22  // Definimos el tipo del sensor 
DHT dht(DHTPIN, DHTTYPE); // Declaramos una instancia de la librería del sensor defininiendo el pin y el tipo
SFE_BMP180 bmp180; // Se declara una instancia de la librería del sensor de presión
DS2745_lib batt; // Se declara una instancia de la librería del monitor de batería

SoftwareSerial xbee(8, 9); // Declaramos la variable xbee donde 8 es RX y 9 es TX y así emular un puerto serie mediante la librería SoftwareSerial

volatile int wdt_count = 1;
volatile int f_wakeup = 0;

// Declaramos el tipo de una estructura 
struct XBeeMessage {
  float data[7];
  uint8_t crc;
};
struct XBeeMessage message;

const int sample_num = 6;
float array_latitud[sample_num]; // circular buffer
float array_longitud[sample_num]; // circular buffer
float sum_array_latitud = 0;
float sum_array_longitud = 0;

void setup() {
  MCUSR &= ~(_BV(WDRF)); // Borrar el indicador WDT en el registro de estado de la MCU

  Serial.begin(115200); // Inicializamos el puerto serie HW
  
  Serial.println(F("Inicializando sensores DHT22, BMP180 y DS2745..."));
  dht.begin(); // Inicializamos el sensor
  bmp180.begin(); // Inicializamos el sensor
  batt.abiarazi(); // Inicializamos el sensor

  Serial.println(F("Despertando GPS..."));
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH); // GPS despierto
  
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW); // xbee despierto
  delay(50); // xbee wake-up time: 13.2ms
  xbee.begin(9600); // inicializamos el puerto serie del XBee
  xbee_conf();
  digitalWrite(7, HIGH); // xbee dormido

  gps_conf();
  gps_init();
 
  Serial.println(F("Configurando modo sleep del Arduino..."));
  setupWDT();

  Serial.println(F("exiting setup()"));
}

void xbee_conf() {
  Serial.println(F("Configurando el modo sleep del XBee..."));
  Serial.println(F("Entrando en modo comando"));
  xbee.print(F("+++")); // modo comando para el XBee
  delay(3000);
  Serial.println(F("Set SM1 sleep mode"));
  xbee.print(F("ATSM1")); xbee.write(0x0D); // elegimos el modo sleep SM1
  delay(3000);
  Serial.println(F("Saliendo del modo comando"));
  xbee.print(F("ATCN")); xbee.write(0x0D); // retorno de carro y salimos
  delay(3000);
}

void gps_conf() {
  Serial.println(F("Configurando GPS..."));
  ss.begin(9600);
  delay(3000);
  Serial.println(F("Set NMEA sentence output frequencies: GGA output once every one position fix"));
  ss.println(F("$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29")); // Set NMEA sentence output frequencies
  delay(3000);
  Serial.println(F("Set NMEA port update rate: 100ms = 0.1s --> 10Hz"));
  ss.println(F("$PMTK220,100*2F")); // Set NMEA port update rate: 100ms = 0.1s --> 10Hz
  delay(3000);
  Serial.println(F("Set search GPS, GLONASS and Galileo satellites"));
  ss.println(F("$PMTK353,1,1,1,0,0*2A")); // Set search GPS, GLONASS and Galileo satellites
                                          // $PMTK353,1,1,0,0,0*2B : Search GPS and GLONASS satellites
  delay(3000);
  /*
  Serial.println(F("Enable to search a SBAS satellite"));
  ss.println(F("$PMTK313,1*2E")); // Enable to search a SBAS satellite
  delay(3000);
  Serial.println(F("DGPS correction data source mode: 2 = SBAS (Include WAAS/EGNOS/GAGAN/MSAS)"));
  ss.println(F("$PMTK301,2*2E")); // DGPS correction data source mode: 2 = SBAS (Include WAAS/EGNOS/GAGAN/MSAS)
  delay(3000);
  */
  //Serial.println(F("Set Hot Restart: Use all available data in the NV Store"));
  //ss.println(F("$PMTK101*32")); // Set Hot Restart: Use all available data in the NV Store
  Serial.println(F("Set Warm Restart: Don't use Ephemeris at re-start"));
  ss.println(F("$PMTK102*31")); // Set Warm Restart: Don't use Ephemeris at re-start
  delay(3000);
}

void gps_init() {
  Serial.println(F("Trying to get a GPS location... Cold start..."));
  smartDelay(120000);
  for(int i=0; i< sample_num; i++) {
    int j=0;
    do {
      smartDelay(1000);
      j++;
    }
    while(!(gps.hdop.hdop()<0.7) && j<60);
    float latitud = gps.location.lat();
    float longitud = gps.location.lng();    
    Serial.print(latitud, 7); Serial.print(F(" ")); Serial.println(longitud, 7);
        
    array_latitud[i] = latitud;
    array_longitud[i] = longitud;
    sum_array_latitud += array_latitud[i];
    sum_array_longitud += array_longitud[i];
  }
}
  
void setupWDT() {
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = 1<<WDP2 | 1<<WDP1; 
  WDTCSR |= _BV(WDIE); 
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // máximo ahorro de energía
}

// This custom version of delay() ensures that the gps object is being "fed".
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do if(ss.available()) gps.encode(ss.read());
  while(millis() - start < ms);
}

void loop() {
  bool flag_enviar_gps = 1;
  digitalWrite(4, HIGH); // despertamos el GPS
  Serial.println(F("Trying to get an updated GPS location... Warm start"));
  smartDelay(120000); // 9600bps/8=1200Bps 
                      // NMEA messages have a maximum length of 82 characters
                      // 1200Bps / 82Bpm ~ 14.6 messages per second
  int j=0;
  do {
    smartDelay(1000);
    j++;
  }
  while(!(gps.hdop.hdop()<0.7) && j<60);
  float latitud = gps.location.lat();
  float longitud = gps.location.lng();    
  Serial.print(F("GPS: ")); Serial.print(latitud,7); Serial.print(F(", ")); Serial.println(longitud,7);
  digitalWrite(4, LOW); // dormimos el GPS

  sum_array_latitud -= array_latitud[0];
  sum_array_longitud -= array_longitud[0];
  // shift circular buffer as FIFO
  for(int i=1; i<sample_num; i++) {
    array_latitud[i-1] = array_latitud[i];
    array_longitud[i-1] = array_longitud[i];
  }
  array_latitud[sample_num-1] = latitud;
  array_longitud[sample_num-1] = longitud;
  sum_array_latitud += array_latitud[sample_num-1];
  sum_array_longitud += array_longitud[sample_num-1];
  Serial.print(F("GPS avg.: ")); Serial.print(latitud,7); Serial.print(F(", ")); Serial.println(longitud,7);

  bool flag_enviar_dht22 = 1;
  float temperature1 = dht.readTemperature(); // Obtenemos la temperatura
  float humidity = dht.readHumidity(); // Obtenemos la humedad
  if(isnan(temperature1) || isnan(humidity)) { // Si se obtiene ninguno de esos valores significará que algo está mal
    Serial.println(F("Failed to read from DHT sensor!"));
    flag_enviar_dht22 = 0;
  } else {
    Serial.print(F("Temperatura1: ")); Serial.print(temperature1); Serial.print(F("ºC")); 
    Serial.print(F("\tHumedad: ")); Serial.print(humidity); Serial.println(F("%"));
  }

  bool flag_enviar_bmp180 = 1;
  char estatus;
  double temperature2, pressure;
  estatus = bmp180.startTemperature(); // Inicio de lectura de temperatura
  if(estatus != 0) {   
    delay(estatus); // Pausa para que finalice la lectura
    estatus = bmp180.getTemperature(temperature2); // Obtener la temperatura
    Serial.print(F("Temperatura2: ")); Serial.print(temperature2); Serial.print(F("ºC"));
    if(estatus != 0) {
      estatus = bmp180.startPressure(3); // Inicio lectura de presión
      if(estatus != 0) {        
        delay(estatus); // Pausa para que finalice la lectura        
        estatus = bmp180.getPressure(pressure, temperature2); // Obtenemos la presión
        if(estatus != 0) {                  
          Serial.print(F("\tPresion: ")); Serial.print(pressure); Serial.println(F("mb")); // Obtenemos la presión       
        }      
      }      
    }   
  }
  if(estatus == 0) { 
    Serial.println(F("Failed to read from BMP180 sensor!"));
    flag_enviar_bmp180 = 0;
  }
  
  if(flag_enviar_dht22) {  
    message.data[0] = humidity;
    message.data[1] = temperature1;
  } else if(flag_enviar_bmp180) {
    message.data[0] = 0.0;
    message.data[1] = temperature2;
    message.data[2] = pressure;
  } else {
    message.data[0] = 0.0;
    message.data[1] = 0.0;
    message.data[2] = 0.0;
  }

  if(flag_enviar_bmp180) {
    message.data[2] = pressure;
  } else {
    message.data[2] = 0.0;
  }

  if(flag_enviar_gps) {
    message.data[3] = latitud;
    message.data[4] = longitud;
  } else {
    message.data[3] = 0.0;
    message.data[4] = 0.0;
  }

  //delay(10000);
  float tension = batt.tentsioaIrakurri();
  float corriente = batt.kontsumoaIrakurri();
  message.data[5] = tension;
  message.data[6] = corriente;

  Serial.println(F("Datos de la estructura: ")); 
  Serial.print(F("Humedad: ")); Serial.println(message.data[0], 2);
  Serial.print(F("Temperatura: ")); Serial.println(message.data[1], 2);
  Serial.print(F("Presion: ")); Serial.println(message.data[2], 2);
  Serial.print(F("Latitud: ")); Serial.println(message.data[3], 7);
  Serial.print(F("Longitud: ")); Serial.println(message.data[4], 7);
  Serial.print(F("Tension: ")); Serial.println(message.data[5], 2);
  Serial.print(F("Corriente: ")); Serial.println(message.data[6], 2);
  message.crc = XORchecksum8((byte*)&message.data, sizeof(message.data)); // XOR de los datos para cuando los reciba poder comprobar que todos los datos son correctos
  Serial.print(F("CRC: ")); Serial.println(message.crc);

  digitalWrite(7, LOW); // despertar xbee
  delay(50); // xbee wake-up time: 13.2ms
  xbee.write((byte*)&message, sizeof(message)); // envio de estructura al xbee
  xbee.flush();
  Serial.println(F("Estructura enviada"));
  digitalWrite(13, HIGH);
  delay(100); // le damos un margen de tiempo para que de tiempo a enviar los datos
  digitalWrite(13, LOW);
  digitalWrite(7, HIGH); // dormir xbee

  Serial.print(F("Arduino sleeps..."));
  delay(100);
  sleepArduino(); // dormir Arduino
  delay(1000);
  Serial.println(F("and wakes up"));
}

// Rutina de atención a la interrupción del Watchdog.
ISR(WDT_vect) {
  if(wdt_count < 600) // despertar cada 600s
    wdt_count++;
  else {
    f_wakeup = 1;
    wdt_count = 1;
  }
}

void sleepArduino(void) {
  power_all_disable(); // Deshabilita los periféricos. 

  // Arduino entra en modo suspensión
  while(f_wakeup == 0) // Arduino permanecerá dormido en este bucle () hasta que se establezca el indicador de activación
    sleep_mode(); // sleep_enable() + sleep_cpu() + sleep_disable()

  power_all_enable(); // el programa continuará desde aquí después del tiempo de espera de WDT
  
  f_wakeup = 0;
}

uint8_t XORchecksum8(const byte *data, size_t dataLength) {
  uint8_t value = 0;
  
  for(size_t i = 0; i < dataLength; i++)
    value ^= (uint8_t)data[i];
  
  return ~value;
}
