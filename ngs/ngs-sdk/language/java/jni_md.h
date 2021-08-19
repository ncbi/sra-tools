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

#ifdef _WIN32
    #define JNIEXPORT __declspec(dllexport)
    #define JNIIMPORT __declspec(dllimport)
    #define JNICALL __stdcall
    
    #define __func__ __FUNCTION__
	#if _MSC_VER < 1900
		inline int vsnprintf(char* str, size_t size, const char* format, va_list ap)
		{
			int count = -1;

			if (size != 0)
				count = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
			if (count == -1)
				count = _vscprintf(format, ap);

			return count;
		}
		inline int snprintf(char* str, size_t size, const char* format, ...)
		{
			int count;
			va_list ap;

			va_start(ap, format);
			count = vsnprintf(str, size, format, ap);
			va_end(ap);

			return count;
		}
	#endif
#else
    #define JNIEXPORT 
    #define JNIIMPORT 
    #define JNICALL   
#endif

typedef int8_t jbyte;
typedef int32_t jint;
typedef int64_t jlong;

#endif /* !_JAVASOFT_JNI_MD_H_ */
