#ifndef RRHFOEM04LIB_H
#define RRHFOEM04LIB_H

#include <hidapi/hidapi.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#  include <windows.h>
#  define RRHF_SLEEP_MS(ms)  Sleep(ms)
#  define RRHF_SLEEP_US(us)  Sleep((us) / 1000)
#else
#  include <unistd.h>
#  define RRHF_SLEEP_MS(ms)  usleep((unsigned int)((ms) * 1000))
#  define RRHF_SLEEP_US(us)  usleep((unsigned int)(us))
#endif

#define RRHF_REPORT_SIZE    64
#define RRHF_VID_DEFAULT    0x1781
#define RRHF_PID_DEFAULT    0x0C10
#define RRHF_UID_LEN        8
#define RRHF_PASSWORD_LEN   4
#define RRHF_MIFARE_KEY_LEN 6
#define RRHF_SERIAL_LEN     16

/* Legacy aliases */
#define REPORT_SIZE RRHF_REPORT_SIZE

typedef unsigned char u8;

#define RRHF_FLAG_DATA_RATE  0x02
#define RRHF_FLAG_ADDRESS    0x20
#define RRHF_FLAG_SELECT     0x10
#define RRHF_FLAG_OPTION     0x40
#define RRHF_FLAG_INVENTORY  0x04

typedef enum {
    RRHF_RF_POWER_100MW = 0x01,
    RRHF_RF_POWER_200MW = 0x02
} RRHF_RFPower;

typedef enum {
    RRHF_KEY_A = 0x60,
    RRHF_KEY_B = 0x61
} RRHF_KeyType;

typedef enum {
    RRHF_CASCADE_1 = 0x93,
    RRHF_CASCADE_2 = 0x95,
    RRHF_CASCADE_3 = 0x97
} RRHF_CascadeLevel;

typedef enum {
    PASSID_READ    = 0x01,
    PASSID_WRITE   = 0x02,
    PASSID_PRIVACY = 0x04,
    PASSID_DESTROY = 0x08,
    PASSID_EASAFI  = 0x10,

    /* Legacy declarations */
    READ    = 0x01,
    WRITE   = 0x02,
    PRIVACY = 0x04,
    DESTROY = 0x08,
    EASAFI  = 0x10
} PASSID;

typedef enum {
    RRHF_RELAY_SINGLE     = 0x00,  
    RRHF_RELAY_CONTINUOUS = 0x01,  
    RRHF_RELAY_NOCHANGE   = 0x02   
} RRHF_RelayMode;

typedef struct {
    u8 protection_pointer;
    u8 protection_conditions;
    u8 lock_bits;            
    uint32_t feature_flags;  
} RRHF_NXPSysInfo;

typedef struct {
    u8 serial[RRHF_SERIAL_LEN];
} RRHF_ReaderInfo;

/* TCP information, not tested yet */
typedef struct {
    u8  ip[4];
    u8  mask[4];
    u8  gateway[4];
    u8  dns_primary[4];
    u8  dns_secondary[4];
    u8  server_ip[4];
    u8  mac[6];
    u16 client_port;
    u16 server_port;
    u8  nbios_name[16];
} RRHF_TCPInfo;

/**
 * Open the reader and initialise the HID library.
 * @param autoconnect  Use default VID/PID (0x1781/0x0C10) when true.
 * @param vid          Vendor  ID – ignored when autoconnect is true.
 * @param pid          Product ID – ignored when autoconnect is true.
 * @return Open device handle, or NULL on failure.
 */
hid_device *initrrhfoem04(bool autoconnect, unsigned short vid, unsigned short pid);

/**
 * Close the reader and release the HID library.
 * @return 0 on success, -1 on error.
 */
int killrrhfoem04(hid_device *handle);

/**
 * Send a raw command frame and read back the response.
 * Prepends the HID report-ID (0x00) and appends a 2-byte CRC automatically.
 *
 * @param handle        Open device handle.
 * @param cmd           Command payload (no report-ID, no CRC).
 * @param cmd_len       Payload length; must be in [1, RRHF_REPORT_SIZE-3].
 * @param response      Caller buffer for the raw HID report.
 * @param response_len  Size of that buffer; must be >= RRHF_REPORT_SIZE.
 * @return Bytes read on success, -1 on error.
 */
int sendcommandrrhfoem04(hid_device *handle,
                         const u8 *cmd, u8 cmd_len,
                         u8 *response, size_t response_len);

