#include "../djtag/djtag.h"
#include <linux/spinlock.h>
#include <asm/spinlock.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#define MAX                          0xFFFFFFFF
#define HLLC0_MODULE_SEL             0x4
#define HLLC1_MODULE_SEL             0x5
#define HLLC2_MODULE_SEL             0x6
#define PA_MODULE_SEL                0x26
#define HLLC_READ_REG(die_id,hllc_id,offset) djtag_readreg(nimbus_djtag_base[die_id],HLLC0_MODULE_SEL + (hllc_id), offset, 0x01) 
#define PA_READ_REG(die_id,pa_id,offset) djtag_readreg(nimbus_djtag_base[die_id],PA_MODULE_SEL + (pa_id), offset, 0x01)
                      
#define PA_H0_RX_REQ_CNT             0x164  
#define PA_H0_RX_SNP_CNT             0x168     
#define PA_H0_RX_RSP_CNT             0x16C     
#define PA_H0_RX_DAT_CNT             0x170   
#define PA_H1_RX_REQ_CNT             0x1C4  
#define PA_H1_RX_SNP_CNT             0x1C8     
#define PA_H1_RX_RSP_CNT             0x1CC     
#define PA_H1_RX_DAT_CNT             0x1D0 
#define PA_H2_RX_REQ_CNT             0x224  
#define PA_H2_RX_SNP_CNT             0x228     
#define PA_H2_RX_RSP_CNT             0x22C     
#define PA_H2_RX_DAT_CNT             0x230 
#define HLLC_PA_TX_CH0_FLIT_CNT      0x110
#define HLLC_PA_TX_CH1_FLIT_CNT      0x114
#define HLLC_PA_TX_CH2_FLIT_CNT      0x118
#define HLLC_PA_TX_CH3_FLIT_CNT      0x11c
#define HLLC_PA_RX_CH0_FLIT_CNT      0x120
#define HLLC_PA_RX_CH1_FLIT_CNT      0x124
#define HLLC_PA_RX_CH2_FLIT_CNT      0x128
#define HLLC_PA_RX_CH3_FLIT_CNT      0x12c

/* die_num:module_num:register_num*/
#define PA_CNT_NUM  DJTAG_DIE_NUM * 1 * 12
u64 * pa_cnt[PA_CNT_NUM];  
#define HLLC_CNT_NUM  DJTAG_DIE_NUM * 3 * 8
u64 * hllc_cnt[HLLC_CNT_NUM];
u64 hllc_bef[HLLC_CNT_NUM] = {0};

void pa_free_mem(void)
{
    u32 i;
    for(i = 0;i < PA_CNT_NUM;i++){
        if(pa_cnt[i])
            vfree(pa_cnt[i]);
    }
    return;
}
void hllc_free_mem(void)
{
    u32 i;
    for(i = 0;i < HLLC_CNT_NUM;i++){
        if(hllc_cnt[i])
            vfree(hllc_cnt[i]);
    }
    return;
}

void get_pa_per_second(void *arg,u32 index)
{
    u32 i,j;
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0;j < 4;j++){
            pa_cnt[j + i * 12 + 0][index] = PA_READ_REG(i,0x0,PA_H0_RX_REQ_CNT + j * 0x4); //每个die有12个事件统计寄存器
        }
        for(j = 0;j < 4;j++){
            pa_cnt[j + i * 12 + 4][index] = PA_READ_REG(i,0x0,PA_H1_RX_REQ_CNT + j * 0x4); //每个die有12个事件统计寄存器
        }
        for(j = 0;j < 4;j++){
            pa_cnt[j + i * 12 + 8][index] = PA_READ_REG(i,0x0,PA_H2_RX_REQ_CNT + j * 0x4); //每个die有12个事件统计寄存器
        }
    }

    return;
}

