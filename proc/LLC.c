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

#define LLC_BANK_NUM            0x004
#define LLC_EVENT_NUM           0x008
#define LLC_MODULE_SEL          0x001
#define LLC_AUCTRL              0x004
#define LLC_BANK_EVENT_TYPE0    0x140
#define LLC_BANK_EVENT_TYPE1    0x144
#define LLC_BANK_EVENT_CNT0     0x170
#define DDRC_CTRL_PERF          0x010
#define DDRC_CFG_STADAT			0x254
#define DDRC_CFG_STACMD			0x260
#define DDRC_CFG_PERF           0x26c
#define DDRC_CFG_STAID          0x270
#define DDRC_CFG_STAIDMSK       0x274
#define DDRC_HIS_FLUX_WR		0x380
#define DDRC_HIS_FLUX_RD		0x384
#define DDRC_HIS_FLUX_WCMD      0x388
#define DDRC_HIS_FLUX_RCMD      0x38c
#define DDRC_HIS_WLATCNT0       0x3A0
#define DDRC_HIS_CMD_SUM        0x3B8

#define LLC_READ_REG(die_id,bank_id,offset) djtag_readreg(totem_djtag_base[die_id],\
                    LLC_MODULE_SEL + bank_id,offset,0x1)                                    
#define LLC_WRITE_REG(die_id,bank_id,offset,value) do{djtag_writereg(totem_djtag_base[die_id],\
                    LLC_MODULE_SEL + bank_id,offset,0x1,value);}while(0) 
#define reg_read(base,offset) (*(u32 *)((void *)base + offset))
#define reg_write(base,offset,value)((*(u32*)((void *)base + offset)) = value)  

#ifdef LLC_BANK_SHOW
#define LLC_CNT_NUM DJTAG_DIE_NUM * LLC_BANK_NUM * LLC_EVENT_NUM  
#else
#define LLC_CNT_NUM DJTAG_DIE_NUM * LLC_EVENT_NUM 
#endif                  
                                          
u64* llc_cnt[LLC_CNT_NUM];
char llc_bank_info[][10] = {"LLC_BANK0","LLC_BANK1","LLC_BANK2","LLC_BANK3"};
u64 llc_bef[LLC_CNT_NUM] = {0};
u64 llc_temp[DJTAG_DIE_NUM][LLC_EVENT_NUM][LLC_BANK_NUM] = {0};

#define DDR_REG_NUM		0x2
#define DDR_NUM         0x2
#define DDR_CNT_NUM DJTAG_DIE_NUM * DDR_NUM * DDR_REG_NUM
u64* ddr_cnt[DDR_CNT_NUM]; 
u64 ddr_bef[DDR_CNT_NUM] = {0};

char ddr_info[][4] = {"wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr",
	"rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd","wr","rd"};

void llc_free_mem(void)
{
    u32 i;
    for(i = 0;i < LLC_CNT_NUM;i++){
        if(llc_cnt[i])
            vfree(llc_cnt[i]);
    }
    return;
}

void ddr_free_mem(void)
{
    u32 i;
    for(i = 0;i < DDR_CNT_NUM;i++){
        if(ddr_cnt[i])
            vfree(ddr_cnt[i]);  
    }
    return;
}

void ddr_base_iounmap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM * 2;i++){
        if(ddr_base[i])
            iounmap((void*)ddr_base[i]);
    }
    return;   
}
    
void ddr_base_ioremap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM * 2;i++)
    {
        ddr_base[i] = ioremap(ddr_phy_base[i],0x10000);
        if(!ddr_base[i]){
            printk("error ioremap[0x%llx];\n",ddr_phy_base[i]);
            goto exit;        
        }
    } 
    return;
exit:
    for(i = 0; i < DJTAG_DIE_NUM * 2;i++){
        if(ddr_base[i])
            iounmap((void*)ddr_base[i]);
    }        
    return;
}

void llc_clear_event(void)
{
	u32 i,j,k;
	for(i = 0; i < DJTAG_DIE_NUM;i++){
		for(j = 0; j < LLC_BANK_NUM;j++){
			for(k = 0; k < LLC_EVENT_NUM;k++){
				LLC_WRITE_REG(i,j,LLC_BANK_EVENT_CNT0 + k * 0x4,0x0);	
			}
		}			
	}	
	return;
}

void llc_set_event_cfg(void)
{
    u32 i,j,temp;
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < LLC_BANK_NUM;j++){
            temp = LLC_READ_REG(i,j,LLC_AUCTRL); 
            temp = temp & (~(1 << 24));
            LLC_WRITE_REG(i,j,LLC_AUCTRL,temp);    
            LLC_WRITE_REG(i,j,LLC_BANK_EVENT_TYPE0,0x03020100);
            LLC_WRITE_REG(i,j,LLC_BANK_EVENT_TYPE1,0x07060504); 
            temp = LLC_READ_REG(i,j,LLC_AUCTRL); 
            temp = temp | (1 << 24);
            LLC_WRITE_REG(i,j,LLC_AUCTRL,temp);         
        }      
    }
    
    return;    
}