/**
 * Calculate CRC-16/CCITT-FALSE over a byte array.
 * The result is bitwise-inverted before being returned.
 */
unsigned short calculateCRC(const u8 *data, u8 len);


/**
 * Single-slot inventory: detect one tag and return its UID.
 * @param outputuid  Buffer of at least RRHF_UID_LEN bytes.
 * @return UID length on success (typically 8), -1 when no tag found.
 */
int ISO15693SingleSlotInventory(hid_device *handle, u8 *outputuid);

/**
 * 16-slot inventory: detect multiple tags.
 * @param outputuids  Buffer large enough for (max_uids * RRHF_UID_LEN) bytes.
 * @param max_uids    Capacity of outputuids in units of UIDs.
 * @return Number of UIDs found (>=0), or -1 on error.
 */
int ISO15693_16SlotInventory(hid_device *handle, u8 *outputuids, u8 max_uids);

/**
 * Full inventory (command 0x1F01): find all tags present.
 * @param outputuids  Buffer large enough for (max_uids * RRHF_UID_LEN) bytes.
 * @param max_uids    Capacity of outputuids in units of UIDs.
 * @return Number of UIDs found (>=0), or -1 on error.
 */
int ISO15693FullInventory(hid_device *handle, u8 *outputuids, u8 max_uids);

/**
 * Select a specific tag by UID for subsequent addressed commands.
 * @param uid  8-byte tag UID.
 * @return 0 on success, -1 on error.
 */
int SelectISO15693(hid_device *handle, const u8 *uid);

/**
 * Quiet: put a tag into the quiet state (it will stop responding).
 * @param uid  8-byte tag UID.
 * @return 0 on success, -1 on error.
 */
int ISO15693Quiet(hid_device *handle, const u8 *uid);

/**
 * Reset to ready. Three flag variants: broadcast (flags=0x02),
 * select-flag (0x12), or addressed (0x22 + UID).
 * Pass uid=NULL to use broadcast or select mode (set use_uid=false).
 * @param uid      8-byte UID (only used when use_uid is true).
 * @param use_uid  If true sends addressed mode; if false sends broadcast.
 * @return 0 on success, -1 on error.
 */
int ISO15693Reset(hid_device *handle, const u8 *uid, bool use_uid);

/**
 * Stay Quiet Persistent: tag enters quiet state that survives brief power loss.
 * @param uid  8-byte tag UID.
 * @return 0 on success, -1 on error.
 */
int ISO15693StayQuietPersistent(hid_device *handle, const u8 *uid);

/**
 * Read a single memory block from the selected/addressed tag.
 * @param outputdata   Destination buffer; must hold at least blocklength bytes.
 * @param blocklength  Bytes per block for this tag (commonly 4).
 * @param blocknumber  Zero-based block index.
 * @return 0 on success, -1 on error.
 */
int ISO15693ReadSingleBlock(hid_device *handle, u8 *outputdata,
                            u8 blocklength, u8 blocknumber);

/**
 * Write a single memory block.
 * @param data         Data to write; must be exactly blocklength bytes.
 * @param blocklength  Bytes per block.
 * @param blocknumber  Zero-based block index.
 * @return 0 on success, -1 on error.
 */
int ISO15693WriteSingleBlock(hid_device *handle, const u8 *data,
                             u8 blocklength, u8 blocknumber);

/**
 * Lock (permanently write-protect) a single block.
 * @param blocknumber  Zero-based block index.
 * @return 0 on success, -1 on error.
 */
int ISO15693LockBlock(hid_device *handle, u8 blocknumber);

/**
 * Read multiple consecutive blocks.
 * @param outputdata   Destination buffer; must hold blocklength*totalblocks bytes.
 * @param blocklength  Bytes per block.
 * @param blocknumber  First block to read.
 * @param totalblocks  Number of blocks to read.
 * @return 0 on success, -1 on error.
 */
int ISO15693ReadMultipleBlocks(hid_device *handle, u8 *outputdata,
                               u8 blocklength, u8 blocknumber, u8 totalblocks);

/**
 * Write multiple blocks in one transaction.
 * @param data         Data to write; must be blocklength*totalblocks bytes.
 * @param blocklength  Bytes per block.
 * @param blockaddress First block address to write.
 * @param totalblocks  Number of blocks to write.
 * @return 0 on success, -1 on error.
 */
