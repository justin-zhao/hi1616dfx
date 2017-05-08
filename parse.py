#!/usr/bin/env python
import argparse
import sys
#result = []
rt = ''
def key_counts(key,list):
    count = 0
    index = len(list)
    for i in range(len(list)):
        if key in list[i]:
            count +=1
            index = (index if i > index else i)
    #print('min=%d' % min)
    return count, index

def check_aa_type(file_name):
    if 'rd' in file_name:
        return 'rd'
    elif 'wr' in file_name:
        return 'wr'
    elif 'cb' in file_name:
        return 'cb'

def work(block_size, input_file, type):
    #global result
    global rt
    lines_temp = [l.strip() for l in input_file.readlines()] 
    newlines = []
    count,index = key_counts(type, lines_temp)
    blocks = count
    lines = lines_temp[index:index + count * block_size]
    #print lines
    #print '%s' % type
    for i in range(blocks):
        first_line = lines[i * block_size]
        
        if 'AA' in first_line:
            aa_type = check_aa_type(input_file.name)
            newlines.append(first_line + "-" + aa_type + "(cycles)" + "," * 7)
            newlines.append(','.join(['aa%d_avg_lat, aa%d_max_lat' % (idx, idx) for idx in range(4)]))
            idxs = [0,2,3,5,6,8,9,11]
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                newlines.append(','.join([l.split(',')[idx]for idx in idxs]))
                
        elif 'PA' in first_line:     
            newlines.append(first_line + "," * 11)
            newlines.append(','.join(['prot%d_rx_req, port%d_rx_sno, port%d_rx_rsp, port%d_rx_dat' % (idx, idx, idx, idx) for idx in range(3)]))
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                newlines.append(','.join([l.split(',')[idx]for idx in range(12)]))
                
        elif 'HLLC' in first_line:
            newlines.append(first_line + "(MB/s)" + "," * 7)
            title = ','.join(['ch%d_pa_hllc' % idx for idx in range(4)])
            title += ',' + ','.join(['ch%d_hllc_pa' % idx for idx in range(4)])
            #title = ','.join(['ch%d_%s' % (idx, from_obj) for from_obj in ['pa_hllc', 'hllc_pa'] for idx in range(4)])
            newlines.append(title)
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                #newlines.append(','.join([l.split(',')[idx]for idx in range(8)]))
                l1 = [l.split(',')[idx]for idx in range(8)]
                #l2 = [str(float(i) * 16) for i in l1]
                l2 = ['%.2f' % (float(i) * 16 / (2**20)) for i in l1]
                newlines.append(','.join(l2))
                
        elif 'HHA' in first_line:
            newlines.append(first_line + "," * 11)
            title = ','.join(['cmd_count', 'llc_inquery', 'snoop_llc', 'llc_dir_hit', 'ddr_rd', 'ddr_wr', 'outer_op', 'ddr_data_rd', 'ddr_data_wr', 'backinvalid', 'cache_dir_hit', 'ddr_dir_hit'])
            newlines.append(title)
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                newlines.append(','.join([l.split(',')[idx]for idx in range(12)]))
                
        elif 'SLLC' in first_line:
            newlines.append(first_line +  "(MB/s)" + "," * 3)
            title = ','.join(['tx_pk_req', 'tx_pk_sno', 'tx_pk_rsp', 'tx_pk_dat'])
            newlines.append(title)
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                #newlines.append(','.join([l.split(',')[idx]for idx in range(4)]))
                l1 = [l.split(',')[idx]for idx in range(4)]
                #l2 = [str(int(i) * 16) for i in l1]
                l2 = ['%.2f' % (float(i) * 16 / (2**20)) for i in l1]
                newlines.append(','.join(l2))
                
        elif 'DDR' in first_line:
            col_num = len(lines[i * block_size + 1].split(',')[:-1])
            newlines.append(first_line +  "(MB/s)" + "," * (col_num - 1))
            title = ','.join(['ddr%d_wr, ddr%d_rd' % (idx, idx) for idx in range(col_num / 2)])
            newlines.append(title)
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                l1 = [l.split(',')[idx]for idx in range(col_num)]
                #l2 = [str(int(i) * 64) for i in l1]
                l2 = ['%.2f' % (float(i) * 32 / (2**20)) for i in l1]
                #print l2
                #newlines.append(','.join([l.split(',')[idx]for idx in range(col_num)]))
                newlines.append(','.join(l2))
                
        elif 'LLC' in first_line:
            col_num = len(lines[i * block_size + 1].split(',')[:-1])
            
            newlines.append(first_line + "(%)" + ",")
            title = ','.join(['rd_hit', 'wr_hit'])
            newlines.append(title)
            tmplines = []
            for l in lines[i * block_size + 2: (i + 1) * block_size]: 
                l1 = [l.split(',')[idx]for idx in range(6)]
                l1 = [float(i) for i in l1]
                #tmplines.append(l1)
                rd_hit = l1[0] + l1[2]
                rd_hit = l1[4] / rd_hit if rd_hit > 0 else 0
                wr_hit = l1[1] + l1[3]
                wr_hit = l1[5] / wr_hit if wr_hit > 0 else 0
                #sum_col = ['%.2f, %.2f' % (sum_col[4] / (sum_col[0] + sum_col[2]), sum_col[5] / (sum_col[1] + sum_col[3]))]
                l2 = ['%.2f, %.2f' % (rd_hit * 100, wr_hit * 100)]
                newlines.append(','.join(l2))
                
            '''
            sum_col = [sum(i) for i in zip(*tmplines)]
            rd_hit = sum_col[0] + sum_col[2]
	    rd_hit = sum_col[4] / rd_hit if rd_hit > 0 else 0
            wr_hit = sum_col[1] + sum_col[3]
	    wr_hit = sum_col[5] / wr_hit if wr_hit > 0 else 0
            #sum_col = ['%.2f, %.2f' % (sum_col[4] / (sum_col[0] + sum_col[2]), sum_col[5] / (sum_col[1] + sum_col[3]))]
	    sum_col = ['%.2f, %.2f' % (rd_hit, wr_hit)]
            tmplines = []
            #tmplines.append(sum_col)
            #print sum_col
            newlines.append(','.join(sum_col))
            #print newlines
            for i in range(block_size - 3):
                #tmplines.append([])
                newlines.append(',')
            '''
             
    #print('\n'.join([','.join([newlines[i + j * (block_size + 1)] for j in range(blocks)])for i in range(block_size + 1)]))
    
    #mergelines = [','.join([newlines[i + j * (block_size)] for j in range(blocks)])for i in range(block_size)]
    '''
    if result:
        result = [result[i] + ',' + mergelines[i] for i in range(len(mergelines))]     
    else:
        result = mergelines
    '''
    #result.append(newlines)
    #print result
    rt = rt + '\n' + '\n'.join(newlines)

