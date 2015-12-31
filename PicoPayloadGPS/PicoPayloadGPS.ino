/*
	PicoHorus Telemetry Payload, with Binary Telemetry.
	Mark Jessop <vk5qi@rfhead.net>
	2015-12-30

	With binary coding library from David Rowe <david@rowetel.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    For a full copy of the GNU General Public License, 
    see <http://www.gnu.org/licenses/>.
*/
#include <SPI.h>
#include "RF22.h"
#include <util/crc16.h>
#include <avr/wdt.h>
#include "TinyGPS.h"
#include "horus_l2.h"

// Uncomment to enable transmitting of the binary & coded data as hex via RTTY.
//#define BINARY_DEBUG

// GPIO 
#define	STATUS_LED	A3
#define BATT	2

// RFM22B IO Lines
#define RF22_SLAVESELECT	10
#define RF22_INTERRUPT		0

// RFM22B Settings
#define TX_FREQ	434.650 // TODO: Tune this a bit, so it's at least on 434.650MHz when the payload is turned on...
#define	RX_FREQ	TX_FREQ
#define TX_POWER	RF22_TXPOW_14DBM  // Options are 1,2,5,8,11,14,17,20 dBm


// Singleton instance of the RFM22B Library 
RF22 rf22(RF22_SLAVESELECT,RF22_INTERRUPT);

// tinyGPS object
TinyGPS gps;

// Variables & Buffers
char txbuffer [128];
char txbuffer2 [128];

int	rfm_temp;
int batt_mv = 0;
unsigned int count = 0;

uint8_t			hour = 0;				// 1 byte
uint8_t			minute = 0;				// 1 byte
uint8_t			second = 0;				// 1 byte
float			latitude = 0.0;			// 4 bytes
float			longitude = 0.0;		// 4 bytes
uint16_t		altitude = 0;			// 2 bytes
uint16_t		velocity = 0;			// 2 bytes
uint8_t			sats = 0;				// 1 byte

char latString[12]; 
char longString[12];

// Python Struct format: "<BHBBBffHBBbBH"
struct TBinaryPacket
{
uint8_t		PayloadID;
uint16_t	Counter;
uint8_t		Hours;
uint8_t		Minutes;
uint8_t		Seconds;
float		Latitude;
float		Longitude;
uint16_t  	Altitude;
uint8_t   Speed; // Speed in Knots (1-255 knots)
uint8_t   Sats;
int8_t   Temp; // Twos Complement Temp value.
uint8_t   BattVoltage; // 0 = 0.5v, 255 = 2.0V, linear steps in-between.
uint16_t Checksum; // CRC16-CCITT Checksum.
};  //  __attribute__ ((packed));

void setup(){
	// Setup IO pins

	wdt_reset();
	wdt_enable(WDTO_8S); // Disabling this for now.

	pinMode(STATUS_LED, OUTPUT);
	analogReference(DEFAULT);
	pinMode(A2, INPUT);
	digitalWrite(STATUS_LED, LOW);

	// Fire up RFM22B chip. If it fails, blink.
	if(!rf22.init()){
		payload_fail();
	}

	rf22.setTxPower(TX_POWER|0x08);
  	RFM22B_RTTY_Mode();
  	 	 
  	// Start up UART and configure GPS unit.
  	Serial.begin(9600);
	configNMEA();
	rtty_txstring("All OK.\n");
	
	// Initialise our Position Strings.
	dtostrf(latitude, 11, 5, latString);
	dtostrf(longitude, 11, 5, longString);
}

