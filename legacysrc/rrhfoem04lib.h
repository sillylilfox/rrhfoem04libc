#ifndef RRHFOEM04LIB_H
#define RRHFOEM04LIB_H
#include <hidapi/hidapi.h>

#define REPORT_SIZE 64
typedef unsigned char u8;

typedef enum {
    READ = 0x01,
    WRITE = 0x02,
    PRIVACY = 0x04,
    DESTROY = 0x08,
    EASAFI = 0x10
} PASSID;

int ISO15693SetPassword(hid_device *handle, unsigned char *uid, PASSID id, unsigned char *password);
int ISO15693_64bitPasswordProtection(hid_device *handle, unsigned char *uid);
int ISO15693GetMultipleBlockSS(hid_device *handle, unsigned char blocknum, unsigned char totalblocks);
int ISO15693ReadSingleBlock(hid_device *handle, unsigned char *outputstring,unsigned char blocklength, unsigned char blocknumber);
int GetSystemInfoISO15693(hid_device *handle, unsigned char* outputstring);
int SelectISO15693(hid_device *handle, unsigned char *uid);
int ISO15693SingleSlotInventory(hid_device *handle, unsigned char* outputuid);
unsigned short calculateCRC(unsigned char data[], unsigned char len);
int sendcommandrrhfoem04(hid_device *handle, unsigned char *cmd, unsigned char cmd_len, unsigned char *response, size_t response_len);
hid_device *initrrhfoem04(bool autoconnect, unsigned short s1, unsigned short s2);
int killrrhfoem04(hid_device *handle);
int BuzzerBeep(hid_device *handle, int times, int delay);

#endif