if __name__ == '__main__':
    
    parser = argparse.ArgumentParser(description='dfx data convert')
    parser.add_argument('-c', '--condition', type=str, default=None)
    parser.add_argument('-t', '--time', type=int, required=True)
    #parser.add_argument('-i', '--input', required=True, metavar='in-file', type=argparse.FileType('r'))
    parser.add_argument('-o', '--output', default=None, metavar='out-file', type=argparse.FileType('w+'))

    args = parser.parse_args()
    '''
    file_type = {
                "/home/hha_sllc_statistic":"HHA",
                "/home/llc_ddr_statistic":"LLC",
                "/home/aa_rd_statistic":"AA",
                "/home/aa_wr_statistic":"AA",
                "/home/aa_cb_statistic":"AA",
                "/home/hha_sllc_statistic":"SLLC",
                "/home/pa_statistic":"PA",
                "/home/pa_statistic":"HLLC",
                "/home/llc_ddr_statistic":"DDR",
                }
    
    
    file_type = {
		"aa_rd_statistic":"AA",
                "aa_wr_statistic":"AA",
                "aa_cb_statistic":"AA",
		"llc_ddr_statistic":"DDR",
                "hha_sllc_statistic":"HHA",
                "pa_statistic":"PA",
                }
    file_type2 = {
		"pa_statistic":"HLLC",
                "llc_ddr_statistic":"LLC",
                "hha_sllc_statistic":"SLLC",
                }
    '''
    
    cond_path = {"aa_cb":"/home/aa_cb_statistic",
                 "aa_rd":"/home/aa_rd_statistic",
                 "aa_wr":"/home/aa_wr_statistic",
                 "ddr":"/home/llc_ddr_statistic",
                 "hha":"/home/hha_sllc_statistic",
                 "hllc":"/home/pa_statistic",
                 "llc":"/home/llc_ddr_statistic",
                 "pa":"/home/pa_statistic",
                 "sllc":"/home/hha_sllc_statistic"
                 }
    cond_type = {"aa_cb":"AA",
                 "aa_rd":"AA",
                 "aa_wr":"AA",
                 "ddr":"DDR",
                 "hha":"HHA",
                 "hllc":"HLLC",
                 "llc":"LLC",
                 "pa":"PA",
                 "sllc":"SLLC"
                 }
    cp = {}
    if args.condition is not None:
        condition = args.condition.lower()
        if  condition not in cond_path:
            print "condition: %s is error" % args.condition
            sys.exit(1)
        cp = {condition:cond_path[condition]}
    else:
        cp = cond_path
    
    for type, file_name in cp.items():
        with open(file_name) as input:
            work(args.time + 1, input, cond_type[type])
    '''
    for file_name, type in file_type2.items():
        with open(file_name) as input:
            work(args.time + 1, input, type)
    '''
    
    if args.output is not None:
        args.output.write(rt)
        args.output.close()
    
    print 'parse data over'

    

