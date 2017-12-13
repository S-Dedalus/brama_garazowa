#include <ELClient.h>
#include <ELClientRest.h>
#include <ELClientWebServer.h>



// Initialize a connection to esp-link using the normal hardware serial port both for
// SLIP and for debug messages.
ELClient esp(&Serial, &Serial);

// Initialize a REST client on the connection to esp-link
ELClientRest rest(&esp);

ELClientWebServer webServer(&esp);

boolean wifiConnected = false;
unsigned long PreviousMillis = 0;
const long Interval = 500; //czas zalaczenia przekaznika otwierania

// Callback made from esp-link to notify of wifi status changes
// Here we print something out and set a global flag
void wifiCb(void *response) {
  ELClientResponse *res = (ELClientResponse*)response;
  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if(status == STATION_GOT_IP) {
      Serial.println("WIFI CONNECTED");
      wifiConnected = true;
    } else {
      Serial.print("WIFI NOT READY: ");
      Serial.println(status);
      wifiConnected = false;
    }
  }
}

void ledButtonPressCb(char * btnId) //tutaj potrzeba tylko jednego przycisku!!!!!!!!!!!
{
  String id = btnId;
  if( id == F("btn_on") )
    digitalWrite(A2, HIGH);
    unsigned long CurrentMillis = millis();
    if (CurrentMillis - PreviousMillis >= Interval) {
      digitalWrite(A2, LOW);
    }
}


void resetCb(void) {
  Serial.println("EL-Client (re-)starting!");
  bool ok = false;
  do {
    ok = esp.Sync();      // sync up with esp-link, blocks for up to 2 seconds
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");
  
  webServer.setup();
}


void setup() {
  Serial.begin(9600);   // the baud rate here needs to match the esp-link config
  Serial.println("EL-Client starting!");

pinMode(A0, INPUT_PULLUP); //krancowka otwarcia
pinMode(A1, INPUT_PULLUP); //krancowka zamkniecia
pinMode(A2, OUTPUT); //przekaznik zamykania/otwierania

  // Sync-up with esp-link, this is required at the start of any sketch and initializes the
  // callbacks to the wifi status change callback. The callback gets called with the initial
  // status right after Sync() below completes.
  esp.wifiCb.attach(wifiCb); // wifi status change callback, optional (delete if not desired)
  bool ok;
  do {
    ok = esp.Sync();      // sync up with esp-link, blocks for up to 2 seconds
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");

  // Get immediate wifi status info for demo purposes. This is not normally used because the
  // wifi status callback registered above gets called immediately. 
 /* esp.GetWifiStatus();
  ELClientPacket *packet;
  if ((packet=esp.WaitReturn()) != NULL) {
    Serial.print("Wifi status: ");
    Serial.println(packet->value);
  }*/

URLHandler *ledHandler = webServer.createURLHandler(F("/SimpleLED.html.json"));
//ledHandler->loadCb.attach(&ledPageLoadAndRefreshCb);
//ledHandler->refreshCb.attach(&ledPageLoadAndRefreshCb);
ledHandler->buttonCb.attach(&ledButtonPressCb);

esp.resetCb = resetCb;
resetCb();

  // Set up the REST client to talk to www.timeapi.org, this doesn't connect to that server,
  // it just sets-up stuff on the esp-link side
  int err = rest.begin("192.168.1.55", 8080);
  if (err != 0) {
    Serial.print("REST begin failed: ");
    Serial.println(err);
    while(1) ;
  }
  Serial.println("EL-REST ready");
}

#define BUFLEN 266

void loop() {
  // process any callbacks coming from esp_link
  esp.Process();

//sprawdzam krancowke od otwierania i zmieniam stan przelacznika w Domoticzu
if (digitalRead(A0) == LOW){
  if(wifiConnected) {
    // Request /utc/now from the previously set-up server
    rest.get("/json.htm?type=command&param=switchlight&idx=29&switchcmd=On");
    char response[BUFLEN];
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: ");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}

//sprawdzam krancowke od zamykania i zmieniam stan przelacznika w Domoticzu
if (digitalRead(A0) == LOW){
  if(wifiConnected) {
    // Request /utc/now from the previously set-up server
    rest.get("/json.htm?type=command&param=switchlight&idx=29&switchcmd=Off");
    char response[BUFLEN];
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: ");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}
}

