#! /bin/sh
#/**
# * @file make_osdep.sh
# * @short Script for generation OS dependent constant.. - not very nice ;-(
# *
# * (c)2002 by Marek Zelem <marek@terminus.sk>
# * $Id: make_osdep.sh,v 1.2 2002/10/23 10:25:43 marek Exp $
# */

grep '#define' /usr/include/linux/capability.h
cat /usr/include/asm-generic/unistd.h | awk '/^#define[        ]+__NR_[a-zA-Z0-9_]+[   ]+[0-9xXa-fA-F]+/ {print "\t{\"sys_" substr($2,6) "\",T_num," $3 "},"; }'

cat /usr/include/linux/capability.h | \
 awk '/^#define[ 	]+CAP_[a-zA-Z0-9_]+[ 	]+[0-9xXa-fA-F]+/  {print "\t{\"" $2 "\",T_num," $3 "},"; }'

