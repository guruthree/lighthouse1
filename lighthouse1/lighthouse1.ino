#include "Sensor.h"

#define PRINT_OVER_SERIAL 1
#define PRINT_OVER_LCD 0
#define USE_WIFI 0
#define PRINT_OVER_HTTP 0
#define PRINT_OVER_TCP 0

#define NUM_SENSORS 3

#ifdef TEENSYDUINO
  // TODO: define Sensors for Teensy
#elif ESP8266
  // TODO: define Sensors for ESP8266
#elif ESP32
  Sensor<18> mySensor1(LED_BUILTIN); // SCK
  Sensor<25> mySensor2(26); // D2 & D3
  Sensor<21> mySensor3(12); // SDA & D13
#else
  #error "Unknown microcontroller, be careful..."
#endif

SensorBase* sensors[NUM_SENSORS] = {&mySensor1, &mySensor2, &mySensor3};

#if PRINT_OVER_LCD == 1
  #include <Adafruit_GFX.h>
  #include <Adafruit_ST7789.h>
  #include <SPI.h>
  #define TFT_CS   D13
  #define TFT_RST  D12
  #define TFT_DC   D11

  #define DISPLAY_WIDTH 135
  #define DISPLAY_HEIGHT 240
  
  Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
  static const uint16_t colors[NUM_SENSORS][2] = {
    {ST77XX_RED, tft.color565(233, 22, 80)}, 
    {ST77XX_GREEN, tft.color565(0, 156, 32)},
    {ST77XX_BLUE, tft.color565(40, 157, 255)}
  };
  uint8_t updatecount = 0;
  #define TFT_REFRESH 4
  int16_t oldvalues[NUM_SENSORS][2][DISPLAY_WIDTH]; // for graph
  #define GRAPH_VERTICAL 119
  #define GRAPH_ZERO 120

  GFXcanvas16 buffercanvas(DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

#if USE_WIFI == 1
  #include <WiFi.h>

  #include "wifidetails.h"

  #if PRINT_OVER_HTTP == 1
    #if ESP32
      #include <AsyncTCP.h>
      #include <ESPAsyncWebServer.h>
      AsyncWebServer server(80);
      void handleRoot(AsyncWebServerRequest *request) {
    #elif ESP8266
      #include <WebServer.h>
      WebServer server(80);
      void handleRoot() {
    #endif
      char outString[255];
      outString[0] = '\0';
      for (int c = 0; c < NUM_SENSORS; c++) {
        sprintf(outString, "%s%0.3f,%0.3f,", outString,
          sensors[c]->getX(),
          sensors[c]->getY()
        );
        yield();
      }
    #if ESP32
      request->send(200, "text/plain", outString);
    #elif ESP8266
      server.send(200, "text/plain", outString);
    #endif
    }
  #endif

  #if PRINT_OVER_TCP == 1
    #include <lwip/sockets.h>
    #include <lwip/netdb.h>

    #if ESP32
      TaskHandle_t TCPtaskHandle;
      static void TCPtask(void* pvParameters) {
        float t;
        
        struct sockaddr_in server;
        int sockfd = socket(AF_INET , SOCK_STREAM, 0);
        int one = 1, client_sock, recvd, res;
//        int timeout = 10000; // ms? how long to wait on broken connection
        
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(81);
        bind(sockfd, (struct sockaddr *)&server, sizeof(server));
        listen(sockfd, 5);
        
        while (1) {
            struct sockaddr_in _client;
            int cs = sizeof(struct sockaddr_in);
            client_sock = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);

            if (client_sock >= 0){
              setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&one, sizeof(int));
              setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
//              setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

              while (1) {
                res = recv(client_sock, &recvd, 1, MSG_WAITALL);
                if (res <= 0) {
                  break; // client disconnected
                }
                else if (recvd == 'Q') {
                  while (mySensor1.isUpdating() && mySensor2.isUpdating() && mySensor3.isUpdating()) {} // don't send mid-update, TODO: generalise this
                  for (int c = 0; c < NUM_SENSORS; c++) {
                    t = sensors[c]->getX();
                    send(client_sock, (uint8_t*)&t, sizeof(float), MSG_DONTWAIT);
                    t = sensors[c]->getY();
                    send(client_sock, (uint8_t*)&t, sizeof(float), MSG_DONTWAIT);
                  }
                }
              } // while (1)
              close(client_sock);
            }
          
          vTaskDelay(2/portTICK_PERIOD_MS); // sleep 2 ms
        } // while (1)
      }
    #elif ESP8266
      // TODO single-threaded port of code
    #endif
  #endif
