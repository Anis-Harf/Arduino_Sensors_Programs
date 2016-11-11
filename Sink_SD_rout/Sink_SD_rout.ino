
/* Ce programme a été développé sur : 
 *  - Un arduino MEGA 2560  
 *  - Une carte Wireless proto Shield avec une XBee S1 connectée à l'entrée SERIAL1 - rate : 38400
 *  - Une carte Arduino WiFi Shield échangeant les données à travers les ports SPI (et non Serial) avec slot MicroSD (MAJ du driver de la carte obligatoire)
 *  - Un serveur distant de BDD - MySQL
 *  - Un serveur distant FTP - FileZilla
 *  - Un point d'accès WiFi (ici SSID:aniswifi)
-- La sortie Serial (Serial0) est utilisée pour le debuggage
Pour desactiver les commentaires de debuggage il faut commenter le #define DEBUG */

#define DEBUG
#include <XBee.h>
XBee xbee = XBee();
#include "MessageSignal.h"
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <sha1.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <mysql.h>

char nom_patient[]="dujardin";
char prenom_patient[]="martin";
int id_patient;
char SELECT_SQL[100];

IPAddress server(192,168,43,126);
char user[] = "anis"; // identifiants MySql
char password[] = "canal+";

char ssid[] = "aniswifi";     //  your network SSID (name)
char pass[] = "00000000";  // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status


Connector my_conn;        // The Connector/Arduino reference
WiFiClient client;
WiFiClient dclient;
unsigned int hiPort,loPort;
File fh;
char outBuf[128];
char outCount;

#define radio_serial Serial1
#define MAX_NC 100
uint16_t address = 0;
unsigned long currentMillis;
unsigned long previousMillis;
unsigned long timeImage;
unsigned long temps= 120000;
int nb_nc =0;

// Structure d'une entrée de la table de routage
struct entree_route {
  uint16_t nc;
  uint16_t promoteur;
};
entree_route tab_nc[MAX_NC];

TxStatusResponse txStatus = TxStatusResponse();
Rx16Response rx = Rx16Response();
byte eRcv();
void efail();
uint16_t getMyAddress();
void receive(byte * payload, uint8_t rssi);
bool is_known(uint16_t addr);
void addChild(uint16_t from, uint16_t addr);
void askChild(int idchild);



void setup() {
  
  radio_serial.begin(38400); 
  xbee.setSerial(radio_serial);
  delay(15000); // délai de mise en route de la radio 802.15.4
  Serial.begin(38400); // initialisation de la sortie de débogage

// ###### Initialisation MicroSD   
if(SD.begin(4) == 0)
  {
#ifdef DEBUG
    Serial.println(F("SD init fail"));   
#endif       
  }

// ###### tentative de connexion WiFi :
   while (status != WL_CONNECTED) {
#ifdef DEBUG
    Serial.print("tentative de connexion WiFi SSID: ");
    Serial.println(ssid);
#endif
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    // attente 10 secondes pour la connexion:
    delay(10000);
  }
#ifdef DEBUG
  Serial.println("Vous etes connectés au réseau");
#endif
// ###### MySql
#ifdef DEBUG
    Serial.println("Connexion à MySql...");
#endif
  if (my_conn.mysql_connect(server, 3306, user, password)) {
    delay(1000);
  }
  else {
#ifdef DEBUG
    Serial.println("Connection echouée à MySql."); 
#endif
    while(1); 
  }
  
// ###### MySql pour la récupération de l'identifiant du patient de la base de données
 sprintf(SELECT_SQL,"SELECT id_patient FROM ehealth.patient WHERE nom='%s' and prenom='%s';",nom_patient,prenom_patient);
#ifdef DEBUG
 Serial.print("requete: ");
 Serial.println(SELECT_SQL);
#endif
 my_conn.cmd_query(SELECT_SQL);
 my_conn.get_columns();
 row_values *row = NULL;
 do {
 row = my_conn.get_next_row();
   if (row != NULL) {
 id_patient = atol(row->values[0]);
#ifdef DEBUG
     Serial.println(id_patient);
#endif
 }
 my_conn.free_row_buffer();
 } while (row != NULL);
 my_conn.free_columns_buffer();

// ###### FTP
if (client.connect(server,21)) {
#ifdef DEBUG
    Serial.println(F("Connecté FTP"));
#endif
  } 
  else {
#ifdef DEBUG
    Serial.println(F("Conenxion echouée au FTP"));
#endif    
    while(1);
  }
  if(!eRcv()) while(1);
  client.println(F("USER anis"));
  if(!eRcv()) while(1);
  client.println(F("PASS password"));
  if(!eRcv()) while(1);
  client.println(F("SYST"));
  if(!eRcv()) while(1);
  client.println(F("Type I"));
  if(!eRcv()) while(1);
  
// ###### Récuperation de l'adresse locale   
while(address == 0){
   address = getMyAddress();
}
#ifdef DEBUG
Serial.print("Mon adresse ");
Serial.println(address);
#endif
    previousMillis = millis();
    temps= 120000;
}

