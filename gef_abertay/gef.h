#ifndef _ABFW_FRAMEWORK_H
#define _ABFW_FRAMEWORK_H

// DEFINES

// TYPES
typedef long long Int64;
typedef unsigned long long UInt64;
typedef int Int32;
typedef unsigned int UInt32;
typedef short Int16;
typedef unsigned short UInt16;
typedef char Int8;
typedef unsigned char UInt8;

// MACROS
#define ReleaseNull(x) if(x != NULL) { x->Release(); x = NULL; }

struct IAllocator;
extern IAllocator *g_alloc;

#endif // _ABFW_FRAMEWORK_H