#include "rrhfoem04lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RESP_IS_ERR(r) ((r)[3] == 0xFF && (r)[4] == 0xFF)

#define SEND_CMD(handle, cmd, resp) \
    do { memset((resp), 0, sizeof(resp)); } while(0); \
    if (sendcommandrrhfoem04((handle), (cmd), sizeof(cmd), (resp), sizeof(resp)) < 0) return -1

hid_device *initrrhfoem04(bool autoconnect, unsigned short vid, unsigned short pid) {
    if (hid_init() != 0) {
        fprintf(stderr, "[rrhfoem04] hid_init failed\n");
        return NULL;
    }
    hid_device *handle = autoconnect
        ? hid_open(RRHF_VID_DEFAULT, RRHF_PID_DEFAULT, NULL)
        : hid_open(vid, pid, NULL);
    if (!handle) {
        fprintf(stderr, "[rrhfoem04] Failed to open device (VID=%04X PID=%04X)\n",
                autoconnect ? RRHF_VID_DEFAULT : vid,
                autoconnect ? RRHF_PID_DEFAULT : pid);
        hid_exit();
        return NULL;
    }
    return handle;
}

int killrrhfoem04(hid_device *handle) {
    if (!handle) return -1;
    hid_close(handle);
    hid_exit();
    return 0;
}

