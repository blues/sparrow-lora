// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "stm32wlxx_hal.h"

// This is the function that copies new firmware into the primary firmware
// execution area.  This function MUST live in "page 1" of the Flash, and
// this function *never* copies page 1 of the flash so that it doesn't
// overwrite itself.  Thus, the contract that this firmware makes with
// future versions of itself is that it will always, always have this
// method in page 1.  Period.
//
// This is the one and only function published by this source file.  The
// reason we don't use any #includes above is to prevent one from inadvertently
// referencing some other method that isn't in this Flash Page 1.
void dfuLoader(uint8_t *flashDest, uint8_t *source, uint32_t pages);

// Ensure that this code is put into the linker section because it will remain in-place
// and won't be overwritten by itself.  Note that if you define DEPENDENCY_TEST this
// will temporarily define these all as ramfunc's, which has the benefit that you
// will get a linker warning if anything that these depend upon are in flash.  Just
// remember to remove the symbol after validation.
#ifdef DEPENDENCY_TEST
#define DFULOADER_FUNC __ramfunc
#else
#define DFULOADER_FUNC __attribute__((__section__(".dfuloader")))
#endif

// Forwards to local copies of HAL methods
static void local__NVIC_SystemReset(void);
static void local_HAL_FLASH_Unlock(void);
static void local_HAL_FLASHEx_Erase(uint16_t page, uint16_t pages);
static void local_FLASH_WaitForLastOperation(void);
static void local_FLASH_PageErase(uint32_t Page);
static void local_FLASH_AcknowledgePageErase(void);
static void local_FLASH_FlushCaches(void);
static void local_HAL_FLASH_Program(uint32_t Address, uint64_t Data);
static void local_FLASH_Program_DoubleWord(uint32_t Address, uint64_t Data);

// Transfer pages from source to destination, safely skipping THIS code's page.  The src and dst
// must be aligned on a page boundary.
void DFULOADER_FUNC dfuLoader(uint8_t *flashDst, uint8_t *flashSrc, uint32_t pages)
{

    // Disable interrupts, because we'll be stepping on the interrupt vectors
    __disable_irq();

    // Unlock the program memory
    local_HAL_FLASH_Unlock();

    // Clear all FLASH flags
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR);

    // Loop, copying pages
    for (size_t page=0; page<pages; page++) {

        // Skip the current page that contains this code
        if (page == 1) {
            continue;
        }

        // Erase the page
        local_HAL_FLASHEx_Erase(page, 1);

        // Program the page
        uint64_t *sourceDoubleWord = (uint64_t *) (flashSrc + (FLASH_PAGE_SIZE * page));
        uint8_t *destPageBase = (uint8_t *) (flashDst + (FLASH_PAGE_SIZE * page));
        for (size_t i=0; i<FLASH_PAGE_SIZE; i+=8) {
            local_HAL_FLASH_Program((uint32_t)(&destPageBase[i]), sourceDoubleWord[i/8]);
        }

    }

    // Restart and run the new firmware
    local__NVIC_SystemReset();

}

// Reboot
static void DFULOADER_FUNC local__NVIC_SystemReset(void)
{
    __DSB();                                                        /* Ensure all outstanding memory accesses included
                                                                       buffered write are completed before reset */
    SCB->AIRCR  = (uint32_t)((0x5FAUL << SCB_AIRCR_VECTKEY_Pos)    |
                             (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
                             SCB_AIRCR_SYSRESETREQ_Msk    );        /* Keep priority group unchanged */
    __DSB();                                                        /* Ensure completion of memory access */
    for(;;) {                                                       /* wait until reset */
        __NOP();
    }
}

// Unlock for programming
static void DFULOADER_FUNC local_HAL_FLASH_Unlock(void)
{
    while (READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U) {
        /* Authorize the FLASH Registers access */
        WRITE_REG(FLASH->KEYR, FLASH_KEY1);
        WRITE_REG(FLASH->KEYR, FLASH_KEY2);
    }
}

// Erase the specified FLASH memory pages
static void DFULOADER_FUNC local_HAL_FLASHEx_Erase(uint16_t page, uint16_t pages)
{

    /* Verify that next operation can be proceed */
    local_FLASH_WaitForLastOperation();

    /* Erase the pages */
    uint32_t index;
    for (index = page; index < (page + pages); index++) {
        /* Start erase page */
        local_FLASH_PageErase(index);

        /* Wait for last operation to be completed */
        local_FLASH_WaitForLastOperation();

    }

    /* If operation is completed or interrupted, disable the Page Erase Bit */
    local_FLASH_AcknowledgePageErase();

    /* Flush the caches to be sure of the data consistency */
    local_FLASH_FlushCaches();

}

// Wait for a FLASH operation to complete.
static void DFULOADER_FUNC local_FLASH_WaitForLastOperation(void)
{

    /* Wait for the FLASH operation to complete by polling on BUSY flag to be reset.
       Even if the FLASH operation fails, the BUSY flag will be reset and an error
       flag will be set */
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) ;

    /* check flash errors. Only ECC correction can be checked here as ECCD
       generates NMI */
#ifdef CORE_CM0PLUS
    uint32_t error = FLASH->C2SR;
#else
    uint32_t error = FLASH->SR;
#endif

    /* Check FLASH End of Operation flag */
    if ((error & FLASH_FLAG_EOP) != 0U) {
        /* Clear FLASH End of Operation pending bit */
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
    }

    /* clear error flags */
    __HAL_FLASH_CLEAR_FLAG(error);

    /* Wait for control register to be written */
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_CFGBSY)) ;

}

