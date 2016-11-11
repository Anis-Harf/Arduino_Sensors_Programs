
/* Ce programme a été développé sur : 
 *  - Un arduino MEGA 2560  
 *  - Une carte Wireless proto Shield avec une XBee S1 connectée à l'entrée SERIAL1 - rate : 38400
 *  - Une caméra TTL Serial connectée sur l'entrée Serial2
-- La sortie Serial (Serial0) est utilisée pour le debuggage
Pour desactiver les commentaires de debuggage il faut commenter le #define DEBUG */

#define DEBUG
#include <XBee.h>
#include <LSY201.h>
#include "MessageSignal.h"
#define camera_serial Serial2
#define radio_serial Serial1

XBee xbee = XBee();
uint16_t address;
LSY201 camera;
uint8_t buf[32];
unsigned long currentMillis;
unsigned long previousMillis;


TxStatusResponse txStatus = TxStatusResponse();
Rx16Response rx = Rx16Response();

// Structure d'une entrée de la table de routage
struct entree_route {
  uint16_t nc;
  uint16_t promoteur;
  uint16_t vers_sink;
};
#define MAX_ROUTE 10
entree_route tab_route[MAX_ROUTE];
int nb_route = 0;

void setup() {
  
  // initialisation caméra
camera.setSerial(camera_serial);
camera_serial.begin(38400);
camera.reset();
delay(1000);

radio_serial.begin(38400);
xbee.setSerial(radio_serial);
delay(15000);

Serial.begin(38400);
while(address == 0){
     address = getMyAddress();
}
#ifdef DEBUG
  Serial.print("Mon adresse ");
  Serial.println(address);
#endif
  
  byte sig = 1;
  MessageSignal signal = MessageSignal(sig, address); // Demande de rejoindre le réseau
  signal.set_payload(sizeof(address), (byte*) &address); // mettre son ID
  Tx16Request tx2 = Tx16Request(0xffff,(byte*) (&signal),signal.buffer_size());

  previousMillis = currentMillis = millis();

while (currentMillis - previousMillis < 90000){
  
  for (int i=0;i<5;i++){
  xbee.send(tx2); // envoyer le message  // penser à le faire plusieurs fois pour qu'il ne soit pas raté par les autres

   xbee.readPacket(50);

    while (xbee.getResponse().isAvailable()) {
      // got something
      
      if (xbee.getResponse().getApiId() == RX_16_RESPONSE) {
        // got a rx packet
                xbee.getResponse().getRx16Response(rx);
        uint8_t len = rx.getDataLength();
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
           //   Serial.println("success.  time to celebrate");
#endif
           } else {
              // the remote XBee did not receive our packet. is it powered on?
#ifdef DEBUG
              Serial.println("the remote XBee did not receive our packet. is it powered on?");
#endif
           }
        }
            xbee.readPacket(50);

        }
     if (xbee.getResponse().isError()) {
#ifdef DEBUG
      Serial.println("Error reading packet.  Error code: ");  
      Serial.println(xbee.getResponse().getErrorCode());
#endif
    }        
      delay(1000);
  }

currentMillis = millis();
}

#ifdef DEBUG
  Serial.println("broadcast emis");
#endif
}

