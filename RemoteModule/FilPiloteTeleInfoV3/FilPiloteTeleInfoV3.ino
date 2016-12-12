#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "Transmiter.h"
#include <SoftwareSerial.h>

//#define debug

/* PIN USED :
  PIN D02  : Serial RX2
  PIN D03  : Fil Pilote 1
  PIN D04  : Fil Pilote 2
  PIN D05  : Fil Pilote 3
  PIN D06  : Fil Pilote 4
  PIN D07  : Led conso peak
  PIN D08  : Relai 1
*/

int ScrVer = 3;

// Fil Pilote
#define FP1 3
#define FP2 4
#define FP3 5
#define FP4 6
#define RE1 8
// Teleinfo
#define RXPIN 2
#define LEDHC 7

/*
 * Init the value the teleinfo
 */
SoftwareSerial TeleInfoSerial(RXPIN,4);
#define startFrame 0x02 //Caractère de début de trame
#define endFrame 0x03 //Caractère de fin de trame
#define startLine 0x0A //Caractère de début de ligne
#define endLine 0x0D //Caractère de fin de ligne
#define maxFrameLen 280
int ConsoMaxHP = 3000;
int ConsoMaxHC = 8000;
char HHPHC;
int ISOUSC;             // intensite souscrite  
int IINST;              // intensite instantanee en A
int IMAX;               // intensite maxi en A
int PAPP;               // puissance apparente en VA
unsigned long HCHC;  // compteur Heures Creuses en W
unsigned long HCHP;  // compteur Heures Pleines en W
String PTEC;            // Regime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
String ADCO;            // adresse compteur
String OPTARIF;         // option tarifaire
String MOTDETAT;        // status word
int i;


int Cycle;


/*
 * Init the value the fil pilote
 */
int StateFP1;
int StateFP2;
int StateFP3;
int StateFP4;
int StateRE1;

void setup() {
  //Ouverture de la liaison série pour le debug
  Serial.begin(9600);
  delay(1000);
  Serial.print("Fil Pilote sketch v");
  Serial.print(ScrVer);
  Serial.print(", libAdrien v");
  Serial.println (LibVer);
  delay(1000);

  Cycle = 0;
  
  //Ouverture de la liaison série pour la TeleInfo
  TeleInfoSerial.begin(1200);
  // Initialization des Led Teleinfo
  pinMode(LEDHC, OUTPUT);
  digitalWrite(LEDHC, LOW);
  i=0;


  //Ouverture de la liaison COM FilPilote
  RadioStart(ChannelFilPilote);
  WirelessData.openReadingPipe(1,pipes[ChannelFilPilote]);
  WirelessData.setAutoAck(1,true);
  WirelessData.startListening();
  WirelessData.printDetails();
  // Initialization des Led FilPilote
  StateFP1 = 0;
  StateFP2 = 0;
  StateFP3 = 0;
  StateFP4 = 0;
  StateRE1 = 0;
  pinMode(FP1, OUTPUT);
  pinMode(FP2, OUTPUT);
  pinMode(FP3, OUTPUT);
  pinMode(FP4, OUTPUT);
  pinMode(RE1, OUTPUT);
  digitalWrite(FP1, HIGH);
  digitalWrite(FP2, HIGH);
  digitalWrite(FP3, HIGH);
  digitalWrite(FP4, HIGH);
  digitalWrite(RE1, HIGH);

}

void loop() {
  uint8_t pipe_num;
  bool DataReceived = false;
  char DatagramRx[3] = {0};

  if (WirelessData.available(&pipe_num))
  {
#ifdef debug
      Serial.println("Got a message about FP");
#endif
      bool done = false;
      while (!done)
      {
        done = WirelessData.read(&DatagramRx, sizeof(DatagramRx));
      }
      HandleDataRx(DatagramRx, pipe_num);
      DataReceived = true;
  }
  Cycle++;

  // 6000 times 10ms => 60 sec
  if (Cycle > 6000) {
#ifdef debug
    Serial.println("Check Tele");
#endif
    //Run the Teleinfo
    InitValue();
    boolean teleInfoReceived = readTeleInfo();
    if(teleInfoReceived)
   {
      displayTeleInfo();
      HandleLed();
      SendTeleInfo();
      Cycle = 0;
    }
  }
  delay(10);
}

