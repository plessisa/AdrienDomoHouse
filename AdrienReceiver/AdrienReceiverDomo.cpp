/**
 * Example RF Radio Receiver
 *
 * This is an example of how to use the RF24 class on RPi, communicating to an Arduino running
 * the receiver sketch.
sudo ln -s /etc/openhab/AdrienReceiver/client5/AdrienReceiverDomo /etc/init.d/
 */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <RF24/RF24.h>
#include <mosquitto.h>

/* Log file definition */
#define LOGFILE "/var/log/AdrienReceiverDomno.log"

/* How many seconds the broker should wait between sending out
 * keep-alive messages. */
#define KEEPALIVE_SECONDS 60

/* Hostname and port for the MQTT broker. */
#define BROKER_HOSTNAME "localhost"
#define BROKER_PORT 1883

#define RETRY_MSQTT 15
#define RETRY_MSQTT_PERIODE 1
#define RETRY_MSQTT_SLEEP_PERIODE 500


/* Settings for destination */
const int NbFilPilote = 5;

struct client_info {
    struct mosquitto *m;
};

using namespace std;

// This are related to the communication proto
const int MaxSizePayload = 8;
const int MaxSizeDatagrame = 10;
const char TEMP = 'T';
const char HUMI = 'H';
const char HCHP = 'P';
const char HCHC = 'C';
const char CONSO = 'S';
const char RADIA = 'R';
const char MOVE = 'M';

// This are related to the radio nRF24l01
#define CE_PIN 22
RF24 WirelessData(22,0); // Replace by CE_PIN
const uint64_t pipes[7] = {0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL, 0xF0F0F0F0E1LL, 0xF0F0F0F0F0LL };
const int ChannelUnused = 0;
const int ChannelTemp1 = 1;
const int ChannelTemp2 = 2;
const int ChannelTemp3 = 3;
const int ChannelTeleInfo = 4;
const int ChannelFilPilote = 5;

static struct mosquitto *init(struct client_info *info);
static bool set_callbacks(struct mosquitto *m);
static bool connect(struct mosquitto *m);
static void Cleanup(struct mosquitto *m);
static void Log (char *message);
static bool FileExists(const std::string& name);


int DebugState = false;

