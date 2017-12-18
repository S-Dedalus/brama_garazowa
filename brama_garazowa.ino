#include <ELClient.h>
#include <ELClientRest.h>
#include <ELClientWebServer.h>


//Nawiązuje połączenie z esp-link za pośrednictwem portu szeregowego dla protokołu SLIP 
//i danych debugowaniw
ELClient esp(&Serial, &Serial);

// Uruchamianie klienta REST dla połączenia uC<->esp
ELClientRest rest(&esp);

//Uruchamia webserwer
ELClientWebServer webServer(&esp);

boolean wifiConnected = false;
boolean otwarte = false;

// Callback od esp-linka, który pilnuje zmian stanu wifi 
// Wypisuje trochę debugu i ustawia globalną flagę
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


//Definiuje zachowanie uC po otrzymaniu informacji, że wciśnięto 
//przycisk
void ledButtonPressCb(char * btnId) 
{
  String id = btnId;
  if( id == F("btn_on") )
    digitalWrite(A2, LOW);
    Serial.println("Wcisnieto przycisk zamykania/otwierania bramy");
    delay(1000);
    digitalWrite(A2, HIGH);
}

void resetCb(void) {
  Serial.println("EL-Client (re-)starting!");
  bool ok = false;
  do {
    ok = esp.Sync();      // synchronizuje się z esp-linkiem, blokuje na ponad 2 sekundy
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");
  
  webServer.setup();
}


void setup() {
  Serial.begin(9600);   
  Serial.println("EL-Client starting!");

pinMode(A0, INPUT_PULLUP); //krancowka otwarcia
pinMode(A1, INPUT_PULLUP); //krancowka zamkniecia
pinMode(A2, OUTPUT); //przekaznik zamykania/otwierania
digitalWrite(A2, LOW);



  //Synchronizuje z esp-link. Jest wymagana na początku każdego skeczu. Inicjalizuje 
  //callback'i do callbacku o zmianie statusu wifi. Callback jest wywoływany ze stanem początkowym
  //tuz po tym jak Sync() zakończy działanie. 
  
  esp.wifiCb.attach(wifiCb); // callback zmian stanu wifi, opcjonalne (wywalić, jeśli niepotrzebne)
  bool ok;
  do {
    ok = esp.Sync();      // synchronizuje esp-link, blokuje system na 2 sek.
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");

URLHandler *ledHandler = webServer.createURLHandler(F("/Sterowanie.html.json"));
ledHandler->buttonCb.attach(&ledButtonPressCb);

esp.resetCb = resetCb;
resetCb();

  // Ustawia klienta REST'a, żeby gadał z 192.168.1.61 na porcie 8080. 
  //Nie łączy się z nim jeszcze, ale zapamiętuje dane po stronie esp-linka
  
  int err = rest.begin("192.168.1.54", 8080);
  if (err != 0) {
    Serial.print("REST begin failed: ");
    Serial.println(err);
    while(1) ;
  }
  Serial.println("EL-REST ready");
}

#define BUFLEN 266

void loop() {

// przetwarza wszystkie callbacki od esp-linka
esp.Process();

//sprawdzam krancowke od otwierania i zmieniam stan przelacznika w Domoticzu
if (digitalRead(A1) == HIGH && otwarte == false){
  if(wifiConnected) {
    // Wysyłanie żądania do domoticza
    rest.get("/json.htm?type=command&param=switchlight&idx=29&switchcmd=On");
    Serial.print("Wysłałem zapytanie json");
    char response[BUFLEN];
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    otwarte = true;
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: OTWARTE");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}

//sprawdzam krancowke od zamykania i zmieniam stan przelacznika w Domoticzu
if (digitalRead(A0) == HIGH && otwarte == true){
  if(wifiConnected) {
    // pobiera metodą GET odpowiedź na zapytanie json z wcześniej ustawionego serwera 
    rest.get("/json.htm?type=command&param=switchlight&idx=29&switchcmd=Off");
    Serial.print("Wysłałem zapytanie json");
    char response[BUFLEN];
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    otwarte = false;
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: ZAMKNIETE");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}
}

