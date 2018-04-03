0.安装与使用：https://github.com/YvesZHI/hi1616dfx/blob/master/manual.txt

1.insmod dfx.ko（需要保证编译时使用内核和使用环境内核匹配，版本不一致时需重新编译）

2.配置参数（测试类型和测试时间），以echo "1 10" > /proc/HI1616_DFX形式配置，1为类型，表示DDR和LLC，10表示时间为10秒，结果输出在/home下
测试类型对照如下：
参数：类型：输出路径
1：DDR和LLC：/home/llc_ddr_statistic：
2：HHA和SLLC：/home/hha_sllc_statistic每行12个数据，对应12个事件的统计结果，整理后输出不需经过公式转换；SLLC，每行4个数据，表示TX侧request、snoop、response、data通道packet计数寄存器
3：AA read：/home/aa_rd_statistic
4：AA write：/home/aa_wr_statistic：
5: AA copyback：/home/aa_cb_statistic
6: PA和HLLC：/home/pa_statistic：

3.整理数据
python parse.py -c 测试条件 -t 时间  -o 输出文件名

测试条件：
1）aa_cb
2)aa_rd
3)aa_wr
4)hha
5)hllc
6)llc
7)pa
9)sllc
时间：
和2中测试时间保持一致

4.整理数据输出格式
类型：输出数据：整理后输出格式
DDR：每行数据依情况而定，依次为DDR0 wr,DDR0 rd,DDR1 wr,DDR1 rd…：整理后输出要求为带宽统计，可用DDR中统计结果乘以32，转换为byte/s，经parse.py整理后单位为MB/s
LLC：每行8个数据，只需关注前6个，对应需求列表中的6个统计结果：整理后输出为读命中率4/(0+2)，写命中率5/(1+3)，（计算公式中数字表示6个统计结果的序号）
HHA：每行12个数据，对应12个事件的统计结果：整理后输出不需经过公式转换
SLLC：每行4个数据，表示TX侧request、snoop、response、data通道packet计数寄存器：整理后输出要求为带宽统计，可用各项统计结果乘以16，转换为byte/s，经parse.py整理后单位为MB/s
AA：每行包括4个AA的延时（平均延时、采样个数、最大延时），延时单位为cycle，整理后输出为平均延时和最大延时
PA：每行包括RX侧Hydra Port0的request、snoop、response、data通道计数，Hydra Port1的，Hydra port2的……：整理后输出不需经过公式转换
HLLC：每行包含8个数据，分别为channel0,1,2,3的PA发送给HLLC的flit计数，channel0,1,2,3的HLLC发给PA的flit计数：整理后输出要求为带宽统计，可用各项统计结果乘以16，转换为byte/s，经parse.py整理后单位为MB/s

