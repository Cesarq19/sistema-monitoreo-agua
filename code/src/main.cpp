#include <Arduino.h>
// #include <SPI.h>
// #include <Wire.h>

// //For more Projects: www.arduinocircuit.com
// byte statusLed    = 5;
// byte sensorInterrupt = 4;  // 0 = digital pin 2
// byte sensorPin       = 4;
// // The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// // litre/minute of flow.
// float calibrationFactor = 4.5;
// volatile byte pulseCount;
// float flowRate;
// unsigned int flowMilliLitres;
// unsigned long totalMilliLitres;
// unsigned long oldTime;

// /*
// Insterrupt Service Routine
//  */
// void pulseCounter()
// {
//   // Increment the pulse counter
//   Serial.println("Entro a pulsos");
//   pulseCount++;
// }

// void setup()
// {
//   // Initialize a serial connection for reporting values to the host
//   Serial.begin(9600);
//   // Set up the status LED line as an output
//   pinMode(statusLed, OUTPUT);
//   digitalWrite(statusLed, LOW);  // We have an active-low LED attached
//   pinMode(sensorPin, INPUT);
//   //digitalWrite(sensorPin, HIGH);
//   pulseCount        = 0;
//   flowRate          = 0.0;
//   flowMilliLitres   = 0;
//   totalMilliLitres  = 0;
//   oldTime           = 0;
//   // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
//   // Configured to trigger on a FALLING state change (transition from HIGH
//   // state to LOW state)
//   attachInterrupt(sensorInterrupt, pulseCounter, RISING);
// }
// /**
//  * Main program loop
//  */
// void loop()
// {
//    if((millis() - oldTime) > 1000)    // Only process counters once per second
//   {
//     // Disable the interrupt while calculating flow rate and sending the value to
//     // the host
//     detachInterrupt(sensorInterrupt);
//     // Because this loop may not complete in exactly 1 second intervals we calculate
//     // the number of milliseconds that have passed since the last execution and use
//     // that to scale the output. We also apply the calibrationFactor to scale the output
//     // based on the number of pulses per second per units of measure (litres/minute in
//     // this case) coming from the sensor.
//     flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
//     // Note the time this processing pass was executed. Note that because we've
//     // disabled interrupts the millis() function won't actually be incrementing right
//     // at this point, but it will still return the value it was set to just before
//     // interrupts went away.
//     oldTime = millis();
//     // Divide the flow rate in litres/minute by 60 to determine how many litres have
//     // passed through the sensor in this 1 second interval, then multiply by 1000 to
//     // convert to millilitres.
//     flowMilliLitres = (flowRate / 60) * 1000;
//     // Add the millilitres passed in this second to the cumulative total
//     totalMilliLitres += flowMilliLitres;
//     unsigned int frac;
//     // Print the flow rate for this second in litres / minute
//     Serial.print("Flow rate: ");
//     Serial.print(int(flowRate));  // Print the integer part of the variable
//     Serial.print("L/min");
//     Serial.print("\t"); 		  // Print tab space
//     // Print the cumulative total of litres flowed since starting
//     Serial.print("Output Liquid Quantity: ");
//     Serial.print(totalMilliLitres);
//     Serial.println("mL");
//     Serial.print("\t"); 		  // Print tab space
// 	Serial.print(totalMilliLitres/1000);
// 	Serial.print("L");
//     // Reset the pulse counter so we can start incrementing again
//     pulseCount = 0;
//     // Enable the interrupt again now that we've finished sending output
//     attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
//   }
// }





#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include <HTTPClient.h>

#include <config_monitoreo.h>

#define led 2
#define EEPROM_SIZE 12

int sensor = 4;
int relay = 5;

RTC_DS3231 rtc;

volatile int numPulsos = 0; 
int numPulsos_old = 0;
int address = 0;

bool estado_relay;
bool estado_valvula;
bool isReset = false;

float flowRate;
float calibrationFactor = 4.5;

unsigned long oldTime = 0;

String serverName = "https://aias.espol.edu.ec/api/hayiot/sensores/";

TaskHandle_t TaskSendData;

// Funcion para validar la hora
void validaHorario(DateTime date)
{
  if (date.hour()==00 && date.minute()==00) 
  {
    if(!isReset)
    {
      Serial.println("Sistema encerado");
      digitalWrite(led, LOW);
      digitalWrite(relay, LOW);
      estado_valvula = true;
      estado_relay = true;
      numPulsos = 0;
      numPulsos_old = 0;
      isReset = true;
      EEPROM.writeShort(address, 0);
      EEPROM.commit();
    }
  }
  else
  {
    isReset = false;
  }
}

