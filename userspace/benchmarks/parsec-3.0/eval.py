#!/usr/bin/env python

# eval script for parsec benchmarks

import os
import sys
import re
import time
import multiprocessing

all_benchmarks = ['blackscholes', 'bodytrack', 'canneal', 'dedup', 'facesim', 'ferret', 'fluidanimate', 'raytrace',  'streamcluster', 'swaptions', 'vips', 'x264']
all_inputs = ['simsmall', 'simmedium', 'simlarge', 'native']
all_actions = ['build', 'uninstall', 'run']
print (sys.version)
print "[smv-eval]--------------------------------------------Welcome--------------------------------------"
print "[smv-eval] Usage: ./eval.py action [run/build/uninstall] benchmark inputsize runs ncore"

runs = 1
cores = 1
benchmarks = []
input_size='simlarge'
action = 'run'
cores = []
changedCores = 0
origCores = multiprocessing.cpu_count()

for a in sys.argv:
    if a in all_benchmarks:
        benchmarks.append(a)
    elif re.match('^[0-9]+$', a):
        runs = int(a)
    elif a in all_inputs:
        input_size = a
    elif a in all_actions:
        action = a
    elif re.match('^[0-9]+core$', a):
        if a == '1core':
            cores.append(1)
        elif a == '2core':
            cores.append(2)
        elif a == '4core':
            cores.append(4)
        elif a == '8core':
            cores.append(8)
        elif a == '12core':
            cores.append(12)

if(len(benchmarks)==0):
    benchmarks = all_benchmarks

def setCPU(core):
    changedCores = 1
    alive_cores = multiprocessing.cpu_count()
    cores_count = 1 # core 0 is always alive
    for cpu in range(1, multiprocessing.cpu_count()):
        if cpu % 2 == 0 and cores_count < cores:
            cmd = 'sudo echo 1 > /sys/devices/system/cpu/cpu'+str(cpu)+'/online'
            cores_count = cores_count + 1
        else :
            cmd = 'sudo echo 0 > /sys/devices/system/cpu/cpu'+str(cpu)+'/online'
        os.system(cmd);
    alive_cores = multiprocessing.cpu_count()
    print '[smv-eval] Set CPU done, alive #cores: ' + str(alive_cores)


def run():
    i = 1
    rv = 0
    for core in cores :
        if core != multiprocessing.cpu_count():
            setCPU(core)
        for bench in benchmarks:
            while (i <= runs):
                print "[smv-eval] Run " + str(i) + ": " + bench + " " + input_size
                cmd = "parsecmgmt -a run -p " + bench + " -i " + input_size + " -n " + str(core)
                print "[smv-eval] Executing: " + cmd
                rv = os.system(cmd)
                if rv != 0:
                    print "[smv-eval] error, rerun"
                    print "[smv-eval] -----------------------------------"
                    continue
                else:
                    print "[smv-eval] Run " + str(i) + " done."
                    print "[smv-eval] -----------------------------------"
                i = i + 1

def build():
    rv = 0
    for bench in benchmarks:
        print "[smv-eval] Building " + bench 
        cmd = "parsecmgmt -a build -p " + bench
        print "[smv-eval] Executing: " + cmd
        rv = os.system(cmd)
        if rv != 0 :
            print "[smv-eval] error building " + bench
            continue
        else:
            print "[smv-eval] successfully built " + bench

def uninstall():
    rv = 0
    for bench in benchmarks:
        print "[smv-eval] Uninstalling " + bench 
        cmd = "parsecmgmt -a uninstall -p " + bench
        print "[smv-eval] Executing: " + cmd
        rv = os.system(cmd)
        if rv != 0 :
            print "[smv-eval] error uninstalling " + bench
            continue
        else:
            print "[smv-eval] successfully uninstalled " + bench

def main():
    start_time = time.time()
    if action == 'run':
        run()
    elif action == 'build':
        build()
    elif action == 'uninstall':
        uninstall()
    elapsed_time = time.time() - start_time

    # resotre CPU cores
    if changedCores:
        setCPU(origCores)
    print "[smv-eval] Finished, time: " + str(elapsed_time) + " seconds."

# call main function
main()
