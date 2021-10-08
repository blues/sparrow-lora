// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "board.h"
#include "main.h"
#include "app.h"

// Flash config device descriptor
typedef struct __attribute__((__packed__))
{
    uint16_t type;
    uint8_t address[ADDRESS_LEN];
    char name[SENSOR_NAME_MAX];
    uint8_t key[AES_KEY_BYTES];
}
peerConfig;

// Flash configuration header
#define FLASH_CONFIG_SIGNATURE_V1   0xF00DD00D
#define FLASH_CONFIG_SIGNATURE      FLASH_CONFIG_SIGNATURE_V1
#define MAX_PEERS                   150
typedef struct {
    uint32_t signature;             // Both signature and version overloaded
    uint32_t peers;                 // Number of peers located immediately after sizeof(flashConfig)
} flashConfig;

// Locations of config data
#define FLASH_CONFIG_BYTES          (FLASH_PAGE_SIZE*8)
#define FLASH_CONFIG_BASE_ADDRESS   ((FLASH_BASE+FLASH_SIZE)-FLASH_CONFIG_BYTES)
#define FLASH_PEER_CONFIG_ADDRESS   (FLASH_CONFIG_BASE_ADDRESS)
#define FLASH_PEER_CONFIG_BYTES     (sizeof(flashConfig))
#define FLASH_PEER_TABLE_ADDRESS    (FLASH_PEER_CONFIG_ADDRESS+FLASH_PEER_CONFIG_BYTES)
#define FLASH_PEER_ENTRY_BYTES      (sizeof(peerConfig))
#define FLASH_PEER_TABLE_BYTES      (MAX_PEERS*FLASH_PEER_ENTRY_BYTES)
#define FLASH_MAX_USED_BYTES        (FLASH_PEER_CONFIG_BYTES+FLASH_PEER_TABLE_BYTES)

// Locations of firmware
#define FLASH_CODE_PAGES            (((FLASH_SIZE-FLASH_CONFIG_BYTES)/2)/FLASH_PAGE_SIZE)
#define FLASH_CODE_MAX_BYTES        (FLASH_CODE_PAGES*FLASH_PAGE_SIZE)
#define FLASH_CODE_BASE             (FLASH_BASE)
#define FLASH_CODE_DFU_BASE         (FLASH_BASE+FLASH_CODE_MAX_BYTES)

// In-memory flash config
static flashConfig config = {0};
static peerConfig *peer = NULL;

// Macros
#define GMAX(x, y) (((x) > (y)) ? (x) : (y))
#define GMIN(x, y) (((x) < (y)) ? (x) : (y))
#define ROUND_DOWN(a, b) (((a) / (b)) * (b))

// Forwards
uint32_t FLASH_Init(void);
bool FLASH_write_at(uint32_t address, uint64_t *pData, uint32_t datalen);

// Get DFU-related flash parameters
void flashCodeParams(uint8_t **activeBase, uint8_t **dfuBase, uint32_t *maxBytes, uint32_t *maxPages)
{
    *activeBase = (uint8_t *) FLASH_CODE_BASE;
    *dfuBase = (uint8_t *) FLASH_CODE_DFU_BASE;
    *maxBytes = FLASH_CODE_MAX_BYTES;
    *maxPages = FLASH_CODE_PAGES;
}

// Returns HAL status
uint32_t FLASH_Init()
{
    bool success = false;

    // Unlock the Program memory
    if (HAL_FLASH_Unlock() == HAL_OK) {

        // Clear all FLASH flags
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR);

        // Re-lock the Program memory
        if (HAL_FLASH_Lock() == HAL_OK) {
            success = true;
        } else {
            APP_PRINTF("flash: init: failed to lock\r\n");
        }

    } else {

        APP_PRINTF("flash: init: failed to unlock\r\n");

    }

    // Done
    return success;

}

// Write to Flash memory
// address: destination address in flash memory
// pData: data to be written to flash. (Must be 8 byte aligned.)
// datalen   Number of bytes to be programmed.
// returns 0 (success) or -1 (failure)
bool FLASH_write_at(uint32_t address, uint64_t *pData, uint32_t datalen)
{
    int i;
    bool success = false;

    __disable_irq();

    for (i = 0; i < datalen; i += 8) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              address + i,
                              *(pData + (i/8) )) != HAL_OK) {
            __enable_irq();
            APP_PRINTF("flash: program error\r\n");
            break;
        }
    }

    // Memory check
    for (i = 0; i < datalen; i += 4) {
        uint32_t *dst = (uint32_t *)(address + i);
        uint32_t *src = ((uint32_t *) pData) + (i/4);
        if ( *dst != *src ) {
            __enable_irq();
            APP_PRINTF("flash: write failed\r\n");
            break;
        }
        success = true;
    }

    __enable_irq();
    return success;
}