int main(int argc, char** argv)
{
	char LogMessage[64];
	sprintf(LogMessage,"Starting Adrien Domo script");
	Log(LogMessage);

	if (argc == 2){
		if (strcmp(argv[1],"-d") == 0){
			DebugState = true;
		}
	}
	// Initiate the MQTT part
	int rc = 0;
	int run = 1;
	int sleepmode = 0;
	struct client_info info;
	memset(&info, 0, sizeof(info));
    	struct mosquitto *m = init(&info);
	if (m == NULL) {
		Log("init() failure");
    		exit(1);
	}
    	info.m = m;
	if (!set_callbacks(m)) {
		Log("set_callbacks() failure\n");
		exit(1);
	}
	while (!connect(m)) {
		Log("connect() failure\n");
		if(sleepmode < RETRY_MSQTT){
			sleep(RETRY_MSQTT_PERIODE);
			sleepmode++;
		} else {
			sleep(RETRY_MSQTT_SLEEP_PERIODE);
		}
	}
	// From here, MQTT if fully configured and ready


	// Initiate the RF24 part
	WirelessData.begin();
	WirelessData.setRetries(5,15);
	WirelessData.openReadingPipe(ChannelTemp1,pipes[ChannelTemp1]);
	WirelessData.openReadingPipe(ChannelTemp2,pipes[ChannelTemp2]);
	WirelessData.openReadingPipe(ChannelTemp3,pipes[ChannelTemp3]);
	WirelessData.openReadingPipe(ChannelTeleInfo,pipes[ChannelTeleInfo]);
	WirelessData.openReadingPipe(ChannelFilPilote,pipes[ChannelFilPilote]);
	WirelessData.setAutoAck(ChannelTemp1,true);
	WirelessData.setAutoAck(ChannelTemp2,true);
	WirelessData.setAutoAck(ChannelTemp3,true);
	WirelessData.setAutoAck(ChannelTeleInfo,true);
	WirelessData.setAutoAck(ChannelFilPilote,true);	
	WirelessData.startListening();
	if (DebugState){
		WirelessData.printDetails();
	}
	// From here, RF24 if fully configured and ready


	// forever loop
	while(run){
		// Start to watch for MQTT update
		rc = mosquitto_loop(m, -1, 1);
		if(run && rc){
			char LogMessage[256];
			sprintf(LogMessage,"Got a fail while running MQTT loop. Error: %d, ErrorCode: %d", sleepmode, rc);
			Log(LogMessage);
			if(sleepmode < RETRY_MSQTT){
				sleep(RETRY_MSQTT_PERIODE);
				sleepmode++;
			} else {
				sleep(RETRY_MSQTT_SLEEP_PERIODE);
			}
			mosquitto_reconnect(m);
		} else {
			sleepmode=0;
			// Stop to watch for MQTT update

			// Start to watch for RF24 update
			uint8_t pipe_num;
			char DatagramRx[10] = {0};
			if ( WirelessData.available(&pipe_num) )
			{
				bool done = false;
				while (!done)
				{
					WirelessData.read(&DatagramRx, sizeof(DatagramRx));
					done = true;
				}
  				char* resultString = strchr(DatagramRx,' ') + 1;
				int sz = 32;
				char topic[sz];
				if (sz < snprintf(topic, sz, "/home/%u/%c/%c", pipe_num, DatagramRx[0], DatagramRx[1])) {
					Log("error building MQTT topic");
					break;
				}
				size_t payload_sz = 32;
				char payload[payload_sz];
				size_t payloadlen = 0;
				payloadlen = snprintf(payload, payload_sz, "%s", resultString);
				if (payload_sz < payloadlen) {
					Log("error building MQTT payload");
					break;;
				}
				if (DebugState) {
					char LogMessage1[256];
					sprintf(LogMessage1,"From Channel: %u, Data: %s", pipe_num, DatagramRx);
					Log(LogMessage1);

					char LogMessage2[256];
					sprintf(LogMessage2,"A->O Send MQTT message. Arduino %u, type: %c (%s), data %s",pipe_num, DatagramRx[0], topic, payload);
					Log(LogMessage2);
				}
				int res = mosquitto_publish(m, NULL, topic, payloadlen, payload, 0, false);
				if (res != MOSQ_ERR_SUCCESS) {
					Log("error while MQTT publish");
					break;
				}
			}
			// Stop to watch for RF24 update
		}
	}
	Cleanup(m);
}

/* Connect to the network. */
static bool connect(struct mosquitto *m) {
    int res = mosquitto_connect(m, BROKER_HOSTNAME, BROKER_PORT, KEEPALIVE_SECONDS);
    return res == MOSQ_ERR_SUCCESS;
}

/* Initialize a mosquitto client. */
static struct mosquitto *init(struct client_info *info) {
    void *udata = (void *)info;
    int buf_sz = 32;
    char buf[buf_sz];
    if (buf_sz < snprintf(buf, buf_sz, "client_%d", getpid())) {
        return NULL;            /* snprintf buffer failure */
    }
    /* Create a new mosquitto client, with the name "client_#{PID}". */
    struct mosquitto *m = mosquitto_new(buf, true, udata);

    return m;
}

/* Callback for successful connection: add subscriptions. */
static void on_connect(struct mosquitto *m, void *udata, int res) {
    if (res == 0) {             /* success */
		// Subscribe the the arduino 5, all equipment, Action
		char MyPatern[32];
		for (int i=1; i<=NbFilPilote; i++) {
			sprintf(MyPatern, "/home/%d/A/%d",ChannelFilPilote, i);
			if (DebugState) {
				char LogMessage[256];
				sprintf(LogMessage,"Lets subscribe for: %s", MyPatern);
				Log(LogMessage);
			}
			mosquitto_subscribe(m, NULL, MyPatern, 0);
		}
		// Subscribe for antoher type
        //mosquitto_subscribe(m, NULL, "tuck", 0);
    } else {
        Log("connection refused");
	exit(1);
    }
}

