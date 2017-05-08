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

#define HHA0_MODULE_SEL             0x10
#define HHA1_MODULE_SEL             0x20
#define DJTAG_MODULE_SEL_TT_SLLC0   0x7
#define DJTAG_MODULE_SEL_NI_SLLC0   0x2

#define HHA_READ_REG(die_id,hha_id,offset) djtag_readreg(totem_djtag_base[die_id],\
                    HHA0_MODULE_SEL + (hha_id << 4),offset,0x1)                                    
#define HHA_WRITE_REG(die_id,hha_id,offset,value) do{djtag_writereg(totem_djtag_base[die_id],\
                    HHA0_MODULE_SEL + (hha_id << 4),offset,0x1,value);}while(0) 
#define SLLCTT_READ_REG(die_id, sllc_id, offset)   djtag_readreg(totem_djtag_base[die_id],\
                         DJTAG_MODULE_SEL_TT_SLLC0 + (sllc_id), offset, 0x01)
#define SLLCTT_WRITE_REG(die_id, sllc_id, offset,value)   do{djtag_writereg(totem_djtag_base[die_id],\
                    	 DJTAG_MODULE_SEL_TT_SLLC0 + (sllc_id),offset,0x1,value);}while(0)                         
#define SLLCNI_READ_REG(die_id, sllc_id, offset)   djtag_readreg(nimbus_djtag_base[die_id],\ 
                        DJTAG_MODULE_SEL_NI_SLLC0 + (sllc_id), offset, 0x01)
#define SLLCNI_WRITE_REG(die_id, sllc_id, offset,value)   do{djtag_writereg(nimbus_djtag_base[die_id],\
                    	 DJTAG_MODULE_SEL_NI_SLLC0 + (sllc_id),offset,0x1,value);}while(0)                         
                                           
#define HHA_CNT_CTRL                0xc0
#define HHA_CLR_STAT				0xa0
#define HHA_EVENT_CNTTYPE_L         0x100
#define HHA_EVENT_CNTTYPE_M         0x104
#define HHA_EVENT_CNTTYPE_H         0x108 
#define HHA_EVENT_CNT0_L            0x10c
#define HHA_EVENT_CNT0_H            0x110  
#define HHA_EVENT_CNT1_L            0x114
#define HHA_EVENT_CNT1_H            0x118    

#define SLLC_BIDIRECTION_CTRL		0x4c
#define SLLC_TX_REQ_PKT_CNT_L       0x150
#define SLLC_TX_REQ_PKT_CNT_H       0x154
#define SLLC_TX_SNP_PKT_CNT_L       0x158
#define SLLC_TX_SNP_PKT_CNT_H       0x15C
#define SLLC_TX_RSP_PKT_CNT_L       0x160
#define SLLC_TX_RSP_PKT_CNT_H       0x164
#define SLLC_TX_DAT_PKT_CNT_L       0x168
#define SLLC_TX_DAT_PKT_CNT_H       0x16C
#define SLLC_RX_REQ_PKT_CNT_L       0x170
#define SLLC_RX_REQ_PKT_CNT_H       0x174
#define SLLC_RX_SNP_PKT_CNT_L       0x178
#define SLLC_RX_SNP_PKT_CNT_H       0x17C
#define SLLC_RX_RSP_PKT_CNT_L       0x180
#define SLLC_RX_RSP_PKT_CNT_H       0x184
#define SLLC_RX_DAT_PKT_CNT_L       0x188
#define SLLC_RX_DAT_PKT_CNT_H       0x18C

#define SLLC_TX_REQ_PKT_CNT         0x50
#define SLLC_TX_SNP_PKT_CNT         0x54
#define SLLC_TX_RSP_PKT_CNT         0x58
#define SLLC_TX_DAT_PKT_CNT         0x5C

