#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <ESP32httpUpdate.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#define TdsSensorPin 27
#define VREF 3.3   // analog reference voltage(Volt) of the ADC
#define SCOUNT 30  // sum of sample point

const char* ssid = "anehh"; //based on your ssid
const char* pass = "y86sxax2u2fje"; // based on your pwd
String serverName = "http://ansf.infinitebe.com/wqms/test.php"; 
unsigned long sendDataPrevMillis = 0;
int count = 0;

WiFiClient wifiClient;

//temp
// GPIO where the DS18B20 is connected to
const int oneWireBus = 2;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

//ph
const int PHPin = 4;
float ph;
float Value = 0;

//turbidity
int turbidityPin = 34;
int analogBuffer[SCOUNT];  // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float temperatureC = 0;
//int turbidity = 0;

//tds
int i = 0;  //TDS
float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25; 
 // current temperature for compensation

int getMedianNum(int bArray[], int iFilterLen) {  //tds function generator
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  sensors.begin();
  pinMode(PHPin, INPUT);         //ph
  pinMode(TdsSensorPin, INPUT);  //tds
  delay(1000);

  //wifi
  WiFi.begin(ssid, pass);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}
void loop() {  //hanya untuk wifi

  if (millis() - sendDataPrevMillis > 20000 || sendDataPrevMillis == 0) {
    count++;
    //external fx called
    getTDS();
    getturbidity();
    getTemp();
    getPH();

    sendDataPrevMillis = millis();

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Build the URL string with the parameters
      // Serial.println(temperatureC);
      // Serial.println(tdsValue);
      // Serial.println(ph);
      // Serial.println(turbidity);
      String httpReqStr = serverName + "?temperature=" + String(temperatureC, 2) + "&tds=" + String(tdsValue, 2) + "&ph=" + String(ph, 1) + "&turbidity=" + String(voltage, 2);

      // Begin the HTTP request
      http.begin(client, httpReqStr.c_str());
      Serial.println(httpReqStr);

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      // Check the returning code
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
  }
}



void getTDS() {
  //tds sensor
  for (i = 0; i < 100; i++) {
    static unsigned long analogSampleTimepoint = millis();
    if (millis() - analogSampleTimepoint > 40U) {  //every 40 milliseconds,read the analog value from the ADC
      analogSampleTimepoint = millis();
      analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);  //read the analog value and store into the buffer
      analogBufferIndex++;
      if (analogBufferIndex == SCOUNT) {
        analogBufferIndex = 0;
      }
    }

    static unsigned long printTimepoint = millis();
    if (millis() - printTimepoint > 800U) {
      printTimepoint = millis();
      for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
        analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

        // read the analog value more stable by the median filtering algorithm, and convert to voltage value
        averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

        //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
        float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
        //temperature compensation
        float compensationVoltage = averageVoltage / compensationCoefficient;

        //convert voltage value to tds value
        tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

        //Serial.print("voltage:");
        //Serial.print(averageVoltage,2);
        //Serial.print("V   ");
      }
      Serial.print("TDS Value:");
      Serial.print(tdsValue, 0);
      Serial.println("ppm");
    }
  }
}

void getturbidity() {
  //turbidity sensor
  int turbiValue = analogRead(turbidityPin);
  float voltage = turbiValue * (5.0 / 1024.0);
  Serial.println ("Sensor Output (V):");
  Serial.println (voltage);
  Serial.println();
  delay(1000);
}

void getTemp() {
  //temp sensor
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);
  Serial.print(temperatureC);
  Serial.print("ºC ");
  Serial.print(temperatureF);
  Serial.println("ºF");
  delay(1000);
}

void getPH() {
  //ph sensor
  Value= analogRead(PHPin);
  Serial.print(Value);
  Serial.print(" | ");
  float voltage = Value * (3.3 / 4095.0);
  ph = (3.3 * voltage);
  Serial.println(ph);
  delay(1000);
}