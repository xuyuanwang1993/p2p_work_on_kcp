/*
  Copyright (c) 2009-2017 Dave Gamble and CJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef CJSON__h
#define CJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__

/* When compiling for windows, we specify a specific calling convention to avoid issues where we are being called from a project with a different default calling convention.  For windows you have 3 define options:

CJSON_HIDE_SYMBOLS - Define this in the case where you don't want to ever dllexport symbols
CJSON_EXPORT_SYMBOLS - Define this on library build when you want to dllexport symbols (default)
CJSON_IMPORT_SYMBOLS - Define this if you want to dllimport symbol

For *nix builds that support visibility attribute, you can define similar behavior by

setting default visibility to hidden by adding
-fvisibility=hidden (for gcc)
or
-xldscope=hidden (for sun cc)
to CFLAGS

then using the CJSON_API_VISIBILITY flag to "export" the same symbols the way CJSON_EXPORT_SYMBOLS does

*/

#define CJSON_CDECL __cdecl
#define CJSON_STDCALL __stdcall

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(CJSON_HIDE_SYMBOLS) && !defined(CJSON_IMPORT_SYMBOLS) && !defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_EXPORT_SYMBOLS
#endif

#if defined(CJSON_HIDE_SYMBOLS)
#define CJSON_PUBLIC(type)   type CJSON_STDCALL
#elif defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllexport) type CJSON_STDCALL
#elif defined(CJSON_IMPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllimport) type CJSON_STDCALL
#endif
#else /* !__WINDOWS__ */
#define CJSON_CDECL
#define CJSON_STDCALL

#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(CJSON_API_VISIBILITY)
#define CJSON_PUBLIC(type)   __attribute__((visibility("default"))) type
#else
#define CJSON_PUBLIC(type) type
#endif
#endif

/* project version */
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 10

#include <stddef.h>

/* CJSON Types: */
#define CJSON_Invalid (0)
#define CJSON_False  (1 << 0)
#define CJSON_True   (1 << 1)
#define CJSON_NULL   (1 << 2)
#define CJSON_Number (1 << 3)
#define CJSON_String (1 << 4)
#define CJSON_Array  (1 << 5)
#define CJSON_Object (1 << 6)
#define CJSON_Raw    (1 << 7) /* raw json */

#define CJSON_IsReference 256
#define CJSON_StringIsConst 512

/* The CJSON structure: */
typedef struct CJSON
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct CJSON *next;
    struct CJSON *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct CJSON *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==CJSON_String  and type == CJSON_Raw */
    char *valuestring;
    /* writing to valueint is DEPRECATED, use CJSON_SetNumberValue instead */
    int valueint;
    /* The item's number, if type==CJSON_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} CJSON;

typedef struct CJSON_Hooks
{
      /* malloc/free are CDECL on Windows regardless of the default calling convention of the compiler, so ensure the hooks allow passing those functions directly. */
      void *(CJSON_CDECL *malloc_fn)(size_t sz);
      void (CJSON_CDECL *free_fn)(void *ptr);
} CJSON_Hooks;

typedef int CJSON_bool;

/* Limits how deeply nested arrays/objects can be before CJSON rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* returns the version of CJSON as a string */
CJSON_PUBLIC(const char*) CJSON_Version(void);

/* Supply malloc, realloc and free functions to CJSON */
CJSON_PUBLIC(void) CJSON_InitHooks(CJSON_Hooks* hooks);

/* Memory Management: the caller is always responsible to free the results from all variants of CJSON_Parse (with CJSON_Delete) and CJSON_Print (with stdlib free, CJSON_Hooks.free_fn, or CJSON_free as appropriate). The exception is CJSON_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a CJSON object you can interrogate. */
CJSON_PUBLIC(CJSON *) CJSON_Parse(const char *value);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match CJSON_GetErrorPtr(). */
CJSON_PUBLIC(CJSON *) CJSON_ParseWithOpts(const char *value, const char **return_parse_end, CJSON_bool require_null_terminated);

