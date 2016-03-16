#!/bin/bash
SMV_DIR="../"
KERNEL_DIR="$SMV_DIR/kernelspace/"
if [ $# -eq 0 ]; then
    echo "Please input remote destination."
    exit -1
fi

echo "=========== Copying modified kernel files from local to $1 ========="
rsync -auv --rsh='ssh ' --exclude-from rsync_exclude  $KERNEL_DIR terry@$1:/home/terry/workspace/smv/kernelspace/
ret=$?
if [ $ret -eq 0 ]; then
    echo "====================== Copied successfully ========================"
else
    echo "====================== Copied failed, error: $ret ======================="
fi
exit $ret
