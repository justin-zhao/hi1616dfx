#include "djtag.h"

/* define system control for every totem of every socket */
#define S0_TA_SC            0x40010000
#define S0_TB_SC            0x60010000
#define S1_TA_SC            0x40040010000
#define S1_TB_SC            0x40060010000
#define S2_TA_SC            0x80040010000
#define S2_TB_SC            0x80060010000
#define S3_TA_SC            0xC0040010000
#define S3_TB_SC            0xC0060010000
/* define sub control for every nimbus of every socket */
#define S0_NA_SC            0xD0000000
#define S0_NB_SC            0x8D0000000
#define S1_NA_SC            0x400D0000000
#define S1_NB_SC            0x408D0000000
#define S2_NA_SC            0x800D0000000
#define S2_NB_SC            0x808D0000000
#define S3_NA_SC            0xC00D0000000
#define S3_NB_SC            0xC08D0000000

/* define DDR base addr */
#define S0_TA_DDR0_ADDR     0x40348000
#define S0_TA_DDR1_ADDR     0x40358000
#define S0_TB_DDR0_ADDR     0x60348000
#define S0_TB_DDR1_ADDR     0x60358000
#define S1_TA_DDR0_ADDR     0x40040348000
#define S1_TA_DDR1_ADDR     0x40040358000
#define S1_TB_DDR0_ADDR     0x40060348000
#define S1_TB_DDR1_ADDR     0x40060358000
#define S2_TA_DDR0_ADDR     0x80040348000
#define S2_TA_DDR1_ADDR     0x80040358000
#define S2_TB_DDR0_ADDR     0x80060348000
#define S2_TB_DDR1_ADDR     0x80060358000
#define S3_TA_DDR0_ADDR     0xC0040348000
#define S3_TA_DDR1_ADDR     0xC0040358000
#define S3_TB_DDR0_ADDR     0xC0060348000
#define S3_TB_DDR1_ADDR     0xC0060358000

/* 根据socket的数目，定义不同的数组(DJTAG_DIE_NUM仅仅指TOTEM或者NIMBUS上的数目) */
#if (SOCKET_NUMBER == 1)
addr_t tt_sc_phy[DJTAG_DIE_NUM] = {S0_TA_SC,S0_TB_SC};
addr_t ni_sc_phy[DJTAG_DIE_NUM] = {S0_NA_SC,S0_NB_SC};
addr_t ddr_phy_base[DJTAG_DIE_NUM * 2] = {S0_TA_DDR0_ADDR,S0_TA_DDR1_ADDR,
       S0_TB_DDR0_ADDR,S0_TB_DDR1_ADDR};
char name[][20] = {"Socket0_TA","Socket0_TB"};
char ni_info[][20] = {"Socket0_NA","Socket0_NB"};
#elif (SOCKET_NUMBER == 2)
addr_t tt_sc_phy[DJTAG_DIE_NUM] = {S0_TA_SC,S0_TB_SC,
    S1_TA_SC,S1_TB_SC};
addr_t ni_sc_phy[DJTAG_DIE_NUM] = {S0_NA_SC,S0_NB_SC,
    S1_NA_SC,S1_NB_SC};
addr_t ddr_phy_base[DJTAG_DIE_NUM * 2] = {S0_TA_DDR0_ADDR,S0_TA_DDR1_ADDR,
       S0_TB_DDR0_ADDR,S0_TB_DDR1_ADDR,S1_TA_DDR0_ADDR,S1_TA_DDR1_ADDR,
       S1_TB_DDR0_ADDR,S1_TB_DDR1_ADDR};    
char name[][20] = {"Socket0_TA","Socket0_TB","Socket1_TA","Socket1_TB"};    
char ni_info[][20] = {"Socket0_NA","Socket0_NB","Socket1_NA","Socket1_NB"}; 
#elif (SOCKET_NUMBER == 4)
#define DJTAG_DIE_NUM       0x8
addr_t tt_sc_phy[DJTAG_DIE_NUM] = {S0_TA_SC,S0_TB_SC,
    S1_TA_SC,S1_TB_SC,S2_TA_SC,S2_TB_SC,S3_TA_SC,
    S3_TB_SC};
addr_t ni_sc_phy[DJTAG_DIE_NUM] = {S0_NA_SC,S0_NB_SC,
    S1_NA_SC,S1_NB_SC,S2_NA_SC,S2_NB_SC,S3_NA_SC,
    S3_NB_SC};
