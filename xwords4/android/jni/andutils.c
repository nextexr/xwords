/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright © 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <sys/time.h>
#include <time.h>

#include "andutils.h"
#include "paths.h"

#include "comtypes.h"
#include "xwstream.h"

void
and_assert( const char* test, int line, const char* file, const char* func )
{
    XP_LOGF( "assertion \"%s\" failed: line %d in %s() in %s",
             test, line, file, func );
    __android_log_assert( test, "ASSERT", "line %d in %s() in %s",
                          line, file, func  );
}

#ifdef __LITTLE_ENDIAN
XP_U32
and_ntohl(XP_U32 ll)
{
    XP_U32 result = 0L;
    for ( int ii = 0; ii < 4; ++ii ) {
        result <<= 8;
        result |= ll & 0x000000FF;
        ll >>= 8;
    }

    return result;
}

XP_U16
and_ntohs( XP_U16 ss )
{
    XP_U16 result;
    result = ss << 8;
    result |= ss >> 8;
    return result;
}

XP_U32
and_htonl( XP_U32 ll )
{
    return and_ntohl( ll );
}


XP_U16
and_htons( XP_U16 ss ) 
{
    return and_ntohs( ss );
}
#else
error error error
#endif

int
getInt( JNIEnv* env, jobject obj, const char* name )
{
    // XP_LOGF( "%s(name=%s)", __func__, name );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    int result = (*env)->GetIntField( env, obj, fid );
    deleteLocalRef( env, cls );
    // XP_LOGF( "%s(name=%s) => %d", __func__, name, result );
    return result;
}

void
getInts( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        uint8_t* ptr = ((uint8_t*)cobj) + si->offset;
        int val = getInt( env, jobj, si->name );
        switch( si->siz ) {
        case 4:
            *(uint32_t*)ptr = val;
            break;
        case 2:
            *(uint16_t*)ptr = val;
            break;
        case 1:
            *ptr = val;
            break;
        }
        /* XP_LOGF( "%s: wrote int %s of size %d with val %d at offset %d", */
        /*          __func__, si->name, si->siz, val, si->offset ); */
    }
}

void
setInt( JNIEnv* env, jobject obj, const char* name, int value )
{
    // XP_LOGF( "%s(name=%s)", __func__, name );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "I");
    XP_ASSERT( !!fid );
    (*env)->SetIntField( env, obj, fid, value );
    deleteLocalRef( env, cls );
    // XP_LOGF( "%s(name=%s) DONE", __func__, name );
}

void
setInts( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        uint8_t* ptr = ((uint8_t*)cobj) + si->offset;
        int val;
        switch( si->siz ) {
        case 4:
            val = *(uint32_t*)ptr;
            break;
        case 2:
            val = *(uint16_t*)ptr;
            break;
        case 1:
            val = *ptr;
            break;
        default:
            val = 0;
            XP_ASSERT(0);
        }
        setInt( env, jobj, si->name, val );
        /* XP_LOGF( "%s: read int %s of size %d with val %d from offset %d",  */
        /*          __func__, si->name, si->siz, val, si->offset ); */
    }
}

bool
setBool( JNIEnv* env, jobject obj, const char* name, bool value )
{
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Z");
    if ( 0 != fid ) {
        (*env)->SetBooleanField( env, obj, fid, value );
        success = true;
    }
    deleteLocalRef( env, cls );

    return success;
}

void
setBools( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_Bool val = *(XP_Bool*)(((uint8_t*)cobj)+si->offset);
        setBool( env, jobj, si->name, val );
        /* XP_LOGF( "%s: read bool %s with val %d from offset %d", __func__, */
        /*          si->name, val, si->offset ); */
    }
}

bool
setString( JNIEnv* env, jobject obj, const char* name, const XP_UCHAR* value )
{
    // XP_LOGF( "%s(%s)", __func__, name );
    bool success = false;
    jclass cls = (*env)->GetObjectClass( env, obj );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Ljava/lang/String;" );
    deleteLocalRef( env, cls );

    if ( 0 != fid ) {
        jstring str = (*env)->NewStringUTF( env, value );
        (*env)->SetObjectField( env, obj, fid, str );
        success = true;
        deleteLocalRef( env, str );
    }

    return success;
}

void
getStrings( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_UCHAR* buf = (XP_UCHAR*)(((uint8_t*)cobj) + si->offset);
        getString( env, jobj, si->name, buf, si->siz );
    }
}