int ISO15693WriteMultipleBlocks(hid_device *handle, const u8 *data,
                                u8 blocklength, u8 blockaddress, u8 totalblocks);

/**
 * Retrieve system information (DSFID, AFI, memory size, IC reference).
 * @param outputdata  Caller buffer of at least RRHF_REPORT_SIZE bytes.
 * @return 0 on success, -1 on error.
 */
int GetSystemInfoISO15693(hid_device *handle, u8 *outputdata);

/**
 * Query the security status of one or more memory blocks.
 * @param blocknum     First block to query.
 * @param totalblocks  Number of consecutive blocks to query.
 * @return 0 on success, -1 on error.
 */
int ISO15693GetMultipleBlockSS(hid_device *handle, u8 blocknum, u8 totalblocks);

/** Write AFI flag. @return 0 on success, -1 on error. */
int ISO15693WriteAFI(hid_device *handle, u8 afi);

/** Lock AFI flag (permanent). @return 0 on success, -1 on error. */
int ISO15693LockAFI(hid_device *handle);

/** Write DSFID flag. @return 0 on success, -1 on error. */
int ISO15693WriteDSFID(hid_device *handle, u8 dsfid);

/** Lock DSFID flag (permanent). @return 0 on success, -1 on error. */
int ISO15693LockDSFID(hid_device *handle);

/**
 * Read the current EAS flag state.
 * @param state_out  Receives 0x00 (inactive) or 0x01 (active).
 * @return 0 on success, -1 on error.
 */
int ISO15693ReadEASFlag(hid_device *handle, u8 *state_out);

/** Set EAS flag. @return 0 on success, -1 on error. */
int ISO15693SetEASFlag(hid_device *handle);

/** Reset EAS flag. @return 0 on success, -1 on error. */
int ISO15693ResetEASFlag(hid_device *handle);

/** Lock EAS flag (permanent). @return 0 on success, -1 on error. */
int ISO15693LockEASFlag(hid_device *handle);

/**
 * Write a 2-byte EAS ID.
 * @param uid     8-byte tag UID (addressed mode).
 * @param eas_id  2-byte EAS identifier.
 * @return 0 on success, -1 on error.
 */
int ISO15693WriteEASID(hid_device *handle, const u8 *uid, const u8 *eas_id);

/**
 * Enable password protection for EAS or AFI.
 * Set option_flag=false to protect EAS; true to protect AFI.
 * @param uid         8-byte tag UID.
 * @param option_afi  If true protect AFI, else protect EAS.
 * @return 0 on success, -1 on error.
 */
int ISO15693PasswordProtectEASAFI(hid_device *handle, const u8 *uid, bool option_afi);

/**
 * Get a 2-byte random number from the tag.
 * @param rng_out  Buffer of at least 2 bytes.
 * @return 0 on success, -1 on error.
 */
int ISO15693GetRandomNumber(hid_device *handle, u8 *rng_out);

/**
 * Transmit an existing password to authenticate access.
 * @param uid       8-byte tag UID.
 * @param id        Which password slot to present (see PASSID).
 * @param password  4-byte password value.
 * @return 0 on success, -1 on error.
 */
int ISO15693SetPassword(hid_device *handle, const u8 *uid,
                        PASSID id, const u8 *password);

/**
 * Write a new password into a slot (requires prior SET PASSWORD authentication).
 * @param uid       8-byte tag UID.
 * @param id        Which password slot to update.
 * @param password  New 4-byte password.
 * @return 0 on success, -1 on error.
 */
int ISO15693WritePassword(hid_device *handle, const u8 *uid,
                          PASSID id, const u8 *password);

/**
 * Lock a password slot so it can no longer be changed.
 * @param uid  8-byte tag UID.
 * @param id   Which slot to lock.
 * @return 0 on success, -1 on error.
 */
int ISO15693LockPassword(hid_device *handle, const u8 *uid, PASSID id);

/**
 * Enable 64-bit password protection on a compatible tag.
 * @param uid  8-byte tag UID.
 * @return 0 on success, -1 on error.
 */
int ISO15693_64bitPasswordProtection(hid_device *handle, const u8 *uid);

/**
 * Set page-level access protection.
 * @param uid               8-byte tag UID.
 * @param block_address     Block address pointer (0-78).
 * @param protection_status Bitmask controlling read/write protection.
 * @return 0 on success, -1 on error.
 */
