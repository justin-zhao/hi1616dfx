#ifndef DFX_COMMON
#define DFX_COMMON

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/proc_fs.h>


#define OS_READ_REG(reg,index) (readl((u64)reg + 4 * index))
#define OS_WRITE_REG(reg,index,val) (writel(val,(u64)reg + 4 * index))


#endif