#define HHA_CNT_NUM                 DJTAG_DIE_NUM * 2 * 12
u64 * hha_cnt[HHA_CNT_NUM]; 
u64 hha_bef[HHA_CNT_NUM] = {0};
#define SLLC_TX_CNT_NUM				0x4
#define SLLCTT_CNT_NUM              DJTAG_DIE_NUM * 4 * SLLC_TX_CNT_NUM
u64 * sllctt_cnt[SLLCTT_CNT_NUM];
u64 sllctt_bef[SLLCTT_CNT_NUM] = {0};
char sllc_name[][10] = {"SLLC0","SLLC1","SLLC2","SLLC3"};
#define SLLCNI_CNT_NUM              DJTAG_DIE_NUM * 2 * SLLC_TX_CNT_NUM
u64 * sllcni_cnt[SLLCNI_CNT_NUM];
u64 sllcni_bef[SLLCNI_CNT_NUM] = {0};

void hha_free_mem(void)
{
	u32 i;
	for(i = 0;i < HHA_CNT_NUM;i++){
		if(hha_cnt[i])
			vfree(hha_cnt[i]);
		hha_bef[i] = 0;
	}
	return;
}

void sllctt_free_mem(void)
{
	u32 i;
	for(i = 0;i < SLLCTT_CNT_NUM;i++){
		if(sllctt_cnt[i])
			vfree(sllctt_cnt[i]);
		sllctt_bef[i] = 0;
	}
	return;
}

void sllcni_free_mem(void)
{
	u32 i;
	for(i = 0;i < SLLCNI_CNT_NUM;i++){
		if(sllcni_cnt[i])
			vfree(sllcni_cnt[i]);
		sllcni_bef[i] = 0;
	}
	return;
}

void sllc_enable_dfx_en(void)
{
	u32 i,j,temp;
	for(i = 0;i < DJTAG_DIE_NUM;i++){
		for(j = 0;j < 4;j++){
			temp = SLLCTT_READ_REG(i,j,SLLC_BIDIRECTION_CTRL);
			temp |= (3 << 18);
			SLLCTT_WRITE_REG(i,j,SLLC_BIDIRECTION_CTRL,temp);
		}
		for(j = 0;j < 2;j++){
			temp = SLLCNI_READ_REG(i,j,SLLC_BIDIRECTION_CTRL);
			temp |= (3 << 18);
			SLLCNI_WRITE_REG(i,j,SLLC_BIDIRECTION_CTRL,temp);
		}
	}
	return;
}

void hha_set_event_cfg(void)
{
	u32 i,temp;
	for(i = 0;i < DJTAG_DIE_NUM;i++){
		temp = HHA_READ_REG(i,0,HHA_CLR_STAT);
		temp |= 0x7FF8;
		HHA_WRITE_REG(i,0,HHA_CLR_STAT,temp);
#if 0
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_L,0x0D0B0908);
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_M,0x1514130E);
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_H,0x24232016);
#else
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_L,0x07060500);
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_M,0x0c0b0908);
		HHA_WRITE_REG(i,0,HHA_EVENT_CNTTYPE_H,0x22201514);
#endif
		HHA_WRITE_REG(i,0,HHA_CNT_CTRL,0x1);
		temp = HHA_READ_REG(i,1,HHA_CLR_STAT);
		temp |= 0x7FF8;
		HHA_WRITE_REG(i,1,HHA_CLR_STAT,temp);
#if 0
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_L,0x0D0B0908);
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_M,0x1514130E);
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_H,0x24232016);
#else
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_L,0x07060500);
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_M,0x0c0b0908);
		HHA_WRITE_REG(i,1,HHA_EVENT_CNTTYPE_H,0x22201514);
#endif
		HHA_WRITE_REG(i,1,HHA_CNT_CTRL,0x1);
	}

	return;
}

