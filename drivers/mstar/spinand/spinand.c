//<MStar Software>
//******************************************************************************
// MStar Software
// Copyright (c) 2010 - 2012 MStar Semiconductor, Inc. All rights reserved.
// All software, firmware and related documentation herein ("MStar Software") are
// intellectual property of MStar Semiconductor, Inc. ("MStar") and protected by
// law, including, but not limited to, copyright law and international treaties.
// Any use, modification, reproduction, retransmission, or republication of all
// or part of MStar Software is expressly prohibited, unless prior written
// permission has been granted by MStar.
//
// By accessing, browsing and/or using MStar Software, you acknowledge that you
// have read, understood, and agree, to be bound by below terms ("Terms") and to
// comply with all applicable laws and regulations:
//
// 1. MStar shall retain any and all right, ownership and interest to MStar
//    Software and any modification/derivatives thereof.
//    No right, ownership, or interest to MStar Software and any
//    modification/derivatives thereof is transferred to you under Terms.
//
// 2. You understand that MStar Software might include, incorporate or be
//    supplied together with third party`s software and the use of MStar
//    Software may require additional licenses from third parties.
//    Therefore, you hereby agree it is your sole responsibility to separately
//    obtain any and all third party right and license necessary for your use of
//    such third party`s software.
//
// 3. MStar Software and any modification/derivatives thereof shall be deemed as
//    MStar`s confidential information and you agree to keep MStar`s
//    confidential information in strictest confidence and not disclose to any
//    third party.
//
// 4. MStar Software is provided on an "AS IS" basis without warranties of any
//    kind. Any warranties are hereby expressly disclaimed by MStar, including
//    without limitation, any warranties of merchantability, non-infringement of
//    intellectual property rights, fitness for a particular purpose, error free
//    and in conformity with any international standard.  You agree to waive any
//    claim against MStar for any loss, damage, cost or expense that you may
//    incur related to your use of MStar Software.
//    In no event shall MStar be liable for any direct, indirect, incidental or
//    consequential damages, including without limitation, lost of profit or
//    revenues, lost or damage of data, and unauthorized system use.
//    You agree that this Section 4 shall still apply without being affected
//    even if MStar Software has been modified by MStar in accordance with your
//    request or instruction for your use, except otherwise agreed by both
//    parties in writing.
//
// 5. If requested, MStar may from time to time provide technical supports or
//    services in relation with MStar Software to you for your use of
//    MStar Software in conjunction with your or your customer`s product
//    ("Services").
//    You understand and agree that, except otherwise agreed by both parties in
//    writing, Services are provided on an "AS IS" basis and the warranty
//    disclaimer set forth in Section 4 above shall apply.
//
// 6. Nothing contained herein shall be construed as by implication, estoppels
//    or otherwise:
//    (a) conferring any license or right to use MStar name, trademark, service
//        mark, symbol or any other identification;
//    (b) obligating MStar or any of its affiliates to furnish any person,
//        including without limitation, you and your customers, any assistance
//        of any kind whatsoever, or any information; or
//    (c) conferring any license or right under any intellectual property right.
//
// 7. These terms shall be governed by and construed in accordance with the laws
//    of Taiwan, R.O.C., excluding its conflict of law rules.
//    Any and all dispute arising out hereof or related hereto shall be finally
//    settled by arbitration referred to the Chinese Arbitration Association,
//    Taipei in accordance with the ROC Arbitration Law and the Arbitration
//    Rules of the Association by three (3) arbitrators appointed in accordance
//    with the said Rules.
//    The place of arbitration shall be in Taipei, Taiwan and the language shall
//    be English.
//    The arbitration award shall be final and binding to both parties.
//
//******************************************************************************
//<MStar Software>
#include <common.h>
#include <malloc.h>
#include <linux/err.h>
//#include <linux/compat.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <asm/io.h>
#include <asm/errno.h>
#include "MsCommon.h"
#include "MsTypes.h"
#include "spinand.h"

#define NAND_USE_FLASH_BBT	0x00010000
#define NAND_BBT_BLOCK_NUM  4

/* SPI NAND messages */

//	#define FPGA

static U32 u32_CurRow = 0;
static U32 u32_CurCol = 0;
/* These really don't belong here, as they are specific to the NAND Model */
static uint8_t scan_ff_pattern[] = {0xff};

