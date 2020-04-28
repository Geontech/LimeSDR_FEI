#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <mutex>
#include <math.h>
#include <stdlib.h>
#include <string>.
#include "lime/LimeSuite.h"

using namespace std;

//Device structure, should be initialize to NULL
lms_device_t* device = NULL;
//lms_stream_t streams[2];
lms_stream_meta_t rx_metadata;
lms_stream_meta_t tx_metadata;
lms_stream_t streams[2];

std::mutex stream_mutex;

int error()
{
    if (device != NULL)
  	LMS_Close(device);
    exit(-1);
}

int device_init(double sample_rate) {
	int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
        error();

    cout << "Devices found: " << n << endl; //print number of devices
    if (n < 1)
        return -1;

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
        error();

    //Initialize device with default configuration
    //Do not use if you want to keep existing configuration
    //Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI
    if (LMS_Init(device)!=0)
        error();
     

    //Set sample rate
    if (LMS_SetSampleRate(device, sample_rate, 0)!=0)
        error();
    cout << "Sample rate: " << sample_rate/1e6 << " MHz" << endl;
       
    return 0;
}

int rx_init(double rxgain, double frequency){

    //Enable RX channel
    //Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_RX, 0, true) != 0)
        error();

    //Set center frequency to 800 MHz
    if (LMS_SetLOFrequency(device, LMS_CH_RX, 0, frequency) != 0)
        error();

	if (LMS_SetNormalizedGain(device, LMS_CH_RX, 0, rxgain) != 0)
	    error();
		    
    //Enable test signal generation
    //To receive data from RF, remove this line or change signal to LMS_TESTSIG_NONE
    //if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NCODIV8, 0, 0) != 0)
    //    error();
        
    streams[0].channel = 0; //channel number
    streams[0].fifoSize = 1024 * 1024; //fifo size in samples
    streams[0].throughputVsLatency = 1; //optimize for max throughput
    streams[0].isTx = false; //RX channel
    streams[0].dataFmt = lms_stream_t::LMS_FMT_F32; //12-bit integers
    if (LMS_SetupStream(device, &streams[0]) != 0)
        error();                 //TX channel

    //Start streaming
    LMS_StartStream(&streams[0]);
    rx_metadata.flushPartialPacket = false; //currently has no effect in RX
    rx_metadata.waitForTimestamp = false; //currently has no effect in RX
}

int tx_init(double frequency, double sample_rate, double txgain, double bandwidth) {

    //Enable TX channel,Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, true)!=0)
        error();

    //Set center frequency
    if (LMS_SetLOFrequency(device,LMS_CH_TX, 0, frequency)!=0)
        error();
    cout << "Center frequency: " << frequency/1e6 << " MHz" << endl;
    
    //Enable test signal generation
    //To receive data from RF, remove this line or change signal to LMS_TESTSIG_NONE
    //if (LMS_SetTestSignal(device, LMS_CH_TX, 0, LMS_TESTSIG_DC, 0, 0) != 0)
    //    error();
    
    if (LMS_SetLPFBW(device, true, 0, bandwidth) != 0) 
    	error(); 
    std::cout << "LMS_SetLPFBW" << std::endl;
    
    //select TX1_1 antenna
    if (LMS_SetAntenna(device, LMS_CH_TX, 0, LMS_PATH_TX2)!=0)
        error();

    //set TX gain
    if (LMS_SetNormalizedGain(device, LMS_CH_TX, 0, txgain) != 0)
        error();

    //calibrate Tx, continue on failure
    LMS_Calibrate(device, LMS_CH_TX, 0, sample_rate, 0);
    
	streams[1].channel = 0;//channel number
    streams[1].fifoSize = 1024*1024;          //fifo size in samples
    streams[1].throughputVsLatency = 1;    //0 min latency, 1 max throughput
    streams[1].dataFmt = lms_stream_t::LMS_FMT_F32; //floating point samples
    streams[1].isTx = true; 
    LMS_SetupStream(device, &streams[1]);
    tx_metadata.flushPartialPacket = false; //do not force sending of incomplete packet
    tx_metadata.waitForTimestamp = true; //Enable synchronization to HW timestamp
}