void get_hha_per_second(void *arg,u32 index)
{
	u32 i,j;
	u64 valueL,valueH,temp;

	for(i = 0;i < DJTAG_DIE_NUM;i++){
		for(j = 0;j < 12;j++){
			valueL = HHA_READ_REG(i,0,HHA_EVENT_CNT0_L + j * 0x8);
			valueH = HHA_READ_REG(i,0,HHA_EVENT_CNT0_H + j * 0x8);
			temp = (valueH << 32) | valueL;
			hha_cnt[j + i * 24][index] = temp - hha_bef[j + i * 24];
			hha_bef[j + i * 24] = temp;
			//printk("DIE[%d]j[%d]valueL[%llx];valueH[%llx];temp[%llx];value[%llx];\n",i,j,valueL,valueH,temp,hha_cnt[j + i * 24][index]);
		}
		for(j = 0;j < 12;j++){
			valueL = HHA_READ_REG(i,1,HHA_EVENT_CNT0_L + j * 0x8);
			valueH = HHA_READ_REG(i,1,HHA_EVENT_CNT0_H + j * 0x8);
			temp = (valueH << 32) | valueL;
			hha_cnt[j + i * 24 + 12][index] = temp - hha_bef[j + i * 24 + 12];
			hha_bef[j + i * 24 + 12] = temp;
			//printk("DIE[%d]j[%d]valueL[%llx];valueH[%llx];temp[%llx];value[%llx];\n",i,j,valueL,valueH,temp,hha_cnt[j + i * 24 + 12][index]);            
		}
	}
}

void get_sllctt_per_second(void *arg,u32 index)
{
	u32 i,j,k,m;
	u64 valueL,valueH,temp;

	for(i = 0;i < DJTAG_DIE_NUM;i++){       
		for(j = 0;j < 4;j++){//4个SLLC
			for(k = 0; k < SLLC_TX_CNT_NUM;k++){//每个SLLC使用4个统计，仅计算4个TX
				valueL = SLLCTT_READ_REG(i,j,SLLC_TX_REQ_PKT_CNT_L + k * 0x8);
				valueH = SLLCTT_READ_REG(i,j,SLLC_TX_REQ_PKT_CNT_H + k * 0x8);
				m = k + j * SLLC_TX_CNT_NUM + i * 4 * SLLC_TX_CNT_NUM;
				temp = (valueH << 32) | valueL;
				sllctt_cnt[m][index] = temp - sllctt_bef[m];
				sllctt_bef[m] = temp;

			}
		}
	}
}

void get_sllcni_per_second(void *arg,u32 index)
{
	u32 i,j,k,m;
	u64 valueL,valueH,temp;

	for(i = 0;i < DJTAG_DIE_NUM;i++){       
		for(j = 0;j < 2;j++){//2个SLLC
			for(k = 0; k < 4;k++){//每个SLLC使用4个统计，仅计算4个TX
				temp  = SLLCNI_READ_REG(i,j,SLLC_TX_REQ_PKT_CNT_L + k * 0x4);
				//valueH = SLLCNI_READ_REG(i,j,SLLC_TX_REQ_PKT_CNT_H + k * 0x8);
				m = k + j * 4 + i * 8;
				temp = (valueH << 32) | valueL;
				sllcni_cnt[m][index] = (temp - sllcni_bef[m]) ? (temp - sllcni_bef[m]) : (MAX - sllcni_bef[m] + temp);
				sllcni_bef[m] = temp;
			}
		}
	}
}