/* struct nand_bbt_descr - bad block table descriptor */
static struct nand_bbt_descr spi_nand_bbt_descr = {
    .options = NAND_BBT_2BIT | NAND_BBT_LASTBLOCK | NAND_BBT_VERSION | NAND_BBT_CREATE | NAND_BBT_WRITE,
    .offs = 0,
    .len = 1,
    .pattern = scan_ff_pattern
};

static struct nand_ecclayout spi_nand_oobinfo = {
    .eccbytes = 32,
    .eccpos = {8, 9, 10, 11, 12, 13, 14, 15,
    24, 25, 26, 27, 28, 29, 30, 31,
    40, 41, 42, 43, 44, 45, 46, 47,
    56, 57, 58, 59, 60, 61, 62, 63},
    .oobavail = 30,
    .oobfree = {
    {2, 6},
    {16, 8},
    {32, 8},
    {48, 8},
    {0, 0}
    },
};

static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr spi_nand_bbt_main_descr = {
    .options		= NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
      NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
    .offs			= 1,
    .len			= 3,
    .veroffs		= 4,
    .maxblocks		= NAND_BBT_BLOCK_NUM,
    .pattern		= bbt_pattern
};

static struct nand_bbt_descr spi_nand_bbt_mirror_descr = {
    .options		= NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
      NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
    .offs			= 1,
    .len			= 3,
    .veroffs		= 4,
    .maxblocks		= NAND_BBT_BLOCK_NUM,
    .pattern		= mirror_pattern
};

SPI_NAND_DRIVER_t gtSpiNandDrv;
void *drvSPINAND_get_DrvContext_address(void) // exposed API
{
  return &gtSpiNandDrv;
}
//	void *drvSPINAND_get_DrvContext_PartInfo(void)
//	{
//	   U32 gau32_PartInfo[0x200/4];
//	   return (void*)((U32)gau32_PartInfo);
//	}


static U8 MDrv_SPINAND_CountBits(U32 u32_x)
{
    U8 u8_i = 0;

    while (u32_x) {
    u8_i++;
    u32_x >>= 1;
    }

    return u8_i-1;
}


uint8_t	spi_nand_read_byte(struct mtd_info *mtd)
{
    u8 u8_word;
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    spi_nand_debug(" spi_nand_read_byte \n");
    u8_word = pSpiNandDrv->pu8_sparebuf[pSpiNandDrv->u32_column];
    if (pSpiNandDrv->u8_statusRequest)
    {
        /*If write protect, the status will be 0x80. Normal will return 0x0. It revert wiht P_NAND. */
        /*See function nand_check_wp in nand_base.c */
        u8_word = ~(*(pSpiNandDrv->pu8_statusbuf));
    }
    pSpiNandDrv->u32_column++;
    return u8_word;
}

u16 spi_nand_read_word(struct mtd_info *mtd)
{
    u16 u16_word;
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    u16_word = ((u16)pSpiNandDrv->pu8_sparebuf[pSpiNandDrv->u32_column] | ((u16)pSpiNandDrv->pu8_sparebuf[pSpiNandDrv->u32_column+1]<<8));
    pSpiNandDrv->u32_column += 2;

    return u16_word;
}

void spi_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
    spi_nand_debug("spi_nand_write_buf Not support");
}

void spi_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
    spi_nand_debug("spi_nand_read_buf Not support");
}

//	int spi_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
//	{
//	    spi_nand_msg("not support");
//		return 0;
//	}

void spi_nand_select_chip(struct mtd_info *mtd, int chip)
{
    spi_nand_debug("spi_nand_select_chip  Not support");
}

void spi_nand_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
    spi_nand_debug("spi_nand_cmd_ctrl Not support");
}

int spi_nand_dev_ready(struct mtd_info *mtd)
{
    spi_nand_debug("spi_nand_dev_ready Not support");

    return 1;
}