void ddr_event_cfg(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM * 2;i++){
        reg_write(ddr_base[i],DDRC_CTRL_PERF,0x1);
        reg_write(ddr_base[i],DDRC_CFG_STADAT,0x40000000);
        reg_write(ddr_base[i],DDRC_CFG_STACMD,0x40000000);
        reg_write(ddr_base[i],DDRC_CFG_PERF,0x20000000);
        reg_write(ddr_base[i],DDRC_CFG_STAID,0x0);
        reg_write(ddr_base[i],DDRC_CFG_STAIDMSK,0x0);
    }  
    return;  
}

void get_llc_per_second(void *arg,u32 index)
{
    u32 i,j,k,n; 
    u64 temp;
	
    #ifdef LLC_BANK_SHOW
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0;j < LLC_BANK_NUM;j++){
            for(k = 0;k < LLC_EVENT_NUM;k++){
                temp = LLC_READ_REG(i,j,LLC_BANK_EVENT_CNT0 + k * 0x4);                
                n = k + LLC_EVENT_NUM * j + LLC_BANK_NUM * LLC_EVENT_NUM * i;
                llc_cnt[n][index] = ((temp >= llc_bef[n]) ? (temp - llc_bef[n]) : (MAX - llc_bef[n] + temp));
                llc_bef[n] = temp;
            }        
        }
    }
    #else
    for(i = 0;i < DJTAG_DIE_NUM;i++){
    	for(j = 0;j < LLC_EVENT_NUM;j++){
			for(k = 0;k < LLC_BANK_NUM;k++){	
				temp = LLC_READ_REG(i,k,LLC_BANK_EVENT_CNT0 + j * 0x4); 
				n = j + i * LLC_EVENT_NUM; 
				llc_cnt[n][index] += (((temp >= llc_temp[i][j][k]) ? (temp - llc_temp[i][j][k]) : (MAX - llc_temp[i][j][k] + temp)));
				//printk("die[%d][%d][%d] reg:[%d];bef[%d];value:[%d];\n",i,j,k,temp,llc_temp[i][j][k],llc_cnt[n][index]);				
				llc_temp[i][j][k] = temp;	
			}    			
    	}
    }
    #endif
    
    return;
}  
void get_ddr_per_second(void *arg,u32 index)
{
    u32 i,j;
    u64 temp;
    
    for(i = 0; i < DJTAG_DIE_NUM * 2;i++){
        temp = reg_read(ddr_base[i],DDRC_HIS_FLUX_WR);
        ddr_cnt[i * DDR_REG_NUM][index] = ((temp >= ddr_bef[i * DDR_REG_NUM]) ? (temp - ddr_bef[i * DDR_REG_NUM]) : (MAX - ddr_bef[i * DDR_REG_NUM] + temp));
        ddr_bef[i * DDR_REG_NUM] = temp;
        temp = reg_read(ddr_base[i],DDRC_HIS_FLUX_RD);
        ddr_cnt[i * DDR_REG_NUM + 1][index] = ((temp >= ddr_bef[i * DDR_REG_NUM + 1]) ? (temp - ddr_bef[i * DDR_REG_NUM + 1]) : (MAX - ddr_bef[i * DDR_REG_NUM + 1] + temp));
        ddr_bef[i * DDR_REG_NUM + 1] = temp;
#if 0
        temp = reg_read(ddr_base[i],DDRC_HIS_FLUX_WCMD);
        ddr_cnt[i * DDR_REG_NUM + 2][index] = ((temp >= ddr_bef[i * DDR_REG_NUM + 2]) ? (temp - ddr_bef[i * DDR_REG_NUM + 2]) : (MAX - ddr_bef[i * DDR_REG_NUM + 2] + temp));
        ddr_bef[i * DDR_REG_NUM + 2] = temp;
        temp = reg_read(ddr_base[i],DDRC_HIS_FLUX_RCMD);
        ddr_cnt[i * DDR_REG_NUM + 3][index] = ((temp >= ddr_bef[i * DDR_REG_NUM + 3]) ? (temp - ddr_bef[i * DDR_REG_NUM + 3]) : (MAX - ddr_bef[i * DDR_REG_NUM + 3] + temp));
        ddr_bef[i * DDR_REG_NUM + 3] = temp;
        for(j = 0;j < 4;j++){
            temp = reg_read(ddr_base[i],DDRC_HIS_WLATCNT0 + j * 0x4);
            ddr_cnt[i * DDR_REG_NUM + 4 + j][index] = ((temp >= ddr_bef[i * DDR_REG_NUM + 4 + j]) ? (temp - ddr_bef[i * DDR_REG_NUM + 4 + j]) : (MAX - ddr_bef[i * DDR_REG_NUM + 4 + j] + temp));
            ddr_bef[i * DDR_REG_NUM + 4 + j] = temp;
        }
        for(j = 0;j < 7;j++){
            temp = reg_read(ddr_base[i],DDRC_HIS_CMD_SUM + j * 0x4);
            ddr_cnt[i * DDR_REG_NUM + 8 + j][index] = ((temp >= ddr_bef[i * DDR_REG_NUM + 8 + j]) ? (temp - ddr_bef[i * DDR_REG_NUM + 8 + j]) : (MAX - ddr_bef[i * DDR_REG_NUM + 8 + j] + temp));
            ddr_bef[i * DDR_REG_NUM + 8 + j] = temp;
        }
#endif        
    }
    return;
}
  