int ISO15693PageProtect(hid_device *handle, const u8 *uid,
                        u8 block_address, u8 protection_status);

/**
 * Lock page-protection conditions (permanent).
 * @param uid           8-byte tag UID.
 * @param block_address Block address pointer (0-78).
 * @return 0 on success, -1 on error.
 */
int ISO15693LockPageProtect(hid_device *handle, const u8 *uid, u8 block_address);

/**
 * Destroy a tag irreversibly (requires Destroy password).
 * @param uid       8-byte tag UID.
 * @param password  4-byte Destroy password.
 * @return 0 on success, -1 on error.
 */
int ISO15693Destroy(hid_device *handle, const u8 *uid, const u8 *password);

/**
 * Enable Privacy mode (tag stops responding until password is re-presented).
 * @param uid       8-byte tag UID.
 * @param password  4-byte Privacy password.
 * @return 0 on success, -1 on error.
 */
int ISO15693EnablePrivacy(hid_device *handle, const u8 *uid, const u8 *password);

/**
 * Get NXP-specific system information.
 * @param info_out  Pointer to RRHF_NXPSysInfo struct to fill in.
 * @return 0 on success, -1 on error.
 */
int ISO15693GetNXPSysInfo(hid_device *handle, const u8 *uid, RRHF_NXPSysInfo *info_out);

/**
 * Read the tag's 32-byte ECC signature (NXP-specific).
 * @param uid        8-byte tag UID.
 * @param sig_out    Buffer of at least 32 bytes.
 * @return 0 on success, -1 on error.
 */
int ISO15693ReadSignature(hid_device *handle, const u8 *uid, u8 *sig_out);

/* ═══════════════════════════════════════════════════════════════════════════
 *  ISO 15693 — TAGIT EXTENSIONS
 * ═══════════════════════════════════════════════════════════════════════════*/

/**
 * Write two consecutive blocks (TI Tag-it command 0x1201).
 * @param data         8 bytes of data (two 4-byte blocks).
 * @param blocknumber  First block number.
 * @return 0 on success, -1 on error.
 */
int ISO15693WriteTwoBlocks(hid_device *handle, const u8 *data, u8 blocknumber);

/**
 * Lock two consecutive blocks (TI Tag-it command 0x1202).
 * @param blocknumber  First block number.
 * @return 0 on success, -1 on error.
 */
int ISO15693LockTwoBlocks(hid_device *handle, u8 blocknumber);

/* ═══════════════════════════════════════════════════════════════════════════
 *  ISO 14443A COMMANDS
 * ═══════════════════════════════════════════════════════════════════════════*/

/**
 * ISO 14443A Request (REQA) – detect card, returns ATQ.
 * @param atq_out  2-byte ATQ buffer.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_Request(hid_device *handle, u8 *atq_out);

/**
 * ISO 14443A Wake-Up (WUPA).
 * @param atq_out  2-byte ATQ buffer.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_WakeUp(hid_device *handle, u8 *atq_out);

/**
 * ISO 14443A Anti-collision loop for one cascade level.
 * @param cascade  Cascade level (RRHF_CASCADE_1/2/3).
 * @param uid_out  4-byte partial UID buffer.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_AntiCollision(hid_device *handle, RRHF_CascadeLevel cascade, u8 *uid_out);

/**
 * ISO 14443A Select at a given cascade level.
 * @param cascade  Cascade level (RRHF_CASCADE_1/2/3).
 * @param uid      4-byte partial UID to select.
 * @param sak_out  1-byte SAK response.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_Select(hid_device *handle, RRHF_CascadeLevel cascade,
                     const u8 *uid, u8 *sak_out);

/**
 * ISO 14443A Halt – put selected card to sleep.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_Halt(hid_device *handle);

/**
 * Convenience inventory: find one ISO 14443A card and return its UID.
 * @param uid_out    Buffer of at least 10 bytes.
 * @param uid_len_out  Receives the actual UID length (4, 7, or 10).
 * @return 0 on success, -1 on error.
 */
int ISO14443A_Inventory(hid_device *handle, u8 *uid_out, u8 *uid_len_out);

/**
 * Select a specific 14443A card by UID.
 * @param uid      UID bytes.
 * @param uid_len  Length of UID (4, 7, or 10).
 * @return 0 on success, -1 on error.
 */
