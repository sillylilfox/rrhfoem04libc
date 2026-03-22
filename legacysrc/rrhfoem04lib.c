#include <stdio.h>
#include <stdlib.h>
#include <hidapi/hidapi.h>
#include <unistd.h> 
#include "rrhfoem04lib.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

int ISO15693SetPassword(hid_device *handle, unsigned char *uid, PASSID id, unsigned char *password) {
    u8 spcmd[17] = {0x11, 0x11, 0x06, 0x22};
    memcpy(spcmd+4, uid, 8);
    spcmd[12] = id;
    memcpy(spcmd+13, password, 4);
    u8 response[REPORT_SIZE];

    sendcommandrrhfoem04(handle, spcmd, sizeof(spcmd), response, REPORT_SIZE);
    if(response[3] == 0xFF && response[4] == 0xFF) return -1;
    return 0;
}

int ISO15693_64bitPasswordProtection(hid_device *handle, unsigned char *uid){
    u8 sfbppb[12] = {12, 0x11, 0x09, 0x22};
    memcpy(sfbppb+4, uid, 8);
    u8 response[REPORT_SIZE];
    sendcommandrrhfoem04(handle, sfbppb, sizeof(sfbppb), response, REPORT_SIZE);
    if(response[3] == 0xFF && response[4] == 0xFF)return -1;
    return 0;
}

int ISO15693GetMultipleBlockSS(hid_device *handle, unsigned char blocknum, unsigned char totalblocks) {
    //NOTE: Get Multiple Block Security Status
    //WARNING ITS STILL BETA HERE. STILL DIDNT MANAGE TO UNDERSTAND THE OUTPUT FORMAT
    u8 gmbss[6] = {0x06, 0x10, 0x0F, 0x02, blocknum, totalblocks};
    u8 response[REPORT_SIZE];
    sendcommandrrhfoem04(handle, gmbss, 6, response, REPORT_SIZE);
    if(response[3] == 0xFF && response[4] == 0xFF)return -1;
    return 0;
}

int ISO15693ReadSingleBlock(hid_device *handle, unsigned char *outputstring, unsigned char blocklength, unsigned char blocknumber) {
    //NOTE: THIS COMMAND HAS 2 OTHER VARIANTS. IF YOU CANT READ WITH THIS, TRY OUT AND MAKE THE OTHER VARIANT
    //WARNING WARNING WARNING ITS STILL ON DEBUG MODE HERE
    u8 rsb[6] = {0x06, 0x10, 0x06, 0x12, blocklength, blocknumber};
    u8 response[REPORT_SIZE];
    sendcommandrrhfoem04(handle, rsb, 6, response, REPORT_SIZE);
    if(response[3] == 0xFF && response[4] == 0xFF) return -1;
    u8 datalen = (response[0]) - (u8)6;
    memcpy(outputstring, response+7, datalen);
    return 0;
}

int GetSystemInfoISO15693(hid_device *handle, unsigned char* outputstring){
    //WARNING ITS STILL BETA HERE
    u8 gsicmd[] = {0x04, 0x10, 0x0E, 0x02};
    u8 response[REPORT_SIZE];
    memset(response, 0, sizeof(response)); //sometimes newly defined strings include garbage inside. here we clear it out!
    memcpy(outputstring, response+6, response[0]);
    return 0;
}

int SelectISO15693(hid_device *handle, unsigned char *uid) {
    u8 selectcmd[12] = {0x0C, 0x10, 0x03, 0x22}; // command ready, but need to put the uid inside
    u8 response[REPORT_SIZE];
    memcpy(selectcmd+4, uid, 8); // copied the uid inside. ready to send!
    sendcommandrrhfoem04(handle, selectcmd, sizeof(selectcmd), response, REPORT_SIZE); //response is copied
    if(response[3] == 0xFF && response[4] == 0xFF) return -1; //something bad happened
    return 0;
}

int ISO15693SingleSlotInventory(hid_device *handle, unsigned char *outputuid) {
    //returns -1 when there is no card available on the reader
    //returns the length of the uid when it succeeds
    u8 cmd[] = {0x04, 0x10, 0x01, 0x26};
    u8 returnbfr[REPORT_SIZE];
    u8 report[REPORT_SIZE];
    for(int i =0; i < 64; i++) {}
    memset(report, 0, sizeof(report));
    sendcommandrrhfoem04(handle, cmd, sizeof(cmd), report, REPORT_SIZE);
    if(report[3] == 0xFF && report[4] == 0xFF) return -1;
    u8 length = report[0];
    size_t uidlen = (length > 6) ? length - 6 : 0;
    memcpy(outputuid, report + 6, uidlen);
    return (int)uidlen;
}

int BuzzerBeep(hid_device *handle, int times, int delay) {
    u8 buzzerframe[] = {0x03, 0xF0, 0x01};
    u8 resp[REPORT_SIZE];
    for(int i = 0; i < times; i++) {
        sendcommandrrhfoem04(handle, buzzerframe, sizeof(buzzerframe), resp, REPORT_SIZE);
        sleep(delay);
    }
}

int sendcommandrrhfoem04(hid_device *handle, unsigned char *cmd, unsigned char cmd_len, unsigned char *response, size_t response_len) {
    if (!handle || !cmd || cmd_len == 0 || cmd_len > REPORT_SIZE) return -1;
    u8 buf[REPORT_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x00;
    unsigned short crcvalue = calculateCRC(cmd, cmd_len);
    memcpy(buf+1, cmd, cmd_len);
    buf[cmd_len+1] = (crcvalue >> 8);
    buf[cmd_len+2] = crcvalue;
    int res = hid_write(handle, buf, REPORT_SIZE);
    if (res < 0) {fprintf(stderr, "hid_write error: %ls\n", hid_error(handle));return -1;}
    usleep(1000);
    res = hid_read(handle, response, response_len);
    if (res < 0) {fprintf(stderr, "hid_read error: %ls\n", hid_error(handle));return -1;}
    return res; 
}

unsigned short calculateCRC(unsigned char data[], unsigned char len) {
    unsigned short crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return (unsigned short)(~crc);
}

hid_device *initrrhfoem04(bool autoconnect, unsigned short s1, unsigned short s2) {
	if(hid_init() != 0) {fprintf(stderr, "hid_init fail..."); return 0;}
	hid_device *handle;
	if(autoconnect == true) handle = hid_open(0x1781, 0x0C10, NULL);
	else handle = hid_open(s1, s2, NULL);
	if (!handle) {fprintf(stderr, "Failed to open device\n"); return 0;}
	return handle;
}

int killrrhfoem04(hid_device *handle) {
	hid_close(handle);
	hid_exit();
}



