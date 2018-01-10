#include "../djtag/djtag.h"

#define DEVICE_NAME "HI1616_DFX"
MODULE_LICENSE("Dual BSD/GPL");

extern void get_llc_statistic(u32 type,u32 _time);
extern void get_hha_statistic(u32 type,u32 _time);
extern void get_aa_rd_statistic(u32 type,u32 _time);
extern void get_aa_wr_statistic(u32 type,u32 _time);
extern void get_aa_cb_statistic(u32 type,u32 _time);
extern void get_pa_statistic(u32 type,u32 _time);

static void list_all(void)
{
    return ;
}

void tt_sc_ioremap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        totem_djtag_base[i] =(u64)ioremap(tt_sc_phy[i],SYSCTRL_REG_SPACE);   
        if(!totem_djtag_base[i])
            goto exit;
        printk("totem_djtag_base[%d] = 0x%llx;phy_base[0x%llx]\r\n",i,totem_djtag_base[i],tt_sc_phy[i]);    
    }
    return;
exit:
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        if(totem_djtag_base[i])
            iounmap((void*)totem_djtag_base[i]);
    }

     return;
}

void tt_sc_iounmap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        if(totem_djtag_base[i])
            iounmap((void*)totem_djtag_base[i]);
    }

    return;        
}

void ni_sc_ioremap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        nimbus_djtag_base[i] =(u64)ioremap(ni_sc_phy[i],SYSCTRL_REG_SPACE);   
        if(!nimbus_djtag_base[i])
            goto exit;
        printk("nimbus_djtag_base[%d] = 0x%llx\r\n",i,nimbus_djtag_base[i]);    
    }
    return;
    
exit:
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        if(nimbus_djtag_base[i])
            iounmap((void*)nimbus_djtag_base[i]);
    }
    
    return;
}

void ni_sc_iounmap(void)
{
    u32 i;
    for(i = 0; i < DJTAG_DIE_NUM;i++){
        if(nimbus_djtag_base[i])
            iounmap((void*)nimbus_djtag_base[i]);
    }

    return;        
}



static ssize_t hi1616_dfx_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char mbuf[100];
    char *p;
    int type,_count;
    char *temp = mbuf;

    _count = copy_from_user(mbuf,buf,count);
    p = strsep(&temp," ");
    if(p == NULL){
        list_all();
        return count;
    }

    type = simple_strtoull(p,0,16);
    
    switch(type & 0xf)
    {
        case 0:
            list_all();
            break;
        case 1:
            get_llc_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break; 
        case 2:
            get_hha_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break; 
        case 3:
            get_aa_rd_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break; 
        case 4:
            get_aa_wr_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break;  
        case 5:
            get_aa_cb_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break;
        case 6:
            get_pa_statistic(type,simple_strtoull(strsep(&temp, " "),NULL,10));
            break; 
        default:
            printk("choice option[1-6] is wrong;\n");                      
    }

    return count;
}

static int hi1616_dfx_open(struct inode *inode, struct file *file)
{
    return 0;
}


static const struct file_operations dfx_fops = {
    .owner = THIS_MODULE,
    .write = hi1616_dfx_write,
    .open  = hi1616_dfx_open,
};


static int __init hi1616_dfx_init(void)
{
    struct proc_dir_entry *perf_dir;

    perf_dir = proc_create(DEVICE_NAME,S_IRUGO,NULL,&dfx_fops);
    if(perf_dir == NULL)
        printk("[ERROR]create proc file fail;\n");

    printk(KERN_ERR "Hi1616 DFX driver init successully;\n");

    return 0;
}


static void __exit hi1616_dfx_exit(void)
{
    remove_proc_entry(DEVICE_NAME, NULL);
    printk(KERN_ERR "Hi1616 dfx driver exited;\n");

    return ;
}

module_init(hi1616_dfx_init);
module_exit(hi1616_dfx_exit);
MODULE_AUTHOR("z00228467 zhangshaokun");
MODULE_DESCRIPTION("Hi1616 DFX");