/* A message was successfully published. */
static void on_publish(struct mosquitto *m, void *udata, int m_id) {
	if(DebugState) {
   		Log("-- published successfully");
	}
}

/* Handle a message that just arrived via one of the subscriptions. */
static void on_message(struct mosquitto *m, void *udata, const struct mosquitto_message *msg) {
	// Cleaning
    if (msg == NULL) {
		Log("Got a null message");
		return;
	}

	// Prepare to extract info from the Address ex: /home/5/1/A
	char MyPaternRx[64];
	int len = sprintf(MyPaternRx, "%s", msg->topic);
	if (len!=11) {
		Log("Wrong address for the equipment");
		return;
	}
	// Prepare to extract info from the Payload ex: 1
	char MyPayload[1];
	int len2 = sprintf(MyPayload, "%s", msg->payload);
	if (len2!=1) {
		Log("Wrong payload for the equipment");
		return;
	}
	// Preapre the pipe
	int Arduino = MyPaternRx[6] - '0';

	if(DebugState) {
		char LogMessage[256];
		sprintf(LogMessage,"O->A Received MQTT message. Arduino %d, Equipment %c(%s), Action %c",Arduino, MyPaternRx[10], MyPaternRx, MyPayload[0]);
		Log(LogMessage);
	}

	// Transmit the message on RF24
	WirelessData.stopListening();
	WirelessData.openWritingPipe(pipes[Arduino]);
	WirelessData.setAutoAck(Arduino,true);
	char DatagramTx[3] = {0};
	sprintf(DatagramTx, "%c %c", MyPaternRx[10], MyPayload[0]);
	bool res = WirelessData.write(DatagramTx, sizeof(DatagramTx));

	if (res) {
		if(DebugState) {
			Log("-- Succefully transmit the data");
		}
	} else {
                char LogMessage[256];
                sprintf(LogMessage,"--Failed transmit the data to Arduino %d, Equipment %c(%s), Action %c",Arduino, MyPaternRx[10], MyPaternRx, MyPayload[0]);
                Log(LogMessage);
        	WirelessData.startListening();
		delay(1000);
	        WirelessData.stopListening();
	        WirelessData.openWritingPipe(pipes[Arduino]);
	        WirelessData.setAutoAck(Arduino,true);
 		res = WirelessData.write(DatagramTx, sizeof(DatagramTx));
	        if (res) {
      	        	Log("-- Succefully Retransmit the data");
        	} else {
                	sprintf(LogMessage,"--Failed retransmit the data to Arduino %d, Equipment %c(%s), Action %c",Arduino, MyPaternRx[10], MyPaternRx, MyPayload[0]);
                	Log(LogMessage);
        	}
	}
	WirelessData.startListening();
	delay(1000);
}

/* Successful subscription hook. */
static void on_subscribe(struct mosquitto *m, void *udata, int mid, int qos_count, const int *granted_qos) {
	if(DebugState) {
    		Log("-- subscribed successfully");
	}
}

/* Register the callbacks that the mosquitto connection will use. */
static bool set_callbacks(struct mosquitto *m) {
    mosquitto_connect_callback_set(m, on_connect);
    mosquitto_publish_callback_set(m, on_publish);
    mosquitto_subscribe_callback_set(m, on_subscribe);
    mosquitto_message_callback_set(m, on_message);
    return true;
}

static void Cleanup(struct mosquitto *m) {
	mosquitto_destroy(m);
	(void)mosquitto_lib_cleanup();
}

static void Log (char *message){
	FILE *file;
	char buff[20];
    	struct tm *sTm;

	if (!FileExists(LOGFILE)) {
		file = fopen(LOGFILE, "w");
	}
	else
		file = fopen(LOGFILE, "a");

	if (file == NULL) {
		return;
	}
	else
	{
    		time_t now = time (0);
    		sTm = localtime (&now);
		strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", sTm);
	    	fprintf (file, "%s -> %s\n", buff, message);
		fclose(file);
	}
//	if (file)
//		fclose(file);

}

static bool FileExists(const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}