int ISO14443A_SelectCard(hid_device *handle, const u8 *uid, u8 uid_len);

/**
 * Select any ISO 14443A card present.
 * @return 0 on success, -1 on error.
 */
int ISO14443A_SelectAnyCard(hid_device *handle);

/* ── Mifare Classic ──────────────────────────────────────────────────────── */

/**
 * Mifare authenticate a block using a key.
 * @param uid       4-byte card UID.
 * @param blockno   Block number to authenticate.
 * @param keytype   RRHF_KEY_A or RRHF_KEY_B.
 * @param key       6-byte key.
 * @return 0 on success, -1 on error.
 */
int Mifare_Authenticate(hid_device *handle, const u8 *uid, u8 blockno,
                        RRHF_KeyType keytype, const u8 *key);

/**
 * Read 16 bytes from a Mifare block.
 * @param blockno   Block number to read.
 * @param data_out  Buffer of at least 16 bytes.
 * @return 0 on success, -1 on error.
 */
int Mifare_Read(hid_device *handle, u8 blockno, u8 *data_out);

/**
 * Write 16 bytes to a Mifare block.
 * @param blockno  Block number to write.
 * @param data     16 bytes of data.
 * @return 0 on success, -1 on error.
 */
int Mifare_Write(hid_device *handle, u8 blockno, const u8 *data);

/* ── Mifare Ultralight ───────────────────────────────────────────────────── */

/**
 * Read 16 bytes from a Mifare Ultralight block.
 * @param blockno   Block number.
 * @param data_out  Buffer of at least 16 bytes.
 * @return 0 on success, -1 on error.
 */
int MifareUL_Read(hid_device *handle, u8 blockno, u8 *data_out);

/**
 * Write 16 bytes to a Mifare Ultralight block.
 * @param blockno  Block number.
 * @param data     16 bytes of data.
 * @return 0 on success, -1 on error.
 */
int MifareUL_Write(hid_device *handle, u8 blockno, const u8 *data);

/* ═══════════════════════════════════════════════════════════════════════════
 *  RFID SYSTEM LEVEL
 * ═══════════════════════════════════════════════════════════════════════════*/

/** Put reader into low-power mode. @return 0/−1. */
int RFID_LowPowerMode(hid_device *handle);

/** Restore reader to normal power. @return 0/−1. */
int RFID_NormalPowerMode(hid_device *handle);

/**
 * Set the RF output power level.
 * @param level  RRHF_RF_POWER_100MW or RRHF_RF_POWER_200MW.
 * @return 0 on success, -1 on error.
 */
int RFID_SetRFPower(hid_device *handle, RRHF_RFPower level);

/**
 * Get the currently configured RF power level.
 * @param level_out  Receives RRHF_RF_POWER_100MW or RRHF_RF_POWER_200MW.
 * @return 0 on success, -1 on error.
 */
int RFID_GetRFPower(hid_device *handle, RRHF_RFPower *level_out);

/** Turn RF field ON. @return 0/−1. */
int RFID_RFTurnOn(hid_device *handle);

/** Turn RF field OFF. @return 0/−1. */
int RFID_RFTurnOff(hid_device *handle);

/**
 * Get reader serial / firmware information.
 * @param info_out  Pointer to RRHF_ReaderInfo to fill.
 * @return 0 on success, -1 on error.
 */
int RR_GetReaderInfo(hid_device *handle, RRHF_ReaderInfo *info_out);

/**
 * Trigger the reader's built-in buzzer once per call.
 * (Low-level single beep, command 0xF001.)
 * @return 0 on success, -1 on error.
 */
int RR_Beep(hid_device *handle);

/**
 * High-level helper: beep N times with a delay between each.
 * @param times     Number of beeps.
 * @param delay_ms  Pause between beeps in milliseconds.
 * @return 0 on success, -1 if any individual beep failed.
 */
int BuzzerBeep(hid_device *handle, int times, int delay_ms);

/** Turn buzzer on continuously (0xF016). @return 0/−1. */
int RR_BuzzerOn(hid_device *handle);

/** Turn buzzer off (0xF015). @return 0/−1. */
int RR_BuzzerOff(hid_device *handle);

/**
 * Request additional-frame UIDs (when more than 7 tags were detected).
 * @param outputuids  Buffer for additional UIDs.
 * @param max_uids    Capacity of outputuids in units of UIDs.
 * @return Number of additional UIDs received, or -1 on error.
 */