void get_hllc_per_second(void *arg,u32 index)
{
    u32 i,j,k,n;
    u64 temp;
	for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < 3;j++){  //每个die有3个HLLC
            for(k = 0; k < 8;k++){//每个HLLC有8个事件(32bit)
                temp = HLLC_READ_REG(i,j,HLLC_PA_TX_CH0_FLIT_CNT + k * 0x4);
                n = k + j * 8 + i * 24;
                hllc_cnt[n][index] = (temp >= hllc_bef[n]) ? (temp - hllc_bef[n]): (MAX - hllc_bef[n] + temp);
                hllc_bef[n] = temp;
            }
        }  
    }
    return;
}

void * pa_statistic_thread(void * arg)
{
    struct test_data *data = (struct test_data*)arg;
    unsigned int loops = data->loops;
    u32 index,i,j,k;
    u8 tmp[0x200];
    struct file *fp;
	mm_segment_t fs;
    
    for(i = 0;i < PA_CNT_NUM;i++){
        pa_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(pa_cnt[i] == NULL){
           printk("error vmalloc buf;\n");
        }    
    }
    for(i = 0;i < HLLC_CNT_NUM;i++){
        hllc_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(hllc_cnt[i] == NULL){
           printk("error vmalloc buf;\n");
        }    
    }
    ni_sc_ioremap();
    
    for(index = 0;index < loops;index++){
        get_pa_per_second(data,index);
        get_hllc_per_second(data,index);
        msleep(1000);
    }
    printk("begin wirte file;\n");   
    
    fp = filp_open("/home/pa_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;
	
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        sprintf(tmp,"PA-->Socket_info:[%s];\n",ni_info[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("PA-->Socket_info:[%s];\n",ni_info[i]);
        for(j = 0;j < loops;j++){
            for(k = 0;k < 12;k++){ 
                sprintf(tmp,"%lld,",pa_cnt[k + i * 12][j]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",pa_cnt[k + i * 12][j]);
            }
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
            //printk("\n");
        }
        //printk("=========================\n"); 
    }
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        sprintf(tmp,"Socket_info:[%s]-->HLLC0;\n",ni_info[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket_info:[%s]-->HLLC0;\n",ni_info[i]);
        for(j = 0;j < loops;j++){
            for(k = 0;k < 8;k++){ 
                sprintf(tmp,"%lld,",hllc_cnt[k + i * 24][j]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",pa_cnt[k + i * 24][j]);
            }
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
            //printk("\n");
        }
        sprintf(tmp,"Socket_info:[%s]-->HLLC1;\n",ni_info[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket_info:[%s]-->HLLC1;\n",ni_info[i]);
        for(j = 0;j < loops;j++){
            for(k = 0;k < 8;k++){ 
                sprintf(tmp,"%lld,",hllc_cnt[k + i * 24 + 8][j]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",pa_cnt[k + i * 24 + 8][j]);
            }
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
            //printk("\n");
        }
        sprintf(tmp,"Socket_info:[%s]-->HLLC2;\n",ni_info[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket_info:[%s]-->HLLC2;\n",ni_info[i]);
        for(j = 0;j < loops;j++){
            for(k = 0;k < 8;k++){ 
                sprintf(tmp,"%lld,",hllc_cnt[k + i * 24 + 16][j]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",pa_cnt[k + i * 24 + 16][j]);
            }
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
            //printk("\n");
        }
        //printk("=========================\n"); 
    }
    filp_close(fp, NULL);
	set_fs(fs);
	printk("end wirte file;\n"); 
    pa_free_mem();
    hllc_free_mem();
    ni_sc_iounmap();
    return;
}


void get_pa_statistic(u32 type,u32 _time)
{
    struct task_struct *tsk;
    struct test_data *data; 
    data = (struct test_data*)kmalloc(sizeof(struct test_data),GFP_KERNEL);
    if(data == NULL){
        printk("[WARNING] kmalloc failed;\n");
        return;
    }
 
    data->loops = _time;
    data->socket_info = (type >> 4) & 0xf;
    //tsk = kthread_run((void*)llc_statistic_thread,(void*)data,"llc statistic");
    tsk = kthread_create((void*)pa_statistic_thread,(void*)data,"pa statistic");
    if(tsk == NULL){
        printk("[WARNING] thread create failed;\n");
        return;
    }
    printk("pa_statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}                                           
