#!/bin/bash
#
# del_stl_duplicates.sh : remove .stl duplicates in a directory
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# authors:
# (C) 2009  Matthias Wenzel <reprap at mazzoo dot de>
#

MD5=del_stl_duplicates.md5sum
md5sum *.stl | sort > ${MD5}

LAST_HASH="invalid"

while read HASH FILE ; do
	if [ ${LAST_HASH} == ${HASH} ] ; then
		echo "removing duplicate ${FILE}"
		rm ${FILE}
	fi
	LAST_HASH=${HASH}
done < ${MD5}
rm -f ${MD5}