int RR_AdditionalFrame(hid_device *handle, u8 *outputuids, u8 max_uids);

/** Soft-reset / restart the reader. @return 0/−1. */
int RR_ResetDevice(hid_device *handle);

/**
 * Set a relay on or off.
 * @param relay_no  1 or 2.
 * @param on        true = ON, false = OFF.
 * @return 0 on success, -1 on error.
 */
int RR_RelayOnOff(hid_device *handle, u8 relay_no, bool on);

/**
 * Turn a relay on and automatically turn it off after a delay.
 * @param relay_no  1 or 2.
 * @param time_100ms  Relay on-time in units of 100 ms (e.g. 10 = 1 second).
 * @return 0 on success, -1 on error.
 */
int RR_RelaySetAutoOff(hid_device *handle, u8 relay_no, u8 time_100ms);

/**
 * NOT TESTED!
 * Retrieve all current TCP/IP configuration from the reader.
 * @param info_out  Pointer to RRHF_TCPInfo struct to fill.
 * @return 0 on success, -1 on error.
 */
int NET_GetTCPInfo(hid_device *handle, RRHF_TCPInfo *info_out);

/**
 * NOT TESTED!
 * Change the reader's own IP address.
 * @param ip  4-byte IPv4 address (network byte order).
 * @return 0/−1.
 */
int NET_SetDeviceIP(hid_device *handle, const u8 ip[4]);

/**
 * NOT TESTED!
 * Change the server/host IP address the reader connects to.
 * @param ip  4-byte IPv4 address.
 * @return 0/−1.
 */
int NET_SetHostIP(hid_device *handle, const u8 ip[4]);

/**
 * NOT TESTED!
 * Change the server (listener) port.
 * @param port  Port number in host byte order.
 * @return 0/−1.
 */
int NET_SetServerPort(hid_device *handle, uint16_t port);

/**
 * NOT TESTED!
 * Change the client port.
 * @param port  Port number in host byte order.
 * @return 0/−1.
 */
int NET_SetClientPort(hid_device *handle, uint16_t port);

/**
 * NOT TESTED!
 * Change the default gateway.
 * @param gw  4-byte IPv4 address.
 * @return 0/−1.
 */
int NET_SetGateway(hid_device *handle, const u8 gw[4]);

/**
 * NOT TESTED!
 * Change the subnet mask.
 * @param mask  4-byte subnet mask.
 * @return 0/−1.
 */
int NET_SetSubnetMask(hid_device *handle, const u8 mask[4]);

/**
 * NOT TESTED!
 * Change the primary DNS server address.
 * @param dns  4-byte IPv4 address.
 * @return 0/−1.
 */
int NET_SetPrimaryDNS(hid_device *handle, const u8 dns[4]);

/**
 * NOT TESTED!
 * Change the secondary DNS server address.
 * @param dns  4-byte IPv4 address.
 * @return 0/−1.
 */
int NET_SetSecondaryDNS(hid_device *handle, const u8 dns[4]);

/**
 * NOT TESTED!
 * Change the device MAC address.
 * @param mac  6-byte MAC address.
 * @return 0/−1.
 */
int NET_SetMACAddress(hid_device *handle, const u8 mac[6]);

/**
 * NOT TESTED!
 * Change the NBIOS/NetBIOS name (up to 16 bytes).
 * @param name  Exactly 16 bytes (pad with zeros if shorter).
 * @return 0/−1.
 */
int NET_SetNBIOSName(hid_device *handle, const u8 name[16]);

/** Reset all TCP settings to the factory-saved defaults. @return 0/−1. */
int NET_ResetToDefaultTCP(hid_device *handle);

/**
 * NOT TESTED!
 * Get the factory-saved default TCP parameters.
 * @param info_out  Pointer to RRHF_TCPInfo to fill (no NBIOS in this response).
 * @return 0 on success, -1 on error.
 */
int NET_GetDefaultTCPInfo(hid_device *handle, RRHF_TCPInfo *info_out);

/**
 * NOT TESTED!
 * Save current TCP parameters as the new factory defaults.
 * @param info  Pointer to RRHF_TCPInfo containing the desired defaults.
 * @return 0 on success, -1 on error.
 */
int NET_SetDefaultTCPInfo(hid_device *handle, const RRHF_TCPInfo *info);

#endif
