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

u32 aa_shift[4] = {2,8,1,0x10};
#define AA_MODULE_SEL           0x6

#define AA_READ_REG(die_id, aa_id, offset) djtag_readreg(totem_djtag_base[die_id],\
                    AA_MODULE_SEL,offset,aa_shift[aa_id]) 
#define AA_WRITE_REG(die_id, aa_id, offset, value)  do{djtag_writereg(totem_djtag_base[die_id],\
                    AA_MODULE_SEL,offset,aa_shift[aa_id],value);}while(0) 
#define AA_DFXCTRL              0x300
#define AA_STATDLY              0x304
#define AA_DLYADD0              0x400                    
#define AA_DLYADD1              0x404 
#define AA_DLYNUM               0x408
#define AA_DLYMAX               0x40C
#define AA_NUM                  0x4
#define AA_CNT_NUM              DJTAG_DIE_NUM * AA_NUM * 3 //4个AA，每个AA统计三个事件 
u64 * aa_cnt[AA_CNT_NUM]; 

void aa_clear_event_cnt(void)
{
	u32 i,j;
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < AA_NUM; j++){
            AA_WRITE_REG(i,j,AA_DLYADD0,0x0);
            AA_WRITE_REG(i,j,AA_DLYADD1,0x0);
            AA_WRITE_REG(i,j,AA_DLYNUM,0x0);
            AA_WRITE_REG(i,j,AA_DLYMAX,0x0);
        }
    }

	return;
}

void aa_free_mem(void)
{
    u32 i;
    for(i = 0;i < AA_CNT_NUM;i++){
        if(aa_cnt[i])
            vfree(aa_cnt[i]);
    }
    aa_clear_event_cnt();
    return;
}

void aa_set_event_cfg(u32 type)
{
    u32 i,j;
    aa_clear_event_cnt();
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < AA_NUM; j++){
            AA_WRITE_REG(i,j,AA_DFXCTRL,0x3);
        }
        for(j = 0; j < AA_NUM; j++){
            AA_WRITE_REG(i,j,AA_STATDLY,type);
        }
    }
    
    return;
}

void get_aa_per_second(void *arg,u32 index)
{
    u32 i,j;
    u64 valueL,valueH,temp;
    
    for(i = 0;i < DJTAG_DIE_NUM;i++){
        for(j = 0; j < AA_NUM;j++){
            valueL = AA_READ_REG(i,j,AA_DLYADD0); 
            valueH = AA_READ_REG(i,j,AA_DLYADD1);
            temp = (valueH << 32) | valueL;
            aa_cnt[1 + 3 * j + 12 * i][index] = AA_READ_REG(i,j,AA_DLYNUM); 
            aa_cnt[0 + 3 * j + 12 * i][index] = (temp / aa_cnt[1 + 3 * j + 12 * i][index]);            
            aa_cnt[2 + 3 * j + 12 * i][index] = AA_READ_REG(i,j,AA_DLYMAX);   
            //printk("die[%d]aa:[%d]low[%x];high[%x]temp:[%llx];cnt[%x]\n",i,j,valueL,valueH,temp,aa_cnt[1 + 3 * j + 12 * i][index]);   
        }
    }
    
    return;
}