/* Render a CJSON entity to text for transfer/storage. */
CJSON_PUBLIC(char *) CJSON_Print(const CJSON *item);
/* Render a CJSON entity to text for transfer/storage without any formatting. */
CJSON_PUBLIC(char *) CJSON_PrintUnformatted(const CJSON *item);
/* Render a CJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
CJSON_PUBLIC(char *) CJSON_PrintBuffered(const CJSON *item, int prebuffer, CJSON_bool fmt);
/* Render a CJSON entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: CJSON is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
CJSON_PUBLIC(CJSON_bool) CJSON_PrintPreallocated(CJSON *item, char *buffer, const int length, const CJSON_bool format);
/* Delete a CJSON entity and all subentities. */
CJSON_PUBLIC(void) CJSON_Delete(CJSON *c);

/* Returns the number of items in an array (or object). */
CJSON_PUBLIC(int) CJSON_GetArraySize(const CJSON *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
CJSON_PUBLIC(CJSON *) CJSON_GetArrayItem(const CJSON *array, int index);
/* Get item "string" from object. Case insensitive. */
CJSON_PUBLIC(CJSON *) CJSON_GetObjectItem(const CJSON * const object, const char * const string);
CJSON_PUBLIC(CJSON *) CJSON_GetObjectItemCaseSensitive(const CJSON * const object, const char * const string);
CJSON_PUBLIC(CJSON_bool) CJSON_HasObjectItem(const CJSON *object, const char *string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when CJSON_Parse() returns 0. 0 when CJSON_Parse() succeeds. */
CJSON_PUBLIC(const char *) CJSON_GetErrorPtr(void);

/* Check if the item is a string and return its valuestring */
CJSON_PUBLIC(char *) CJSON_GetStringValue(CJSON *item);

/* These functions check the type of an item */
CJSON_PUBLIC(CJSON_bool) CJSON_IsInvalid(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsFalse(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsTrue(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsBool(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsNull(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsNumber(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsString(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsArray(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsObject(const CJSON * const item);
CJSON_PUBLIC(CJSON_bool) CJSON_IsRaw(const CJSON * const item);

/* These calls create a CJSON item of the appropriate type. */
CJSON_PUBLIC(CJSON *) CJSON_CreateNull(void);
CJSON_PUBLIC(CJSON *) CJSON_CreateTrue(void);
CJSON_PUBLIC(CJSON *) CJSON_CreateFalse(void);
CJSON_PUBLIC(CJSON *) CJSON_CreateBool(CJSON_bool boolean);
CJSON_PUBLIC(CJSON *) CJSON_CreateNumber(double num);
CJSON_PUBLIC(CJSON *) CJSON_CreateString(const char *string);
/* raw json */
CJSON_PUBLIC(CJSON *) CJSON_CreateRaw(const char *raw);
CJSON_PUBLIC(CJSON *) CJSON_CreateArray(void);
CJSON_PUBLIC(CJSON *) CJSON_CreateObject(void);

/* Create a string where valuestring references a string so
 * it will not be freed by CJSON_Delete */
CJSON_PUBLIC(CJSON *) CJSON_CreateStringReference(const char *string);
/* Create an object/arrray that only references it's elements so
 * they will not be freed by CJSON_Delete */
CJSON_PUBLIC(CJSON *) CJSON_CreateObjectReference(const CJSON *child);
CJSON_PUBLIC(CJSON *) CJSON_CreateArrayReference(const CJSON *child);

/* These utilities create an Array of count items. */
CJSON_PUBLIC(CJSON *) CJSON_CreateUcharArray(const unsigned char *numbers, int count);
CJSON_PUBLIC(CJSON *) CJSON_CreateIntArray(const int *numbers, int count);
CJSON_PUBLIC(CJSON *) CJSON_CreateFloatArray(const float *numbers, int count);
CJSON_PUBLIC(CJSON *) CJSON_CreateDoubleArray(const double *numbers, int count);
CJSON_PUBLIC(CJSON *) CJSON_CreateStringArray(const char **strings, int count);

/* Append item to the specified array/object. */
CJSON_PUBLIC(void) CJSON_AddItemToArray(CJSON *array, CJSON *item);
CJSON_PUBLIC(void) CJSON_AddItemToObject(CJSON *object, const char *string, CJSON *item);
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the CJSON object.
 * WARNING: When this function was used, make sure to always check that (item->type & CJSON_StringIsConst) is zero before
 * writing to `item->string` */
CJSON_PUBLIC(void) CJSON_AddItemToObjectCS(CJSON *object, const char *string, CJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing CJSON to a new CJSON, but don't want to corrupt your existing CJSON. */
CJSON_PUBLIC(void) CJSON_AddItemReferenceToArray(CJSON *array, CJSON *item);
CJSON_PUBLIC(void) CJSON_AddItemReferenceToObject(CJSON *object, const char *string, CJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
CJSON_PUBLIC(CJSON *) CJSON_DetachItemViaPointer(CJSON *parent, CJSON * const item);
CJSON_PUBLIC(CJSON *) CJSON_DetachItemFromArray(CJSON *array, int which);
CJSON_PUBLIC(void) CJSON_DeleteItemFromArray(CJSON *array, int which);
CJSON_PUBLIC(CJSON *) CJSON_DetachItemFromObject(CJSON *object, const char *string);
CJSON_PUBLIC(CJSON *) CJSON_DetachItemFromObjectCaseSensitive(CJSON *object, const char *string);
CJSON_PUBLIC(void) CJSON_DeleteItemFromObject(CJSON *object, const char *string);
CJSON_PUBLIC(void) CJSON_DeleteItemFromObjectCaseSensitive(CJSON *object, const char *string);

/* Update array items. */
CJSON_PUBLIC(void) CJSON_InsertItemInArray(CJSON *array, int which, CJSON *newitem); /* Shifts pre-existing items to the right. */
CJSON_PUBLIC(CJSON_bool) CJSON_ReplaceItemViaPointer(CJSON * const parent, CJSON * const item, CJSON * replacement);
CJSON_PUBLIC(void) CJSON_ReplaceItemInArray(CJSON *array, int which, CJSON *newitem);
CJSON_PUBLIC(void) CJSON_ReplaceItemInObject(CJSON *object,const char *string,CJSON *newitem);
CJSON_PUBLIC(void) CJSON_ReplaceItemInObjectCaseSensitive(CJSON *object,const char *string,CJSON *newitem);

/* Duplicate a CJSON item */
CJSON_PUBLIC(CJSON *) CJSON_Duplicate(const CJSON *item, CJSON_bool recurse);
/* Duplicate will create a new, identical CJSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */
/* Recursively compare two CJSON items for equality. If either a or b is NULL or invalid, they will be considered unequal.
 * case_sensitive determines if object keys are treated case sensitive (1) or case insensitive (0) */
CJSON_PUBLIC(CJSON_bool) CJSON_Compare(const CJSON * const a, const CJSON * const b, const CJSON_bool case_sensitive);


CJSON_PUBLIC(void) CJSON_Minify(char *json);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
CJSON_PUBLIC(CJSON*) CJSON_AddNullToObject(CJSON * const object, const char * const name);
CJSON_PUBLIC(CJSON*) CJSON_AddTrueToObject(CJSON * const object, const char * const name);
CJSON_PUBLIC(CJSON*) CJSON_AddFalseToObject(CJSON * const object, const char * const name);
CJSON_PUBLIC(CJSON*) CJSON_AddBoolToObject(CJSON * const object, const char * const name, const CJSON_bool boolean);
CJSON_PUBLIC(CJSON*) CJSON_AddNumberToObject(CJSON * const object, const char * const name, const double number);
CJSON_PUBLIC(CJSON*) CJSON_AddStringToObject(CJSON * const object, const char * const name, const char * const string);
CJSON_PUBLIC(CJSON*) CJSON_AddRawToObject(CJSON * const object, const char * const name, const char * const raw);
CJSON_PUBLIC(CJSON*) CJSON_AddObjectToObject(CJSON * const object, const char * const name);
CJSON_PUBLIC(CJSON*) CJSON_AddArrayToObject(CJSON * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define CJSON_SetIntValue(object, number) ((object) ? (object)->valueint = (object)->valuedouble = (number) : (number))
/* helper for the CJSON_SetNumberValue macro */
CJSON_PUBLIC(double) CJSON_SetNumberHelper(CJSON *object, double number);
#define CJSON_SetNumberValue(object, number) ((object != NULL) ? CJSON_SetNumberHelper(object, (double)number) : (number))

/* Macro for iterating over an array or object */
#define CJSON_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* malloc/free objects using the malloc/free functions that have been set with CJSON_InitHooks */
CJSON_PUBLIC(void *) CJSON_malloc(size_t size);
CJSON_PUBLIC(void) CJSON_free(void *object);

#ifdef __cplusplus
}
#endif

#endif
