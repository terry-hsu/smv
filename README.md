SMVs: Enforcing Least Privilege Memory Views for Multithreaded Applications
---------------------------------------------------------------------


### Authors ###
- [Terry Hsu](http://www.cs.purdue.edu/homes/hsu62) <<terryhsu@purdue.edu>>

- [Kevin Hoffman](http://www.kevinjhoffman.com/) <<khoffman@efolder.net>>

- [Patrick Eugster](https://www.cs.purdue.edu/homes/peugster/) <<peugster@cs.purdue.edu>>

- [Mathias Payer](http://nebelwelt.net/) <<mathias.payer@nebelwelt.net>>


### What do SMVs do? ###
SMVs provide strong threads compartmentalization through kernel-level page table management.

### Source tree structure ###
- kernelspace/: things run in kernel space      
    - linux-4.4.5-smv/: core of the modified Linux kernel suppor for the SMV model.       
    - netlink-module/: communication channel for user space programs 
- userspace/: things run in user space          
    - api/: user space smv library 
    - apps/: real-world applications use cases 
        - firefox/firefox-45.0-smv/: security-enhanced firefox desktop web browser 
        - httpd/httpd-2.4.18/: security-enhanced Apache httpd web server 
        - cherokee/: security-enhanced Cherokee web server         
    - benchmarks/:  
        - parsec-3.0/: multithreaded C/C++ benchmarks for complex thread/memory interaction. 
    - testcases/: simple test cases for the smv model; used in early development stage.         
- docs/: CCS'16 research paper describing the smv model design rationale. 

### Using SMVs ###
- SMVs have been tested on x86 platform.
- To use SMVs, you need to compile SMV kernel, SMV LKM, and SMV API.
    - To build SMV kernel (based on Linux kernel 4.4.5) and SMV LKM (loadable kernel module):
        - ``` % cd $SMV/kernelspace/ ```
        - ``` % ./x86_install_native.sh ```
    - To build SMV API (userspace API):
        - ``` % cd $SMV/userspace/ ```
        - ``` % ./install_api.sh ```
- Then, include the following header in your source code:
    - ``` #include <smv_lib.h> ```
- Finally, compile your program by linking the smv library:
    - ``` g++ target.c -o target -lsmv_lib ```

### Citing SMVs ###

If you use SMVs, please cite our reearch paper published at CCS'16, included as doc/smv-ccs16.pdf.

@InProceedings{smv,   
 author    = {Hsu, Terry Ching-Hsiang and Hoffman, Kevin and Eugster, Patrick and Payer, Mathias},   
 title     = {{Enforcing Least Privilege Memory Views for Multithreaded Applications}},   
 booktitle = {Proceedings of the 23rd ACM Conference on Computer and Communications Security},   
 year      = {2016},   
 series    = {CCS '16},   
 address   = {New York, NY, USA},   
 publisher = {ACM},   
 doi       = {10.1145/2976749.2978327},   
 isbn      = {978-1-4503-4139-4/16/10},   
 location  = {Vienna, Austria},   
 url       = {http://dx.doi.org/10.1145/2976749.2978327},   
}

### Acknowledgement ###
This work was supported by the National Science Foundation under grant No. TC-1117065, TWC- 1421910, CNS-1464155, 
and by European Research Council under grant FP7- 617805.

