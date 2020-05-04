#!/bin/sh
if [ ! -e user-settings.mkfg ] ; then
    printf '/LIBS/GUID = "%.24s%.12s"\n' `cat build-id` `basename "$(awk -F: '$3~/^\/docker/{print $3;exit}' /proc/self/cgroup)"` > user-settings.mkfg
fi