void
setStrings( JNIEnv* env, jobject jobj, void* cobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        // XP_LOGF( "calling setString(%s)", si->name );
        XP_UCHAR* val = (XP_UCHAR*)(((uint8_t*)cobj) + si->offset);
        setString( env, jobj, si->name, val );
    }
}

void
getString( JNIEnv* env, jobject obj, const char* name, XP_UCHAR* buf,
           int bufLen )
{
    // XP_LOGF( "%s(%s)", __func__, name );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Ljava/lang/String;" );
    XP_ASSERT( !!fid );
    jstring jstr = (*env)->GetObjectField( env, obj, fid );
    jsize len = 0;
    if ( !!jstr ) {             /* might be null */
        len = (*env)->GetStringUTFLength( env, jstr );
        XP_ASSERT( len < bufLen );
        const char* chars = (*env)->GetStringUTFChars( env, jstr, NULL );
        XP_MEMCPY( buf, chars, len );
        (*env)->ReleaseStringUTFChars( env, jstr, chars );
        deleteLocalRef( env, jstr );
    }
    buf[len] = '\0';

    deleteLocalRef( env, cls );
    // XP_LOGF( "%s(%s) DONE", __func__, name );
}

XP_UCHAR* 
getStringCopy( MPFORMAL JNIEnv* env, jstring jstr )
{
    XP_UCHAR* result = NULL;
    if ( NULL != jstr ) {
        jsize len = 1 + (*env)->GetStringUTFLength( env, jstr );
        const char* chars = (*env)->GetStringUTFChars( env, jstr, NULL );
        result = XP_MALLOC( mpool, len );
        XP_MEMCPY( result, chars, len );
        (*env)->ReleaseStringUTFChars( env, jstr, chars );
    }
    return result;
}

bool
getObject( JNIEnv* env, jobject obj, const char* name, const char* sig,
           jobject* ret )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, sig );
    XP_ASSERT( !!fid );
    *ret = (*env)->GetObjectField( env, obj, fid );
    XP_ASSERT( !!*ret );

    deleteLocalRef( env, cls );
    return true;
}

void
setObject( JNIEnv* env, jobject obj, const char* name, const char* sig,
           jobject val )
{
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, sig );
    XP_ASSERT( !!fid );
    (*env)->SetObjectField( env, obj, fid, val );

    deleteLocalRef( env, cls );
}

bool
getBool( JNIEnv* env, jobject obj, const char* name )
{
    bool result;
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, name, "Z");
    XP_ASSERT( !!fid );
    result = (*env)->GetBooleanField( env, obj, fid );
    deleteLocalRef( env, cls );
    return result;
}

void
getBools( JNIEnv* env, void* cobj, jobject jobj, const SetInfo* sis, XP_U16 nSis )
{
    for ( int ii = 0; ii < nSis; ++ii ) {
        const SetInfo* si = &sis[ii];
        XP_Bool val = getBool( env, jobj, si->name );
        *(XP_Bool*)(((uint8_t*)cobj)+si->offset) = val;
        /* XP_LOGF( "%s: wrote bool %s with val %d at offset %d", __func__,  */
        /*          si->name, val, si->offset ); */
    }
}

jintArray
makeIntArray( JNIEnv *env, int count, const void* vals, size_t elemSize )
{
    jintArray array = (*env)->NewIntArray( env, count );
    XP_ASSERT( !!array );
    jint* elems = (*env)->GetIntArrayElements( env, array, NULL );
    XP_ASSERT( !!elems );
    jint elem;
    for ( int ii = 0; ii < count; ++ii ) {
        switch( elemSize ) {
        case sizeof(XP_U32):
            elem = *(XP_U32*)vals;
            break;
        case sizeof(XP_U16):
            elem = *(XP_U16*)vals;
            break;
        case sizeof(XP_U8):
            elem = *(XP_U8*)vals;
            break;
        default:
            XP_ASSERT(0);
            break;
        }
        vals += elemSize;
        elems[ii] = elem;
    }
    (*env)->ReleaseIntArrayElements( env, array, elems, 0 );
    return array;
}

jbyteArray
makeByteArray( JNIEnv *env, int siz, const jbyte* vals )
{
    jbyteArray array = (*env)->NewByteArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        jbyte* elems = (*env)->GetByteArrayElements( env, array, NULL );
        XP_ASSERT( !!elems );
        XP_MEMCPY( elems, vals, siz * sizeof(*elems) );
        (*env)->ReleaseByteArrayElements( env, array, elems, 0 );
    }
    return array;
}

