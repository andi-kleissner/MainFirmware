/**************************************
bluetooth.c
Created: 02/26/2014
Author: Paul Burris
**************************************/
#include "mk20dx128.h"
#include "core_pins.h"
#include "usb_serial.h"
#include "bluetooth.h"
#include "HardwareSerial.h"
#include "common.h"

#define BT_WRITE_BUF_SIZE 14
#define BT_WHITELIST_BUF_SIZE 12
#define BT_SET_MODE_BUF_SIZE 7
#define BT_RESET_BUF_SIZE 6
#define BT_SET_ADV_PARAM_SIZE 10

#define USB_DEBUG

char bt_write_flag = 0;
char bt_whitelist_flag = 0;
char bt_set_mode_flag = 0;
char bt_connected_flag = 0;
char bt_set_adv_parameters_flag = 0;
char bt_armed = '0';
char bt_sound = '0';
char bt_sound_select = '0';
char bt_sound_delay = '0';
char bt_sound_test = '0';
char bt_new_data = 0; 

char bt_serial_out[] = {0x0D, 0x00, 0x09, 0x02, 0x00, 0x14, 0x00, 0x00, 0x05, 0x30, 0x30, 0x30, 0x30, 0x30};
char bt_whitelist_append[] = {0x0B , 0x00, 0x07, 0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
char bt_set_mode[] = {0x06, 0x00, 0x02, 0x06, 0x01, 0x02, 0x02};
char bt_reset[] = {0x05, 0x00, 0x01, 0x00, 0x00, 0x00};
char bt_set_adv_parameters[] = {0x09, 0x00, 0x05, 0x06, 0x08, 0x06, 0x40, 0x06, 0x60, 0x04};

//this is for debugging
void simplePrint(char* S){
	while(*S){
		usb_serial_putchar(*S++);
	}
}

/************************************************************
Init sets up the serial port and pulls up the reset_n pin.
***********************************************************/
void bluetooth_init(){
	serial_begin(BAUD2DIV(38400));
	serial_format(SERIAL_8N1);
	serial_clear();

	pinMode(2, OUTPUT);
	digitalWriteFast(2, HIGH);

	//TODO: implement whitelist
}

/********************************************************************
 Write Attribute updates the attributes that are exposed to the app.
********************************************************************/
void bluetooth_write(char handshake, char arm_disarm, char sound_on_off, char sound_sel, char sound_delay){
	bt_serial_out[9] = handshake;
	bt_serial_out[10] = arm_disarm;
	bt_serial_out[11] = sound_on_off;
	bt_serial_out[12] = sound_sel;
	bt_serial_out[13] = sound_delay;
	
	serial_write(bt_serial_out, BT_WRITE_BUF_SIZE);
}

/********************************************************
Set mode, starts the bluetooth advertisement.
********************************************************/
void bluetooth_set_mode(char discover, char connect){
	bt_set_mode[5] = discover;
	bt_set_mode[6] = connect;
	
	serial_write(bt_set_mode, BT_SET_MODE_BUF_SIZE);
}

/***************************************************************
Whitelist append adds a device to the whitelist
***************************************************************/
void bluetooth_whitelist_append(char* device_address, char address_type){
	int i;
	for(i=0; i<6; i++){
		bt_whitelist_append[i+5] = device_address[i];
	}
	bt_whitelist_append[11] = address_type;
	
	serial_write(bt_whitelist_append, BT_WHITELIST_BUF_SIZE);
}

/****************************************************************
Reset is a software reset for the module if passed 0, hardware if passed 1.
TODO: MAKE THIS HAPPEN
****************************************************************/
void bluetooth_reset(void){
//	serial_write(bt_reset, BT_RESET_BUF_SIZE);
	digitalWriteFast(2, LOW);
	delay(30);
	digitalWriteFast(2, HIGH);
}

/**********************************************************
Set Adv Parameters sets up how often the bluetooth advertises 
and on which channels.
**********************************************************/
void bluetooth_set_adv_parameters(uint16_t min, uint16_t max, uint8_t channel_selector){
	bt_set_adv_parameters[5] = (min & 0xff);
	bt_set_adv_parameters[6] = (min>>8);
	bt_set_adv_parameters[7] = (max & 0xff);
	bt_set_adv_parameters[8] = (max>>8);
	bt_set_adv_parameters[9] = channel_selector;

	serial_write(bt_set_adv_parameters, BT_SET_ADV_PARAM_SIZE);
}

/**********************************************************
This function checks for the response of the Bluetooth moodule 
and should be called on a regular basis
*********************************************************/
void bluetooth_update(){
	char msg_type;
	char length;
	char class;
	char method;
	char data[32];
	char i = 0;
	
	#ifdef USB_DEBUG
	usb_serial_putchar((char)serial_available());
	#endif
	
	if((serial_available()>4)){
		msg_type = serial_getchar();
		length = serial_getchar();
		//usb_serial_putchar(length);
		class = serial_getchar();
		method = serial_getchar();
		
		for(i=0;i<length;i++){
			data[i] = serial_getchar();
			if(i>30){
				break;
				serial_clear();
			}
		}
		
		switch(msg_type){
			case 0x00: //response message
				#ifdef USB_DEBUG
				simplePrint("Response: ");
				#endif
				switch(class){
					case 0x00: //system
						switch(method){
							case 0x0A: //whitelist append
								if((data[0] & data[1]) == 0){bt_whitelist_flag = 1;}
								else{bt_whitelist_flag = 0;}
							break;
						}
					break;
					case 0x02: //attribute database
						switch(method){
							case 0x00: //write
								#ifdef USB_DEBUG
								simplePrint("Write\n");
								#endif
								if((data[0] & data[1]) == 0){bt_write_flag = 1;}
								else{bt_write_flag = 0;}
							break;
						}
					break;
					case 0x06: //GAP
						switch(method){
							case 0x01: //set mode
								if((data[0] & data[1]) == 0){
									bt_set_mode_flag = 1;
									#ifdef USB_DEBUG
									simplePrint("Mode Set\n");
									#endif
									bluetooth_write('0', '0', '0', '0', '0');
								}else{
									bt_set_mode_flag = 0;
									#ifdef USB_DEBUG
									simplePrint("Mode NOT Set\n");
									#endif
								}
							break;
							
							case 0x08: //set advertisement parameters
								if((data[0] & data[1]) == 0){
									bt_set_adv_parameters_flag = 1;
									#ifdef USB_DEBUG
									simplePrint("Advertisement Parameters Set\n");
									#endif
									bluetooth_set_mode(BT_GENERAL_DISCOVERABLE, BT_UNDIRECTED_CONNECTABLE);
								}else{
									bt_set_adv_parameters_flag = 0;
								}
								
							break;
						}
					break;
				}
			break;

			case 0x80: //event message
				#ifdef USB_DEBUG
				simplePrint("Event: ");
				#endif
				switch(class){
					case 0x00: //Bootup
						simplePrint("Bluetooth");
						switch(method){
							case 0x00: //bootup
								simplePrint(" Boot!\n");
								//TODO: put in whitlelist append, then do this
								bluetooth_set_adv_parameters(BT_ADV_MIN, BT_ADV_MAX, BT_SINGLE_CHANNEL);
							break;
						}
					break;
					
					case 0x02: //Data
						switch(method){
							case 0x00: //status
								bt_new_data = 1;
								bt_sound_test = data[7];
								bt_armed = data[8];
								bt_sound = data[9];
								bt_sound_select = data[10];
								bt_sound_delay = data[11];

								#ifdef USB_DEBUG
								simplePrint("New data!\n");
								usb_serial_putchar(bt_sound_test);
								usb_serial_putchar(bt_armed);
								usb_serial_putchar(bt_sound);
								usb_serial_putchar(bt_sound_select);
								usb_serial_putchar(bt_sound_delay);
								#endif
								
							break;
						}
					break;
					
					case 0x03: //Connection
						switch(method){
							case 0x00: //status
								#ifdef USB_DEBUG
								simplePrint("Connected\n");
								#endif
								bt_connected_flag = 1;

							break;
							
							case 0x04: //disconnected
								#ifdef USB_DEBUG
								simplePrint("Disconnected\n");
								#endif
								//bluetooth_set_mode(BT_GENERAL_DISCOVERABLE, BT_UNDIRECTED_CONNECTABLE);
								bluetooth_set_adv_parameters(BT_ADV_MIN, BT_ADV_MAX, BT_SINGLE_CHANNEL);
								bt_connected_flag = 0;
								bt_set_mode_flag = 0;
							break;
						}
					break;
				}
			break;
			
			default:
				#ifdef USB_DEBUG
				simplePrint("Default\n");
				#endif
			break;
		}
	}
}

/*********************************
bluetooth_end
This function should be used to terminate bluetooth connection.
*********************************/
void bluetooth_end(void){
	serial_end();
}