void spi_nand_cmdfunc(struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
    U32 ret;
    int u32Ret;

    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    pSpiNandDrv->u8_statusRequest = FALSE;
   // U8 *pu8_statusbuf = pSpiNandDrv->pu8_statusbuf;
    switch (command) {
    case NAND_CMD_STATUS:
        spi_nand_debug("NAND_CMD_STATUS");
        pSpiNandDrv->u8_statusRequest = TRUE;
        u32Ret = MDrv_SPINAND_ReadStatusRegister(pSpiNandDrv->pu8_statusbuf, SPI_NAND_REG_PROT);
        if (u32Ret != ERR_SPINAND_SUCCESS)
            spi_nand_err(" ReadStatusRegister != ERR_SPINAND_SUCCESS~!  %d \n", u32Ret);
    break;

    case NAND_CMD_PAGEPROG:
        spi_nand_debug("NAND_CMD_PAGEPROG");
    break;

    case NAND_CMD_READOOB:
        spi_nand_debug("NAND_CMD_READOOB");
        u32Ret=MDrv_SPINAND_Read(page_addr, (U8 *)pSpiNandDrv->pu8_pagebuf, (U8 *)pSpiNandDrv->pu8_sparebuf);
        pSpiNandDrv->u32_column = column;
        if (u32Ret != ERR_SPINAND_SUCCESS)
            spi_nand_err(" MDrv_SPINAND_Read != ERR_SPINAND_SUCCESS~!  %d \n", u32Ret);
    break;

    case NAND_CMD_READID:
        spi_nand_debug("NAND_CMD_READID");
        if(MDrv_SPINAND_Init(&(pSpiNandDrv->tSpinandInfo)) != TRUE)
        {
            spi_nand_err("MDrv_SPINAND_Init fail");
        }
        memcpy((void *)pSpiNandDrv->pu8_sparebuf, (const void *)pSpiNandDrv->tSpinandInfo.au8_ID,SPINAND_ID_SIZE);
    break;

    case NAND_CMD_ERASE2:
        spi_nand_debug("NAND_CMD_ERASE2");
    break;

    case NAND_CMD_ERASE1:
        spi_nand_debug("NAND_CMD_ERASE1, page_addr: 0x%X\n", page_addr);
        pSpiNandDrv->u8_status = NAND_STATUS_READY|NAND_STATUS_TRUE_READY;
        ret = MDrv_SPINAND_BLOCK_ERASE(page_addr);
        if(ret != ERR_SPINAND_SUCCESS)
        {
            spi_nand_err("MDrv_SPINAND_Erase=%ld", ret);
            pSpiNandDrv->u8_status |= NAND_STATUS_FAIL;
        }
    break;

    case NAND_CMD_READ0:
        spi_nand_debug("NAND_CMD_READ0");
        u32_CurRow = page_addr;
        u32_CurCol = column;
    break;

    case NAND_CMD_SEQIN:
        spi_nand_debug("NAND_CMD_SEQIN");
        /* send PAGE_PROG command(0x1080) */
        u32_CurRow = page_addr;
        u32_CurCol = column;
    break;

    default:
        spi_nand_err("unsupported command %02Xh\n", command);
    break;
    }
    return;
}

int spi_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    spi_nand_debug("spi_nand_waitfunc");

    return (int)pSpiNandDrv->u8_status;
}
/**
 * nand_write_page - write one page
 * @mtd: MTD device structure
 * @chip: NAND chip descriptor
 * @buf: the data to write
 * @oob_required: must write chip->oob_poi to OOB
 * @page: page number to write
 * @cached: cached programming
 * @raw: use _raw version of write_page
 */
//int spi_nand_write_page(struct mtd_info *mtd,
//                    struct nand_chip *chip, const uint8_t *buf,
//                    int page, int cached, int raw)

int spi_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
       uint32_t offset, int data_len,
       const uint8_t *buf, int oob_required,
       int page, int cached, int raw)
{
    U32 ret;

    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
//	    U8 *pu8_MainBuf = pSpiNandDrv->pu8_pagebuf;
//	    U8 *pu8_SpareBuf = pSpiNandDrv->pu8_sparebuf;


    spi_nand_debug("spi_nand_write_page 0x%X", page);

    ret = MDrv_SPINAND_Write(page,(U8*)buf, pSpiNandDrv->pu8_sparebuf);
    if(ret != ERR_SPINAND_SUCCESS)
    {
        spi_nand_err("MDrv_SPINAND_Write=%ld\n", ret);
        return -EIO;
    }

    return 0;
}

void spi_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{
    spi_nand_debug(" spi_nand_ecc_hwctl Not support");
}

int spi_nand_ecc_calculate(struct mtd_info *mtd, const uint8_t *dat, uint8_t *ecc_code)
{
    spi_nand_debug("spi_nand_ecc_calculate Not support");

    return 0;
}

