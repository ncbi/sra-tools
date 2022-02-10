/*
 * %W% %E%
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

#ifndef _JAVASOFT_JNI_MD_H_
#define _JAVASOFT_JNI_MD_H_

/* TEMPORARY - THIS MAY NEED TO BE ON A PER-SYSTEM BASIS */
#include <stdint.h>

#ifdef _WIN32_WINNT
    #define JNIEXPORT __declspec(dllexport)
    #define JNIIMPORT __declspec(dllimport)
    #define JNICALL __stdcall
#else
    #define JNIEXPORT 
    #define JNIIMPORT 
    #define JNICALL   
#endif

typedef int8_t jbyte;
typedef int32_t jint;
typedef int64_t jlong;

#endif /* !_JAVASOFT_JNI_MD_H_ */