unsigned short calculateCRC(const u8 *data, u8 len) {
    unsigned short crc = 0xFFFF;
    for (u8 i = 0; i < len; i++) {
        crc ^= (unsigned short)data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return (unsigned short)(~crc);
}

int sendcommandrrhfoem04(hid_device *handle,
                         const u8 *cmd, u8 cmd_len,
                         u8 *response, size_t response_len) {
    if (!handle || !cmd || cmd_len == 0 || cmd_len > (RRHF_REPORT_SIZE - 3)) {
        fprintf(stderr, "[rrhfoem04] sendcommand: invalid arguments\n");
        return -1;
    }

    u8 buf[RRHF_REPORT_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x00; /* HID report ID */
    memcpy(buf + 1, cmd, cmd_len);

    unsigned short crc = calculateCRC(cmd, cmd_len);
    buf[cmd_len + 1] = (u8)(crc >> 8);
    buf[cmd_len + 2] = (u8)(crc & 0xFF);

    int res = hid_write(handle, buf, RRHF_REPORT_SIZE);
    if (res < 0) {
        fprintf(stderr, "[rrhfoem04] hid_write error: %ls\n", hid_error(handle));
        return -1;
    }

    RRHF_SLEEP_US(1000);

    res = hid_read(handle, response, response_len);
    if (res < 0) {
        fprintf(stderr, "[rrhfoem04] hid_read error: %ls\n", hid_error(handle));
        return -1;
    }
    return res;
}

int ISO15693SingleSlotInventory(hid_device *handle, u8 *outputuid) {
    const u8 cmd[] = {0x04, 0x10, 0x01, 0x26};
    u8 resp[RRHF_REPORT_SIZE];
    memset(resp, 0, sizeof(resp));
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;

    u8 length  = resp[0];
    size_t uid_len = (length > 6) ? (size_t)(length - 6) : 0;
    if (uid_len > RRHF_UID_LEN) uid_len = RRHF_UID_LEN;
    memcpy(outputuid, resp + 6, uid_len);
    return (int)uid_len;
}

static int _parse_uid_list(const u8 *resp, u8 *outputuids, u8 max_uids) {
    if (RESP_IS_ERR(resp)) return -1;
    u8 count = resp[5];
    if (count > max_uids) count = max_uids;
    memcpy(outputuids, resp + 6, (size_t)count * RRHF_UID_LEN);
    return (int)count;
}

int ISO15693_16SlotInventory(hid_device *handle, u8 *outputuids, u8 max_uids) {
    const u8 cmd[] = {0x04, 0x10, 0x02, 0x06};
    u8 resp[RRHF_REPORT_SIZE];
    memset(resp, 0, sizeof(resp));
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    return _parse_uid_list(resp, outputuids, max_uids);
}

int ISO15693FullInventory(hid_device *handle, u8 *outputuids, u8 max_uids) {
    const u8 cmd[] = {0x04, 0x1F, 0x01, 0x06};
    u8 resp[RRHF_REPORT_SIZE];
    memset(resp, 0, sizeof(resp));
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    return _parse_uid_list(resp, outputuids, max_uids);
}

int SelectISO15693(hid_device *handle, const u8 *uid) {
    u8 cmd[12] = {0x0C, 0x10, 0x03, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693Quiet(hid_device *handle, const u8 *uid) {
    u8 cmd[12] = {0x0C, 0x10, 0x04, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693Reset(hid_device *handle, const u8 *uid, bool use_uid) {
    u8 resp[RRHF_REPORT_SIZE];
    if (use_uid) {
        u8 cmd[12] = {0x0C, 0x10, 0x05, 0x22, 0,0,0,0,0,0,0,0};
        memcpy(cmd + 4, uid, RRHF_UID_LEN);
        if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    } else {
        const u8 cmd[] = {0x04, 0x10, 0x05, 0x02};
        if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    }
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693StayQuietPersistent(hid_device *handle, const u8 *uid) {
    u8 cmd[12] = {0x0C, 0x11, 0x13, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693ReadSingleBlock(hid_device *handle, u8 *outputdata,
                            u8 blocklength, u8 blocknumber) {
    const u8 cmd[] = {0x06, 0x10, 0x06, 0x12, blocklength, blocknumber};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* Response: [0]=len [1..2]=cmd [3..4]=err [5]=resp_flag [6]=blk_ss [7+]=data */
    u8 datalen = (resp[0] > 6) ? resp[0] - 6 : 0;
    memcpy(outputdata, resp + 7, datalen);
    return 0;
}

int ISO15693WriteSingleBlock(hid_device *handle, const u8 *data,
                             u8 blocklength, u8 blocknumber) {
    /* Frame len = 6 + blocklength; max payload fits in one HID report */
    if (blocklength > (RRHF_REPORT_SIZE - 9)) return -1;
    u8 cmd[RRHF_REPORT_SIZE];
    cmd[0] = (u8)(6 + blocklength);
    cmd[1] = 0x10; cmd[2] = 0x07;  /* command code 0x1007 */
    cmd[3] = 0x02;                  /* flags: data-rate, broadcast */
    cmd[4] = blocklength;
    cmd[5] = blocknumber;
    memcpy(cmd + 6, data, blocklength);
    u8 cmd_len = (u8)(6 + blocklength);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, cmd_len, resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockBlock(hid_device *handle, u8 blocknumber) {
    const u8 cmd[] = {0x05, 0x10, 0x08, 0x02, blocknumber};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693ReadMultipleBlocks(hid_device *handle, u8 *outputdata,
                               u8 blocklength, u8 blocknumber, u8 totalblocks) {
    const u8 cmd[] = {0x07, 0x10, 0x09, 0x02, blocklength, blocknumber, totalblocks};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* Response payload starts at byte 6 (after len+cmd+err+flags) */
    u8 datalen = (resp[0] > 6) ? resp[0] - 6 : 0;
    memcpy(outputdata, resp + 6, datalen);
    return 0;
}

int ISO15693WriteMultipleBlocks(hid_device *handle, const u8 *data,
                                u8 blocklength, u8 blockaddress, u8 totalblocks) {
    size_t data_bytes = (size_t)blocklength * totalblocks;
    if (data_bytes > (RRHF_REPORT_SIZE - 10)) return -1;
    u8 cmd[RRHF_REPORT_SIZE];
    cmd[0] = (u8)(7 + data_bytes);  /* frame length */
    cmd[1] = 0x1F; cmd[2] = 0x02;  /* command code 0x1F02 */
    cmd[3] = 0x02;                  /* flags */
    cmd[4] = blocklength;
    cmd[5] = blockaddress;
    cmd[6] = totalblocks;
    memcpy(cmd + 7, data, data_bytes);
    u8 cmd_len = (u8)(7 + data_bytes);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, cmd_len, resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int GetSystemInfoISO15693(hid_device *handle, u8 *outputdata) {
    const u8 cmd[] = {0x04, 0x10, 0x0E, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    memset(resp, 0, sizeof(resp));
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    u8 datalen = (resp[0] > 6) ? resp[0] - 6 : 0;
    memcpy(outputdata, resp + 6, datalen);
    return 0;
}

int ISO15693GetMultipleBlockSS(hid_device *handle, u8 blocknum, u8 totalblocks) {
    const u8 cmd[] = {0x06, 0x10, 0x0F, 0x02, blocknum, totalblocks};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693WriteAFI(hid_device *handle, u8 afi) {
    const u8 cmd[] = {0x05, 0x10, 0x0A, 0x02, afi};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockAFI(hid_device *handle) {
    const u8 cmd[] = {0x04, 0x10, 0x0B, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693WriteDSFID(hid_device *handle, u8 dsfid) {
    const u8 cmd[] = {0x05, 0x10, 0x0C, 0x02, dsfid};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockDSFID(hid_device *handle) {
    const u8 cmd[] = {0x04, 0x10, 0x0D, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693ReadEASFlag(hid_device *handle, u8 *state_out) {
    const u8 cmd[] = {0x04, 0x11, 0x01, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    *state_out = resp[5]; /* state byte follows error code */
    return 0;
}

int ISO15693SetEASFlag(hid_device *handle) {
    const u8 cmd[] = {0x04, 0x11, 0x02, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693ResetEASFlag(hid_device *handle) {
    const u8 cmd[] = {0x04, 0x11, 0x03, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockEASFlag(hid_device *handle) {
    const u8 cmd[] = {0x04, 0x11, 0x04, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693WriteEASID(hid_device *handle, const u8 *uid, const u8 *eas_id) {
    /* frame: len(1) + cmd(2) + flags(1) + uid(8) + eas_id(2) = 14 bytes payload */
    u8 cmd[14] = {0x0E, 0x11, 0x11, 0x22, 0,0,0,0,0,0,0,0, 0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    memcpy(cmd + 12, eas_id, 2);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693PasswordProtectEASAFI(hid_device *handle, const u8 *uid, bool option_afi) {
    /* flags: 0x22 (addressed) | 0x40 (option) when protecting AFI */
    u8 flags = option_afi ? (0x22 | 0x40) : 0x22;
    u8 cmd[12] = {0x0C, 0x11, 0x10, flags, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693GetRandomNumber(hid_device *handle, u8 *rng_out) {
    const u8 cmd[] = {0x04, 0x11, 0x05, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* Random number is 2 bytes starting at resp[5] */
    rng_out[0] = resp[5];
    rng_out[1] = resp[6];
    return 0;
}

int ISO15693SetPassword(hid_device *handle, const u8 *uid,
                        PASSID id, const u8 *password) {
    u8 cmd[17] = {0x11, 0x11, 0x06, 0x22,
                  0,0,0,0,0,0,0,0,  /* UID */
                  0,                  /* PASSID */
                  0,0,0,0};          /* password */
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    cmd[12] = (u8)id;
    memcpy(cmd + 13, password, RRHF_PASSWORD_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693WritePassword(hid_device *handle, const u8 *uid,
                          PASSID id, const u8 *password) {
    u8 cmd[17] = {0x11, 0x11, 0x07, 0x22,
                  0,0,0,0,0,0,0,0,
                  0, 0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    cmd[12] = (u8)id;
    memcpy(cmd + 13, password, RRHF_PASSWORD_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockPassword(hid_device *handle, const u8 *uid, PASSID id) {
    u8 cmd[13] = {0x0D, 0x11, 0x08, 0x22,
                  0,0,0,0,0,0,0,0,  /* UID */
                  0};                /* PASSID */
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    cmd[12] = (u8)id;
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693_64bitPasswordProtection(hid_device *handle, const u8 *uid) {
    u8 cmd[12] = {12, 0x11, 0x09, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693PageProtect(hid_device *handle, const u8 *uid,
                        u8 block_address, u8 protection_status) {
    u8 cmd[14] = {0x0E, 0x11, 0x0A, 0x22,
                  0,0,0,0,0,0,0,0,  /* UID */
                  0,                  /* block_address */
                  0};                 /* protection_status */
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    cmd[12] = block_address;
    cmd[13] = protection_status;
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockPageProtect(hid_device *handle, const u8 *uid, u8 block_address) {
    u8 cmd[13] = {0x0D, 0x11, 0x0B, 0x22,
                  0,0,0,0,0,0,0,0,  /* UID */
                  0};                /* block_address */
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    cmd[12] = block_address;
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693Destroy(hid_device *handle, const u8 *uid, const u8 *password) {
    u8 cmd[16] = {0x10, 0x11, 0x0C, 0x22,
                  0,0,0,0,0,0,0,0,  /* UID */
                  0,0,0,0};          /* password */
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    memcpy(cmd + 12, password, RRHF_PASSWORD_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693EnablePrivacy(hid_device *handle, const u8 *uid, const u8 *password) {
    u8 cmd[16] = {0x10, 0x11, 0x0D, 0x22,
                  0,0,0,0,0,0,0,0,
                  0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    memcpy(cmd + 12, password, RRHF_PASSWORD_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693GetNXPSysInfo(hid_device *handle, const u8 *uid, RRHF_NXPSysInfo *info_out) {
    u8 cmd[12] = {0x0C, 0x11, 0x12, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* Response after err: protection_pointer(1) protection_cond(1) lock_bits(1) features(4) */
    info_out->protection_pointer   = resp[5];
    info_out->protection_conditions= resp[6];
    info_out->lock_bits            = resp[7];
    info_out->feature_flags        = ((uint32_t)resp[8]  << 24)
                                   | ((uint32_t)resp[9]  << 16)
                                   | ((uint32_t)resp[10] <<  8)
                                   |  (uint32_t)resp[11];
    return 0;
}

int ISO15693ReadSignature(hid_device *handle, const u8 *uid, u8 *sig_out) {
    u8 cmd[12] = {0x0C, 0x11, 0x14, 0x22, 0,0,0,0,0,0,0,0};
    memcpy(cmd + 4, uid, RRHF_UID_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* 32-byte signature starts at resp[5] */
    memcpy(sig_out, resp + 5, 32);
    return 0;
}

int ISO15693WriteTwoBlocks(hid_device *handle, const u8 *data, u8 blocknumber) {
    u8 cmd[13] = {0x0D, 0x12, 0x01, 0x42, blocknumber,
                  0,0,0,0,0,0,0,0};  /* 8 bytes of data */
    memcpy(cmd + 5, data, 8);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO15693LockTwoBlocks(hid_device *handle, u8 blocknumber) {
    const u8 cmd[] = {0x05, 0x12, 0x02, 0x42, blocknumber};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO14443A_Request(hid_device *handle, u8 *atq_out) {
    const u8 cmd[] = {0x04, 0x20, 0x01, 0x26};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    atq_out[0] = resp[5];
    atq_out[1] = resp[6];
    return 0;
}

int ISO14443A_WakeUp(hid_device *handle, u8 *atq_out) {
    const u8 cmd[] = {0x04, 0x20, 0x02, 0x52};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    atq_out[0] = resp[5];
    atq_out[1] = resp[6];
    return 0;
}

int ISO14443A_AntiCollision(hid_device *handle, RRHF_CascadeLevel cascade, u8 *uid_out) {
    const u8 cmd[] = {0x04, 0x20, 0x06, (u8)cascade};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* 4-byte UID at resp[5..8] */
    memcpy(uid_out, resp + 5, 4);
    return 0;
}

int ISO14443A_Select(hid_device *handle, RRHF_CascadeLevel cascade,
                     const u8 *uid, u8 *sak_out) {
    u8 cmd[8] = {0x08, 0x20, 0x04, (u8)cascade, 0,0,0,0};
    memcpy(cmd + 4, uid, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    *sak_out = resp[5];
    return 0;
}

int ISO14443A_Halt(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x20, 0x05};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO14443A_Inventory(hid_device *handle, u8 *uid_out, u8 *uid_len_out) {
    const u8 cmd[] = {0x03, 0x2F, 0x01};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    *uid_len_out = resp[5];
    memcpy(uid_out, resp + 6, *uid_len_out);
    return 0;
}

int ISO14443A_SelectCard(hid_device *handle, const u8 *uid, u8 uid_len) {
    u8 cmd[8];
    cmd[0] = 0x08;
    cmd[1] = 0x2F; cmd[2] = 0x02;
    cmd[3] = uid_len;
    memcpy(cmd + 4, uid, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int ISO14443A_SelectAnyCard(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x2F, 0x03};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}


int Mifare_Authenticate(hid_device *handle, const u8 *uid, u8 blockno,
                        RRHF_KeyType keytype, const u8 *key) {
    /* frame: len(1)+cmd(2)+uid(4)+blockno(1)+keytype(1)+key(6) = 15 bytes */
    u8 cmd[15];
    cmd[0] = 0x0F;
    cmd[1] = 0x21; cmd[2] = 0x01;
    memcpy(cmd + 3, uid, 4);
    cmd[7] = blockno;
    cmd[8] = (u8)keytype;
    memcpy(cmd + 9, key, RRHF_MIFARE_KEY_LEN);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int Mifare_Read(hid_device *handle, u8 blockno, u8 *data_out) {
    const u8 cmd[] = {0x04, 0x21, 0x02, blockno};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    memcpy(data_out, resp + 5, 16);
    return 0;
}

int Mifare_Write(hid_device *handle, u8 blockno, const u8 *data) {
    u8 cmd[20];
    cmd[0] = 0x14;
    cmd[1] = 0x21; cmd[2] = 0x03;
    cmd[3] = blockno;
    memcpy(cmd + 4, data, 16);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int MifareUL_Read(hid_device *handle, u8 blockno, u8 *data_out) {
    const u8 cmd[] = {0x04, 0x22, 0x01, blockno};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    memcpy(data_out, resp + 5, 16);
    return 0;
}

int MifareUL_Write(hid_device *handle, u8 blockno, const u8 *data) {
    u8 cmd[20];
    cmd[0] = 0x14;
    cmd[1] = 0x22; cmd[2] = 0x02;
    cmd[3] = blockno;
    memcpy(cmd + 4, data, 16);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RFID_LowPowerMode(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x00, 0x01};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RFID_NormalPowerMode(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x00, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RFID_SetRFPower(hid_device *handle, RRHF_RFPower level) {
    const u8 cmd[] = {0x04, 0x00, 0x03, (u8)level};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RFID_GetRFPower(hid_device *handle, RRHF_RFPower *level_out) {
    const u8 cmd[] = {0x03, 0xF0, 0x24};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    *level_out = (RRHF_RFPower)resp[5];
    return 0;
}

int RFID_RFTurnOn(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x00, 0x04};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RFID_RFTurnOff(hid_device *handle) {
    const u8 cmd[] = {0x03, 0x00, 0x05};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RR_GetReaderInfo(hid_device *handle, RRHF_ReaderInfo *info_out) {
    const u8 cmd[] = {0x03, 0xF0, 0x00};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    memcpy(info_out->serial, resp + 5, RRHF_SERIAL_LEN);
    return 0;
}

int RR_Beep(hid_device *handle) {
    const u8 cmd[] = {0x03, 0xF0, 0x01};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int BuzzerBeep(hid_device *handle, int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        if (RR_Beep(handle) < 0) return -1;
        if (i < times - 1)
            RRHF_SLEEP_MS(delay_ms);
    }
    return 0;
}

int RR_BuzzerOn(hid_device *handle) {
    const u8 cmd[] = {0x03, 0xF0, 0x16};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RR_BuzzerOff(hid_device *handle) {
    const u8 cmd[] = {0x03, 0xF0, 0x15};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RR_AdditionalFrame(hid_device *handle, u8 *outputuids, u8 max_uids) {
    const u8 cmd[] = {0x03, 0xF0, 0x02};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    u8 count = resp[5];
    if (count > max_uids) count = max_uids;
    memcpy(outputuids, resp + 6, (size_t)count * RRHF_UID_LEN);
    return (int)count;
}

int RR_ResetDevice(hid_device *handle) {
    const u8 cmd[] = {0x03, 0xFF, 0x03};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}


int RR_RelayOnOff(hid_device *handle, u8 relay_no, bool on) {
    const u8 cmd[] = {0x05, 0xF0, 0x19, relay_no, on ? 0x01 : 0x00};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int RR_RelaySetAutoOff(hid_device *handle, u8 relay_no, u8 time_100ms) {
    const u8 cmd[] = {0x05, 0xF0, 0x22, relay_no, time_100ms};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}


/*
* THIS PART IS FOR THE NETWORK CONTROL
* THIS PART IS ***NOT*** TESTED!
*/

int NET_GetTCPInfo(hid_device *handle, RRHF_TCPInfo *info_out) {
    const u8 cmd[] = {0x03, 0xF0, 0x14};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* response layout after err (byte 5 onward):
       ip(4) mask(4) gw(4) dns1(4) dns2(4) server_ip(4) mac(6) client_port(2) server_port(2) nbios(16) */
    const u8 *p = resp + 5;
    memcpy(info_out->ip,           p,      4); p += 4;
    memcpy(info_out->mask,         p,      4); p += 4;
    memcpy(info_out->gateway,      p,      4); p += 4;
    memcpy(info_out->dns_primary,  p,      4); p += 4;
    memcpy(info_out->dns_secondary,p,      4); p += 4;
    memcpy(info_out->server_ip,    p,      4); p += 4;
    memcpy(info_out->mac,          p,      6); p += 6;
    info_out->client_port = (uint16_t)((p[0] << 8) | p[1]); p += 2;
    info_out->server_port = (uint16_t)((p[0] << 8) | p[1]); p += 2;
    memcpy(info_out->nbios_name,   p,     16);
    return 0;
}

int NET_SetDeviceIP(hid_device *handle, const u8 ip[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x0A, 0,0,0,0};
    memcpy(cmd + 3, ip, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetHostIP(hid_device *handle, const u8 ip[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x0C, 0,0,0,0};
    memcpy(cmd + 3, ip, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetServerPort(hid_device *handle, uint16_t port) {
    u8 cmd[5] = {0x05, 0xF0, 0x0B, (u8)(port >> 8), (u8)(port & 0xFF)};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetClientPort(hid_device *handle, uint16_t port) {
    u8 cmd[5] = {0x05, 0xF0, 0x0D, (u8)(port >> 8), (u8)(port & 0xFF)};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetGateway(hid_device *handle, const u8 gw[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x0E, 0,0,0,0};
    memcpy(cmd + 3, gw, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetSubnetMask(hid_device *handle, const u8 mask[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x12, 0,0,0,0};
    memcpy(cmd + 3, mask, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetPrimaryDNS(hid_device *handle, const u8 dns[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x0F, 0,0,0,0};
    memcpy(cmd + 3, dns, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetSecondaryDNS(hid_device *handle, const u8 dns[4]) {
    u8 cmd[7] = {0x07, 0xF0, 0x10, 0,0,0,0};
    memcpy(cmd + 3, dns, 4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetMACAddress(hid_device *handle, const u8 mac[6]) {
    u8 cmd[9] = {0x09, 0xF0, 0x11, 0,0,0,0,0,0};
    memcpy(cmd + 3, mac, 6);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_SetNBIOSName(hid_device *handle, const u8 name[16]) {
    u8 cmd[19];
    cmd[0] = 0x13; cmd[1] = 0xF0; cmd[2] = 0x13;
    memcpy(cmd + 3, name, 16);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_ResetToDefaultTCP(hid_device *handle) {
    const u8 cmd[] = {0x03, 0xF0, 0x28};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}

int NET_GetDefaultTCPInfo(hid_device *handle, RRHF_TCPInfo *info_out) {
    const u8 cmd[] = {0x03, 0xF0, 0x26};
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    /* layout: reader_ip(4) mask(4) gw(4) dns1(4) dns2(4) host_ip(4) mac(6) host_port(2) reader_port(2) */
    const u8 *p = resp + 5;
    memcpy(info_out->ip,           p, 4); p += 4;
    memcpy(info_out->mask,         p, 4); p += 4;
    memcpy(info_out->gateway,      p, 4); p += 4;
    memcpy(info_out->dns_primary,  p, 4); p += 4;
    memcpy(info_out->dns_secondary,p, 4); p += 4;
    memcpy(info_out->server_ip,    p, 4); p += 4;
    memcpy(info_out->mac,          p, 6); p += 6;
    info_out->client_port = (uint16_t)((p[0] << 8) | p[1]); p += 2;
    info_out->server_port = (uint16_t)((p[0] << 8) | p[1]);
    return 0;
}

int NET_SetDefaultTCPInfo(hid_device *handle, const RRHF_TCPInfo *info) {
    /* frame: 0x1F, cmd(2), reader_ip(4), reader_port(2), host_ip(4), host_port(2),
              mask(4), gw(4), dns1(4), dns2(4)  = 31 payload bytes */
    u8 cmd[31];
    cmd[0] = 0x1F; cmd[1] = 0xF0; cmd[2] = 0x27;
    u8 *p = cmd + 3;
    memcpy(p, info->ip,           4); p += 4;
    p[0] = (u8)(info->server_port >> 8);
    p[1] = (u8)(info->server_port & 0xFF);
    p += 2;
    memcpy(p, info->server_ip,    4); p += 4;
    p[0] = (u8)(info->client_port >> 8);
    p[1] = (u8)(info->client_port & 0xFF);
    p += 2;
    memcpy(p, info->mask,         4); p += 4;
    memcpy(p, info->gateway,      4); p += 4;
    memcpy(p, info->dns_primary,  4); p += 4;
    memcpy(p, info->dns_secondary,4);
    u8 resp[RRHF_REPORT_SIZE];
    if (sendcommandrrhfoem04(handle, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) return -1;
    if (RESP_IS_ERR(resp)) return -1;
    return 0;
}