jbyteArray
streamToBArray( JNIEnv *env, XWStreamCtxt* stream )
{
    int nBytes = stream_getSize( stream );
    jbyteArray result = (*env)->NewByteArray( env, nBytes );
    jbyte* jelems = (*env)->GetByteArrayElements( env, result, NULL );
    stream_getBytes( stream, jelems, nBytes );
    (*env)->ReleaseByteArrayElements( env, result, jelems, 0 );
    return result;
}

void
setBoolArray( JNIEnv* env, jbooleanArray jarr, int count, 
              const jboolean* vals )
{
    jboolean* elems = (*env)->GetBooleanArrayElements( env, jarr, NULL );
    XP_ASSERT( !!elems );
    XP_MEMCPY( elems, vals, count * sizeof(*elems) );
    (*env)->ReleaseBooleanArrayElements( env, jarr, elems, 0 );
} 

jbooleanArray
makeBooleanArray( JNIEnv *env, int siz, const jboolean* vals )
{
    jbooleanArray array = (*env)->NewBooleanArray( env, siz );
    XP_ASSERT( !!array );
    if ( !!vals ) {
        setBoolArray( env, array, siz, vals );
    }
    return array;
}

void
getIntsFromArray( JNIEnv* env, int dest[], jintArray arr, int count, bool del )
{
    jint* ints = (*env)->GetIntArrayElements(env, arr, 0);
    for ( int ii = 0; ii < count; ++ii ) {
        dest[ii] = ints[ii];
    }
    (*env)->ReleaseIntArrayElements( env, arr, ints, 0 );
    if ( del ) {
        deleteLocalRef( env, arr );
    }
}

void
setIntInArray( JNIEnv* env, jintArray arr, int index, int val )
{
    jint* ints = (*env)->GetIntArrayElements( env, arr, 0 );
#ifdef DEBUG
    jsize len = (*env)->GetArrayLength( env, arr );
    XP_ASSERT( len > index );
#endif
    ints[index] = val;
    (*env)->ReleaseIntArrayElements( env, arr, ints, 0 );
}

jobjectArray
makeStringArray( JNIEnv *env, int siz, const XP_UCHAR** vals )
{
    jclass clas = (*env)->FindClass(env, "java/lang/String");
    jstring empty = (*env)->NewStringUTF( env, "" );
    jobjectArray jarray = (*env)->NewObjectArray( env, siz, clas, empty );
    deleteLocalRefs( env, clas, empty, DELETE_NO_REF );

    for ( int ii = 0; !!vals && ii < siz; ++ii ) {    
        jstring jstr = (*env)->NewStringUTF( env, vals[ii] );
        (*env)->SetObjectArrayElement( env, jarray, ii, jstr );
        deleteLocalRef( env, jstr );
    }

    return jarray;
}

jstring
streamToJString( JNIEnv *env, XWStreamCtxt* stream )
{
    int len = stream_getSize( stream );
    XP_UCHAR buf[1 + len];
    stream_getBytes( stream, buf, len );
    buf[len] = '\0';

    jstring jstr = (*env)->NewStringUTF( env, buf );

    return jstr;
}

jmethodID
getMethodID( JNIEnv* env, jobject obj, const char* proc, const char* sig )
{
    XP_ASSERT( !!env );
    jclass cls = (*env)->GetObjectClass( env, obj );
    XP_ASSERT( !!cls );
#ifdef DEBUG
    char buf[128] = {0};
    /* int len = sizeof(buf); */
    /* getClassName( env, obj, buf, &len ); */
#endif
    jmethodID mid = (*env)->GetMethodID( env, cls, proc, sig );
    if ( !mid ) {
        XP_LOGF( "%s: no mid for proc %s, sig %s in object of class %s",
                 __func__, proc, sig, buf );
    }
    XP_ASSERT( !!mid );
    deleteLocalRef( env, cls );
    return mid;
}

void
setTypeSetFieldIn( JNIEnv* env, const CommsAddrRec* addr, jobject jTarget, 
                   const char* fieldName )
{
    jobject jtypset = addrTypesToJ( env, addr );
    XP_ASSERT( !!jtypset );
    jclass cls = (*env)->GetObjectClass( env, jTarget );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, fieldName, //"conTypes", 
                                       "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";" );
    XP_ASSERT( !!fid );
    (*env)->SetObjectField( env, jTarget, fid, jtypset );
    deleteLocalRef( env, jtypset );
}

