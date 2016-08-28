#!/bin/bash
SMV_DIR="$(PWD)/../"
USERSPACE_DIR="$SMV_DIR/userspace/"
if [ $# -lt 3 ]; then
echo "./rsync_userspace.sh username url dest"
exit -1
fi

echo "=========== Copying modified files from local to $2 ========="
rsync -auv --rsh='ssh ' --no-times --exclude-from rsync_exclude  $USERSPACE_DIR $1@$2:$3
ret=$?
if [ $ret -eq 0 ]; then
echo "====================== Copied successfully ========================"
else
echo "====================== Copied failed, error: $ret ======================="
fi
exit $ret