// Erase the specified FLASH memory page.
static void DFULOADER_FUNC local_FLASH_PageErase(uint32_t Page)
{
#ifdef CORE_CM0PLUS
    MODIFY_REG(FLASH->C2CR, FLASH_CR_PNB, ((Page << FLASH_CR_PNB_Pos) | FLASH_CR_PER | FLASH_CR_STRT));
#else
    MODIFY_REG(FLASH->CR, FLASH_CR_PNB, ((Page << FLASH_CR_PNB_Pos) | FLASH_CR_PER | FLASH_CR_STRT));
#endif
}

// Acknlowldge the page erase operation.
static void DFULOADER_FUNC local_FLASH_AcknowledgePageErase(void)
{
#ifdef CORE_CM0PLUS
    CLEAR_BIT(FLASH->C2CR, (FLASH_CR_PER | FLASH_CR_PNB));
#else
    CLEAR_BIT(FLASH->CR, (FLASH_CR_PER | FLASH_CR_PNB));
#endif
}

// Flush the instruction and data caches.
static void DFULOADER_FUNC local_FLASH_FlushCaches(void)
{
    /* Flush instruction cache  */
    if (READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) == 1U) {
        /* Disable instruction cache  */
        __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
        /* Reset instruction cache */
        __HAL_FLASH_INSTRUCTION_CACHE_RESET();
        /* Enable instruction cache */
        __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    }

#ifdef CORE_CM0PLUS
#else
    /* Flush data cache */
    if (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) == 1U) {
        /* Disable data cache  */
        __HAL_FLASH_DATA_CACHE_DISABLE();
        /* Reset data cache */
        __HAL_FLASH_DATA_CACHE_RESET();
        /* Enable data cache */
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
#endif
}

// Program double word or fast program of a row at a specified address.
static void DFULOADER_FUNC local_HAL_FLASH_Program(uint32_t Address, uint64_t Data)
{
    /* Verify that next operation can be proceed */
    local_FLASH_WaitForLastOperation();
    /* Program double-word (64-bit) at a specified address */
    local_FLASH_Program_DoubleWord(Address, Data);

    /* Wait for last operation to be completed */
    local_FLASH_WaitForLastOperation();

    /* If the program operation is completed, disable the PG or FSTPG Bit */
#ifdef CORE_CM0PLUS
    CLEAR_BIT(FLASH->C2CR, FLASH_CR_PG);
#else
    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
#endif
}

// Program double-word (64-bit) at a specified address.
static void DFULOADER_FUNC local_FLASH_Program_DoubleWord(uint32_t Address, uint64_t Data)
{
#ifdef CORE_CM0PLUS
    /* Set PG bit */
    SET_BIT(FLASH->C2CR, FLASH_CR_PG);
#else
    /* Set PG bit */
    SET_BIT(FLASH->CR, FLASH_CR_PG);
#endif

    /* Program first word */
    *(uint32_t *)Address = (uint32_t)Data;

    /* Barrier to ensure programming is performed in 2 steps, in right order
      (independently of compiler optimization behavior) */
    __ISB();

    /* Program second word */
    *(uint32_t *)(Address + 4U) = (uint32_t)(Data >> 32U);
}