void loop() {

 xbee.readPacket();

    if (xbee.getResponse().isAvailable()) {
      // got something
      
      if (xbee.getResponse().getApiId() == RX_16_RESPONSE) {
        // got a rx packet
                xbee.getResponse().getRx16Response(rx);
        uint8_t len = rx.getDataLength();
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
           //   Serial.println("success.  time to celebrate");
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

}
inline void receive(byte * payload, uint8_t rssi) {
    uint8_t msg_id = payload[0];
#ifdef DEBUG
    Serial.print("RSSI : ");
    Serial.println(rssi);
#endif
    if (msg_id == 1) // demande de rejoindre le réseau
    {
      if (rssi<75){
#ifdef DEBUG
              Serial.print(" demande de rejoindre le reseau ");
#endif
      MessageSignal *message = reinterpret_cast<MessageSignal*> (payload);

      bool trouve = (address == (uint16_t) *(message->payload())); // faut pas que ça soit moi
#ifdef DEBUG
              Serial.println(*(message->payload()));
#endif
         if(!trouve){     
            MessageSignal join = MessageSignal(1, address); 
            join.set_payload(message->payload_size(),message->payload());
            Tx16Request tx = Tx16Request(0xffff,(byte*) (&join),join.buffer_size());
            xbee.send(tx);
         }
      for (int i=0; i<nb_route;i++){
        if (tab_route[i].nc == *(message->payload())){ // nc deja connu
          trouve = true;
          break;
        }
        }
        if(!trouve){
#ifdef DEBUG
          Serial.print("nouveau noeud cam signale");
          Serial.println(*(message->payload()));
#endif
          if (nb_route<MAX_ROUTE){ // on peut l'ajouter comme nc dans la table
            entree_route route;
            route.nc = (uint16_t) *(message->payload());
            route.promoteur = message->source();
            tab_route[nb_route]=route;
            nb_route++;
          }
        }
      }
    }
    else if (msg_id == 2) { //requete
          MessageSignal *message = reinterpret_cast<MessageSignal*> (payload);
      
      if (address == (uint16_t) *(message->payload()) ) { //la requete m'est destinée //  
          camera.takePicture();
         uint16_t offset = 0;
           byte datatype = 3;
           byte endData = 4;
          MessageSignal data = MessageSignal(datatype, address); 
          Tx16Request tx;
          while (camera.readJpegFileContent(offset, buf, sizeof(buf)))
            {
              data.set_payload(sizeof(buf),buf);
              data.set_nc(address);
              tx = Tx16Request(message->source(),(byte*) (&data),data.buffer_size());
              xbee.send(tx); 

              offset += sizeof(buf);
              delay(50);  // Tres important !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            }
            data = MessageSignal(endData, address);  //image terminée
            data.set_nc(address);
            tx = Tx16Request(message->source(),(byte*) (&data),data.buffer_size());
            xbee.send(tx); 
              
            camera.stopTakingPictures(); //Otherwise picture won't update
      }
      else { // je dois router la requete
#ifdef DEBUG
                  Serial.print(" Je route une requete venant de ");
                  Serial.print(message->source());
                  Serial.print(" vers le nc : ");
                  Serial.print((uint16_t) *(message->payload()));
#endif
        uint16_t next = majTab((uint16_t) *(message->payload()), message->source());
#ifdef DEBUG
                  Serial.print(" j'envoie a : ");
                  Serial.println(next);
        #endif
        MessageSignal data = MessageSignal(2, address); 
        data.set_payload(message->payload_size(),message->payload());
        Tx16Request tx =  Tx16Request(next,(byte*) (&data),data.buffer_size());
        xbee.send(tx); 

      }
    } 
    else if (msg_id == 3 || msg_id == 4){ // message de donnée que je dois forwarder
#ifdef DEBUG
             if (msg_id == 3)   Serial.print(" Je forwarde un msg de données venant du nc ");
             else Serial.print(" Je forwarde un msg de fin données venant du nc ");
#endif
           MessageSignal *message = reinterpret_cast<MessageSignal*> (payload);
#ifdef DEBUG
           Serial.print(message->nc());
           Serial.print(" vers ");
#endif
      uint16_t next = getVersSink(message->nc());
#ifdef DEBUG
      Serial.println(next);
#endif
        MessageSignal data = MessageSignal(msg_id, address); 
        data.set_nc(message->nc());
        data.set_payload(message->payload_size(),message->payload()); 
        Tx16Request tx =  Tx16Request(next,(byte*) (&data),data.buffer_size());
        xbee.send(tx); 
    }
}

inline uint16_t majTab (uint16_t nc,uint16_t versSink ) { //récupére le prochain saut et enregistre la provenance pour le retour
  for (int i=0; i<nb_route;i++){    
    if (tab_route[i].nc == nc){
      tab_route[i].vers_sink = versSink;
      return tab_route[i].promoteur;
    }
    }
    return -1;
}

inline uint16_t getVersSink(uint16_t nc){
  for (int i=0; i<nb_route;i++){    
    if (tab_route[i].nc == nc){
      return tab_route[i].vers_sink;
    }
    }
    return -1;
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