void loop() {

 xbee.readPacket();

    if (xbee.getResponse().isAvailable()) {
      // got something
      
      if (xbee.getResponse().getApiId() == RX_16_RESPONSE) {
        // got a rx packet
                xbee.getResponse().getRx16Response(rx);
        uint8_t len = rx.getDataLength();
#ifdef DEBUG
        Serial.print("RSSI : ");
        Serial.println(rx.getRssi());
#endif
        byte payload[len];
        memcpy(payload, rx.getData(), len);
        receive(payload,rx.getRssi());
        }
        else if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
            xbee.getResponse().getTxStatusResponse(txStatus);
        
         // get the delivery status, the fifth byte
           if (txStatus.getStatus() == SUCCESS) {
              // success.  time to celebrate
#ifdef DEBUG
              //Serial.println("success.  time to celebrate");
#endif              
           } else {
              // the remote XBee did not receive our packet. is it powered on?
#ifdef DEBUG
              Serial.println("the remote XBee did not receive our packet. is it powered on?");
#endif              
           }
        }    
        }
    else if (xbee.getResponse().isError()) {
#ifdef DEBUG
      Serial.println("Error reading packet.  Error code: ");  
      Serial.println(xbee.getResponse().getErrorCode());
#endif
    } 
  
   currentMillis = millis();
  
   if (currentMillis - previousMillis > temps) {
    for (int i=0; i<nb_nc; i++){
        askChild(i);
        delay(1000);
    }
            temps = 60000;
            previousMillis = millis();
   }
  
}



inline void receive(byte * payload, uint8_t rssi) {
    uint8_t msg_id = payload[0];
    if (msg_id == 1) { //identifiant du type de message 1 == demande de rejoindre le réseau
      if (rssi<75){
        MessageSignal *message = reinterpret_cast<MessageSignal*> (payload);
        if (!is_known((uint16_t) *(message->payload()))) { // verifier que ce n'est pas deja connu comme nc
            addChild(message->source(),(uint16_t) *(message->payload()));
        }
      }
    } 
}

inline bool is_known(uint16_t addr){
  for (int i=0;i<nb_nc;i++){
    if (addr == tab_nc[i].nc) return true;
  }
  return false;
}

inline void addChild(uint16_t from, uint16_t addr){
  if (nb_nc<MAX_NC){
        entree_route route;
        route.nc = addr;
        route.promoteur = from;
        tab_nc[nb_nc]= route;
#ifdef DEBUG
        Serial.println("");
        Serial.print("jajoute un noeud camera id : ");
        Serial.print(route.nc);
        Serial.print(" ca me vient de : ");
        Serial.print(route.promoteur);
        Serial.println("");
#endif        
        nb_nc++;
  }
}