void * llc_statistic_thread(void *arg)
{
    struct test_data *data = (struct test_data*)arg;
    unsigned int loops = data->loops;
    u32 index,i,j,k,n;
    u8 tmp[0x200];
    struct file *fp;
	mm_segment_t fs;
    
    for(i = 0;i < LLC_CNT_NUM;i++){
        llc_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(llc_cnt[i] == NULL)
           printk("error vmalloc buf;\n");  
    } 
    for(i = 0;i < DDR_CNT_NUM;i++){
        ddr_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(ddr_cnt[i] == NULL)
           printk("error vmalloc buf;\n");  
        ddr_bef[i] = 0;   //clear to 0
    } 
    
    #ifdef LLC_BANK_SHOW
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0;j < LLC_BANK_NUM;j++){
            for(k = 0;k < LLC_EVENT_NUM;k++){
            	n = k + LLC_EVENT_NUM * j + LLC_BANK_NUM * LLC_EVENT_NUM * i;
            	llc_bef[n] = 0;
            }
        }
    }
    #else
    for(i = 0;i < DJTAG_DIE_NUM;i++){
    	for(j = 0;j < LLC_EVENT_NUM;j++){
    		for(index = 0;index < loops;index++){
    			llc_cnt[j + i * LLC_EVENT_NUM][index] = 0;  //clear to 0
    		}
    		for(k = 0;k < LLC_BANK_NUM;k++){
    			llc_temp[i][j][k] = 0;
    		}	
    	}
    }
    #endif
     
    tt_sc_ioremap();
    llc_set_event_cfg();
    ddr_base_ioremap();
    ddr_event_cfg();
    
    for(index = 0;index < loops;index++){
        get_llc_per_second(data,index);
        get_ddr_per_second(data,index);
        msleep(1000);
    }
    printk("begin wirte file;\n");   
    
    fp = filp_open("/home/llc_ddr_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;
	
    #ifdef LLC_BANK_SHOW    
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < LLC_BANK_NUM;j++){
            printk("LLC Information:[%s];[%s]\n",name[i],llc_bank_info[j]);
            for(k = 0; k < loops;k++){ 
                for(n = 0; n < LLC_EVENT_NUM;n++){ 
                    printk("%lld,",llc_cnt[n + LLC_EVENT_NUM * j + LLC_BANK_NUM * LLC_EVENT_NUM * i][k]);
                }
                printk("\n"); 
            }
            printk("\n");    
        }
        printk("=========================\n");  
    }
    #else
    for(i = 0;i < DJTAG_DIE_NUM;i++){
    	sprintf(tmp,"LLC Information:[%s];\n",name[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
    	//printk("LLC Information:[%s];\n",name[i]);
    	for(j = 0; j < loops;j++){ 
    		for(k = 0; k < LLC_EVENT_NUM;k++){
    			sprintf(tmp,"%lld,",llc_cnt[k + LLC_EVENT_NUM * i][j]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
    			//fp->f_op->write(fp, tmp, strlen(tmp),&fp->f_pos);   			
    			//printk("%lld,",llc_cnt[k + LLC_EVENT_NUM * i][j]);	
    		} 
    		sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
    		//printk("\n");
    	}
    }
    #endif 
    sprintf(tmp,"DDR test information:\n"); 
    vfs_write(fp, tmp, strlen(tmp),&fp->f_pos); 
    //printk("DDR information:\n");
    for(i = 0;i < DDR_CNT_NUM;i++){
    	sprintf(tmp,"%s ",ddr_info[i]); 
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
    	//printk("%s ",ddr_info[i]);
	}
	sprintf(tmp,"\n");
    vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
	//printk("\n");
    for(k = 0; k < loops;k++){ 
        for(i = 0;i < DDR_CNT_NUM;i++){
        	sprintf(tmp,"%lld,",ddr_cnt[i][k]);
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
            //printk("%lld,",ddr_cnt[i][k]);
        } 
        sprintf(tmp,"\n");
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos); 
        //printk("\n"); 
    } 
       
    filp_close(fp, NULL);
	set_fs(fs);
	printk("end wirte file;\n"); 
	llc_clear_event();
    ddr_free_mem();
    llc_free_mem();   
    tt_sc_iounmap();       
    ddr_base_iounmap();    
        
    return; 
}

void get_llc_statistic(u32 type,u32 _time)
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
    tsk = kthread_create((void*)llc_statistic_thread,(void*)data,"llc statistic");
    if(tsk == NULL){
        printk("[WARNING] thread create failed;\n");
        return;
    }
    printk("llc_statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}