// Funcion que imprime en pantalla la fecha y hora actual
void imprimeFecha(DateTime date)
{
  const char* diasDeLaSemana[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
  int numdia = date.dayOfTheWeek();
  Serial.print(diasDeLaSemana[numdia]);
  Serial.print(" ");
  Serial.print(date.day());
  Serial.print('/');
  Serial.print(date.month());
  Serial.print('/');
  Serial.print(date.year());
  Serial.print(" ");
  Serial.print(date.hour());
  Serial.print(':');
  Serial.print(date.minute());
  Serial.print(':');
  Serial.println(date.second());
}

// Funcion de contar pulsos
void IRAM_ATTR cuentaPulsos()
{
  numPulsos++;
}

// Funcion que envia los datos
void send_data(float data) 
{
  if (data)
  {
    if (WiFi.status() == WL_CONNECTED) 
    {
    HTTPClient http;
    String info = "{";
    info += "\"id\":\"" + String(ID) + "\",";
    info += "\"sensedAt\":\"\",";
    info += "\"data\":[{";
    info += "\"val\":" + String(data) + ",";
    info += "\"type\":\"flow\"";
    info += "}]}";

    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    http.POST(info);
    // Read response
    Serial.println(http.getString());

    // Disconnect
    http.end();
    }
  } 
}

// Tarea para enviar datos
void sendDataTask(void * parameter) 
{
  for (;;) 
  {
    float litros_de_agua = 0.107 * numPulsos; // Relación entre número de pulsos y litros de agua
    send_data(litros_de_agua);
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Espera 30 segundos
  }
}

void setup() 
{
  // Inicializo el puerto serial
  Serial.begin(9600);

  delay(2000);

  // Inicializo la EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Fallo en iniciar la EEPROM");
    Serial.println("Reiniciando...");
    delay(1000);
    ESP.restart();
  }

  // Inicializa la conexion a Wifi
  Serial.println("******************************************************");
  Serial.print("Conectando a ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //actualizar pulsos nuevos y viejos con lo de la EEPROM
  numPulsos_old = EEPROM.readShort(address);
  numPulsos = numPulsos_old;
  
  estado_valvula = false;
  estado_relay = false;
  
  // Comprobamos si tenemos el RTC conectado
  if (! rtc.begin())
  {
    Serial.println("No hay un módulo RTC");
    while (1);
  }

  //Ponemos en hora con los valores de la fecha y la hora en que el sketch ha sido compilado.
  rtc.adjust(DateTime(__DATE__, __TIME__));

  // Configuro los pines
  pinMode(led, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(sensor, INPUT);

  // Cambiar el estado de pines
  digitalWrite(sensor, HIGH);
  digitalWrite(led, LOW);
  digitalWrite(relay, LOW);

  // Activar la interrupcion para contar los pulsos del flujometro
  attachInterrupt(sensor, cuentaPulsos, FALLING);

  // Se crea la tarea para el envio de datos a HayIoT
  xTaskCreatePinnedToCore(
    sendDataTask, 
    "SendDataTask",
    8192,
    NULL,
    1,
    &TaskSendData,
    0
  );
}

// void loop()
// {
//   unsigned long realtime = millis() - oldTime;
//   if(realtime > 1000)
//   {
//     detachInterrupt(sensor);
//     if(numPulsos_old != numPulsos)
//     {
//       numPulsos_old = numPulsos;
//       EEPROM.writeShort(address, numPulsos);
//       EEPROM.commit();
//     }
//     flowRate = ((1000.0 / realtime) * numPulsos) / calibrationFactor;
//     oldTime = millis();
//     numPulsos = 0;
//     attachInterrupt(sensor, cuentaPulsos, FALLING);
//     Serial.println(flowRate);
//   }
// }

void loop() {
  //if(son diferentes) entonces se guarda en la EEPROM
  //Serial.println(numPulsos_old);
  
  //EEPROM.writeShort(address, numPulsos);
  //Serial.println(numPulsos);
  
  if (numPulsos_old != numPulsos)
  {
    numPulsos_old = numPulsos;
    EEPROM.writeShort(address, numPulsos_old);
    EEPROM.commit();
    Serial.println(numPulsos);
  }
  
  float limite = 15;
  //viejo = nuevo
  float litros_de_agua = 0.107*numPulsos; //Relacion entre numero de pulsos y litros de agua
  Serial.print("Pulsos: ");
  Serial.println(numPulsos);
  Serial.print("Litros de agua: ");
  Serial.println(litros_de_agua);
  DateTime fecha = rtc.now(); //Guarda la fecha en una variable llamada fecha
  imprimeFecha(fecha); //Muestra en monitor la fecha y hora actual
  //Serial.println(litros_de_agua);
  //Serial.println(limite);
  if (litros_de_agua >= limite)
  {
    detachInterrupt(digitalPinToInterrupt(sensor));
    digitalWrite(relay, HIGH);
    digitalWrite(led, HIGH);
    estado_valvula = false;
    estado_relay = false;
    delay(100);
    attachInterrupt(digitalPinToInterrupt(sensor), cuentaPulsos, RISING);
    Serial.println("Limite superado");
  }
  else
  {
    detachInterrupt(digitalPinToInterrupt(sensor));
    digitalWrite(relay, LOW);
    digitalWrite(led, LOW);
    estado_valvula = true;
    estado_relay = true;
    delay(100);
    attachInterrupt(digitalPinToInterrupt(sensor), cuentaPulsos, RISING);
  }
  
  validaHorario(fecha); //Valida que sea la hora indicada para reiniciar el conteo de pulsos
  Serial.print("Sensor: ");
  Serial.println(digitalRead(sensor));
  vTaskDelay(10000 / portTICK_PERIOD_MS);
}