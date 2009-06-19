#!/bin/bash
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