inline void askChild(int idchild){
  byte req = 2;
  MessageSignal reqe = MessageSignal(req, address); 
  uint16_t dest = tab_nc[idchild].nc;
#ifdef DEBUG
  Serial.print("ask : ");
  Serial.println(dest);
#endif  
  reqe.set_payload(sizeof(dest),(byte*) &dest); 
  Tx16Request tx = Tx16Request(tab_nc[idchild].promoteur,(byte*)(&reqe),reqe.buffer_size() );
  xbee.send(tx); 
  bool imagecomplet = false;
  char INSERT_SQL[100];
  bool continu = true;

  char fileName[50];
  sprintf(fileName,"-%d_%d#.jpg",tab_nc[idchild].nc,id_patient);
#ifdef DEBUG
  Serial.print("nom fichier: ");
  Serial.println(fileName);
#endif  
  SD.remove(fileName);
  fh = SD.open(fileName,FILE_WRITE);
    if(!fh)
  {
#ifdef DEBUG
    Serial.println(F("SD open fail"));
#endif    
    while(1);    
  }
  if(!fh.seek(0))
  {
#ifdef DEBUG
    Serial.println(F("Rewind fail"));
#endif
    fh.close();
    while(1);    
  }
  timeImage = millis();
  
  while(!imagecomplet && continu){ 
  
  if (millis()-timeImage >30000) continu = false;
 xbee.readPacket();
    
    if (xbee.getResponse().isAvailable()) {
      // got something
      
      if (xbee.getResponse().getApiId() == RX_16_RESPONSE) {
        // got a rx packet
                xbee.getResponse().getRx16Response(rx);
        uint8_t len = rx.getDataLength();
        byte payload[len];

        memcpy(payload, rx.getData(), len);
    if (payload[0] == 4){ // endData
    imagecomplet=true;

    //MySql
#ifdef DEBUG
    Serial.println(id_patient);
#endif   
    sprintf(INSERT_SQL,"INSERT INTO ehealth.image (id_patient,nom_fichier) VALUES (%d,'%s');",id_patient,fileName);
#ifdef DEBUG
    Serial.print("requete Insert: ");
    Serial.println(INSERT_SQL);
#endif    
    my_conn.cmd_query(INSERT_SQL);
    
    //FTP

  client.println(F("PASV"));

  if(!eRcv()) while(1);
  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL,"(,");
    array_pasv[i] = atoi(tStr);
    if(tStr == NULL)
    {
#ifdef DEBUG
      Serial.println(F("Bad PASV Answer"));    
#endif
    }
  }

  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;
#ifdef DEBUG
  Serial.print(F("Data port: "));
#endif
  hiPort = hiPort | loPort;
#ifdef DEBUG
  Serial.println(hiPort);
#endif  

if (dclient.connect(server,hiPort)) {
#ifdef DEBUG
    Serial.println(F("Data connected"));
#endif    
  } 
  else {
#ifdef DEBUG
    Serial.println(F("Data connection failed"));
#endif
    client.stop();
    fh.close();
    while(1);
  }


  client.print(F("STOR "));
  client.println(fileName);

  if(!eRcv())
  {
    dclient.stop();
    while(1);
  }
#ifdef DEBUG
  Serial.println(F("Writing"));
#endif  
  if(!fh.seek(0))
  {
#ifdef DEBUG
    Serial.println(F("Rewind fail"));
#endif    
  }
  byte clientBuf[64];
  int clientCount = 0;

  while(fh.available())
  {
    clientBuf[clientCount] = fh.read();
    clientCount++;

    if(clientCount > 63)
    {
      dclient.write(clientBuf,64);
      clientCount = 0;
    }
  }
  if(clientCount > 0) {
    dclient.write(clientBuf,clientCount);
  }
  delay(2000);
  dclient.stop();
#ifdef DEBUG
  Serial.println(F("Data disconnected"));
#endif
  client.println(F("ABOR"));
  if(!eRcv()) while(1);
    delay(2000);
    fh.close();
#ifdef DEBUG
    Serial.println(F("SD closed"));
#endif    
    // fin fichier
    } 
    else if (payload[0] == 3){ // paquet de données
          MessageSignal *donnee = reinterpret_cast<MessageSignal*> (payload);
          for (int i = 0; i < donnee->payload_size(); i ++) {

          //  Serial.print((char) *(donnee->payload()+ i));
             fh.write((char) *(donnee->payload()+ i));
          }
      }
     else if (payload[0] == 1 ) { // pendant transmission un noeud s'est signalé
#ifdef DEBUG
      Serial.println("pendant transmission un noeud s'est signalé");
#endif      
      receive(payload,rx.getRssi());
     }
    }
   else if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
            xbee.getResponse().getTxStatusResponse(txStatus);
        
         // get the delivery status, the fifth byte
           if (txStatus.getStatus() == SUCCESS) {
              // success.  time to celebrate
#ifdef DEBUG
            //  Serial.println("success.  time to celebrate");
#endif            
           } else {
              // the remote XBee did not receive our packet. is it powered on?
#ifdef DEBUG
              Serial.println("the remote XBee did not receive our packet. is it powered on? ***");
#endif              
           }
        }  
    }
    else if (xbee.getResponse().isError()) {
#ifdef DEBUG
      Serial.println("Error reading packet.  Error code: ");  
      Serial.println(xbee.getResponse().getErrorCode());
#endif      
    }

  }
}

uint16_t getMyAddress(){

  uint8_t my[] = {'M','Y'};
  AtCommandRequest atRequest = AtCommandRequest(my);
  AtCommandResponse atResponse = AtCommandResponse();
  uint16_t adresse = 0;
  xbee.send(atRequest);

  // wait up to 5 seconds for the status response
  if (xbee.readPacket(5000)) {
    // got a response!

    // should be an AT command response
    if (xbee.getResponse().getApiId() == AT_COMMAND_RESPONSE) {
      xbee.getResponse().getAtCommandResponse(atResponse);

      if (atResponse.isOk()) {

        if (atResponse.getValueLength() > 0) {

          uint8_t * bit = ((uint8_t*) & adresse);
          bit[1] = *(atResponse.getValue());
          bit[0] = *(atResponse.getValue()+1);

        }
      } 
      else {
#ifdef DEBUG
        Serial.print("Command return error code: ");
        Serial.println(atResponse.getStatus(), HEX);
#endif        
      }
    } else {
#ifdef DEBUG
      Serial.print("Expected AT response but got ");
      Serial.print(xbee.getResponse().getApiId(), HEX);
#endif      
    }   
  } else {
    // at command failed
    if (xbee.getResponse().isError()) {
#ifdef DEBUG
       Serial.print("Error reading packet.  Error code: ");  
      Serial.println(xbee.getResponse().getErrorCode());
#endif
    } 
    else {
#ifdef DEBUG
        Serial.print("No response from radio");  
#endif        
    }
  }
  atRequest.clearCommandValue(); 
  return adresse;
}


byte eRcv()
{
  byte respCode;
  byte thisByte;

  while(!client.available()) {
    delay(1);
  }

  respCode = client.peek();

  outCount = 0;

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;      
      outBuf[outCount] = 0;
    }
  }
  if(respCode >= '4')
  {
#ifdef DEBUG
    Serial.println("refus commande");
#endif
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;

  client.println(F("QUIT"));

  while(!client.available()) delay(1);

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  client.stop();
#ifdef DEBUG
  Serial.println(F("Command disconnected --- efail"));
#endif
  fh.close();
#ifdef DEBUG
  Serial.println(F("SD closed"));
#endif  
}