void * aa_rd_statistic_thread(void * arg)
{
    struct test_data *data = (struct test_data*)arg;
    unsigned int loops = data->loops;
    //unsigned int skt_info = data->socket_info;
    u32 index,i,j,k;
    u8 tmp[0x200];
    struct file *fp;
	mm_segment_t fs;
	loff_t pos;
    
    for(i = 0;i < AA_CNT_NUM;i++){
        aa_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(aa_cnt[i] == NULL){
           printk("error vmalloc buf;\n");
        }    
    }
    
    tt_sc_ioremap();
    aa_set_event_cfg(0x0);
    for(index = 0;index < loops;index++){
        get_aa_per_second(data,index);
        msleep(1000);
    }
    printk("begin wirte file;\n");   
    
    fp = filp_open("/home/aa_rd_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;

    for(i = 0;i < DJTAG_DIE_NUM;i++){
        sprintf(tmp,"Socket info:[%s];-->AA\n",name[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket info:[%s];-->AA\n",name[i]);
        for(k = 0;k < loops;k++){
            for(j = 0;j < 12;j++){//每个TOTEM的AA有12个事件统计计数器；
                sprintf(tmp,"%lld,",aa_cnt[j + 12 * i][k]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",aa_cnt[j + 12 * i][k]);                            
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
    aa_free_mem();
    tt_sc_iounmap();
    return;
}
                    
void get_aa_rd_statistic(u32 type,u32 _time)
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
    tsk = kthread_create((void*)aa_rd_statistic_thread,(void*)data,"aa_rd statistic");
    if(tsk == NULL){
        printk("[WARNING] thread create failed;\n");
        return;
    }
    printk("aa_rd statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}   

void * aa_wr_statistic_thread(void * arg)
{
    struct test_data *data = (struct test_data*)arg;
    unsigned int loops = data->loops;
    //unsigned int skt_info = data->socket_info;
    u32 index,i,j,k;
    u8 tmp[0x200];
    struct file *fp;
	mm_segment_t fs;
    
    for(i = 0;i < AA_CNT_NUM;i++){
        aa_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(aa_cnt[i] == NULL){
           printk("error vmalloc buf;\n");
        }    
    }
    
    tt_sc_ioremap();
    aa_set_event_cfg(0x1);
    for(index = 0;index < loops;index++){
        get_aa_per_second(data,index);
        msleep(1000);
    }
    printk("begin wirte file;\n");   
    
    fp = filp_open("/home/aa_wr_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;

    for(i = 0;i < DJTAG_DIE_NUM;i++){
    	sprintf(tmp,"Socket info:[%s];-->AA\n",name[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket info:[%s];-->AA\n",name[i]);
        for(k = 0;k < loops;k++){
            for(j = 0;j < 12;j++){//每个TOTEM的AA有12个事件统计计数器；
                sprintf(tmp,"%lld,",aa_cnt[j + 12 * i][k]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",aa_cnt[j + 12 * i][k]);           
            } 
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);    
    		//printk("\n");   
        }
        //printk("=========================\n"); 
    }
    filp_close(fp, NULL);
	set_fs(fs);
	printk("End wirte file;\n"); 
    aa_free_mem();
    tt_sc_iounmap();
    return;
}
                    
void get_aa_wr_statistic(u32 type,u32 _time)
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
    tsk = kthread_create((void*)aa_wr_statistic_thread,(void*)data,"aa_wr statistic");
    if(tsk == NULL){
        printk("[WARNING] thread create failed;\n");
        return;
    }
    printk("aa_wr statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}    

void * aa_cb_statistic_thread(void * arg)
{
    struct test_data *data = (struct test_data*)arg;
    unsigned int loops = data->loops;
    //unsigned int skt_info = data->socket_info;
    u32 index,i,j,k;
    u8 tmp[0x200];
    struct file *fp;
	mm_segment_t fs;
    
    for(i = 0;i < AA_CNT_NUM;i++){
        aa_cnt[i] = vmalloc(sizeof(u64) * loops);
        if(aa_cnt[i] == NULL){
           printk("error vmalloc buf;\n");
        }    
    }
    
    tt_sc_ioremap();
    aa_set_event_cfg(0x2);
    for(index = 0;index < loops;index++){
        get_aa_per_second(data,index);
        msleep(1000);
    }
    printk("begin wirte file;\n");   
    
    fp = filp_open("/home/aa_cb_statistic", O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0;

    for(i = 0;i < DJTAG_DIE_NUM;i++){
    	sprintf(tmp,"Socket info:[%s];-->AA\n",name[i]);
    	vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
        //printk("Socket info:[%s];-->AA\n",name[i]);
        for(k = 0;k < loops;k++){
            for(j = 0;j < 12;j++){//每个TOTEM的AA有12个事件统计计数器；
                sprintf(tmp,"%lld,",aa_cnt[j + 12 * i][k]);
    			vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);
                //printk("%lld,",aa_cnt[j + 12 * i][k]);          
            }  
            sprintf(tmp,"\n");
    		vfs_write(fp, tmp, strlen(tmp),&fp->f_pos);   
        }
    }
    filp_close(fp, NULL);
	set_fs(fs);
	printk("End wirte file;\n"); 
    aa_free_mem();
    tt_sc_iounmap();
    return;
}
                    
void get_aa_cb_statistic(u32 type,u32 _time)
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
    tsk = kthread_create((void*)aa_cb_statistic_thread,(void*)data,"aa_cb statistic");
    if(tsk == NULL){
        printk("[WARNING] thread create failed;\n");
        return;
    }
    printk("aa_cb statistic wake up process;\n");
    msleep(100);
    wake_up_process(tsk);
 
    return;   
}                                     
