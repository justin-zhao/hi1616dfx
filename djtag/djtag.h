#ifndef DFX_DJTAG
#define DFX_DJTAG

#include "../include/common.h"

typedef u64 addr_t;
typedef union t2DjtagMstrCfg{
    struct{
        unsigned int chain_unit_cfg_en       :   16;
        unsigned int debug_module_sel        :   8 ;
        unsigned int reserved                :   5 ;
        unsigned int djtag_mstr_wr           :   1 ;
        unsigned int djtag_nor_cfg_en        :   1 ;
        unsigned int djtag_mstr_disable      :   1 ;
    }bits;

    unsigned int u32 ;
}T2_DJTAG_MSTR_CFG_U;

typedef union t2DjtagMstrStartEn{
    struct{
        unsigned int djtag_mstr_start_en    :   1 ;
        unsigned int reserved               :   31;
    }bits;

    unsigned int u32;
}T2_DJTAG_MSTR_START_EN_U;

typedef union t2DjtagOpSt{
    struct{
        unsigned int unit_conflict          :   8 ;
        unsigned int djtag_op_done          :   1 ;
        unsigned int debug_bus_en           :   1 ;
        unsigned int reserved0              :   6 ;
        unsigned int rdata_changed          :   10;
        unsigned int reserved1              :   6 ;   
    }bits;

    unsigned int u32;
}T2_DJTAG_OP_ST_U;

#define T2_DJTAG_MSTR_ADDR_REG      0xD810
#define T2_DJTAG_MSTR_DATA_REG      0xD814
#define T2_DJTAG_MSTR_CFG_REG       0xD818
#define T2_DJTAG_MSTR_START_EN_REG  0xD81C
#define T2_DJTAG_RD_DATA0_REG       0xE800
#define T2_DJTAG_RD_DATA1_REG       0xE804
#define T2_DJTAG_RD_DATA2_REG       0xE808
#define T2_DJTAG_RD_DATA3_REG       0xE80C
#define T2_DJTAG_RD_DATA4_REG       0xE810
#define T2_DJTAG_RD_DATA5_REG       0xE814
#define T2_DJTAG_RD_DATA6_REG       0xE818
#define T2_DJTAG_RD_DATA7_REG       0xE81C
#define T2_DJTAG_RD_DATA8_REG       0xE820
#define T2_DJTAG_RD_DATA9_REG       0xE824
#define T2_DJTAG_OP_ST_REG          0xE828

#define MAX                     0xFFFFFFFF

extern u32 djtag_readreg(u64 base,u32 module_sel,u32 offset,u32 cfg_en);
extern u32 djtag_writereg(u64 base,u32 module_sel,u32 offset,u32 cfg_en, u32 value);

#define SOCKET_NUMBER  2 

#if (SOCKET_NUMBER == 1)
#define DJTAG_DIE_NUM       0x2
#elif (SOCKET_NUMBER == 2)
#define DJTAG_DIE_NUM       0x4
#elif (SOCKET_NUMBER == 4)
#define DJTAG_DIE_NUM       0x8
#endif
#define SYSCTRL_REG_SPACE       0x10000  
extern addr_t tt_sc_phy[DJTAG_DIE_NUM];
extern addr_t ni_sc_phy[DJTAG_DIE_NUM];
extern addr_t totem_djtag_base[DJTAG_DIE_NUM];
extern addr_t nimbus_djtag_base[DJTAG_DIE_NUM];
extern addr_t ddr_phy_base[DJTAG_DIE_NUM * 2] ;
extern addr_t ddr_base[DJTAG_DIE_NUM * 2];
extern void tt_sc_iounmap(void); 
extern void tt_sc_ioremap(void);
extern void ni_sc_ioremap(void);
extern void ni_sc_iounmap(void);
extern char name[][20]; 
extern char ni_info[][20]; 
typedef struct test_data{
    unsigned int socket_info;
    unsigned int loops;
    unsigned int delay;
}test_data;


#endif

