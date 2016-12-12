//#define debug
int LibVer =8;

// This are related to the communication proto
const int MaxSizePayload = 8;
const int MaxSizeDatagrame = 10;
const char TEMP = 'T';
const char HUMI = 'H';
const char HP = 'P';
const char HC = 'C';
const char CONSO = 'S';
const char RADIA = 'R';

// This are related to the radio nRF24l01
#define CE_PIN 9
#define CSN_PIN 10
const uint64_t pipes[7] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL, 0xF0F0F0F0E1LL, 0xF0F0F0F0F0LL };
RF24 WirelessData(CE_PIN, CSN_PIN);
const int ChannelUnused = 0;
const int ChannelTemp1 = 1;
const int ChannelTemp2 = 2;
const int ChannelTemp3 = 3;
const int ChannelTeleInfo = 4;
const int ChannelFilPilote = 5;

#ifdef debug
  int TxFailed;
  int TxSuccess;
  int TxTotal;
#endif

void RadioStart(int ChannelUsed) {
  WirelessData.begin();
  WirelessData.setRetries(5,15);
  WirelessData.openWritingPipe(pipes[ChannelUsed]);
  WirelessData.setAutoAck(1);
  WirelessData.stopListening();

#ifdef debug
  TxFailed = 0;
  TxSuccess = 0;
  TxTotal = 0;
#endif
}

void StatTx(bool ResultTx, unsigned long timeStart) {
  #ifdef debug
    TxTotal++;
    if(!ResultTx) {
      TxFailed++;
      Serial.print("packet delivery failed. Failure rate: ");
      Serial.print((TxFailed*100)/TxTotal);
      Serial.println("%.");
    } else {
      TxSuccess++;
      unsigned long time2 = micros();
      time2 = time2 - timeStart;
      Serial.print("Time from message sent to receive Ack packet: ");
      Serial.print(time2);
      Serial.print(" . Success rate: ");
      Serial.print((TxSuccess*100)/TxTotal);
      Serial.println("%.");
    }
  #endif
}

void FinalTx (char DataTx[], int SizeTx)
{
#ifdef debug
  Serial.print("Datagram Transmit(");
  Serial.print(SizeTx, DEC);
  Serial.print("): ");
  Serial.println(DataTx);
#endif

  WirelessData.stopListening();
  unsigned long time1 = micros();
  bool Result = WirelessData.write(DataTx, SizeTx);
  StatTx(Result, time1);
  WirelessData.startListening();
}

void RadioTransmit(char Type, int ID, float flValueTx) {
  String strValueTx;
  char caValueTx[MaxSizePayload];
  char ReadyToTx[MaxSizeDatagrame];
  
  strValueTx = String(flValueTx); //converting integer into a string
  strValueTx.toCharArray(caValueTx,MaxSizePayload); //passing the value of the string to the character array
  sprintf(ReadyToTx,"%c%d %s",Type,ID,caValueTx);
  FinalTx(ReadyToTx, sizeof(ReadyToTx));
}

void RadioTransmit(char Type, int ID, unsigned long loValueTx) {
  String strValueTx;
  char caValueTx[MaxSizePayload];
  char ReadyToTx[MaxSizeDatagrame];
  
  strValueTx = String(loValueTx); //converting integer into a string
  strValueTx.toCharArray(caValueTx,MaxSizePayload); //passing the value of the string to the character array
  sprintf(ReadyToTx,"%c%d %s",Type,ID,caValueTx);
  FinalTx(ReadyToTx, sizeof(ReadyToTx));
}

void RadioTransmit(char Type, int ID, int iValueTx) {
  String strValueTx;
  char caValueTx[MaxSizePayload];
  char ReadyToTx[MaxSizeDatagrame];
  
  strValueTx = String(iValueTx); //converting integer into a string
  strValueTx.toCharArray(caValueTx,MaxSizePayload); //passing the value of the string to the character array
  sprintf(ReadyToTx,"%c%d %s",Type,ID,caValueTx);
  FinalTx(ReadyToTx, sizeof(ReadyToTx));
}