/* Copy C object data into Java object */
void
setJAddrRec( JNIEnv* env, jobject jaddr, const CommsAddrRec* addr )
{
    XP_ASSERT( !!addr );
    setTypeSetFieldIn( env, addr, jaddr, "conTypes" );

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        switch ( typ ) {
        case COMMS_CONN_NONE:
            break;
        case COMMS_CONN_RELAY:
            setInt( env, jaddr, "ip_relay_port", addr->u.ip_relay.port );
            setString( env, jaddr, "ip_relay_hostName", addr->u.ip_relay.hostName );
            setString( env, jaddr, "ip_relay_invite", addr->u.ip_relay.invite );
            setBool( env, jaddr, "ip_relay_seeksPublicRoom",
                     addr->u.ip_relay.seeksPublicRoom );
            setBool( env, jaddr, "ip_relay_advertiseRoom",
                     addr->u.ip_relay.advertiseRoom );
            break;
        case COMMS_CONN_SMS:
            setString( env, jaddr, "sms_phone", addr->u.sms.phone );
            setInt( env, jaddr, "sms_port", addr->u.sms.port );
            break;
        case COMMS_CONN_BT:
            setString( env, jaddr, "bt_hostName", addr->u.bt.hostName );
            setString( env, jaddr, "bt_btAddr", addr->u.bt.btAddr.chars );
            break;
        case COMMS_CONN_P2P:
            setString( env, jaddr, "p2p_addr", addr->u.p2p.mac_addr );
            break;
        default:
            XP_ASSERT(0);
        }
    }
}

jobject
addrTypesToJ( JNIEnv* env, const CommsAddrRec* addr )
{
    XP_ASSERT( !!addr );
    jclass cls = 
        (*env)->FindClass( env, PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") );
    XP_ASSERT( !!cls );
    jmethodID initId = (*env)->GetMethodID( env, cls, "<init>", "()V" );
    XP_ASSERT( !!initId );
    jobject result = (*env)->NewObject( env, cls, initId );
    XP_ASSERT( !!result );

    jmethodID mid2 = getMethodID( env, result, "add", 
                                  "(Ljava/lang/Object;)Z" );
    XP_ASSERT( !!mid2 );
    CommsConnType typ;
    /* far as it gets */
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        jobject jtyp = intToJEnum( env, typ, 
                                   PKG_PATH("jni/CommsAddrRec$CommsConnType") );
        XP_ASSERT( !!jtyp );
        (*env)->CallBooleanMethod( env, result, mid2, jtyp );
        deleteLocalRef( env, jtyp );
    }
    deleteLocalRef( env, cls );
    return result;
}