addr_t ddr_phy_base[DJTAG_DIE_NUM * 2] = {S0_TA_DDR0_ADDR,S0_TA_DDR1_ADDR,
       S0_TB_DDR0_ADDR,S0_TB_DDR1_ADDR,S1_TA_DDR0_ADDR,S1_TA_DDR1_ADDR,
       S1_TB_DDR0_ADDR,S1_TB_DDR1_ADDR,S2_TA_DDR0_ADDR,S2_TA_DDR1_ADDR,
       S2_TB_DDR0_ADDR,S2_TB_DDR1_ADDR,S3_TA_DDR0_ADDR,S3_TA_DDR1_ADDR,
       S3_TB_DDR0_ADDR,S3_TB_DDR1_ADDR};     
char name[][20] = {"Socket0_TA","Socket0_TB","Socket1_TA","Socket1_TB",
                   "Socket2_TA","Socket2_TB","Socket3_TA","Socket3_TB"};  
char ni_info[][20] = {"Socket0_NA","Socket0_NB","Socket1_NA","Socket1_NB",
                   "Socket2_NA","Socket2_NB","Socket3_NA","Socket3_NB"};                      
#endif

addr_t totem_djtag_base[DJTAG_DIE_NUM];
addr_t nimbus_djtag_base[DJTAG_DIE_NUM];
addr_t ddr_base[DJTAG_DIE_NUM * 2];
         

unsigned int djtag_readreg(u64 base,u32 module_sel,u32 offset,u32 cfg_en)
{
    unsigned int i = 0, value;
    T2_DJTAG_MSTR_CFG_U MstrCfg;
    T2_DJTAG_OP_ST_U OpSt;
    T2_DJTAG_MSTR_START_EN_U MstrStart;

    /* set offset */
    OS_WRITE_REG(base + T2_DJTAG_MSTR_ADDR_REG,0,offset);
    MstrCfg.u32 = OS_READ_REG(base + T2_DJTAG_MSTR_CFG_REG,0);
    MstrCfg.bits.chain_unit_cfg_en = cfg_en;
    MstrCfg.bits.debug_module_sel = module_sel;
    MstrCfg.bits.djtag_mstr_wr = 0;
    MstrCfg.bits.djtag_nor_cfg_en = 1;
    MstrCfg.bits.djtag_mstr_disable = 0; 
    OS_WRITE_REG(base + T2_DJTAG_MSTR_CFG_REG,0,MstrCfg.u32);
    OS_WRITE_REG(base + T2_DJTAG_MSTR_START_EN_REG,0,0x1);
    do{
        MstrStart.u32 = OS_READ_REG(base + T2_DJTAG_MSTR_START_EN_REG,0);
    }while(1 == MstrStart.bits.djtag_mstr_start_en);

    do{
        OpSt.u32 = OS_READ_REG(base + T2_DJTAG_OP_ST_REG,0);
    }while(0 == OpSt.bits.djtag_op_done);
    
    while(1 != cfg_en){
        cfg_en = (cfg_en >> 1);
        i++;
    }

    value = OS_READ_REG((base + T2_DJTAG_RD_DATA0_REG + i * 0x4),0);

    return value;
}

unsigned int djtag_writereg(u64 base,u32 module_sel,u32 offset,u32 cfg_en, u32 value)
{
    T2_DJTAG_MSTR_CFG_U MstrCfg;
    T2_DJTAG_OP_ST_U OpSt;
    T2_DJTAG_MSTR_START_EN_U MstrStart;

    /* set offset */
    OS_WRITE_REG(base + T2_DJTAG_MSTR_ADDR_REG,0,offset);
    MstrCfg.u32 = OS_READ_REG(base + T2_DJTAG_MSTR_CFG_REG,0);
    MstrCfg.bits.chain_unit_cfg_en = cfg_en;
    MstrCfg.bits.debug_module_sel = module_sel;
    MstrCfg.bits.djtag_mstr_wr = 1;
    MstrCfg.bits.djtag_nor_cfg_en = 1;
    MstrCfg.bits.djtag_mstr_disable = 0; 
    OS_WRITE_REG(base + T2_DJTAG_MSTR_CFG_REG,0,MstrCfg.u32);
    OS_WRITE_REG(base + T2_DJTAG_MSTR_DATA_REG,0x0,value);
    OS_WRITE_REG(base + T2_DJTAG_MSTR_START_EN_REG,0,0x1);
    do{
        MstrStart.u32 = OS_READ_REG(base + T2_DJTAG_MSTR_START_EN_REG,0);
    }while(1 == MstrStart.bits.djtag_mstr_start_en);

    do{
        OpSt.u32 = OS_READ_REG(base + T2_DJTAG_OP_ST_REG,0);
    }while(0 == OpSt.bits.djtag_op_done);
    
    return 0;
}
