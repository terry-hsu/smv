#!/bin/bash
SMV_DIR="../"
KERNEL_DIR="$SMV_DIR/kernelspace/"
if [ $# -lt 3 ]; then
    echo "./rsync-kernel.sh username url dir"
    exit -1
fi

echo "=========== Copying modified kernel files from local to $2 ========="
rsync -auv --rsh='ssh ' --no-times --exclude-from rsync_exclude  $KERNEL_DIR $1@$2:$3
ret=$?
if [ $ret -eq 0 ]; then
    echo "====================== Copied successfully ========================"
else
    echo "====================== Copied failed, error: $ret ======================="
fi
exit $ret