void RX(bool stats) {
    //Initialize data buffers
    const int buffer_size = 1024*8;
    float buffer[buffer_size * 2]; //buffer to hold complex values (2*samples))
    
    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = t1;
    while (true) //run for 5 seconds
    {
        //Receive samples
        int samplesRead = LMS_RecvStream(&streams[0], buffer, buffer_size, &rx_metadata, 1000);
	//I and Q samples are interleaved in buffer: IQIQIQ...
        //printf("Received %d samples\n", samplesRead);
        
        for (int i = 0; i < samplesRead; i++) {
		    if (true/*buffer[i] > .1 || buffer[i] < -.1*/) {
				if (i%2 == 0) {
					std::cout << "IMAG: " << i << " " << buffer[i] << std::endl;
			   	}
			   	else {
					std::cout << "REAL: " << i << " " << buffer[i] << std::endl;
			   	}
		   	}
        }
        //Print data rate (once per second)
        if (chrono::high_resolution_clock::now() - t2 > chrono::seconds(1) && stats)
        {
            t2 = chrono::high_resolution_clock::now();
            lms_stream_status_t status;
            LMS_GetStreamStatus(&streams[0], &status);  //Get stream status
            cout << "RX data rate: " << status.linkRate / 1e6 << " MB/s\n"; //link data rate
        }
	/*
		INSERT CODE FOR PROCESSING RECEIVED SAMPLES
	*/
	}
	
    //Stop streaming
    LMS_StopStream(&streams[0]); //stream is stopped but can be started again with LMS_StartStream()
    LMS_DestroyStream(device, &streams[0]); //stream is deallocated and can no longer be used

}

void TX(double f_ratio, double tone_freq, bool stats) {
    const int buffer_size = 1024*8;
	float tx_buffer[2*buffer_size];     //buffer to hold complex values (2*samples))
    for (int i = 0; i <buffer_size; i++) {      //generate TX tone
        const double pi = acos(-1);
        double w = 2*pi*i*f_ratio;
        tx_buffer[2*i] = cos(w);
        tx_buffer[2*i+1] = sin(w);
    }   
    cout << "Tx tone frequency: " << tone_freq/1e6 << " MHz" << endl;

    const int send_cnt = int(buffer_size*f_ratio) / f_ratio; 
    cout << "sample count per send call: " << send_cnt << std::endl;
    
    sleep(5);
    LMS_StartStream(&streams[1]);         //Start streaming
    std::cout << "Started stream" << std::endl;
    //Streaming
    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = t1;
    while (true) //run for 10 seconds
    {
        //Transmit samples
        int ret = LMS_SendStream(&streams[1], tx_buffer, send_cnt, nullptr, 1000);
        if (ret != send_cnt) {
            cout << "error: samples sent: " << ret << "/" << send_cnt << endl;
            sleep(1);
        }
        //Print data rate (once per second)
        if (chrono::high_resolution_clock::now() - t2 > chrono::seconds(1) && stats)
        {
            t2 = chrono::high_resolution_clock::now();
            lms_stream_status_t status;
            LMS_GetStreamStatus(&streams[1], &status);  //Get stream status
            cout << "TX data rate: " << status.linkRate / 1e6 << " MB/s\n"; //link data rate
        }
    }
	
	
    //Stop streaming
    LMS_StopStream(&streams[1]);
    LMS_DestroyStream(device, &streams[1]);

    //Disable TX channel
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, false)!=0) {
        error();
    }
}

int main(int argc, char *argv[]) {
	std::string::size_type sz;
	double rxgain, txgain;
	bool stats = true;
	
	if (argc != 4) {
		std::cout << "usage: " << argv[0] << " <rxgain> <txgain> <stats>" << std::endl;
		return 0;
	}
	else {
		rxgain = std::atof(argv[1]);
		txgain = std::atof(argv[2]);
		stats = std::atoi(argv[3]);
	}

	const double bandwidth = 10e6;
	const double frequency = 200e6;  //center frequency to 500 MHz
    const double sample_rate = 5e6;    //sample rate to 5 MHz
    const double tone_freq = 1e6; //tone frequency
    const double f_ratio = tone_freq/sample_rate;
	if(device_init(sample_rate)) {
		error();
		return 255;
	}
	
	//rx_init(rxgain, frequency);
	tx_init(frequency, sample_rate, txgain, bandwidth);
	
	
	//std::thread rx (RX, stats);
	std::thread tx (TX, f_ratio, tone_freq, stats);
	
	std::cout << "Started RX and TX" << std::endl;
	
	//rx.join();
	tx.join();

    //Close device
    if (LMS_Close(device)==0) {
        cout << "Closed" << endl;
	}
	return 0;
}