/* Writes a java version of CommsAddrRec into a C one */
void
getJAddrRec( JNIEnv* env, CommsAddrRec* addr, jobject jaddr )
{
    /* Iterate over types in the set in jaddr, and for each call
       addr_addType() and then copy in the types. */
    // LOG_FUNC();
    jclass cls = (*env)->GetObjectClass( env, jaddr );
    XP_ASSERT( !!cls );
    jfieldID fid = (*env)->GetFieldID( env, cls, "conTypes", 
                                       "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";" );
    XP_ASSERT( !!fid );         /* failed */
    jobject jtypeset = (*env)->GetObjectField( env, jaddr, fid );
    XP_ASSERT( !!jtypeset );
    jmethodID mid = getMethodID( env, jtypeset, "getTypes", 
                                 "()[L" PKG_PATH("jni/CommsAddrRec$CommsConnType;") );
    XP_ASSERT( !!mid );
    jobject jtypesarr = (*env)->CallObjectMethod( env, jtypeset, mid );
    XP_ASSERT( !!jtypesarr );
    jsize len = (*env)->GetArrayLength( env, jtypesarr );
    for ( int ii = 0; ii < len; ++ii ) {
        jobject jtype = (*env)->GetObjectArrayElement( env, jtypesarr, ii );
        jint asInt = jEnumToInt( env, jtype );
        deleteLocalRef( env, jtype );
        CommsConnType typ = (CommsConnType)asInt;

        addr_addType( addr, typ );

        switch ( typ ) {
        case COMMS_CONN_RELAY:
            addr->u.ip_relay.port = getInt( env, jaddr, "ip_relay_port" );
            getString( env, jaddr, "ip_relay_hostName", addr->u.ip_relay.hostName,
                       VSIZE(addr->u.ip_relay.hostName) );
            getString( env, jaddr, "ip_relay_invite", addr->u.ip_relay.invite,
                       VSIZE(addr->u.ip_relay.invite) );
            addr->u.ip_relay.seeksPublicRoom =
                getBool( env, jaddr, "ip_relay_seeksPublicRoom" );
            addr->u.ip_relay.advertiseRoom =
                getBool( env, jaddr, "ip_relay_advertiseRoom" );

            break;
        case COMMS_CONN_SMS:
            getString( env, jaddr, "sms_phone", addr->u.sms.phone,
                       VSIZE(addr->u.sms.phone) );
            // XP_LOGF( "%s: got SMS; phone=%s", __func__, addr->u.sms.phone );
            addr->u.sms.port = getInt( env, jaddr, "sms_port" );
            break;
        case COMMS_CONN_BT:
            getString( env, jaddr, "bt_hostName", addr->u.bt.hostName,
                       VSIZE(addr->u.bt.hostName) );
            getString( env, jaddr, "bt_btAddr", addr->u.bt.btAddr.chars,
                       VSIZE(addr->u.bt.btAddr.chars) );
            break;
        case COMMS_CONN_P2P:
            getString( env, jaddr, "p2p_addr", addr->u.p2p.mac_addr,
                       VSIZE(addr->u.p2p.mac_addr) );
            break;
        default:
            XP_ASSERT(0);
        }
    }
    deleteLocalRefs( env, cls, jtypeset, jtypesarr, DELETE_NO_REF );
}

jint
jenumFieldToInt( JNIEnv* env, jobject j_gi, const char* field, 
                 const char* fieldSig )
{
    jclass clazz = (*env)->GetObjectClass( env, j_gi );
    XP_ASSERT( !!clazz );
    char sig[128];
    snprintf( sig, sizeof(sig), "L%s;", fieldSig );
    jfieldID fid = (*env)->GetFieldID( env, clazz, field, sig );
    XP_ASSERT( !!fid );
    jobject jenum = (*env)->GetObjectField( env, j_gi, fid );
    XP_ASSERT( !!jenum );
    jint result = jEnumToInt( env, jenum );

    deleteLocalRefs( env, clazz, jenum, DELETE_NO_REF );
    return result;
}

void
intToJenumField( JNIEnv* env, jobject jobj, int val, const char* field, 
                 const char* fieldSig )
{
    jclass clazz = (*env)->GetObjectClass( env, jobj );
    XP_ASSERT( !!clazz );
    char buf[128];
    snprintf( buf, sizeof(buf), "L%s;", fieldSig );
    jfieldID fid = (*env)->GetFieldID( env, clazz, field, buf );
    XP_ASSERT( !!fid );         /* failed */
    deleteLocalRef( env, clazz );

    jobject jenum = (*env)->GetObjectField( env, jobj, fid );
    if ( !jenum ) {       /* won't exist in new object */
        jclass clazz = (*env)->FindClass( env, fieldSig );
        XP_ASSERT( !!clazz );
        jmethodID mid = getMethodID( env, clazz, "<init>", "()V" );
        XP_ASSERT( !!mid );
        jenum = (*env)->NewObject( env, clazz, mid );
        XP_ASSERT( !!jenum );
        (*env)->SetObjectField( env, jobj, fid, jenum );
        deleteLocalRef( env, clazz );
    }

    jobject jval = intToJEnum( env, val, fieldSig );
    XP_ASSERT( !!jval );
    (*env)->SetObjectField( env, jobj, fid, jval );
    deleteLocalRef( env, jval );
} /* intToJenumField */

/* Cons up a new enum instance and set its value */
jobject
intToJEnum( JNIEnv* env, int val, const char* enumSig )
{
    jobject jenum = NULL;
    jclass clazz = (*env)->FindClass( env, enumSig );
    XP_ASSERT( !!clazz );

    char buf[128];
    snprintf( buf, sizeof(buf), "()[L%s;", enumSig );
    jmethodID mid = (*env)->GetStaticMethodID( env, clazz, "values", buf );
    XP_ASSERT( !!mid );

    jobject jvalues = (*env)->CallStaticObjectMethod( env, clazz, mid );
    XP_ASSERT( !!jvalues );
    XP_ASSERT( val < (*env)->GetArrayLength( env, jvalues ) );
    /* get the value we want */
    jenum = (*env)->GetObjectArrayElement( env, jvalues, val );
    XP_ASSERT( !!jenum );

    deleteLocalRefs( env, jvalues, clazz, DELETE_NO_REF );
    return jenum;
} /* intToJEnum */

jint
jEnumToInt( JNIEnv* env, jobject jenum )
{
    jmethodID mid = getMethodID( env, jenum, "ordinal", "()I" );
    XP_ASSERT( !!mid );
    return (*env)->CallIntMethod( env, jenum, mid );
}

XWStreamCtxt*
and_empty_stream( MPFORMAL AndGlobals* globals )
{
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            globals, 0, NULL );
    return stream;
}