int spi_nand_ecc_correct(struct mtd_info *mtd, uint8_t *dat, uint8_t *read_ecc, uint8_t *calc_ecc)
{
    spi_nand_debug(" spi_nand_ecc_correct Not support");

    return 0;
}

int spi_nand_ecc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
    uint8_t *buf, int oob_required, int page)
{
    U32 ret;
    ret = MDrv_SPINAND_Read(page, (U8 *)buf, (U8 *)chip->oob_poi);
    if(ret != ERR_SPINAND_SUCCESS && ret != ERR_SPINAND_ECC_BITFLIP)
    {
        spi_nand_err("MDrv_SPINAND_Read=%ld", ret);
    }
    if(ret == ERR_SPINAND_ECC_BITFLIP)
    {
        mtd->ecc_stats.corrected += 1;
    }

    return 0;
}

int spi_nand_ecc_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
    spi_nand_msg("spi_nand_ecc_write_page_raw NOT support");
    return 0;
}


int spi_nand_ecc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
    uint8_t *buf, int oob_required, int page)
{
    U32 ret;

    ret = MDrv_SPINAND_Read(page, (U8 *)buf, (U8 *)chip->oob_poi);
    if(ret != ERR_SPINAND_SUCCESS && ret != ERR_SPINAND_ECC_BITFLIP)
    {
        spi_nand_err("MDrv_SPINAND_Read=%ld", ret);
        mtd->ecc_stats.failed++;
    }
    if(ret == ERR_SPINAND_ECC_BITFLIP)
    {
        mtd->ecc_stats.corrected += 1;
    }

    return 0;
}

int spi_nand_ecc_read_subpage(struct mtd_info *mtd, struct nand_chip *chip,
    uint32_t offs, uint32_t len, uint8_t *buf, int page)
{
    U32 ret = 0;
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    if(offs + len > pSpiNandDrv->tSpinandInfo.u16_PageByteCnt){
        spi_nand_err("read len exceeds page len\n");
    return -EINVAL;
    }

    buf += offs;

    ret = MDrv_SPINAND_Read_RandomIn(u32_CurRow, offs, len, buf);

    if(ret == ERR_SPINAND_SUCCESS)
    {
    }
    else if(ret == ERR_SPINAND_ECC_ERROR)
        mtd->ecc_stats.failed++;
    else if(ret == ERR_SPINAND_ECC_BITFLIP)
        mtd->ecc_stats.corrected += 1;
    else
    return -EIO;

    return  0;
}

int spi_nand_ecc_write_page(struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
    spi_nand_msg("spi_nand_ecc_write_page Not support");
    return  0;
}

int spi_nand_ecc_read_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
    U32 ret;
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    ret = MDrv_SPINAND_Read(page, (U8 *)pSpiNandDrv->pu8_pagebuf, (U8 *)chip->oob_poi);
    if(ret != ERR_SPINAND_SUCCESS)
    {
        spi_nand_err("MDrv_SPINAND_Read=%ld", ret);
    }

    return 0;
}

int spi_nand_ecc_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
    U32 ret;
    spi_nand_msg("spi_nand_ecc_write_oob\n");
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    memset((void *)pSpiNandDrv->pu8_pagebuf, 0xFF, mtd->writesize);
    ret = MDrv_SPINAND_Write(page, (U8 *)pSpiNandDrv->pu8_pagebuf, (U8 *)chip->oob_poi);
    if(ret != ERR_SPINAND_SUCCESS)
    {
        spi_nand_err("MDrv_SPINAND_Write=%ld", ret);
        return -EIO;
    }

    return 0;
}

static U32 CheckSum(U8 *pu8_Data, U16 u16_ByteCnt)
{
    U32 u32_Sum = 0;

    while (u16_ByteCnt--)
    u32_Sum += *pu8_Data++;

    return u32_Sum;
}

