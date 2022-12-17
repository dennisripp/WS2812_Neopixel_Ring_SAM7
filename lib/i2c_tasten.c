//#include "kernel.h"
//#include "kernel_id.h"
//#include "ecrobot_interface.h"
#include "i2c.h"


#define LOWSPEED_9V 1
#define LOWSPEED    2

// Tastefeld             +-------------------------------------------+
// +---------------+     |               PCA 8574A                   |
// | 6	O  	  O	12 |     | P7  P6  P5  P4   P3     P2     P1     P0  |
// | 5	O  	  O	11 |     +-------------------------------------------+
// | -----         |           |   |   |    |      |      |      |
// | 4	O  	  O	10 |           |   |   +------------------------------
// | 3	O  	  O	9  |           |   |        |/ 1   |/ 2   |/ 3   |/ 4
// |         ----- |           |   +----------------------------------
// | 2  O  	  O	8  |           |            |/ 5   |/ 6   |/ 7   |/ 8
// | 1  O  	  O	7  |           +--------------------------------------
// |    +-----+    |                        |/ 9   |/ 10  |/ 11  |/ 12
// +----+-----+----+
//Ansterung
// 0. 1 Zeile auf 0 setzen (0x6F)
// 1. Wert zurücklesen und unterste 4 Bit auswerten (Tasten 1..4)
// 2. 2 Zeile auf 0 setzen (0x5F)
// 3. Wert zurücklesen und unterste 4 Bit auswerten (Tasten 5..8)
// 4. 3 Zeile auf 0 setzen (0x3F)
// 5. Wert zurücklesen und unterste 4 Bit auswerten (Tasten 9..12)
//Note
//- Senden/Lesen eines Befehls über den i2c-Bus dauert ca 10ms

const unsigned char matrix[]=
{
	0x6F,	 //Reihe 1	 0110 1111
	0x5F,	 //Reihe 2	 0101 1111
	0x3F     //Reihe 3   0011 1111
};

static unsigned char tast_state=0;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/**
@brief
@details   Initialisierung der Taster
           d.h. Initialisierung der entsprechenden Schnittstelle als I2C Schnittstelle (LowSpeed)
@param     int port  --> NXT_PORT_Sx
@return
@note      Keine Mehrfachinitierung auf unterschiedlichen Ports möglich
           Es kann max. ein Tastenblock je NXT Baustein verwendet werden
*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void tast_init(int port)
{
	nxt_avr_set_input_power(port, LOWSPEED);
	i2c_enable(port);
	
	tast_state=0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/**
@brief
@details   Abfrage der 4 Tasten einer Zeile
@param     int port    --> NXT_PORT_Sx
@param     uint16_t *tasten --> Zeiger auf Tastenvariable
@return
@note      Durchlaufzeit der Routine: ca 10ms
*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void tast_abfrage(int port,int &tasten)
{
	byte inI2Ccmd[];
	byte outbuf[];
	int  count=1;

	if((tast_state & 0x01) == 0x00)  //Test auf Gerade Zahl --> Aktuelle Zeile senden
	{
	//	ecrobot_send_i2c(port, 0x40, matrix[tast_state>>1] ,0,0);
		ecrobot_send_i2c(port, 0x20, matrix[tast_state>>1] ,0,0);
		
		while(i2c_busy(port_id) == 0);
		//i2c_start_transaction(port, 0x40, i2c_reg, 1, buf, len, 1);
		i2c_start_transaction(port, 0x20, i2c_reg, 1, buf, len, 1);
		
	}
	else                             //Ungerade Zahl -> Spaltenstatus zurücklesen
	{
		while(i2c_busy(port_id) == 0);
		i2c_start_transaction(port, address, i2c_reg, 1, buf, 1, 0);

		ArrayInit(inI2Ccmd, 0, 1);  // set the buffer to hold 2 values (initially all are zero)
		inI2Ccmd[0] = 0x40;         //I2C Adresse des  PCA 8574A
		I2CBytes(port, inI2Ccmd, count, outbuf);

		//Bestimmung der 4 Tastenpositionen innerhalb des 12-Bit Bitmaske
		unsigned char shift = (taste_state & 0x06)*2;
		//5 -> 8    101  100
		//3 -> 4    011  010
		//1 -> 0    001  000

		if( (outbuf[0] & 0x01) == 0x00)
			tasten |=  (1<<shift);
		else
			tasten &= ~(1<<shift);

		if( (outbuf[0] & 0x02) == 0x00)
			tasten |=  (1<<(shift+1));
		else
			tasten &= ~(1<<(shift+1));

		if( (outbuf[0] & 0x04) == 0x00)
			tasten |=  (1<<(shift+2));
		else
			tasten &= ~(1<<(shift+2));

		if( (outbuf[0] & 0x08) == 0x00)
			tasten |=  (1<<(shift+3));
		else
			tasten &= ~(1<<(shift+3));
	}
  
	tast_state=tast_state==5?0:tast_state+1;
}