void loop(){
	wdt_reset();

	RFM22B_RTTY_Mode();
	
	// Collect Sensor Data
	rfm_temp = (rf22.temperatureRead( RF22_TSRANGE_M64_64C,0 ) / 2) - 64; // Internal Temp. Degrees.
	batt_mv = (int)(analogRead(BATT) * 1800.0/1024.0); // Battery Voltage, in mV
	
	// GPS DATA ACQUISITION (Poll uBlox GPS unit for data. This is pretty sloooow.)
	feedGPS(); // up to 3 second delay here

	// Process the GPS data.
	gps.f_get_position(&latitude, &longitude);
	sats = (uint8_t)gps.sats();
	altitude = (uint16_t)gps.f_altitude();
	velocity = (uint16_t)gps.f_speed_kmph();
	gps.crack_datetime(0, 0, 0, &hour, &minute, &second);

	// Only put the latitude information into the string if we have lock.
	if(sats != 0){
		dtostrf(latitude, 11, 5, latString);
		dtostrf(longitude, 11, 5, longString);
	}

	// ASCII String Generation.	
	sprintf(txbuffer, "$$$$$HORUS,%d,%02u:%02u:%02d,%s,%s,%u,%u,%u,%d,%d", count, hour, minute, second, trim(latString), trim(longString), altitude, velocity, sats,rfm_temp,batt_mv);
	sprintf(txbuffer, "%s*%04X\n", txbuffer, gps_CRC16_checksum(txbuffer));
	digitalWrite(STATUS_LED, LOW);
	wdt_reset();
	rtty_txstring(txbuffer);

	// Build and transmit binary packet.
	int pkt_len = build_binary_packet(txbuffer);

	#ifdef BINARY_DEBUG
	// Transmit the uncoded binary payload as hex.
	PrintHex(txbuffer,pkt_len,txbuffer2);
	rtty_txstring(txbuffer2);
	rtty_txbyte('\n');
	#endif

	// Encode binary payload using David Rowe's library.
	int coded_len = horus_l2_encode_tx_packet((unsigned char*)txbuffer2,(unsigned char*)txbuffer,pkt_len);

	#ifdef BINARY_DEBUG
	// Transmit the *coded* binary payload as hex.
	PrintHex(txbuffer2,coded_len,txbuffer);
	rtty_txstring(txbuffer);
	rtty_txbyte('\n');
	#endif

	// Transmit binary packet, with a short delay before it.
	delay(100);
	fsk_txarray(txbuffer2,coded_len);
	delay(100);

	// Increment packet counter.
	count++;

	// Check we can communicate with the RFM22 chip.
	// If we can't blink some LED's and let the watchdog timer reset the chip. 
	uint8_t deviceType = rf22.spiRead(RF22_REG_00_DEVICE_TYPE);
    if (   deviceType != RF22_DEVICE_TYPE_RX_TRX
        && deviceType != RF22_DEVICE_TYPE_TX){
    	payload_fail();
    }
}

int build_binary_packet(char *TxLine)
{
  struct TBinaryPacket BinaryPacket;

  BinaryPacket.PayloadID = 0x01;
  BinaryPacket.Counter = count;
  BinaryPacket.Hours = hour;
  BinaryPacket.Minutes = minute;
  BinaryPacket.Seconds = second;
  BinaryPacket.Latitude = latitude;
  BinaryPacket.Longitude = longitude;
  BinaryPacket.Altitude = altitude;
  BinaryPacket.Speed = velocity;
  BinaryPacket.BattVoltage = GetBattVoltage();
  BinaryPacket.Sats = sats;
  BinaryPacket.Temp = (int8_t)rfm_temp;

  BinaryPacket.Checksum = (uint16_t)crc16((unsigned char*)&BinaryPacket,sizeof(BinaryPacket)-2);

  memcpy(TxLine, &BinaryPacket, sizeof(BinaryPacket));
	
  return sizeof(struct TBinaryPacket);
}


void alert_sound(int loops){
	RFM22B_RTTY_Mode();
	delay(100);
	for(int i = 0; i<loops; i++){
		rf22.spiWrite(0x073, 0x00);
		delay(250);
		rf22.spiWrite(0x073, 0x02);
		delay(250);
		wdt_reset();
	}
}

// Blink both LEDs simultaneously and fast, to indicate a failure.
void payload_fail(){
	for(int i = 0; i<20; i++){
		digitalWrite(STATUS_LED, LOW);
		delay(300);
		digitalWrite(STATUS_LED, HIGH);
		delay(300);
		wdt_reset();
	}
	// Intentionally keep on blinking until the watchdog timer resets.
	while(1){
		digitalWrite(STATUS_LED, LOW);
		delay(300);
		digitalWrite(STATUS_LED, HIGH);
		delay(300);
	}
}

uint16_t gps_CRC16_checksum (char *string)
{
	size_t i;
	uint16_t crc;
	uint8_t c;
 
	crc = 0xFFFF;
 
	// Calculate checksum ignoring the first five $s
	for (i = 5; i < strlen(string); i++)
	{
		c = string[i];
		crc = _crc_xmodem_update (crc, c);
	}
 
	return crc;
}
