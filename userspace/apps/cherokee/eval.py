#!/usr/bin/env python

# eval script for Cherokee benchmarks

import os
import sys
import re
import time
import multiprocessing

all_sizes = ['10KB', '100KB', '1MB', '2M', '4M', '8M', '50M', '100M']
#all_sizes = ['5000']
print (sys.version)

runs = 10
requests = 100000
#concurrency = multiprocessing.cpu_count()
concurrency = 4
input_size=[]

for a in sys.argv:
    if re.match('^[0-9]+$', a):
        runs = int(a)
    elif a in all_sizes:
        input_size.append(a)

if(len(input_size)==0):
    input_size = all_sizes

def createPages ():
    for sz in input_size:
        cmd = "truncate -s " + sz + " ./build/var/www/htdoc_" + sz + ".html"
        rv = os.system(cmd)

def run():
    i = 1
    rv = 0
    #start cherokee server
    cmd = "sudo ./build/sbin/cherokee-worker &"
    print "[smv-eval] Executing: " + cmd
    os.system(cmd)
    time.sleep(3)
       
    #start ab
    print "[smv-eval] starting ApacheBench"
    for sz in input_size :
        i = 1
        while (i <= runs):
            print "[smv-eval] Run " + str(i) + " ab for page sz " + sz
            cmd = "ab -k -n " + str(requests) + " -c " + str(concurrency) + " http://localhost/htdoc_" + sz + ".html"
            print "[smv-eval] Executing: " + cmd
            rv = os.system(cmd)
            i = i + 1

    #stop the server
    cmd = "sudo pkill -9 cherokee-worker"
    print "[smv-eval] Executing: " + cmd
    os.system(cmd)

def main():
    print "[smv-eval]--------------------------------------------Welcome--------------------------------------"
    print "[smv-eval] Runs " + str (runs) + ", concurrent level " + str(concurrency) + ", requests " + str(requests)
    start_time = time.time()
    createPages()
    run()
    elapsed_time = time.time() - start_time
    print "[smv-eval] Finished, time: " + str(elapsed_time) + " seconds."

# call main function
main()