// Perform startup duties
void flashDFUInit()
{
    APP_PRINTF("\r\n");
    APP_PRINTF("flash:  peers: %d\r\n", MAX_PEERS);
    APP_PRINTF("       config: %d bytes\r\n", FLASH_MAX_USED_BYTES);
    APP_PRINTF("               %d spare\r\n", FLASH_CONFIG_BYTES-FLASH_MAX_USED_BYTES);
    APP_PRINTF("         code: %d bytes\r\n", MX_Image_Size());
    APP_PRINTF("               %d pages\r\n", MX_Image_Pages());
    APP_PRINTF("          max: %d bytes\r\n", FLASH_CODE_MAX_BYTES);
    APP_PRINTF("               %d pages\r\n", FLASH_CODE_PAGES);
    APP_PRINTF("mem:     heap: %d bytes\r\n", MX_Heap_Size(NULL));
    APP_PRINTF("               %d actual\r\n", NoteMemAvailable());
    APP_PRINTF("\r\n");

}

// Write to flash, returning true if success
bool flashWrite(uint8_t *flashDest, void *source, uint32_t bytes)
{
    uint8_t *ramSource = source;
    bool success = true;
    int remaining = bytes;

    uint8_t *page_cache = malloc(FLASH_PAGE_SIZE);
    if (page_cache == NULL) {
        return false;
    }

    FLASH_Init();

    do {
        uint32_t fl_addr = ROUND_DOWN((uint32_t)flashDest, FLASH_PAGE_SIZE);
        int fl_offset = (uint32_t)flashDest - fl_addr;
        int len = GMIN(FLASH_PAGE_SIZE - fl_offset, bytes);

        // Load from the flash into the cache
        memcpy(page_cache, (void *) fl_addr, FLASH_PAGE_SIZE);
        // Update the cache from the source
        memcpy((uint8_t *)page_cache + fl_offset, ramSource, len);

        // Erase the page, and write the cache
        uint32_t PageError;
        FLASH_EraseInitTypeDef EraseInit = {0};
        EraseInit.NbPages = 1;
        EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
        EraseInit.Page = (fl_addr - FLASH_BASE) / FLASH_PAGE_SIZE;
        if (HAL_FLASH_Unlock() != HAL_OK) {
            APP_PRINTF("flash: error unlocking flash for Erase\r\n");
        }
        if (HAL_FLASHEx_Erase(&EraseInit, &PageError) != HAL_OK) {
            APP_PRINTF("flash: hal erase error\r\n");
            success = false;
        } else {
            if (!FLASH_write_at(fl_addr, (uint64_t *)page_cache, FLASH_PAGE_SIZE)) {
                APP_PRINTF("flash: retrying write error\r\n");
                if (!FLASH_write_at(fl_addr, (uint64_t *)page_cache, FLASH_PAGE_SIZE)) {
                    APP_PRINTF("flash: unrecoverable write error\r\n");
                    success = false;
                }
            }
            if (success) {
                flashDest += len;
                ramSource += len;
                remaining -= len;
            }
        }
    } while (success && (remaining > 0));

    // Done
    free(page_cache);
    return success;

}

// Get the number of peers
uint32_t flashConfigPeers()
{
    return config.peers;
}

// Read peer config
void flashConfigLoad()
{

    // Read the header
    memcpy(&config, (uint8_t *)FLASH_CONFIG_BASE_ADDRESS, sizeof(flashConfig));
    if (config.signature != FLASH_CONFIG_SIGNATURE) {
        config.signature = FLASH_CONFIG_SIGNATURE;
        config.peers = 0;
    }

    // Allocate peer buffer
    if (peer != NULL) {
        free(peer);
    }
    peer = (peerConfig *) malloc(config.peers * sizeof(peerConfig));
    if (peer == NULL) {
        config.peers = 0;
        APP_PRINTF("*** can't allocate peers - peer table reset ***\r\n");
        return;
    }
    memcpy(peer, (uint8_t *)FLASH_PEER_TABLE_ADDRESS, config.peers * sizeof(peerConfig));

}

