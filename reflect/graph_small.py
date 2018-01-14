import matplotlib
matplotlib.use("Agg")
import sys
import matplotlib.pyplot as plt
import os
import collections
import numpy as np
import pandas as pd
import seaborn as sns

PACKETS_SENT=100000000

sns.set_style("whitegrid")


dir_prefix = './64_multicore/'
file_arr = os.listdir(dir_prefix)
pdict = {}



def mean(data):
    """Return the sample arithmetic mean of data."""
    n = len(data)
    if n < 1:
        raise ValueError('mean requires at least one data point')
    return sum(data)/n # in Python 2 use sum(data)/float(n)

def _ss(data):
    """Return sum of square deviations of sequence data."""
    c = mean(data)
    ss = sum((x-c)**2 for x in data)
    return ss

def stddev(data, ddof=0):
    """Calculates the population standard deviation
    by default; specify ddof=1 to compute the sample
    standard deviation."""
    n = len(data)
    if n < 2:
        raise ValueError('variance requires at least two data points')
    ss = _ss(data)
    pvar = ss/(n-ddof)
    return pvar**0.5


for ffile in file_arr:
    fp = open(dir_prefix+ffile)
    mem_access=int(ffile.split('_')[1])
    if mem_access not in pdict:
        pdict[mem_access] = []
    for i, line in enumerate(fp):
        if i == 47:
            packet_rate = 1-(float(line.split('=')[1].strip('\n'))/PACKETS_SENT)
            pdict[mem_access].append(packet_rate)
    fp.close()


o_pdict = collections.OrderedDict(sorted(pdict.items()))




#for key in o_pdict:
#    mem_mean = (1-(float(mean(pdict[key])))/PACKETS_SEN
#    mem_std = (int(stddev(pdict[key])))
#    o_pdict[key] = []
#    o_pdict[key].append(mem_mean)
#    o_pdict[key].append(mem_std)

#print o_pdict


############Fonts#################
SMALL_SIZE = 16
MEDIUM_SIZE = 22
BIGGER_SIZE = 32

plt.rc('font', size=SMALL_SIZE)          # controls default text sizes
plt.rc('axes', titlesize=SMALL_SIZE)     # fontsize of the axes title
plt.rc('axes', labelsize=MEDIUM_SIZE)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
plt.rc('ytick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
plt.rc('legend', fontsize=SMALL_SIZE)    # legend fontsize
plt.rc('figure', titlesize=BIGGER_SIZE)  # fontsize of the figure title
##################################




clist = []
fig,axes = plt.subplots(nrows=1, ncols=1,figsize=(36,15))

for key in o_pdict:
    clist.append((key, o_pdict[key][0]))
    clist.append((key, o_pdict[key][1]))
    clist.append((key, o_pdict[key][2]))
    clist.append((key, o_pdict[key][3]))

df = pd.DataFrame(data=clist, columns=['mem access', 'PDR'])
sns.set_color_codes('muted')
sns_plot = sns.barplot(x='mem access', y='PDR', data=df, color="b")


fig = sns_plot.get_figure()
fig.savefig("out.png")



    
#print clist


#print o_pdict

#plt.figure(num=None, figsize=(24, 18), dpi=80, facecolor='w', edgecolor='k')
#plt.plot(drd)



#mean_pdict = {}
#
#mem_labels = []
#mem_mean = []
#mem_std = []
#
#for key in pdict:
#    mem_labels.append(int(key))
#    mem_mean.append(int(mean(pdict[key])))
#    mem_std.append(int(stddev(pdict[key])))
#
#mem_labels = sorted(mem_labels)

#print mem_labels
#
#
#for key in pdict:
#    print key, mean(pdict[key]), int(stddev(pdict[key]))
#
##print pdict
    