void * hha_statistic_thread(void * arg)
{
	struct test_data *data = (struct test_data*)arg;
	unsigned int loops = data->loops;
	//unsigned int skt_info = data->socket_info;
	u32 index,i,j,k,n;
	u8 tmp[0x200];
	struct file *fp;
	mm_segment_t fs;

	for(i = 0;i < HHA_CNT_NUM;i++){
		hha_cnt[i] = vmalloc(sizeof(u64) * loops);
		if(hha_cnt[i] == NULL){
			printk("error vmalloc buf;\n");
		}  
		hha_bef[i] = 0;  
	}
	for(i = 0;i < SLLCTT_CNT_NUM;i++){
		sllctt_cnt[i] = vmalloc(sizeof(u64) * loops);
		if(sllctt_cnt[i] == NULL){
			printk("error vmalloc buf;\n");
		} 
		sllctt_bef[i] = 0;   
	}
	for(i = 0;i < SLLCNI_CNT_NUM;i++){
		sllcni_cnt[i] = vmalloc(sizeof(u64) * loops);
		if(sllcni_cnt[i] == NULL){
			printk("error vmalloc buf;\n");
		} 
		sllcni_bef[i] = 0;   
	}

	tt_sc_ioremap();
	ni_sc_ioremap();
	sllc_enable_dfx_en();
	hha_set_event_cfg();

	for(index = 0;index < loops;index++){
		get_hha_per_second(data,index);
		get_sllctt_per_second(data,index);
		get_sllcni_per_second(data,index);
		msleep(1000);
	}
	printk("begin wirte file;\n");   

	fp = filp_open("/home/hha_sllc_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;

	for(i = 0;i < DJTAG_DIE_NUM;i++){
		sprintf(tmp,"Socket infor:[%s]-->HHA0;\n",name[i]);
		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
		//printk("Socket infor:[%s]-->HHA0;\n",name[i]);
		for(j = 0;j < loops;j++){
			for(k = 0;k < 12;k++){
				sprintf(tmp,"%lld,",hha_cnt[k + i * 24][j]);
				vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
				//printk("%lld,",hha_cnt[k + i * 24][j]);
			}
			sprintf(tmp,"\n");
			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
			//printk("\n");
		}
		sprintf(tmp,"Socket infor:[%s]-->HHA1;\n",name[i]);
		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
		//printk("Socket infor:[%s]-->HHA1;\n",name[i]);
		for(j = 0;j < loops;j++){
			for(k = 0;k < 12;k++){
				sprintf(tmp,"%lld,",hha_cnt[k + i * 24 + 12][j]);
				vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
				//printk("%lld,",hha_cnt[k + i * 24 + 12][j]);
			}
			//printk("\n");
			sprintf(tmp,"\n");
			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
		}
		//printk("=========================\n");
	}    
	for(i = 0;i < DJTAG_DIE_NUM;i++){
		for(n = 0; n < 4;n++){ //4个SLLC
			sprintf(tmp,"Socket infor:[%s]-->SLLC%d;\n",name[i],n);
			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);   
			//printk("Socket infor:[%s]-->SLLC%d;\n",name[i],n);     		    	
			for(j = 0;j < loops;j++){
				for(k = 0;k < 4;k++){ //打印每个SLLC TX方向的4个事件
					sprintf(tmp,"%lld,",sllctt_cnt[k + n * 4 + i * 16][j]);
					vfs_write(fp, tmp, strlen(tmp),&fp->f_pos); 
					//printk("%lld,",sllctt_cnt[k + n * 4 + i * 16][j]);
				}
				sprintf(tmp,"\n");
				vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
				//printk("\n");                
			} 
		}
		//printk("=========================\n");
	} 
	for(i = 0;i < DJTAG_DIE_NUM;i++){
		for(n = 0; n < 2;n++){ //2个SLLC
			sprintf(tmp,"Socket infor:[%s]-->SLLC%d;\n",ni_info[i],n);
			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos); 
			//printk("Socket infor:[%s]-->SLLC%d;\n",name[i],n); 
			for(j = 0;j < loops;j++){
				for(k = 0;k < 4;k++){ //打印每个SLLC TX方向的4个事件
					sprintf(tmp,"%lld,",sllcni_cnt[k + n * 4 + i * 8][j]);
					vfs_write(fp, tmp, strlen(tmp),&fp->f_pos); 
					//printk("%lld,",sllcni_cnt[k + n * 4 + i * 8][j]);
				}
				sprintf(tmp,"\n");
				vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
				//printk("\n");
			} 
		}
		//printk("=========================\n"); 
	}   

	filp_close(fp, NULL);
	set_fs(fs);
	printk("end wirte file;\n"); 
	hha_free_mem();
	sllctt_free_mem();
	sllcni_free_mem();     
	tt_sc_iounmap();
	ni_sc_iounmap();

	return;
}

void get_hha_statistic(u32 type,u32 _time)
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
	tsk = kthread_create((void*)hha_statistic_thread,(void*)data,"hha statistic");
	if(tsk == NULL){
		printk("[WARNING] thread create failed;\n");
		return;
	}
	printk("hha_statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}                                  