// Update the config
bool flashConfigUpdate()
{

    // Update peer table
    if (!flashWrite((uint8_t *)FLASH_PEER_TABLE_ADDRESS, peer, config.peers * sizeof(peerConfig))) {
        APP_PRINTF("*** can't write peers ***\r\n");
        return false;
    }

    // Update header
    if (!flashWrite((uint8_t *)FLASH_CONFIG_BASE_ADDRESS, &config, sizeof(flashConfig))) {
        APP_PRINTF("*** can't write config ***\r\n");
        return false;
    }

    // Success
    return true;

}

// Clear the config and restart
void flashConfigFactoryReset()
{

    config.signature = 0;
    config.peers = 0;
    if (!flashWrite((uint8_t *)FLASH_CONFIG_BASE_ADDRESS, &config, sizeof(flashConfig))) {
        APP_PRINTF("*** can't reset config ***\r\n");
    }

    ledIndicateAck(3);

    NVIC_SystemReset();

}

// Find a peer, returning true if found
bool flashConfigFindPeerByType(uint16_t peertype, uint8_t *retAddress, uint8_t *retKey, char *retName)
{
    for (int i=0; i<config.peers; i++) {
        if ((peer[i].type & peertype) != 0) {
            if (retAddress != NULL) {
                memcpy(retAddress, peer[i].address, ADDRESS_LEN);
            }
            if (retKey != NULL) {
                memcpy(retKey, peer[i].key, AES_KEY_BYTES);
            }
            if (retName != NULL) {
                memcpy(retName, peer[i].name, SENSOR_NAME_MAX);
            }
            return true;
        }
    }
    return false;
}

// Find a peer by Address, returning true if found
bool flashConfigFindPeerByAddress(uint8_t *address, uint16_t *retPeerType, uint8_t *retKey, char *retName)
{
    for (int i=0; i<config.peers; i++) {
        if (memcmp(address, peer[i].address, ADDRESS_LEN) == 0) {
            if (retPeerType != NULL) {
                *retPeerType = peer->type;
            }
            if (retKey != NULL) {
                memcpy(retKey, peer[i].key, AES_KEY_BYTES);
            }
            if (retName != NULL) {
                memcpy(retName, peer[i].name, SENSOR_NAME_MAX);
            }
            return true;
        }
    }
    return false;
}

// Update the name of a sensor in-memory if the address matches at least the least significant bytes
// of the address specified, and return true if it is found and if it was changed.
bool flashConfigUpdatePeerName(uint8_t *address, uint8_t addressLen, char *name)
{
    for (int i=0; i<config.peers; i++) {
        if (memcmp(address, peer[i].address, addressLen) == 0) {
            if (strcmp(name, peer[i].name) == 0) {
                return false;
            }
            strlcpy(peer[i].name, name, sizeof(peer[i].name));
            return true;
        }
    }
    return false;
}

// Add a peer if it's not already there, and replace it if it's there
bool flashConfigUpdatePeer(uint16_t peertype, uint8_t *address, uint8_t *key)
{

    // Create a new entry
    peerConfig newEntry = {0};
    newEntry.type = peertype;
    memcpy(newEntry.address, address, sizeof(newEntry.address));
    memcpy(newEntry.key, key, sizeof(newEntry.key));

    // Find the peer
    peerConfig *entry = NULL;
    for (int i=0; i<config.peers; i++) {
        if (memcmp(address, peer[i].address, ADDRESS_LEN) == 0) {
            entry = &peer[i];
            memcpy(newEntry.name, entry->name, sizeof(newEntry.name));
            break;
        }
    }

    // If not present, grow, else update if different
    bool update = false;
    if (entry == NULL) {
        peerConfig *new = (peerConfig *) malloc((config.peers+1) * sizeof(peerConfig));
        if (new == NULL) {
            return false;
        }
        memcpy(new, peer, config.peers * sizeof(peerConfig));
        free(peer);
        peer = new;
        entry = &peer[config.peers++];
        update = true;
    } else {
        if (entry->type != newEntry.type) {
            update = true;
        }
        if (memcmp(entry->key, newEntry.key, sizeof(newEntry.key)) != 0) {
            update = true;
        }
    }

    // Update if appropriate
    if (update) {
        memcpy(entry, &newEntry, sizeof(newEntry));
        if (!flashConfigUpdate()) {
            APP_PRINTF("*** can't update config ***\r\n");
            return false;
        }
    }

    // Done
    return true;

}
