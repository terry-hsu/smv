#!/usr/bin/env python

# eval script for httpd benchmarks

import os
import sys
import re
import time
import multiprocessing

all_sizes = ['10KB', '100KB', '1MB', '2M', '4M', '8M', '50M', '100M']
#all_sizes = ['5000']
print (sys.version)

runs = 10
requests = 10000
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
        cmd = "truncate -s " + sz + " ./build/htdocs/htdoc_" + sz + ".html"
        rv = os.system(cmd)

def createObjects (size):
    total_size = 1024 * 1024 * size
    sz = 100 # 100KB/object
    i = 0
    while(total_size > 0):
        cmd = "truncate -s " + str(sz) + "KB" +" ./build/htdocs/objects/object_" + str(i) + ".obj"
        os.system(cmd)
        total_size = total_size - (sz*1024)
        i = i + 1

    # get the list of file names just created
    fn = "build/htdocs/htdoc_" + str(size) + "MB.log"
    cmd = "ls -al build/htdocs/objects | awk '{print $9}' | sort > " + fn
    os.system(cmd)
    # remove first 3 lines for the current and the upper directories
    #cmd = "sed -e '1,3d' " + fn
    #os.system(cmd)
    # convert space to ASCII NULL for httperf
    #cmd = "tr \"\\n\" \"\\0\" < " + fn + " > build/htdocs/wlog.log"   
    #os.system(cmd)

def run():
    i = 1
    rv = 0
    #start httpd server
    cmd = "sudo ./build/bin/httpd -k start -X &"
    print "[smv-eval] Executing: " + cmd
    os.system(cmd)
    time.sleep(3)
       
    #start ab
    print "[smv-eval] starting ApacheBench"
    for sz in input_size :
        i = 1
        while (i <= runs):
            print "[smv-eval] Run " + str(i) + " ab for page sz " + sz
            cmd = "./build/bin/ab -k -n " + str(requests) + " -c " + str(concurrency) + " http://localhost/htdoc_" + sz + ".html"
            print "[smv-eval] Executing: " + cmd
            rv = os.system(cmd)
            i = i + 1

    #stop the server
    cmd = "sudo ./build/bin/httpd -k stop"
    print "[smv-eval] Executing: " + cmd
    os.system(cmd)

def main():
    print "[smv-eval]--------------------------------------------Welcome--------------------------------------"
    print "[smv-eval] Runs " + str (runs) + ", concurrent level " + str(concurrency) + ", requests " + str(requests)
    start_time = time.time()
    createPages()
    #createObjects(10) # create 10MB of objects
    run()
    elapsed_time = time.time() - start_time
    print "[smv-eval] Finished, time: " + str(elapsed_time) + " seconds."

# call main function
main()
