#!/bin/bash
SMV_DIR="../.."
KERNEL_DIR="$SMV_DIR/kernelspace/linux-4.4.5-smv/"
if [ $# -eq 0 ]; then
    echo "Please input remote destination."
    exit -1
fi

echo "=========== Copying modified kernel files from local to $1 ========="
rsync -auv --rsh='ssh ' --exclude-from rsync_exclude  $KERNEL_DIR terry@$1:/home/terry/workspace/smv/kernelspace/linux-4.4.5-smv/
ret=$?
if [ $ret -eq 0 ]; then
    echo "====================== Copied successfully ========================"
else
    echo "====================== Copied failed, error: $ret ======================="
fi
exit $ret