void setNandInformation(void)
{
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    pSpiNandDrv->u8_SectorByteCntBits = MDrv_SPINAND_CountBits(pSpiNandDrv->tSpinandInfo.u16_SectorByteCnt);
    pSpiNandDrv->u16_PageSectorCnt = pSpiNandDrv->tSpinandInfo.u16_PageByteCnt >> pSpiNandDrv->u8_SectorByteCntBits;
    pSpiNandDrv->u8_PageSectorCntBits = MDrv_SPINAND_CountBits(pSpiNandDrv->u16_PageSectorCnt);
    pSpiNandDrv->u8_PageByteCntBits = MDrv_SPINAND_CountBits(pSpiNandDrv->tSpinandInfo.u16_PageByteCnt);
    pSpiNandDrv->u8_BlkPageCntBits = MDrv_SPINAND_CountBits(pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt);

    pSpiNandDrv->u8_status = NAND_STATUS_READY|NAND_STATUS_TRUE_READY;
    pSpiNandDrv->u32_column = 0;
    pSpiNandDrv->pu8_pagebuf = kmalloc(pSpiNandDrv->tSpinandInfo.u16_PageByteCnt, GFP_KERNEL);
    pSpiNandDrv->pu8_sparebuf = kmalloc(pSpiNandDrv->tSpinandInfo.u16_SpareByteCnt, GFP_KERNEL);
    pSpiNandDrv->pu8_statusbuf = kmalloc(16, GFP_KERNEL);
}
void dumpNandInformation(void)
{
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
//	    printf("u8_SectorByteCntBits %d\n", pSpiNandDrv->u8_SectorByteCntBits);
//	    printf("u8_PageSectorCntBits %d\n", pSpiNandDrv->u8_PageSectorCntBits);
//	    printf("u8_PageByteCntBits %d\n", pSpiNandDrv->u8_PageByteCntBits);
//	    printf("u8_BlkPageCntBits %d\n", pSpiNandDrv->u8_BlkPageCntBits);
    printf("Bytes / Page :  %d\n", pSpiNandDrv->tSpinandInfo.u16_PageByteCnt);
    printf("Pages / Block: %d\n", pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt);
    printf("Sector/ Page :  %d\n", pSpiNandDrv->u16_PageSectorCnt);
    printf("Spare / Page :  %d\n", pSpiNandDrv->tSpinandInfo.u16_SpareByteCnt);


}
/*
 * Board-specific NAND initialization.
 * - hwcontrol: hardwarespecific function for accesing control-lines
 * - dev_ready: hardwarespecific function for  accesing device ready/busy line
 * - eccmode: mode of ecc, see defines
 */