void SendTeleInfo() {
  //Ouverture de la liaison radio
  RadioStart(ChannelTeleInfo);
  
  RadioTransmit(CONSO, 1, PAPP);
  delay(100); 
  RadioTransmit(HC, 1, HCHC);
  delay(100); 
  RadioTransmit(HP, 1, HCHP);
}
void HandleLed(){
  if (PTEC == "HP.."){
    digitalWrite(LEDHC, LOW);
  } else if (PTEC == "HC.."){
    digitalWrite(LEDHC, HIGH);
  } else {
    digitalWrite(LEDHC, LOW);
  }
}

void InitValue(){
  // variables initializations
  ADCO = "270622224349";
  OPTARIF = "----";
  ISOUSC = 0;
  HCHC = 0L;  // compteur Heures Creuses en W
  HCHP = 0L;  // compteur Heures Pleines en W
  PTEC = "----";    // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
  HHPHC = '-';
  IINST = 0;        // intensité instantanée en A
  IMAX = 0;         // intensité maxi en A
  PAPP = 0;         // puissance apparente en VA
  MOTDETAT = "------";
}

//=================================================================================================================
// Capture des trames de Teleinfo
//=================================================================================================================
boolean readTeleInfo()
{
  int comptChar=0; // variable de comptage des caractères reçus 
  char charIn=0; // variable de mémorisation du caractère courant en réception

  char bufferTeleinfo[21] = "";
  int bufferLen = 0;
  int checkSum;

  int sequenceNumnber= 0;    // number of information group

  //--- wait for starting frame character 
  while (charIn != startFrame)
  { // "Start Text" STX (002 h) is the beginning of the frame
    if (TeleInfoSerial.available())
      charIn = TeleInfoSerial.read() & 0x7F; // Serial.read() vide buffer au fur et à mesure
  }
  //  while (charIn != endFrame and comptChar<=maxFrameLen)
  while (charIn != endFrame)
  { // tant que des octets sont disponibles en lecture : on lit les caractères
    if (TeleInfoSerial.available())
    {
      charIn = TeleInfoSerial.read() & 0x7F;
      // incrémente le compteur de caractère reçus
      comptChar++;
      if (charIn == startLine)
        bufferLen = 0;
      bufferTeleinfo[bufferLen] = charIn;
      // on utilise une limite max pour éviter String trop long en cas erreur réception
      // ajoute le caractère reçu au String pour les N premiers caractères
      if (charIn == endLine)
      {
        checkSum = bufferTeleinfo[bufferLen -1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum)
        {// we clear the 1st character
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen -3);
          bufferTeleinfo[bufferLen -3] =  0x00;
          sequenceNumnber++;
          if (! handleBuffer(bufferTeleinfo, sequenceNumnber))
          {
            Serial.println("Sequence error...");
            return false;
          }
        }
        else
        {
          Serial.println("Checksum error... Corrupt data");
          return false;
        }
      }
      else
        bufferLen++;
    }
    if (comptChar > maxFrameLen)
    {
      Serial.println("Overflow error ...");
      return false;
    }
  }
  return true;
}

//=================================================================================================================
// Calculates teleinfo Checksum
//=================================================================================================================
char chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
  for (i=1; i<(len-2); i++) 
    sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return(sum);
}

//=================================================================================================================
// Frame parsing
//=================================================================================================================
//void handleBuffer(char *bufferTeleinfo, uint8_t len)
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber)
{
  // create a pointer to the first char after the space
  char* resultString = strchr(bufferTeleinfo,' ') + 1;
  boolean sequenceIsOK;
  
  switch(sequenceNumnber)
  {
  case 1:
    if (sequenceIsOK = bufferTeleinfo[0]=='A')
      ADCO = String(resultString);
    break;
  case 2:
    if (sequenceIsOK = bufferTeleinfo[0]=='O')
      OPTARIF = String(resultString);
    break;
  case 3:
    if (sequenceIsOK = bufferTeleinfo[1]=='S')
      ISOUSC = atol(resultString);
    break;
  case 4:
    if (sequenceIsOK = bufferTeleinfo[3]=='C')
      HCHC = atol(resultString);
      HCHC = HCHC/1000;
    break;
  case 5:
    if (sequenceIsOK = bufferTeleinfo[3]=='P')
      HCHP = atol(resultString);
      HCHP = HCHP/1000;
    break;
  case 6:
    if (sequenceIsOK = bufferTeleinfo[1]=='T')
      PTEC = String(resultString);
    break;
  case 7:
    if (sequenceIsOK = bufferTeleinfo[1]=='I')
      IINST =atol(resultString);
    break;
  case 8:
    if (sequenceIsOK = bufferTeleinfo[1]=='M')
      IMAX =atol(resultString);
    break;
  case 9:
    if (sequenceIsOK = bufferTeleinfo[1]=='A')
      PAPP =atol(resultString);
    break;
  case 10:
    if (sequenceIsOK = bufferTeleinfo[1]=='H')
      HHPHC = resultString[0];
    break;
  case 11:
    if (sequenceIsOK = bufferTeleinfo[1]=='O')
      MOTDETAT = String(resultString);
    break;
  }
#ifdef debug
  if(!sequenceIsOK)
  {
    Serial.print("Out of sequence...");
    Serial.println(bufferTeleinfo);
  }
#endif
  return sequenceIsOK;
}