XP_U32
getCurSeconds( JNIEnv* env )
{
    jclass clazz = (*env)->FindClass( env, PKG_PATH("Utils") );
    XP_ASSERT( !!clazz );
    jmethodID mid = (*env)->GetStaticMethodID( env, clazz,
                                               "getCurSeconds", "()J" );
    jlong result = (*env)->CallStaticLongMethod( env, clazz, mid );

    deleteLocalRef( env, clazz );
    return result;
}

void deleteLocalRef( JNIEnv* env, jobject jobj )
{
    if ( NULL != jobj ) {
        (*env)->DeleteLocalRef( env, jobj );
    }
}

void
deleteLocalRefs( JNIEnv* env, ... )
{
    va_list ap;
    va_start( ap, env );
    for ( ; ; ) {
        jobject jnext = va_arg( ap, jobject );
        if ( DELETE_NO_REF == jnext ) {
            break;
        }
        deleteLocalRef( env, jnext );
    }
    va_end( ap );
}

#ifdef DEBUG
void 
android_debugf( const char* format, ... )
{
    char buf[1024];
    va_list ap;
    int len;
    struct tm* timp;
    struct timeval tv;
    struct timezone tz;

    gettimeofday( &tv, &tz );
    timp = localtime( &tv.tv_sec );

    len = snprintf( buf, sizeof(buf), "%.2d:%.2d:%.2d: ", 
                    timp->tm_hour, timp->tm_min, timp->tm_sec );
    if ( len < sizeof(buf) ) {
        va_start(ap, format);
        vsnprintf( buf + len, sizeof(buf)-len, format, ap );
        va_end(ap);
    }
    
    (void)__android_log_write( ANDROID_LOG_DEBUG, 
# if defined VARIANT_xw4
                               "xw4"
# elif defined VARIANT_xw4dbg
                               "x4bg"
# endif
                               , buf );
}

/* Print an object's class name into buffer.
 *
 * NOTE: this must be called in advance of any jni error, because methods on
 * env can't be called once there's an exception pending.
 */
#if 0
static void
getClassName( JNIEnv* env, jobject obj, char* out, int* outLen )
{
    XP_ASSERT( !!obj );
    jclass cls1 = (*env)->GetObjectClass( env, obj );

    // First get the class object
    jmethodID mid = (*env)->GetMethodID( env, cls1, "getClass",
                                         "()Ljava/lang/Class;" );
    jobject clsObj = (*env)->CallObjectMethod( env, obj, mid );

    // Now get the class object's class descriptor
    jclass cls2 = (*env)->GetObjectClass( env, clsObj );
    // Find the getName() method on the class object
    mid = (*env)->GetMethodID( env, cls2, "getName", "()Ljava/lang/String;" );

    // Call the getName() to get a jstring object back
    jstring strObj = (jstring)(*env)->CallObjectMethod( env, clsObj, mid );

    jint slen = (*env)->GetStringUTFLength( env, strObj );
    if ( slen < *outLen ) {
        *outLen = slen;
        (*env)->GetStringUTFRegion( env, strObj, 0, slen, out );
        out[slen] = '\0';
    } else {
        *outLen = 0;
        out[0] = '\0';
    }
    deleteLocalRefs( env, clsObj, cls1, cls2, strObj, DELETE_NO_REF );
    LOG_RETURNF( "%s", out );
}
#endif
#endif

/* #ifdef DEBUG */
/* XP_U32 */
/* andy_rand( const char* caller ) */
/* { */
/*     XP_U32 result = rand(); */
/*     XP_LOGF( "%s: returning 0x%lx to %s", __func__, result, caller ); */
/*     LOG_RETURNF( "%lx", result ); */
/*     return result; */
/* } */
/* #endif */

#ifndef MEM_DEBUG
void
and_freep( void** ptrp )
{
    if ( !!*ptrp ) {
        free( *ptrp );
        *ptrp = NULL;
    }
}
#endif