int board_nand_init(struct nand_chip *nand)
{
    U8 u8_i;
    U32 u32_ret;
//	    struct mtd_info *mtd;
    PARTITION_INFO_t *ptPartInfo;

    int partinfoAddr = 1; //Partition Information is in the page 1
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    spi_nand_msg("here");

    if(MDrv_SPINAND_Init(&(pSpiNandDrv->tSpinandInfo)) != TRUE)
    {
        spi_nand_err("MDrv_SPINAND_Init fail");
        return -1;
    }

    setNandInformation();
//	    dumpNandInformation();

    if(!pSpiNandDrv->pu8_pagebuf || !pSpiNandDrv->pu8_sparebuf)
    {
        spi_nand_err("Can not alloc memory for page/spare buffer");
        return -1;
    }

    ptPartInfo = (PARTITION_INFO_t *)pSpiNandDrv->pu8_pagebuf;
    for(u8_i = 0 ; u8_i < 10 ; u8_i += 2)
    {
        u32_ret = MDrv_SPINAND_Read(u8_i*pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt, pSpiNandDrv->pu8_pagebuf, pSpiNandDrv->pu8_sparebuf);
        if(u32_ret == ERR_SPINAND_SUCCESS)
        {
            if(memcmp((const void *) pSpiNandDrv->pu8_pagebuf, SPINAND_FLASH_INFO_TAG, 16) == 0)
            {
                //Partition Information is in the page 1
                u32_ret = MDrv_SPINAND_Read((u8_i * pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt) + partinfoAddr, pSpiNandDrv->pu8_pagebuf, pSpiNandDrv->pu8_sparebuf);
                if(u32_ret == ERR_SPINAND_SUCCESS)
                {
                    if(ptPartInfo->u32_ChkSum == CheckSum((u8*)&(ptPartInfo->u16_SpareByteCnt), 0x200 - 0x04))
                    break;
                }
            }

        }
        else
        {
            spi_nand_err("Can't search CIS in no_block: %d",u8_i);
        }
    }
    if(u8_i == 10)
    {
        spi_nand_err("CIS doesn't contain part info");
        pSpiNandDrv->u8_HasPNI = 0;
    }
    else
    {
        spi_nand_msg("CIS contains part info");
        pSpiNandDrv->u8_HasPNI = 1;
        memcpy((void *)&pSpiNandDrv->tPartInfo, (const void *) ptPartInfo, 0x200);
    }

    /* please refer to include/linux/nand.h for more info. */
    nand->read_byte = spi_nand_read_byte;
    nand->read_word = spi_nand_read_word;
    nand->write_buf = spi_nand_write_buf;
    nand->read_buf = spi_nand_read_buf;
/*		nand->verify_buf = spi_nand_verify_buf;*/
    nand->select_chip = spi_nand_select_chip;
    nand->cmd_ctrl = spi_nand_cmd_ctrl;
    nand->dev_ready = spi_nand_dev_ready;
    nand->cmdfunc = spi_nand_cmdfunc;
    nand->waitfunc = spi_nand_waitfunc;
    nand->write_page = spi_nand_write_page;
    nand->scan_bbt = nand_default_bbt;
    nand->options = NAND_USE_FLASH_BBT;
    nand->chip_delay = 0;
    nand->badblock_pattern = &spi_nand_bbt_descr; //using default badblock pattern.
    nand->bbt_td = &spi_nand_bbt_main_descr;
    nand->bbt_md = &spi_nand_bbt_mirror_descr;

    /*nand->require_randomizer = 0;
    nand->ecc_corretable_bit = 8;
    nand->ecc_bitflip_threshold = 2;
    */
    nand->ecc.mode = NAND_ECC_HW;
    nand->ecc.size = pSpiNandDrv->tSpinandInfo.u16_PageByteCnt;
    nand->ecc.bytes = (pSpiNandDrv->tSpinandInfo.u16_SpareByteCnt>>1);
//	    nand->ecc.sectors = 4;
    nand->ecc.layout =  &spi_nand_oobinfo;

    nand->ecc.hwctl = spi_nand_ecc_hwctl;
    nand->ecc.calculate = spi_nand_ecc_calculate;
    nand->ecc.correct = spi_nand_ecc_correct;
    nand->ecc.read_page_raw = spi_nand_ecc_read_page_raw;
    nand->ecc.write_page_raw = spi_nand_ecc_write_page_raw;
    nand->ecc.read_page = spi_nand_ecc_read_page;
    nand->ecc.read_subpage = spi_nand_ecc_read_subpage;
    nand->ecc.write_page = spi_nand_ecc_write_page;
    nand->ecc.read_oob = spi_nand_ecc_read_oob;
    nand->ecc.write_oob = spi_nand_ecc_write_oob;
//    mtd->_read_oob = spi_nand_ecc_read_oob;


    spi_nand_msg("board_SPI_nand_init end");

    return 0;
}

void dumpInformation(void)
{
    SPI_NAND_DRIVER_t *pSpiNandDrv = (SPI_NAND_DRIVER_t*)drvSPINAND_get_DrvContext_address();
    printk("u8_SectorByteCntBits %d\n", pSpiNandDrv->u8_SectorByteCntBits);
    printk("u16_PageSectorCnt %d\n", pSpiNandDrv->u16_PageSectorCnt);
    printk("u8_PageSectorCntBits %d\n", pSpiNandDrv->u8_PageSectorCntBits);
    printk("u8_PageByteCntBits %d\n", pSpiNandDrv->u8_PageByteCntBits);
    printk("u8_BlkPageCntBits %d\n", pSpiNandDrv->u8_BlkPageCntBits);
    printk("pSpiNandDrv->tSpinandInfo.u16_PageByteCnt %d\n", pSpiNandDrv->tSpinandInfo.u16_PageByteCnt);
    printk("pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt %d\n", pSpiNandDrv->tSpinandInfo.u16_BlkPageCnt);
    printk("pSpiNandDrv->tSpinandInfo.u16_SpareByteCnt %d\n", pSpiNandDrv->tSpinandInfo.u16_SpareByteCnt);
    printk("au8_ID[0]=%x ", pSpiNandDrv->tSpinandInfo.au8_ID[0]);
    printk("au8_ID[1]=%x ", pSpiNandDrv->tSpinandInfo.au8_ID[1]);
    printk("\n");

}