#endif

#if ESP32
  TaskHandle_t taskHandles[NUM_SENSORS];

  static void MyTask(void* pvParameters) {
    SensorBase* localSensor = (SensorBase*) pvParameters;
    while(1) {
      localSensor->processPulses();
      vTaskDelay(2/portTICK_PERIOD_MS); // sleep 2 ms
    }
  }
#endif

// the setup routine runs once when you press reset:
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  delay(500);
  
#if PRINT_OVER_LCD == 1
  tft.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  tft.setSPISpeed(40e6); // 80 MHz is top speed of ESP-32?
  tft.fillScreen(ST77XX_RED);
#endif

#if USE_WIFI == 1
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // high power mode to reduce latency
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());    
  delay(500);
   
#if PRINT_OVER_HTTP == 1
#if ESP32
  server.on("/", HTTP_GET, handleRoot);
#elif ESP8266
  server.on("/", handleRoot);
#endif
  server.begin();
  Serial.println("HTTP server started");
  delay(500);
#endif
#endif

#if PRINT_OVER_TCP == 1
  xTaskCreate(TCPtask, "TCPtask", 16384, NULL, 2, &TCPtaskHandle);
  Serial.println("TCP server setup");
  delay(500);
#endif
  
  for (int c = 0; c < NUM_SENSORS; c++) {
    sensors[c]->setup();
  }
  Serial.println("Sensors setup");
  delay(500);

#if ESP32
  char temp[20];
  for (int c = 0; c < NUM_SENSORS; c++) {
    sprintf(temp, "Task%i", c);
    xTaskCreate(MyTask, temp, 8192, sensors[c], 4, &taskHandles[c]);
  }
  Serial.println("Sensor tasks setup");
  delay(500);
#endif
  
  digitalWrite(LED_BUILTIN, LOW); // LED off means we're ready
#if PRINT_OVER_LCD == 1
//  tft.fillScreen(ST77XX_BLUE);
  tft.fillScreen(ST77XX_BLACK);
#endif
}

// the loop routine runs over and over again forever:
void loop() {

#ifndef ESP32
  for (int c = 0; c < NUM_SENSORS; c++) {
    sensors[c]->processPulses();
  }
#endif

#if PRINT_OVER_SERIAL == 1
  for (int c = 0; c < NUM_SENSORS; c++) {
    Serial.print(sensors[c]->getX());
    Serial.print(", ");
    Serial.print(sensors[c]->getY());
    Serial.print(", ");
  }
  Serial.print("\n");
#endif

#if PRINT_OVER_LCD == 1
  if (updatecount == TFT_REFRESH) {
    updatecount = 0;

    buffercanvas.setCursor(0, 0);
    buffercanvas.setTextSize(2);
    buffercanvas.fillScreen(ST77XX_BLACK);
    float tf;
    for (int c = 0; c < NUM_SENSORS; c++) {
      buffercanvas.setTextColor(colors[c][0], ST77XX_BLACK);
      tf = sensors[c]->getX();
      buffercanvas.println(tf);
      oldvalues[c][0][DISPLAY_WIDTH-1] = (-tf/45.0f)*GRAPH_VERTICAL;
      
      buffercanvas.setTextColor(colors[c][1], ST77XX_BLACK);
      tf = sensors[c]->getY();
      buffercanvas.print(" ");
      buffercanvas.println(tf);
      oldvalues[c][1][DISPLAY_WIDTH-1] = (-tf/45.0f)*GRAPH_VERTICAL;
    }

    // draw graph
    for (uint8_t d = 1; d < DISPLAY_WIDTH; d++) {
      for (uint8_t c = 0; c < NUM_SENSORS; c++) {
        buffercanvas.drawLine(d-1, GRAPH_ZERO+oldvalues[c][0][d-1], d, GRAPH_ZERO+oldvalues[c][0][d], colors[c][0]);
        oldvalues[c][0][d-1] = oldvalues[c][0][d];
        buffercanvas.drawLine(d-1, GRAPH_ZERO+oldvalues[c][1][d-1], d, GRAPH_ZERO+oldvalues[c][1][d], colors[c][1]);
        oldvalues[c][1][d-1] = oldvalues[c][1][d];
      }
    }

    tft.drawRGBBitmap(0, 0, buffercanvas.getBuffer(), DISPLAY_WIDTH, DISPLAY_HEIGHT);    
  }
  updatecount++;
#endif

  digitalWriteFastLOW(LED_BUILTIN);
//  mySensor1.printReceivedData();

  delay(8); // 120 Hz?
  yield();
}