//=================================================================================================================
// This function displays the TeleInfo Internal counters
// It's usefull for debug purpose
//=================================================================================================================
void displayTeleInfo()
{
#ifdef debug
  Serial.print(F(" "));
  Serial.println();
  Serial.print("Sequence ");
  Serial.println(i);
  Serial.print("ADCO ");
  Serial.println(ADCO);
  Serial.print("OPTARIF ");
  Serial.println(OPTARIF);
  Serial.print("ISOUSC ");
  Serial.println(ISOUSC);
  Serial.print("HCHC ");
  Serial.println(HCHC);
  Serial.print("HCHP ");
  Serial.println(HCHP);
  Serial.print("PTEC ");
  Serial.println(PTEC);
  Serial.print("IINST ");
  Serial.println(IINST);
  Serial.print("IMAX ");
  Serial.println(IMAX);
  Serial.print("PAPP ");
  Serial.println(PAPP);
  Serial.print("HHPHC ");
  Serial.println(HHPHC);
  Serial.print("MOTDETAT ");
  Serial.println(MOTDETAT);
#endif
}

void HandleDataRx(char DataRx[], int Pipe)
{
#ifdef debug
  char* resultString = strchr(DataRx,' ') + 1;
  Serial.print("Fil Pilote ");
  Serial.print(DataRx[0]);
  Serial.print(", value: ");
  Serial.println(String(resultString));
#endif
  
  if (DataRx[0]=='1')
  {
    if (DataRx[2] == '1') {
      digitalWrite(FP1, LOW);
      StateFP1 = 1;
    } else {
      digitalWrite(FP1, HIGH);
      StateFP1 = 0;
    }
    RadioTransmit(RADIA, 1, StateFP1);
  }
  else if (DataRx[0]=='2')
  {
    if (DataRx[2] == '1') {
      digitalWrite(FP2, LOW);
      StateFP2 = 1;
    } else {
      digitalWrite(FP2, HIGH);
      StateFP2 = 0;
    }
    RadioTransmit(RADIA, 2, StateFP2);
  }
  else if (DataRx[0]=='3')
  {
    if (DataRx[2] == '1') {
      digitalWrite(FP3, LOW);
      StateFP3 = 1;
    } else {
      digitalWrite(FP3, HIGH);
      StateFP3 = 0;
    }
    RadioTransmit(RADIA, 3, StateFP3);
  }
  else if (DataRx[0]=='4')
  {
    if (DataRx[2] == '1') {
      digitalWrite(FP4, LOW);
      StateFP4 = 1;
    } else {
      digitalWrite(FP4, HIGH);
      StateFP4 = 0;
    }
    RadioTransmit(RADIA, 4, StateFP4);
  }
  else if (DataRx[0]=='5')
  {
    if (DataRx[2] == '1') {
      digitalWrite(RE1, LOW);
      StateFP4 = 1;
    } else {
      digitalWrite(RE1, HIGH);
      StateFP4 = 0;
    }
    RadioTransmit(RADIA, 5, StateRE1);
  }
}


/*
 * Example de trame
 * 

START 
Line: ADCO 020828500754 @
Line: OPTARIF HC.. <
Line: ISOUSC 45 ?
Line: HCHC 014422192 _
Line: HCHP 021519491 3
Line: PTEC HP..  
Line: IINST 002 Y
Line: IMAX 042 E
Line: PAPP 00400 %
Line: HHPHC D /
Line: MOTDETAT 000000 B
STOP

 */

