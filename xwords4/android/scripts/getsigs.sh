#!/bin/bash

set -e -u

NODE=xw4
VARIANT=xw4NoSMS
CLASSPATH=${CLASSPATH:-""}

usage() {
    [ $# -gt 0 ] && echo "Error: $1"
    echo "usage: $0 [--variant xw4|xw4d]"
    exit 1
}
while [ $# -gt 0 ]; do
	case $1 in
		--help)
			usage
			;;
		--variant)
			VARIANT=$2
			shift
			;;
		*)
			usage "unexpected flag $1"
			;;
	esac
	shift
done

cd $(dirname $0)/../app/build/intermediates/classes/${VARIANT}/debug

javah -o /tmp/javah$$.txt org.eehouse.android.${NODE}.jni.XwJNI
javap -s org.eehouse.android.${NODE}.jni.XwJNI
javap -s org.eehouse.android.${NODE}.jni.DrawCtx
javap -s org.eehouse.android.${NODE}.jni.UtilCtxt
javap -s org.eehouse.android.${NODE}.jni.CommsAddrRec
javap -s org.eehouse.android.${NODE}.jni.CommsAddrRec\$CommsConnTypeSet
javap -s org.eehouse.android.${NODE}.jni.TransportProcs
javap -s org.eehouse.android.${NODE}.jni.JNIUtils
javap -s org.eehouse.android.${NODE}.Utils
cat /tmp/javah$$.txt
rm /tmp/javah$$.txt
