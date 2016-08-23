#!/bin/bash
SMV_DIR="$(PWD)/../"
USERSPACE_DIR="$SMV_DIR/userspace/"
if [ $# -eq 0 ]; then
echo "Please input remote destination."
exit -1
fi

echo "=========== Copying modified files from local to $1 ========="
rsync -auv --rsh='ssh ' --no-times --exclude-from rsync_exclude  $USERSPACE_DIR terry@$1:/home/terry/workspace/smv/userspace/
ret=$?
if [ $ret -eq 0 ]; then
echo "====================== Copied successfully ========================"
else
echo "====================== Copied failed, error: $ret ======================="
fi
exit $ret
