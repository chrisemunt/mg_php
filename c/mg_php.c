/*
   ----------------------------------------------------------------------------
   | mg_php: PHP Extension for M/Cache/IRIS                                   |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2016-2019 M/Gateway Developments Ltd,                      |
   | Surrey UK.                                                               |
   | All rights reserved.                                                     |
   |                                                                          |
   | http://www.mgateway.com                                                  |
   |                                                                          |
   | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
   | not use this file except in compliance with the License.                 |
   | You may obtain a copy of the License at                                  |
   |                                                                          |
   | http://www.apache.org/licenses/LICENSE-2.0                               |
   |                                                                          |
   | Unless required by applicable law or agreed to in writing, software      |
   | distributed under the License is distributed on an "AS IS" BASIS,        |
   | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
   | See the License for the specific language governing permissions and      |
   | limitations under the License.                                           |
   ----------------------------------------------------------------------------
*/

/*
3.0.1 13 June 2019
   Open Source version
*/

/*
#define ZTS 1
*/

/* Normalize Windows Build environment in /Zend/zend_modules.h */
#if defined(_WIN32)
#include "php_version.h"
#if PHP_MAJOR_VERSION == 7 && PHP_MINOR_VERSION == 1
#define MG_ZEND_BUILD_SYSTEM "VC14"
#endif
#endif

#ifdef ZTS
#define MG_ZEND_BUILD_TS           1
#endif

#if defined(_WIN32)              /* Windows */
#if defined(_MSC_VER)
#if (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE    1
#define _CRT_NONSTDC_NO_DEPRECATE   1
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#endif
#endif
#endif

/* include standard header */

#include "php.h"
#include "ext/standard/info.h"
#include "SAPI.h"
#include "php_ini.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#if !defined(HPUX)
#include <sys/select.h>
#endif
#include <sys/time.h>

#include <fcntl.h>
#if !defined(FREEBSD) && !defined(MACOSX)
#include <termio.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#endif

#if defined(PHP_MAJOR_VERSION) && defined(PHP_MINOR_VERSION) && defined(PHP_RELEASE_VERSION)
#define MG_PHP_VERSION ((PHP_MAJOR_VERSION * 10000) + (PHP_MINOR_VERSION * 100) + PHP_RELEASE_VERSION)
#else
#define MG_PHP_VERSION 40000
#endif

#define MGSI                 1

#ifdef _WIN32
#define MG_LOG_FILE          "c:/mgsi/bin/mg_php.log"
#else
#define MG_LOG_FILE          "/usr/mgsi/bin/mg_php.log"
#endif

#define MG_VERSION           "3.0.1"

#define MG_IPA               "127.0.0.1"

#ifdef MGSI
#define MG_PORT              7041
#else
#define MG_PORT              80
#endif

#define MG_DLM               "\x02"
#define MG_EOD               "\x07"
#define MG_AK                "\x04"
#define MG_AR                "\x05"
#define MG_ERR               "\x06"


/*
#define MG_BUF               16384
*/
#define MG_BUF               32768
/*
#define MG_BUF               65000
#define MG_BUF               128000
#define MG_BUF               262144
*/

#define MG_MAXCON            32
#define MG_MAXARG            32
#define MG_MAXKEY            256

#define MG_T_VAR             0
#define MG_T_STRING          1
#define MG_T_INTEGER         2
#define MG_T_FLOAT           3
#define MG_T_LIST            4

#define MG_TX_DATA           0
#define MG_TX_AKEY           1
#define MG_TX_AREC           2
#define MG_TX_EOD            3
#define MG_TX_AREC_FORMATTED 9

#define MG_ES_DELIM          0
#define MG_ES_BLOCK          1

#define MG_RECV_HEAD         8

#define MG_CHUNK_SIZE_BASE   62

#ifdef ZTS
#define MG_EMALLOC           1
#endif

/*

E_ERROR
E_WARNING
E_USER_ERROR
E_USER_WARNING
*/

#if MG_PHP_VERSION >= 70000
#define MG_AREF_P(a)      &a
#define MG_AREF_PP(a)     &a
#define MG_STR(a)         estrndup(Z_STRVAL_P(&a), Z_STRLEN_P(&a))
#define MG_Z_STRVAL(a)    Z_STRVAL_P(a)
#define MG_Z_STRLEN(a)    Z_STRLEN_P(a)
#define MG_Z_ISREF(a)     Z_ISREF(a)
#else
#define MG_AREF_P(a)      *a
#define MG_AREF_PP(a)     a
#define MG_STR(a)         Z_STRVAL_PP(a)
#define MG_Z_STRVAL(a)    Z_STRVAL_PP(a)
#define MG_Z_STRLEN(a)    Z_STRLEN_PP(a)
#define MG_Z_ISREF(a)     PZVAL_IS_REF(*a)
#endif

#if 0
#define MG_WRONG_PARAM_COUNT                WRONG_PARAM_COUNT
#define MG_WRONG_PARAM_COUNT_AND_FREE_BUF   WRONG_PARAM_COUNT
#define MG_RETURN_TRUE                      RETURN_TRUE
#define MG_RETURN_TRUE_AND_FREE_BUF         RETURN_TRUE
#define MG_RETURN_FALSE                     RETURN_FALSE
#define MG_RETURN_FALSE_AND_FREE_BUF        RETURN_FALSE
#define MG_RETURN_STRING                    RETURN_STRING
#define MG_RETURN_STRING_AND_FREE_BUF       RETURN_STRING

#else

#define MG_WRONG_PARAM_COUNT WRONG_PARAM_COUNT

#define MG_WRONG_PARAM_COUNT_AND_FREE_BUF \
   { \
      mg_buf_free(p_buf); \
      WRONG_PARAM_COUNT; \
   } \

#define MG_RETURN_TRUE \
   { \
      RETVAL_TRUE; \
      return; \
   } \

#define MG_RETURN_TRUE_AND_FREE_BUF \
   { \
      RETVAL_TRUE; \
      mg_buf_free(p_buf); \
      return; \
   } \

#define MG_RETURN_FALSE \
   { \
      RETVAL_FALSE; \
      return; \
   } \

#define MG_RETURN_FALSE_AND_FREE_BUF \
   { \
      RETVAL_FALSE; \
      mg_buf_free(p_buf); \
      return; \
   } \


#if MG_PHP_VERSION >= 70000

#define MG_RETURN_STRING(s, duplicate) \
   { \
      RETVAL_STRING(s); \
      return; \
   } \

#define MG_RETURN_STRING_AND_FREE_BUF(s, duplicate) \
   { \
      RETVAL_STRING(s); \
      mg_buf_free(p_buf); \
      return; \
   } \

#else

#define MG_RETURN_STRING(s, duplicate) \
   { \
      RETVAL_STRING(s, duplicate); \
      return; \
   } \

#define MG_RETURN_STRING_AND_FREE_BUF(s, duplicate) \
   { \
      RETVAL_STRING(s, duplicate); \
      mg_buf_free(p_buf); \
      return; \
   } \

#endif

#endif

#define MG_ERROR0(e) \
   if (p_page && p_page->error_mode == 1) \
      php_error(E_USER_WARNING, "%s", e); \
   else if (p_page && p_page->error_mode == 9) { \
      strcpy(p_page->error_mess, e); \
      MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1); \
   } \
   else \
      php_error(E_USER_ERROR, "%s", e); \
   MG_RETURN_FALSE_AND_FREE_BUF; \


#define MG_ERROR1(e) \
   if (p_page && p_page->error_mode == 1) \
      php_error(E_USER_WARNING, "%s", e); \
   else if (p_page && p_page->error_mode == 9) { \
      strcpy(p_page->error_mess, e); \
      MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1); \
   } \
   else \
      php_error(E_USER_ERROR, "%s", e); \
   MG_RETURN_FALSE_AND_FREE_BUF; \


#define MG_ERROR2(e) \
   if (p_page && p_page->error_mode == 1) \
      php_error(E_USER_WARNING, "%s", e); \
   else if (p_page && p_page->error_mode == 9) { \
      strcpy(p_page->error_mess, e); \
      return 2; \
   } \
   else \
      php_error(E_USER_ERROR, "%s", e); \
   return 1; \


#define MG_MEMCHECK(e, c) \
   if (p_page && p_page->mem_error == 1) { \
      mg_close_connection(p_page, chndle, c); \
      if (p_page && p_page->error_mode == 1) \
         php_error(E_USER_WARNING, e); \
      else if (p_page && p_page->error_mode == 9) { \
         strcpy(p_page->error_mess, e); \
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1); \
      } \
      else \
         php_error(E_USER_ERROR, e); \
      MG_RETURN_FALSE_AND_FREE_BUF; \
   } \

#if 0

#define MG_FTRACE(f) \
   { \
      char buffer[256]; \
      MGG(fun_no) ++; \
      sprintf(buffer, "<br> %d : %d : %s",  MGG(req_no), MGG(fun_no), f); \
      strcat(MGG(trace), buffer); \
   } \


#else


#define MG_FTRACE(f) MGG(fun_no) ++;

#endif


typedef struct tagMGPTYPE {
   short type;
   short byref;
} MGPTYPE;


typedef struct tagMGAREC {
   int kn;
   unsigned char kz[MG_MAXKEY];
   unsigned char * krec[MG_MAXKEY];
#if MG_PHP_VERSION >= 70000
   zend_string * krec_ex[MG_MAXKEY];
#else
   unsigned char * krec_ex[MG_MAXKEY];
#endif
   int ksize[MG_MAXKEY];
   unsigned char vz;
   unsigned char * vrec;
   int vsize;
} MGAREC;


typedef struct tagCONX {
#ifdef _WIN32
   WSADATA     wsadata;
   SOCKET      sockfd;
#else
   int         sockfd;
#endif
   char        ip_address[32];
   int         port;
   char        error[256];
   short       eod;
   short       keep_alive;
   short       in_use;
} CONX, *LPCONX;

static int le_mg_user;


typedef struct tagMGBUF {
   unsigned long size;
   unsigned long curr_size;
   unsigned char *p_buffer;
   unsigned char buffer[MG_BUF];
} MGBUF;


typedef struct tagMGSTR {
   unsigned int size;
   unsigned char *ps;
} MGSTR;


typedef struct tagMGUSER {
   char buffer[MG_BUF];
} MGUSER;

typedef struct tagMGPAGE {
   short mem_error;
   short storage_mode;
   short no_retry;
   int error_mode;
   int header_len;
   int timeout;
   char error_code[128];
   char error_mess[256];
   char eod[4];
   char server[32];
   char uci[32];
   LPCONX pcon[MG_MAXCON];
} MGPAGE;

ZEND_BEGIN_MODULE_GLOBALS(mg)
   long     req_no;
   long     fun_no;
   char     trace[32000];
   MGPAGE  *p_page;
ZEND_END_MODULE_GLOBALS(mg)

#ifdef ZTS
# define MGG(v) TSRMG(mg_globals_id, zend_mg_globals *, v)
#else
# define MGG(v) (mg_globals.v)
#endif

extern zend_module_entry mg_module_entry;
#define phpext_mg_ptr &_module_entry

PHP_MINIT_FUNCTION(mg);
PHP_MSHUTDOWN_FUNCTION(mg);
PHP_RINIT_FUNCTION(mg);
PHP_RSHUTDOWN_FUNCTION(mg);
PHP_MINFO_FUNCTION(mg);

#if MG_PHP_VERSION >= 50500

static PHP_FUNCTION(m_test);
static PHP_FUNCTION(m_dump_trace);
static PHP_FUNCTION(m_ext_version);
static PHP_FUNCTION(m_set_error_mode);
static PHP_FUNCTION(m_set_storage_mode);
static PHP_FUNCTION(m_set_timeout);
static PHP_FUNCTION(m_set_no_retry);
static PHP_FUNCTION(m_set_server);
static PHP_FUNCTION(m_set_uci);
static PHP_FUNCTION(m_get_last_error);
static PHP_FUNCTION(m_set);
static PHP_FUNCTION(m_get);
static PHP_FUNCTION(m_kill);
static PHP_FUNCTION(m_data);
static PHP_FUNCTION(m_order);
static PHP_FUNCTION(m_previous);
static PHP_FUNCTION(m_html);
static PHP_FUNCTION(m_html_method);
static PHP_FUNCTION(m_http);
static PHP_FUNCTION(m_proc);
static PHP_FUNCTION(m_proc_byref);
static PHP_FUNCTION(m_method);
static PHP_FUNCTION(m_method_byref);
static PHP_FUNCTION(m_merge_to_db);
static PHP_FUNCTION(m_merge_from_db);
static PHP_FUNCTION(m_return_to_applet);
static PHP_FUNCTION(m_return_to_client);
static PHP_FUNCTION(m_array_test);

/* cmtxxx */
#if MG_PHP_VERSION >= 70000
ZEND_BEGIN_ARG_INFO(m_proc_byref_ainfo, 1)
   ZEND_ARG_PASS_INFO(0) /* first argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(0) /* second argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_method_byref_ainfo, 1)
   ZEND_ARG_PASS_INFO(0) /* first argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(0) /* second argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_array_test_ainfo, 1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO()

#if 0
ZEND_BEGIN_ARG_INFO_EX(m_merge_from_db_byref_ainfo, 0, 0, 1)
    ZEND_ARG_INFO(0, parameter_name) /* first argument can be a literal if zero */
    ZEND_ARG_INFO(0, parameter_name) /* second argument can be a literal if zero */
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
    ZEND_ARG_INFO(1, parameter_name)
ZEND_END_ARG_INFO()
#else
ZEND_BEGIN_ARG_INFO(m_merge_from_db_byref_ainfo, 1)
   ZEND_ARG_PASS_INFO(0) /* first argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(0) /* second argument can be a literal if zero */
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_ARG_PASS_INFO(1)
   ZEND_END_ARG_INFO()
#endif

#endif

static const zend_function_entry mg_functions[] =
{
    PHP_FE(m_test, NULL)
    PHP_FE(m_dump_trace, NULL)
    PHP_FE(m_ext_version, NULL)
    PHP_FE(m_set_error_mode, NULL)
    PHP_FE(m_set_storage_mode, NULL)
    PHP_FE(m_set_timeout, NULL)
    PHP_FE(m_set_no_retry, NULL)
    PHP_FE(m_set_server, NULL)
    PHP_FE(m_set_uci, NULL)
    PHP_FE(m_get_last_error, NULL)
    PHP_FE(m_set, NULL)
    PHP_FE(m_get, NULL)
    PHP_FE(m_kill, NULL)
    PHP_FE(m_data, NULL)
    PHP_FE(m_order, NULL)
    PHP_FE(m_previous, NULL)
    PHP_FE(m_html, NULL)
    PHP_FE(m_html_method, NULL)
    PHP_FE(m_http, NULL)
    PHP_FE(m_proc, NULL)
#if MG_PHP_VERSION >= 70000
    PHP_FE(m_proc_byref, m_proc_byref_ainfo)
#else
    PHP_FE(m_proc_byref, NULL)
#endif
    PHP_FE(m_method, NULL)
#if MG_PHP_VERSION >= 70000
    PHP_FE(m_method_byref, m_method_byref_ainfo)
#else
    PHP_FE(m_method_byref, NULL)
#endif
    PHP_FE(m_merge_to_db, NULL)
#if MG_PHP_VERSION >= 70000
    PHP_FE(m_merge_from_db, m_merge_from_db_byref_ainfo)
#else
    PHP_FE(m_merge_from_db, NULL)
#endif
    PHP_FE(m_return_to_applet, NULL)
    PHP_FE(m_return_to_client, NULL)
    PHP_FE(m_array_test, m_array_test_ainfo)
    {NULL, NULL, NULL}
};

#else

PHP_FUNCTION(m_test);
PHP_FUNCTION(m_dump_trace);
PHP_FUNCTION(m_ext_version);
PHP_FUNCTION(m_set_error_mode);
PHP_FUNCTION(m_set_storage_mode);
PHP_FUNCTION(m_set_timeout);
PHP_FUNCTION(m_set_no_retry);
PHP_FUNCTION(m_set_server);
PHP_FUNCTION(m_set_uci);
PHP_FUNCTION(m_get_last_error);
PHP_FUNCTION(m_set);
PHP_FUNCTION(m_get);
PHP_FUNCTION(m_kill);
PHP_FUNCTION(m_data);
PHP_FUNCTION(m_order);
PHP_FUNCTION(m_previous);
PHP_FUNCTION(m_html);
PHP_FUNCTION(m_html_method);
PHP_FUNCTION(m_http);
PHP_FUNCTION(m_proc);
PHP_FUNCTION(m_proc_byref);
PHP_FUNCTION(m_method);
PHP_FUNCTION(m_method_byref);
PHP_FUNCTION(m_merge_to_db);
PHP_FUNCTION(m_merge_from_db);
PHP_FUNCTION(m_return_to_applet);
PHP_FUNCTION(m_return_to_client);
PHP_FUNCTION(m_array_test);

function_entry mg_functions[] =
{
    PHP_FE(m_test, NULL)
    PHP_FE(m_dump_trace, NULL)
    PHP_FE(m_ext_version, NULL)
    PHP_FE(m_set_error_mode, NULL)
    PHP_FE(m_set_storage_mode, NULL)
    PHP_FE(m_set_timeout, NULL)
    PHP_FE(m_set_no_retry, NULL)
    PHP_FE(m_set_server, NULL)
    PHP_FE(m_set_uci, NULL)
    PHP_FE(m_get_last_error, NULL)
    PHP_FE(m_set, NULL)
    PHP_FE(m_get, NULL)
    PHP_FE(m_kill, NULL)
    PHP_FE(m_data, NULL)
    PHP_FE(m_order, NULL)
    PHP_FE(m_previous, NULL)
    PHP_FE(m_html, NULL)
    PHP_FE(m_html_method, NULL)
    PHP_FE(m_http, NULL)
    PHP_FE(m_proc, NULL)
    PHP_FE(m_proc_byref, NULL)
    PHP_FE(m_method, NULL)
    PHP_FE(m_method_byref, NULL)
    PHP_FE(m_merge_to_db, NULL)
    PHP_FE(m_merge_from_db, NULL)
    PHP_FE(m_return_to_applet, NULL)
    PHP_FE(m_return_to_client, NULL)
    PHP_FE(m_array_test, NULL)
    {NULL, NULL, NULL}
};

#endif /* #if MG_PHP_VERSION >= 50500 */

/* compiled module information */
zend_module_entry mg_module_entry =
{
   STANDARD_MODULE_HEADER,
   "mg_php module",
   mg_functions,
	PHP_MINIT(mg),
	PHP_MSHUTDOWN(mg),
	PHP_RINIT(mg),
	PHP_RSHUTDOWN(mg),
	PHP_MINFO(mg),
   NO_VERSION_YET,
   STANDARD_MODULE_PROPERTIES
};


ZEND_DECLARE_MODULE_GLOBALS(mg)

/* implement standard "stub" routine to introduce ourselves to Zend */
#if COMPILE_DL_MG_MODULE
ZEND_GET_MODULE(mg)
#endif


#ifdef _WIN32
static WORD    VersionRequested;
#else
extern int     errno;
#endif

#ifndef _WIN32
pthread_mutex_t mgsi_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static unsigned long request_no = 0;

static char minit[256] = {'\0'};

char xxx[128];
int nnn = 0;

#if MG_PHP_VERSION >= 70000
int                  mg_type(zval * item);
int                  mg_get_integer(zval * item);
double               mg_get_float(zval * item);
char *               mg_get_string(zval * item, zval * item_tmp, int * size);
int                  mg_php_error(MGPAGE *p_page, char *buffer);
int                  mg_get_input_arguments(int argument_count, zval *parameter_array[]); /* cmtxxx */
static const char *  mg_array_lookup_string(HashTable *ht, const char *idx);
int                  mg_array_add_record(zval *ppa, MGAREC * arec, int mode);
int                  mg_array_parse(MGPAGE *p_page, int chndle, zval *ppa, MGBUF *p_buf, int mode, short byref);
#else
int                  mg_type(zval ** item);
int                  mg_get_integer(zval ** item);
double               mg_get_float(zval ** item);
char *               mg_get_string(zval ** item, zval ** item_tmp, int * size);
int                  mg_php_error(MGPAGE *p_page, char *buffer);
static const char *  mg_array_lookup_string(HashTable *ht, const char *idx);
int                  mg_array_add_record(zval **ppa, MGAREC * arec, int mode);
int                  mg_array_parse(MGPAGE *p_page, int chndle, zval **ppa, MGBUF *p_buf, int mode, short byref);
#endif /* #if MG_PHP_VERSION >= 70000 */


int                  mg_open_connection(MGPAGE *p_page, int *chndle, short context);
int                  mg_close_connection(MGPAGE *p_page, int chndle, short context);

int                  mg_request_header(MGPAGE *p_page, MGBUF *p_buf, char *command);
int                  mg_request_add(MGPAGE *p_page, int chndle, MGBUF *p_buf, unsigned char *element, int size, short byref, short type);

int                  mg_pow(int n10, int power);
int                  mg_encode_size64(int n10);
int                  mg_decode_size64(int nxx);
int                  mg_encode_size(unsigned char *esize, int size, short base);
int                  mg_decode_size(unsigned char *esize, int len, short base);
int                  mg_encode_item_header(unsigned char * head, int size, short byref, short type);
int                  mg_decode_item_header(unsigned char * head, int * size, short * byref, short * type);
int                  mg_send_request(MGPAGE *p_page, int chndle, MGBUF *p_buf, int mode);
int                  mg_read_response(MGPAGE *p_page, int chndle, MGBUF *p_buf, int size, int mode);
int                  mg_ucase(char *string);
int                  mg_lcase(char *string);
int                  mg_buf_init(MGBUF *p_buf);
int                  mg_buf_resize(MGBUF *p_buf, unsigned long size);
int                  mg_buf_free(MGBUF *p_buf);
int                  mg_buf_cpy(MGBUF *p_buf, char *buffer, unsigned long size);
int                  mg_buf_cat(MGBUF *p_buf, char *buffer, unsigned long size);
void *               mg_malloc(unsigned long size);
void *               mg_realloc(void *p_buffer, unsigned long size);
int                  mg_free(void *p_buffer);
unsigned long        mg_get_last_error(int context);
int                  mg_pause(unsigned long msecs);
int                  mg_log_event(char *event, char *title);


#if MG_PHP_VERSION >= 70000
static void mg_free_user(void *rsrc TSRMLS_DC) {
#else
static void mg_free_user(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
#endif

#if 0

   MGUSER *p_mg_user_le;
/*
mg_log_event("mg_free_user", "p_php");
*/
   p_mg_user_le = (MGUSER *) rsrc->ptr;
#endif

   return;
}


/* {{{ php_mysql_init_globals
 */
static void php_mg_init_globals(zend_mg_globals *mg_globals)
{
/*
mg_log_event("php_mg_init_globals", "p_php");
*/
	mg_globals->req_no = 0;
	mg_globals->fun_no = 0;
   *(mg_globals->trace) = '\0';
   mg_globals->p_page = NULL;

}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mg)
{
   int n;
   time_t now;
   char buffer[256];

ZEND_INIT_MODULE_GLOBALS(mg, php_mg_init_globals, NULL);

#if 0
   le_mg_user = zend_register_list_destructors_ex(mg_free_user, NULL, "mg_user", module_number);
#endif
/*
mg_log_event("ZEND_INIT_MODULE_GLOBALS", "p_php");
*/
   now = time(NULL);
   sprintf(buffer, "<br> 0 : 0 : minit : %s", ctime(&now));
   for (n = 0; buffer[n] != '\0'; n ++) {
      if ((unsigned int) buffer[n] < 32) {
         buffer[n] = '\0';
         break;
      }
   }
   strcat(buffer, "\r\n");
   strcat(minit, buffer);
/*
   le_mg_user = zend_register_resource_destructors_ex(mg_free_user, NULL, "mg_user", module_number);
*/

/*
	struct protoent *pe;

	le_socket	= zend_register_list_destructors_ex(destroy_socket,	NULL, le_socket_name, module_number);
	le_iov		= zend_register_list_destructors_ex(destroy_iovec,	NULL, le_iov_name, module_number);

	REGISTER_LONG_CONSTANT("AF_UNIX",		AF_UNIX,		CONST_CS | CONST_PERSISTENT);

	if ((pe = getprotobyname("tcp"))) {
		REGISTER_LONG_CONSTANT("SOL_TCP", pe->p_proto, CONST_CS | CONST_PERSISTENT);
	}

	if ((pe = getprotobyname("udp"))) {
		REGISTER_LONG_CONSTANT("SOL_UDP", pe->p_proto, CONST_CS | CONST_PERSISTENT);
	}
*/

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mg)
{

/*
   mg_log_event("M Shutdown", "mg_php");
*/

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mg)
{
   int n;

   request_no ++;

   MGG(req_no) = request_no;
   MGG(fun_no) = 0;
   strcpy(MGG(trace), minit);

   MGG(p_page) = (MGPAGE *) mg_malloc(sizeof(MGPAGE));
/*
{
   char buffer[256];
   sprintf(buffer, "MGG(p_page)=%x", MGG(p_page));
   mg_log_event(buffer, "p_php: R Startup");
}
*/

   MGG(p_page)->mem_error = 0;
   MGG(p_page)->storage_mode = 0;
   MGG(p_page)->timeout = 0;
   MGG(p_page)->no_retry = 0;
   strcpy(MGG(p_page)->server, "");
   strcpy(MGG(p_page)->uci, "");

   strcpy(MGG(p_page)->eod, MG_EOD);
   for (n = 0; n < MG_MAXCON; n ++) {
      MGG(p_page)->pcon[n] = NULL;
   }

   MG_FTRACE("rinit");
/*
mg_log_event("R Startup", "p_php");
*/

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mg)
{
   int n;
/*
mg_log_event("R Shutdown", "mg_php");
*/

   if (MGG(p_page) != NULL) {

      for (n = 0; n < MG_MAXCON; n ++) {
         if (MGG(p_page)->pcon[n] != NULL) {
            mg_close_connection(MGG(p_page), n, 0);
         }
      }
      mg_free((void *) MGG(p_page));
   }

	return SUCCESS;
}
/* }}} */



/* {{{ PHP_MINFO_FUNCTION
 */

PHP_MINFO_FUNCTION(mg)
{

	php_info_print_table_start();

   php_info_print_table_header(2, "Property", "Value");

	php_info_print_table_row(2, "Version", MG_VERSION);

	php_info_print_table_end();

}
/* }}} */


/* implement function that is meant to be made available to PHP */

ZEND_FUNCTION(m_test)
{
   char test[64];

   strcpy(test, "test");

   MG_RETURN_STRING(test, 1);

}


ZEND_FUNCTION(m_dump_trace)
{
   MGPAGE *p_page;

   p_page = MGG(p_page);

   MG_FTRACE("m_dump_trace");

   zend_write(MGG(trace), (uint) strlen(MGG(trace)));

   MG_RETURN_STRING(MGG(trace), 1);
}



ZEND_FUNCTION(m_ext_version)
{
   long parameter = 0;
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle = 0;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_ext_version");

#if 0
{
   MGUSER *p_mg_user;

   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   p_mg_user = NULL;
   ZEND_FETCH_RESOURCE(p_mg_user, MGUSER *, parameter_array, -1, "mg_user", le_mg_user);
   if (p_mg_user)
      mg_log_event(p_mg_user->buffer, "mg_php: reg");

   p_mg_user = (MGUSER *) mg_malloc(sizeof(MGUSER));

   strcpy(p_mg_user->buffer, "my stuff");
   ZEND_REGISTER_RESOURCE(return_value, p_mg_user, le_mg_user);
}
#endif

   nnn ++;
   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 0)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */

   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   mg_buf_cpy(p_buf, "S", 1);

   for (n = 0; n < argument_count; n ++) {

      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);

   }

   sprintf(p_buf->p_buffer, "M/Gateway Developments Ltd. - mg_php: PHP Gateway to M - Version %s", MG_VERSION);
   p_buf->curr_size = (unsigned long) strlen(p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}



ZEND_FUNCTION(m_set_error_mode)
{
   char buffer[128];
   int argument_count, n, mode;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_error_mode");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

/*
   for (n = 0; n < argument_count; n ++) {
      convert_to_string_ex(MG_AREF_PP(parameter_array[n]));
      mg_log_event((char *) MG_AREF_PP(parameter_array[n]), "cmcmcm0");
   }
*/

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 100);
   buffer[100] = '\0';
   mode = (int) strtol(buffer, NULL, 10);

   if (mode == 0) {
      p_page->error_mode = 0;
      strcpy(p_page->error_code, "");
      strcpy(p_page->error_mess, "");
   }
   else if (mode == 1) {
      p_page->error_mode = 1;
      strcpy(p_page->error_code, "");
      strcpy(p_page->error_mess, "");
   }
   else if (mode == 9) {
      p_page->error_mode = 9;
      strcpy(p_page->error_code, "-1");
      strcpy(p_page->error_mess, "");

      if (argument_count > 1) {
         convert_to_string_ex(MG_AREF_PP(parameter_array[1]));
         strncpy(buffer, MG_STR(parameter_array[1]), 100);
/*
mg_log_event(buffer, "cmcmcm");
*/
         buffer[100] = '\0';
         strcpy(p_page->error_code, buffer);
      }
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;

}


ZEND_FUNCTION(m_set_storage_mode)
{
   char buffer[128];
   int argument_count, n, mode;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_storage_mode");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 100);
   buffer[100] = '\0';
   mode = (int) strtol(buffer, NULL, 10);

   if (mode == 0) {
      p_page->storage_mode = 0;
   }
   else if (mode == 1) {
      p_page->storage_mode = 1;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;
}

ZEND_FUNCTION(m_set_timeout)
{
   char buffer[128];
   int argument_count, n, timeout;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_timeout");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 100);
   buffer[100] = '\0';
   timeout = (int) strtol(buffer, NULL, 10);

   if (timeout >= 0) {
      p_page->timeout = timeout;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;
}


ZEND_FUNCTION(m_set_no_retry)
{
   char buffer[128];
   int argument_count, n, no_retry;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_no_retry");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 100);
   buffer[100] = '\0';
   no_retry = (int) strtol(buffer, NULL, 10);

   if (no_retry == 0) {
      p_page->no_retry = 0;
   }
   else if (no_retry == 1) {
      p_page->no_retry = 1;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;

}

ZEND_FUNCTION(m_set_server)
{
   char buffer[128];
   int argument_count, n;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_server");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 31);
   buffer[31] = '\0';

   strcpy(p_page->server, buffer);

   MG_RETURN_TRUE;
}


ZEND_FUNCTION(m_set_uci)
{
   char buffer[128];
   int argument_count, n;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   MG_FTRACE("m_set_uci");

   p_page->error_mode = 0;
   strcpy(p_page->error_code, "");
   strcpy(p_page->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(MG_AREF_PP(parameter_array[0]));
   strncpy(buffer, MG_STR(parameter_array[0]), 31);
   buffer[31] = '\0';

   strcpy(p_page->uci, buffer);

   MG_RETURN_TRUE;

}


ZEND_FUNCTION(m_get_last_error)
{
   char buffer[256];
   int argument_count, n;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   MGPAGE *p_page;

   p_page = MGG(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }


   MG_FTRACE("m_get_last_error");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   for (n = 0; n < argument_count; n ++) {

      convert_to_string_ex(MG_AREF_PP(parameter_array[n]));

   }

   strcpy(buffer, p_page->error_mess);

   MG_RETURN_STRING(buffer, 1);

}


ZEND_FUNCTION(m_set)
{
   long parameter = 0;
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_set");

/*
   mg_log_event("test", "m_set()");
*/
   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);

   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "S");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

/*
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &parameter) == FAILURE) {
        return;
    }
*/
/*
   mg_log_event("test", "m_set() open");
*/

/*
   mg_log_event("test", "m_set() send");
*/
   mg_send_request(p_page, chndle, p_buf, 1);

/*
   mg_log_event("test", "m_set() read");
*/
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

/*
   mg_log_event("test", "m_set() close");
*/
   mg_close_connection(p_page, chndle, 1);
/*
   mg_log_event("test", "m_set() exit");
*/
   parameter ++;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}


ZEND_FUNCTION(m_get)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_get");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "G");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

/*
   pthread_mutex_lock(&mgsi_lock);
*/

   mg_send_request(p_page, chndle, p_buf, 1);

   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

/*
strcpy(buffer, "sgit");
*/
/*
{
nnn ++;
char buf[256];
sprintf(buf, " nnnx = %d", nnn);
mg_request_add(p_page, chndle, p_buf, buf);
mg_request_add(p_page, chndle, p_buf, lp_connection->error);

}
*/

/*
   pthread_mutex_unlock(&mgsi_lock);
*/
   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}



ZEND_FUNCTION(m_kill)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_kill");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "K");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}



ZEND_FUNCTION(m_data)
{
   MGBUF mgbuf, *p_buf;   
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_data");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "D");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}



ZEND_FUNCTION(m_order)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_order");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "O");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}



ZEND_FUNCTION(m_previous)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_previous");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "P");

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}



ZEND_FUNCTION(m_html)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_html");

/*
   n = sapi_add_header_ex("x=y", 3, 1, 1 TSRMLS_DC);
   n = sapi_add_header("x=y", 3, 1);
*/

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "H");

   for (n = 0; n < argument_count; n ++) {
      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, 0);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_html() function has been exceeded (1MB)", 1);
   }

   mg_send_request(p_page, chndle, p_buf, 1);

   while ((n = mg_read_response(p_page, chndle, p_buf, 2048, 1))) {
      zend_write(p_buf->p_buffer + MG_RECV_HEAD, n - MG_RECV_HEAD);
   }

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}



ZEND_FUNCTION(m_html_method)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_html_method");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "y");

   for (n = 0; n < argument_count; n ++) {
      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, 0);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_html_method() function has been exceeded (1MB)", 1);
   }

   mg_send_request(p_page, chndle, p_buf, 1);

   while ((n = mg_read_response(p_page, chndle, p_buf, 2048, 1))) {
      zend_write(p_buf->p_buffer + MG_RECV_HEAD, n - MG_RECV_HEAD);
   }

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}


ZEND_FUNCTION(m_http)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL, *p, *p1, *p2;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_http");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 2 || argument_count > 3)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   if (Z_TYPE_P(MG_AREF_P(parameter_array[argument_count - 2])) != IS_ARRAY) {
      MG_ERROR0("The penultimate argument to the 'm_http()' function must be an array");
   }

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "h");

   for (n = 0; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, 0);
      }
      else if (n == (argument_count - 1)) {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);

/*
{
char buffer[256];
   strcpy(buffer, "Content-Type: text/xml/mine");
	sapi_add_header(buffer, strlen(buffer), 1);
   strcpy(buffer, "Mine: asdasdsa");
	sapi_add_header(buffer, strlen(buffer), 1);

   strcpy(buffer, "HTTP/1.0 200 OK");
	sapi_add_header(buffer, strlen(buffer), 1);
}
*/

   n = 0;
   while (mg_read_response(p_page, chndle, p_buf, MG_BUF, 1)) {
      n ++;
      if (n == 1) {

         p = strstr(p_buf->p_buffer + MG_RECV_HEAD, "\r\n\r\n");
         if (p) {
            p += 2;
            *p = '\0';
            p += 2;
            p1 = p_buf->p_buffer + MG_RECV_HEAD;
            for (;;) {
               p2 = strstr(p1, "\r\n");
               if (!p2)
                  break;
               *p2 = '\0';
               if (strncmp(p1, "HTTP/", 5))
	               sapi_add_header(p1, (int) strlen(p1), 1);
               p1 = p2 + 2;
            }
         }
         else {
            p = p_buf->p_buffer + MG_RECV_HEAD;
         }
         zend_printf("%s", p);
      }
      else
         zend_printf("%s", (p_buf->p_buffer + MG_RECV_HEAD));
   }

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}



ZEND_FUNCTION(m_proc)
{
   short phase;
   int res_open, res_send, res_recv, attempt_no;
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, n, offs, array, len;
/*
   MGPTYPE ptype[256];
   char *param[256];
*/
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   phase = 0;
   res_open = -9;
   res_send = -9;
   res_recv = -9;
   p_buf = NULL;
   p_page = NULL;

#ifdef _WIN32
__try {
#endif

/*
   mg_log_event("test", "***m_proc***");
*/
   phase = 1;

   p_page = MGG(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   phase = 3;

   MG_FTRACE("m_proc");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   phase = 4;

   attempt_no = 0;

m_proc_retry:

   if (attempt_no) {
      mg_pause(1000 * attempt_no);
      if (attempt_no > 10) {
         if (!strlen(lp_connection->error))
            sprintf(p_page->error_mess, "mg_php v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", MG_VERSION, res_open, res_send, res_recv);

         MG_ERROR1(p_page->error_mess);
      }
   }
   attempt_no ++;

   n = mg_open_connection(p_page, &chndle, 1);
   res_open = n;
   if (!n) {

      goto m_proc_retry;

      MG_ERROR1(p_page->error_mess);
   }

   phase = 5;

   mg_request_header(p_page, p_buf, "X");

   phase = 6;

   offs = 0;
   array = 0;
   for (n = 0; n < argument_count; n ++) {

      if (MG_Z_ISREF(parameter_array[n])) {
         array = 1;
         byref = 1;
      }
      else {
         byref = 0;
      }
/*
{
   char buffer[256];
   sprintf(buffer, "M_PROC input at: %d; byref=%d", n, byref);
   mg_log_event(buffer, "cmcmcm");
}
*/

      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, byref);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }

   phase = 7;

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_proc() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_send_request(p_page, chndle, p_buf, 1);

   if (!res_send) {
      goto m_proc_retry;
      MG_ERROR1(p_page->error_mess);
   }
   phase = 9;

   res_recv = mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   if (!res_recv) {
      goto m_proc_retry;
      MG_ERROR1(p_page->error_mess);
   }

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

/*
{
   char buffer[256];
   sprintf(buffer, "array flag = %d", array);
   mg_log_event(buffer, "OUTPUTS");
}
*/

   offs = 1;
   array = 1;
   data = p_buf->p_buffer + MG_RECV_HEAD;

   if (array) {
      short byref, type, stop, anybyref;
      int n, n1, rn, hlen, size, clen, rlen, argc, rec_len, offset, size0;
      unsigned char *parg, *par, *data0;
      char stype[4];
      MGAREC arec;

      phase = 12;

      stop = 0;
      offset = 2;
      parg = (p_buf->p_buffer + MG_RECV_HEAD);

      clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
      stype[0] = p_buf->p_buffer[5];
      stype[1] = p_buf->p_buffer[6];
      stype[2] = '\0';
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : response clen = %ld; curr=%ld stype=%s;", clen, p_buf->curr_size, stype);
   mg_log_event(p_buf->p_buffer, buffer);
}
*/

      if (!strcmp(stype, "cv")) {
/*
mg_log_event("Array flag set but no data", "STOP");
*/
         anybyref = 0;
         size0 = p_buf->curr_size - MG_RECV_HEAD;
      }
      else {

         rlen = 0;
         argc = 0;
         anybyref = 0;
         for (n = 0;; n ++) {

            hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
            if ((hlen + size + rlen) > clen) {
               break;
            }
            parg += hlen;
            rlen += hlen;
            if (!n) {
               data0 = parg;
               size0 = size;
            }
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT (ma_proc) %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/
            if (type == MG_TX_AREC) {
               par = parg;
               rn = 0;
               rec_len = 0;
               arec.kn = 0;

               parg += size;
               rlen += size;

/*
{
   char buffer[256];
   sprintf(buffer, "array an = %d", n);
   mg_log_event(buffer, "OUTPUTS");
}
*/
               for (n1 = 0;; n1 ++) {
                  hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
                  if ((hlen + size + rlen) > clen) {
                     stop = 1;
                     break;
                  }
                  if (type == MG_TX_EOD) {
                     parg += (hlen + size);
                     rlen += (hlen + size);
                     break;
                  }
                  parg += hlen;
                  rlen += hlen;
                  rec_len += hlen;
                  rec_len += size;

/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n1, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/

                  if (type == MG_TX_DATA) {
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY RECORD %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d; rec_len=%d", n1, argc, hlen, size, byref, type, rlen, clen, rec_len);
   mg_log_event(par, buffer);
   *(parg + size) = c;
}
*/

                     arec.vrec = parg;
                     arec.vsize = size;

                     if (argc >= offset && (argc - offset) < argument_count) {
                        anybyref = 1;
                        mg_array_add_record(MG_AREF_PP(parameter_array[argc - offset]), &arec, 0);
                     }

                     arec.kn = 0;

                     par = parg;
                     rec_len = 0;
                  }
                  else {
                     arec.krec[arec.kn] = parg;
                     arec.ksize[arec.kn] = size;
                     arec.kn ++;
                  }
                  parg += size;
                  rlen += size;
               }
            }
            else {

               char c;

               if (argc >= offset && (argc - offset) < argument_count) {

                  if (MG_Z_ISREF(parameter_array[argc - offset])) {

                     anybyref = 1;
                     c = *(parg + size);
                     *(parg + size) = '\0';

#if MG_PHP_VERSION >= 70000
                     ZVAL_STRING(&parameter_array[argc - offset], parg);
#else
                     MG_Z_STRLEN(parameter_array[argc - offset]) = size;
                     MG_Z_STRVAL(parameter_array[argc - offset]) = erealloc(MG_Z_STRVAL(parameter_array[argc - offset]), sizeof(char) * (size + 32));
                     strcpy(MG_Z_STRVAL(parameter_array[argc - offset]), parg);
#endif
                     *(parg + size) = c;
                  }

               }

               parg += size;
               rlen += size;
            }

            if (rlen >= clen || stop) {
               stop = 0;
               break;
            }
            argc ++;
         }

      }

      if (stop) {
         MG_ERROR1("ma_merge_from_db: Bad return data");
      }
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : anybyref=%d; size0=%d", anybyref, size0);
   mg_log_event(buffer, "TEST");
}
*/

      if (anybyref) {
         data = data0;
         *(data + size0) = '\0';
      }
   }
/*
   if (attempt_no > 1) {
      char buffer[256];
      sprintf(buffer, "Request served on retry: attempt_no=%d; res_recv=%d; p_buf->curr_size=%ld", attempt_no, res_recv, p_buf->curr_size);
      mg_log_event(buffer, "Diagnostic");
   }
*/
   phase = 99;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;
   char buffer[256];

   __try {
      code = GetExceptionCode();
      sprintf(buffer, "Exception caught in f:m_proc: %x|%d", code, phase);
      MG_ERROR1(buffer);

/*
      mg_log_event(buffer, "Error Condition");
      if (p_buf) {
         sprintf(buffer, "Error Condition in f:m_proc: res_open=%d; res_send=%d; res_recv=%d; p_buf->curr_size=%ld; p_buf->size=%ld", res_open, res_send, res_recv, p_buf->curr_size, p_buf->size);
         mg_log_event(p_buf->p_buffer, buffer);
      }
*/
   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif

}


ZEND_FUNCTION(m_proc_byref)
{
   short phase;
   int res_open, res_send, res_recv, attempt_no;
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, n, offs, array, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
/*
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
*/
   zval	*parameter_array[MG_MAXARG];
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   phase = 0;
   res_open = -9;
   res_send = -9;
   res_recv = -9;
   p_buf = NULL;
   p_page = NULL;

#ifdef _WIN32
__try {
#endif

   phase = 1;

   p_page = MGG(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   phase = 3;

   MG_FTRACE("m_proc");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */

#if MG_PHP_VERSION >= 70000
   n = mg_get_input_arguments(argument_count, parameter_array);
   if (n == FAILURE) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

#else
   if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
#endif

   phase = 4;

   attempt_no = 0;

m_proc_byref_retry:

   if (attempt_no) {
      mg_pause(1000 * attempt_no);
      if (attempt_no > 10) {
         if (!strlen(lp_connection->error))
            sprintf(p_page->error_mess, "mg_php v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", MG_VERSION, res_open, res_send, res_recv);
         MG_ERROR1(p_page->error_mess);
      }
   }
   attempt_no ++;

   n = mg_open_connection(p_page, &chndle, 1);
   res_open = n;
   if (!n) {
      goto m_proc_byref_retry;

      MG_ERROR1(p_page->error_mess);
   }

   phase = 5;

   mg_request_header(p_page, p_buf, "X");

   phase = 6;

   offs = 0;
   array = 0;
   for (n = 0; n < argument_count; n ++) {
#if MG_PHP_VERSION >= 70000
      if (MG_Z_ISREF(*parameter_array[n])) {
#else
      if (MG_Z_ISREF(parameter_array[n])) {
#endif
         array = 1;
         byref = 1;
      }
      else {
         if (n > 0) {
            byref = 1;
         }
      }
/*
{
   char buffer[256];
   sprintf(buffer, "M_PROC input at: %d; byref=%d", n, byref);
   mg_log_event(buffer, "cmcmcm");
}
*/
/*
      zend_printf("\r\nOK news argument_count=%d; n=%d; byref=%d;\r\n", argument_count, n, byref);
*/

#if MG_PHP_VERSION >= 70000

      if (Z_TYPE_P(parameter_array[n]) == IS_ARRAY) {
         array = 1;
         byref = 1;
/*
         zend_printf("\r\narg %d is array byref=%d", n, byref);
*/
         mg_array_parse(p_page, chndle, parameter_array[n], p_buf, 0, byref);
      }
      else {
         if (n > 0) {
            byref = 1;
         }
/*
         zend_printf("\r\narg %d is string byref=%d", n, byref);
*/
         data = mg_get_string(parameter_array[n], NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
#else
      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         array = 1;
         byref = 1;

         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, byref);
      }
      else {
         if (n > 0) {
            byref = 1;
         }
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }

#endif
   }

   phase = 7;

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_proc_byref() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_send_request(p_page, chndle, p_buf, 1);

/*
{
   int n;
   char buffer[256];
   for (n = 0; n < p_buf->curr_size; n++) {
      if ((int) p_buf->p_buffer[n] < 32)
         buffer[n] = '#';
      else
         buffer[n] = p_buf->p_buffer[n];
   }
   buffer[n] = '\0';
   zend_printf("\r\nREQUEST %d: ... %s", p_buf->curr_size, buffer);
}
*/

   if (!res_send) {
      goto m_proc_byref_retry;
      MG_ERROR1(p_page->error_mess);
   }
   phase = 9;

   res_recv = mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   if (!res_recv) {
      goto m_proc_byref_retry;
      MG_ERROR1(p_page->error_mess);
   }

/*
{
   int n;
   char buffer[256];
   for (n = 0; n < res_recv; n++) {
      if ((int) p_buf->p_buffer[n] < 32)
         buffer[n] = '#';
      else
         buffer[n] = p_buf->p_buffer[n];
   }
   buffer[n] = '\0';
   zend_printf("\r\nRESULT %d: ... %s", res_recv, buffer);
}
*/

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

/*
{
   char buffer[256];
   sprintf(buffer, "array flag = %d", array);
   mg_log_event(buffer, "OUTPUTS");
}
*/

   offs = 1;
   array = 1;
   data = p_buf->p_buffer + MG_RECV_HEAD;

   if (array) {
      short byref, type, stop, anybyref;
      int n, n1, rn, hlen, size, clen, rlen, argc, rec_len, offset, size0;
      unsigned char *parg, *par, *data0;
      char stype[4];
      MGAREC arec;

      phase = 12;

      stop = 0;
      offset = 2;
      parg = (p_buf->p_buffer + MG_RECV_HEAD);

      clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
      stype[0] = p_buf->p_buffer[5];
      stype[1] = p_buf->p_buffer[6];
      stype[2] = '\0';
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : response clen = %ld; curr=%ld stype=%s;", clen, p_buf->curr_size, stype);
   mg_log_event(p_buf->p_buffer, buffer);
}
*/

      if (!strcmp(stype, "cv")) {
/*
mg_log_event("Array flag set but no data", "STOP");
*/
         anybyref = 0;
         size0 = p_buf->curr_size - MG_RECV_HEAD;
      }
      else {

         rlen = 0;
         argc = 0;
         anybyref = 0;
         for (n = 0;; n ++) {

            hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
            if ((hlen + size + rlen) > clen) {
               break;
            }
            parg += hlen;
            rlen += hlen;
            if (!n) {
               data0 = parg;
               size0 = size;
            }
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT (ma_proc) %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/
            if (type == MG_TX_AREC) {
               par = parg;
               rn = 0;
               rec_len = 0;
               arec.kn = 0;

               parg += size;
               rlen += size;

/*
{
   char buffer[256];
   sprintf(buffer, "array an = %d", n);
   mg_log_event(buffer, "OUTPUTS");
}
*/
               for (n1 = 0;; n1 ++) {
                  hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
                  if ((hlen + size + rlen) > clen) {
                     stop = 1;
                     break;
                  }
                  if (type == MG_TX_EOD) {
                     parg += (hlen + size);
                     rlen += (hlen + size);
                     break;
                  }
                  parg += hlen;
                  rlen += hlen;
                  rec_len += hlen;
                  rec_len += size;

/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n1, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/

                  if (type == MG_TX_DATA) {
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY RECORD %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d; rec_len=%d", n1, argc, hlen, size, byref, type, rlen, clen, rec_len);
   mg_log_event(par, buffer);
   *(parg + size) = c;
}
*/

                     arec.vrec = parg;
                     arec.vsize = size;

                     if (argc >= offset && (argc - offset) < argument_count) {
                        anybyref = 1;
#if MG_PHP_VERSION >= 70000
                        mg_array_add_record(parameter_array[argc - offset], &arec, 0);
#else
                        mg_array_add_record(MG_AREF_PP(parameter_array[argc - offset]), &arec, 0);
#endif
                     }

                     arec.kn = 0;

                     par = parg;
                     rec_len = 0;
                  }
                  else {
                     arec.krec[arec.kn] = parg;
                     arec.ksize[arec.kn] = size;
                     arec.kn ++;
                  }
                  parg += size;
                  rlen += size;
               }
            }
            else {
               char c;

/*
               zend_printf("\r\nA String argc=%d; offset=%d; size=%d; parg=%s;", argc, offset, size, parg);
*/
/*
               if (argc >= offset && (argc - offset) < argument_count) {
*/
               if (argc > offset && (argc - offset) < argument_count) {

/*
                  if (MG_Z_ISREF(parameter_array[argc - offset])) {
*/
                     anybyref = 1;
                     c = *(parg + size);
                     *(parg + size) = '\0';

#if MG_PHP_VERSION >= 70000

/*
                     Reference: http://www.phpinternalsbook.com/zvals/memory_management.html
*/

/*
                     zend_printf("\r\nargc=%d; offset=%d; size=%d; parg=%s;\r\n", argc, offset, size, parg);
*/

                     ZVAL_STRING(parameter_array[argc - offset], parg);

/*
                     MAKE_STD_ZVAL(zv_src);
                     ZVAL_STRING(&zv_src, (char *) parg);
                     ZVAL_COPY_VALUE(&parameter_array[argc - offset], &zv_src);

                     strcpy(parameter_array[argc - offset], parg);
                     strcpy(MG_Z_STRVAL(&parameter_array[argc - offset]), parg);
*/

/* sort of works */
/*
                     MG_Z_STRLEN(&parameter_array[argc - offset]) = size;
                     zend_string_realloc(MG_Z_STRVAL(&parameter_array[argc - offset]), size + 32, 1);
                     strcpy(MG_Z_STRVAL(&parameter_array[argc - offset]), parg);
*/
#elif MG_PHP_VERSION >= 50600
/*
                     Reference: http://www.phpinternalsbook.com/zvals/memory_management.html
*/

/*
                     zend_printf("\r\nargc=%d; size=%d; result=%s; parg=%s;\r\n", argc, size, xxx, parg);
*/
                     {
                        zval *zv_src;

                        MAKE_STD_ZVAL(zv_src);
                        ZVAL_STRING(zv_src, (char *) parg, 1);
                        ZVAL_COPY_VALUE(*parameter_array[argc - offset], zv_src);
                     }
#else
                     MG_Z_STRLEN(parameter_array[argc - offset]) = size;
                     MG_Z_STRVAL(parameter_array[argc - offset]) = (char *) erealloc(MG_Z_STRVAL(parameter_array[argc - offset]), sizeof(char) * (size + 32));
                     strcpy(MG_Z_STRVAL(parameter_array[argc - offset]), parg);
#endif
                     *(parg + size) = c;
/*
                  }
*/

               }

               parg += size;
               rlen += size;
            }

            if (rlen >= clen || stop) {
               stop = 0;
               break;
            }
            argc ++;
         }

      }

      if (stop) {
         MG_ERROR1("m_proc_byref: Bad return data");
      }
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : anybyref=%d; size0=%d", anybyref, size0);
   mg_log_event(buffer, "TEST");
}
*/

      if (anybyref) {
         data = data0;
         *(data + size0) = '\0';
      }
   }
/*
   if (attempt_no > 1) {
      char buffer[256];
      sprintf(buffer, "Request served on retry: attempt_no=%d; res_recv=%d; p_buf->curr_size=%ld", attempt_no, res_recv, p_buf->curr_size);
      mg_log_event(buffer, "Diagnostic");
   }
*/
   phase = 99;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;
   char buffer[256];

   __try {
      code = GetExceptionCode();
      sprintf(buffer, "Exception caught in f:m_proc_byref: %x|%d", code, phase);
      MG_ERROR1(buffer);

/*
      mg_log_event(buffer, "Error Condition");
      if (p_buf) {
         sprintf(buffer, "Error Condition in f:m_proc_byref: res_open=%d; res_send=%d; res_recv=%d; p_buf->curr_size=%ld; p_buf->size=%ld", res_open, res_send, res_recv, p_buf->curr_size, p_buf->size);
         mg_log_event(p_buf->p_buffer, buffer);
      }
*/
   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif
}


ZEND_FUNCTION(m_method)
{
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, n, offs, array, len;
/*
   MGPTYPE ptype[256];
   char *param[256];
*/
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_method");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "x");

   offs = 0;
   array = 0;

   for (n = 0; n < argument_count; n ++) {

      if (MG_Z_ISREF(parameter_array[n])) {
         array = 1;
         byref = 1;
      }
      else
         byref = 0;
/*
{
   char buffer[256];
   sprintf(buffer, "M_METHOD input at: %d; byref=%d", n, byref);
   mg_log_event(buffer, "cmcmcm");
}
*/

      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, byref);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_method() function has been exceeded (1MB)", 1);
   }

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   offs = 1;
   array = 1;
   data = p_buf->p_buffer + MG_RECV_HEAD;

   if (array) {
      short byref, type, stop, anybyref;
      int n, n1, rn, hlen, size, clen, rlen, argc, rec_len, offset, size0;
      unsigned char *parg, *par, *data0;
      char stype[4];
      MGAREC arec;

      stop = 0;
      offset = 2;
      parg = (p_buf->p_buffer + MG_RECV_HEAD);

      clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
      stype[0] = p_buf->p_buffer[5];
      stype[1] = p_buf->p_buffer[6];
      stype[2] = '\0';
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : response clen = %ld; curr=%ld stype=%s;", clen, p_buf->curr_size, stype);
   mg_log_event(p_buf->p_buffer, buffer);
}
*/

      if (!strcmp(stype, "cv")) {
/*
mg_log_event("Array flag set but no data", "STOP");
*/
         anybyref = 0;
         size0 = p_buf->curr_size - MG_RECV_HEAD;
      }
      else {

         rlen = 0;
         argc = 0;
         anybyref = 0;
         for (n = 0;; n ++) {
            hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
            if ((hlen + size + rlen) > clen) {
               break;
            }
            parg += hlen;
            rlen += hlen;
            if (!n) {
               data0 = parg;
               size0 = size;
            }
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT (ma_proc) %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/
            if (type == MG_TX_AREC) {
               par = parg;
               rn = 0;
               rec_len = 0;
               arec.kn = 0;

               parg += size;
               rlen += size;

/*
{
   char buffer[256];
   sprintf(buffer, "array an = %d", n);
   mg_log_event(buffer, "OUTPUTS");
}
*/
               for (n1 = 0;; n1 ++) {
                  hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
                  if ((hlen + size + rlen) > clen) {
                     stop = 1;
                     break;
                  }
                  if (type == MG_TX_EOD) {
                     parg += (hlen + size);
                     rlen += (hlen + size);
                     break;
                  }
                  parg += hlen;
                  rlen += hlen;
                  rec_len += hlen;
                  rec_len += size;

/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n1, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/

                  if (type == MG_TX_DATA) {
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY RECORD %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d; rec_len=%d", n1, argc, hlen, size, byref, type, rlen, clen, rec_len);
   mg_log_event(par, buffer);
   *(parg + size) = c;
}
*/

                     arec.vrec = parg;
                     arec.vsize = size;

                     if (argc >= offset && (argc - offset) < argument_count) {
                        anybyref = 1;
                        mg_array_add_record(MG_AREF_PP(parameter_array[argc - offset]), &arec, 0);
                     }

                     arec.kn = 0;

                     par = parg;
                     rec_len = 0;
                  }
                  else {
                     arec.krec[arec.kn] = parg;
                     arec.ksize[arec.kn] = size;
                     arec.kn ++;
                  }
                  parg += size;
                  rlen += size;
               }
            }
            else {

               char c;

               if (argc >= offset && (argc - offset) < argument_count) {

                  if (MG_Z_ISREF(parameter_array[argc - offset])) {

                     anybyref = 1;
                     c = *(parg + size);
                     *(parg + size) = '\0';

#if MG_PHP_VERSION >= 70000
                     ZVAL_STRING(&parameter_array[argc - offset], parg);
#else
                     MG_Z_STRLEN(parameter_array[argc - offset]) = size;
                     MG_Z_STRVAL(parameter_array[argc - offset]) = erealloc(MG_Z_STRVAL(parameter_array[argc - offset]), sizeof(char) * (size + 32));
                     strcpy(MG_Z_STRVAL(parameter_array[argc - offset]), parg);
#endif

                     *(parg + size) = c;
                  }

               }

               parg += size;
               rlen += size;
            }

            if (rlen >= clen || stop) {
               stop = 0;
               break;
            }
            argc ++;
         }
      }

      if (stop) {
         MG_ERROR1("m_method: Bad return data");
      }

      if (anybyref) {
         data = data0;
         *(data + size0) = '\0';
      }
   }

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);
}


ZEND_FUNCTION(m_method_byref)
{
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, n, offs, array, len;
/*
   MGPTYPE ptype[256];
   char *param[256];
*/
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
/*
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
*/
   zval	*parameter_array[MG_MAXARG];
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_method");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
#if MG_PHP_VERSION >= 70000
   n = mg_get_input_arguments(argument_count, parameter_array);
   if (n == FAILURE) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

#else

   if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
#endif

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "x");

   offs = 0;
   array = 0;

   for (n = 0; n < argument_count; n ++) {

#if MG_PHP_VERSION >= 70000
      if (MG_Z_ISREF(*parameter_array[n])) {
#else
      if (MG_Z_ISREF(parameter_array[n])) {
#endif
         array = 1;
         byref = 1;
      }
      else {
         byref = 0;
         if (n > 0) {
            byref = 1;
         }
      }
/*
{
   char buffer[256];
   sprintf(buffer, "M_METHOD input at: %d; byref=%d", n, byref);
   mg_log_event(buffer, "cmcmcm");
}
*/

#if MG_PHP_VERSION >= 70000

      if (Z_TYPE_P(parameter_array[n]) == IS_ARRAY) {
         array = 1;
         byref = 1;
/*
         zend_printf("\r\narg %d is array byref=%d", n, byref);
*/
         mg_array_parse(p_page, chndle, parameter_array[n], p_buf, 0, byref);
      }
      else {
         if (n > 0) {
            byref = 1;
         }
/*
         zend_printf("\r\narg %d is string byref=%d", n, byref);
*/
         data = mg_get_string(parameter_array[n], NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
#else
      if (Z_TYPE_P(MG_AREF_P(parameter_array[n])) == IS_ARRAY) {
         array = 1;
         byref = 1;
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, byref);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }

#endif

   }

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_method_byref() function has been exceeded (1MB)", 1);
   }

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   offs = 1;
   array = 1;
   data = p_buf->p_buffer + MG_RECV_HEAD;

   if (array) {
      short byref, type, stop, anybyref;
      int n, n1, rn, hlen, size, clen, rlen, argc, rec_len, offset, size0;
      unsigned char *parg, *par, *data0;
      char stype[4];
      MGAREC arec;

      stop = 0;
      offset = 2;
      parg = (p_buf->p_buffer + MG_RECV_HEAD);

      clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
      stype[0] = p_buf->p_buffer[5];
      stype[1] = p_buf->p_buffer[6];
      stype[2] = '\0';
/*
{
   char buffer[256];
   sprintf(buffer, "OUTPUTS : response clen = %ld; curr=%ld stype=%s;", clen, p_buf->curr_size, stype);
   mg_log_event(p_buf->p_buffer, buffer);
}
*/

      if (!strcmp(stype, "cv")) {
/*
mg_log_event("Array flag set but no data", "STOP");
*/
         anybyref = 0;
         size0 = p_buf->curr_size - MG_RECV_HEAD;
      }
      else {

         rlen = 0;
         argc = 0;
         anybyref = 0;
         for (n = 0;; n ++) {
            hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
            if ((hlen + size + rlen) > clen) {
               break;
            }
            parg += hlen;
            rlen += hlen;
            if (!n) {
               data0 = parg;
               size0 = size;
            }
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT (ma_proc) %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/
            if (type == MG_TX_AREC) {
               par = parg;
               rn = 0;
               rec_len = 0;
               arec.kn = 0;

               parg += size;
               rlen += size;

/*
{
   char buffer[256];
   sprintf(buffer, "array an = %d", n);
   mg_log_event(buffer, "OUTPUTS");
}
*/
               for (n1 = 0;; n1 ++) {
                  hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
                  if ((hlen + size + rlen) > clen) {
                     stop = 1;
                     break;
                  }
                  if (type == MG_TX_EOD) {
                     parg += (hlen + size);
                     rlen += (hlen + size);
                     break;
                  }
                  parg += hlen;
                  rlen += hlen;
                  rec_len += hlen;
                  rec_len += size;

/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n1, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/

                  if (type == MG_TX_DATA) {
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY RECORD %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d; rec_len=%d", n1, argc, hlen, size, byref, type, rlen, clen, rec_len);
   mg_log_event(par, buffer);
   *(parg + size) = c;
}
*/

                     arec.vrec = parg;
                     arec.vsize = size;

                     if (argc >= offset && (argc - offset) < argument_count) {
                        anybyref = 1;
#if MG_PHP_VERSION >= 70000
                        mg_array_add_record((zval *) MG_AREF_PP(parameter_array[argc - offset]), &arec, 0);
#else
                        mg_array_add_record((zval **) MG_AREF_PP(parameter_array[argc - offset]), &arec, 0);
#endif
                     }

                     arec.kn = 0;

                     par = parg;
                     rec_len = 0;
                  }
                  else {
                     arec.krec[arec.kn] = parg;
                     arec.ksize[arec.kn] = size;
                     arec.kn ++;
                  }
                  parg += size;
                  rlen += size;
               }
            }
            else {

               char c;

               if (argc >= offset && (argc - offset) < argument_count) {
/*
               if (argc > offset && (argc - offset) < argument_count) {
*/
#if MG_PHP_VERSION >= 70000
                  if (MG_Z_ISREF(*parameter_array[argc - offset])) {
#else
                  if (MG_Z_ISREF(parameter_array[argc - offset])) {
#endif

                     anybyref = 1;
                     c = *(parg + size);
                     *(parg + size) = '\0';

#if MG_PHP_VERSION >= 70000

/*
                     Reference: http://www.phpinternalsbook.com/zvals/memory_management.html
*/

/*
                     zend_printf("\r\nargc=%d; offset=%d; size=%d; parg=%s;\r\n", argc, offset, size, parg);
*/

                     ZVAL_STRING(parameter_array[argc - offset], parg);

/*
                     MAKE_STD_ZVAL(zv_src);
                     ZVAL_STRING(&zv_src, (char *) parg);
                     ZVAL_COPY_VALUE(&parameter_array[argc - offset], &zv_src);

                     strcpy(parameter_array[argc - offset], parg);
                     strcpy(MG_Z_STRVAL(&parameter_array[argc - offset]), parg);
*/

/* sort of works */
/*
                     MG_Z_STRLEN(&parameter_array[argc - offset]) = size;
                     zend_string_realloc(MG_Z_STRVAL(&parameter_array[argc - offset]), size + 32, 1);
                     strcpy(MG_Z_STRVAL(&parameter_array[argc - offset]), parg);
*/
#elif MG_PHP_VERSION >= 50600
/*
                     Reference: http://www.phpinternalsbook.com/zvals/memory_management.html
*/

/*
                     zend_printf("\r\nargc=%d; size=%d; result=%s; parg=%s;\r\n", argc, size, xxx, parg);
*/
                     {
                        zval *zv_src;

                        MAKE_STD_ZVAL(zv_src);
                        ZVAL_STRING(zv_src, (char *) parg, 1);
                        ZVAL_COPY_VALUE(*parameter_array[argc - offset], zv_src);
                     }
#else
                     MG_Z_STRLEN(parameter_array[argc - offset]) = size;
                     MG_Z_STRVAL(parameter_array[argc - offset]) = erealloc(MG_Z_STRVAL(parameter_array[argc - offset]), sizeof(char) * (size + 32));
                     strcpy(MG_Z_STRVAL(parameter_array[argc - offset]), parg);
#endif
                     *(parg + size) = c;
                  }

               }

               parg += size;
               rlen += size;
            }

            if (rlen >= clen || stop) {
               stop = 0;
               break;
            }
            argc ++;
         }
      }

      if (stop) {
         MG_ERROR1("m_method_byref: Bad return data");
      }

      if (anybyref) {
         data = data0;
         *(data + size0) = '\0';
      }
   }

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);
}


ZEND_FUNCTION(m_merge_to_db)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_merge_to_db");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 3)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   if (Z_TYPE_P(MG_AREF_P(parameter_array[argument_count - 1])) != IS_STRING) {
      MG_ERROR0("The last (options) argument to the 'm_merge_to_db()' function must be a string");
   }
   if (Z_TYPE_P(MG_AREF_P(parameter_array[argument_count - 2])) != IS_ARRAY) {
      MG_ERROR0("The penultimate argument to the 'm_merge_to_db()' function must be an array");
   }

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "M");

   for (n = 0; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, 0);
      }
      else if (n == (argument_count - 1)) {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_lcase(data);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->curr_size > 1000000) {
      p_page->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_merge_to_db() function has been exceeded (1MB)", 1);
   }

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }


}



ZEND_FUNCTION(m_merge_from_db)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, offs, array, len;
/*
   MGPTYPE ptype[256];
   char *param[256];
*/

   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval *parameter_array_d[MG_MAXARG];
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_merge_from_db");

   /* cmtxxx */

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (3 arguments) */
   if (argument_count < 3) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

#if MG_PHP_VERSION >= 70000
   for (n = 0; n < argument_count; n ++) {
      parameter_array_d[n] = &parameter_array_a[n];
      ZVAL_DEREF(parameter_array_d[n]);
      if (Z_TYPE_P(parameter_array_d[n]) == IS_ARRAY) {
         SEPARATE_ARRAY(parameter_array_d[n]);
      }
   }
   if (Z_TYPE_P(parameter_array_d[argument_count - 1]) != IS_STRING) {
      MG_ERROR0("The last (options) argument to the 'm_merge_from_db()' function must be a string");
   }
   if (Z_TYPE_P(parameter_array_d[argument_count - 2]) != IS_ARRAY   ) {
      MG_ERROR0("The penultimate argument to the 'm_merge_from_db()' function must be an array");
   }
#else
   if (Z_TYPE_P(MG_AREF_P(parameter_array[argument_count - 1])   ) != IS_STRING) {
      MG_ERROR0("The last (options) argument to the 'm_merge_from_db()' function must be a string");
   }
   if (Z_TYPE_P(MG_AREF_P(parameter_array[argument_count - 2])) != IS_ARRAY) {
      MG_ERROR0("The penultimate argument to the 'm_merge_from_db()' function must be an array");
   }
#endif

/*
   for (n = 0; n < argument_count; n ++) {
      printf("\r\nn=%d; &parameter_array_a[n]=%p; &parameter_array[n]=%p; parray=%p;", n, &parameter_array_a[n], &parameter_array[n], parameter_array_d[n]); 
      printf("\r\nn=%d; type=%d; type1=%d; (IS_ARRAY=%d, IS_REFERENCE=%d)", n, Z_TYPE_P(MG_AREF_P(parameter_array[n])), Z_TYPE_P(parameter_array_d[n]), IS_ARRAY, IS_REFERENCE); 
   }
   printf("\r\n");
*/

   n = mg_open_connection(p_page, &chndle, 1);
   if (!n) {
      MG_ERROR1(lp_connection->error);
   }

   mg_request_header(p_page, p_buf, "m");

   offs = 0;
   array = 0;

   for (n = 0; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
/*
{
   char buffer[256];
   sprintf(buffer, "array n = %d; argument_count=%d;", n, argument_count);
   printf("\r\nINPUTS %s\r\n", buffer);
   mg_log_event(buffer, "INPUTS");
}
*/
#if MG_PHP_VERSION >= 70000
         mg_array_parse(p_page, chndle, parameter_array_d[n], p_buf, 0, 1);
#else
         mg_array_parse(p_page, chndle, MG_AREF_PP(parameter_array[n]), p_buf, 0, 1);
#endif
      }
      else {
         data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
         mg_request_add(p_page, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   mg_send_request(p_page, chndle, p_buf, 1);
   mg_read_response(p_page, chndle, p_buf, MG_BUF, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_close_connection(p_page, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   offs = 1;
   array = 1;

   if (array) {
      short byref, type, stop;
      int n, n1, rn, hlen, size, clen, rlen, argc, rec_len, offset;
      unsigned char *parg, *par;
      MGAREC arec;

      stop = 0;
      offset = 2;
      parg = (p_buf->p_buffer + MG_RECV_HEAD);

      clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);

      rlen = 0;
      argc = 0;
      for (n = 0;; n ++) {
         hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
         if ((hlen + size + rlen) > clen) {
            stop = 1;
            break;
         }
         parg += hlen;
         rlen += hlen;
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   zend_printf("\r\nRESULT %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n, argc, hlen, size, byref, type, rlen, clen);
   *(parg + size) = c;
}
*/
         parg += size;
         rlen += size;
         if (type == MG_TX_AREC) {
            par = parg;
            rn = 0;
            rec_len = 0;
            arec.kn = 0;
/*
{
   char buffer[256];
   sprintf(buffer, "array an = %d", n);
   mg_log_event(buffer, "OUTPUTS");
}
*/
            for (n1 = 0;; n1 ++) {
               hlen = mg_decode_item_header(parg, &size, (short *) &byref, (short *) &type);
               if ((hlen + size + rlen) > clen) {
                  stop = 1;
                  break;
               }
               if (type == MG_TX_EOD) {
                  parg += (hlen + size);
                  rlen += (hlen + size);
                  break;
               }
               parg += hlen;
               rlen += hlen;
               rec_len += hlen;
               rec_len += size;

/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d", n1, argc, hlen, size, byref, type, rlen, clen);
   mg_log_event(parg, buffer);
   *(parg + size) = c;
}
*/
               if (type == MG_TX_DATA) {
/*
{
   unsigned char c;
   char buffer[256];
   c = *(parg + size);
   *(parg + size) = '\0';
   sprintf(buffer, "RESULT ARRAY RECORD %d: argc=%d; hlen=%d; size=%d; byref=%d; type=%d; rlen=%d; clen=%d; rec_len=%d", n1, argc, hlen, size, byref, type, rlen, clen, rec_len);
   mg_log_event(par, buffer);
   printf("\r\npar=%s, buffer=%s;\r\n", par, buffer);
   *(parg + size) = c;
}
*/

                  arec.vrec = parg;
                  arec.vsize = size;
#if MG_PHP_VERSION >= 70000
                  mg_array_add_record(parameter_array_d[argument_count - 2], &arec, 0); /* cmtxxx */
#else
                  mg_array_add_record(MG_AREF_PP(parameter_array[n]), &arec, 0);
#endif
                  arec.kn = 0;

                  par = parg;
                  rec_len = 0;
               }
               else {
                  arec.krec[arec.kn] = parg;
                  arec.ksize[arec.kn] = size;
                  arec.kn ++;
               }
               parg += size;
               rlen += size;
            }
         }
         if (rlen >= clen || stop)
            break;
         argc ++;
      }

      if (stop) {
         MG_ERROR1("m_merge_from_db: Bad return data");
      }
   }

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
}



ZEND_FUNCTION(m_return_to_applet)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle = 0;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_return_to_applet");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
/*
   mg_buf_cpy(p_buf, MG_EOD);
*/

    mg_buf_cpy(p_buf, "\x07", 1);

   for (n = 0; n < argument_count; n ++) {
/*
      if (n)
         mg_request_add(p_page, chndle, p_buf, MG_DLM);
      convert_to_string_ex(MG_AREF_PP(parameter_array[n]));

      mg_request_add(p_page, chndle, p_buf, MG_Z_STRVAL(MG_AREF_PP(parameter_array[n])));
*/

      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);

      mg_buf_cat(p_buf, data, len);

   }

   p_buf->p_buffer[p_buf->curr_size] = '\0';
/*
   mg_request_add(p_page, chndle, p_buf, MG_EOD);
*/

   zend_printf("%s", p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}


ZEND_FUNCTION(m_return_to_client)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif
   int chndle = 0;
   CONX con;
   LPCONX lp_connection;
   MGPAGE *p_page;

   p_page = MGG(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf);

   MG_FTRACE("m_return_to_applet");

   lp_connection = &con;

   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
/*
   mg_buf_cpy(p_buf, MG_EOD);
*/

    mg_buf_cpy(p_buf, "\x07", 1);

   for (n = 0; n < argument_count; n ++) {
/*
      if (n)
         mg_request_add(p_page, chndle, p_buf, MG_DLM);
      convert_to_string_ex(MG_AREF_PP(parameter_array[n]));

      mg_request_add(p_page, chndle, p_buf, MG_Z_STRVAL(MG_AREF_PP(parameter_array[n])));
*/
      data = mg_get_string(MG_AREF_PP(parameter_array[n]), NULL, &len);
      mg_buf_cat(p_buf, data, len);

   }

   p_buf->p_buffer[p_buf->curr_size] = '\0';

/*
   mg_request_add(p_page, chndle, p_buf, MG_EOD);
*/

   zend_printf("%s", p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}


ZEND_FUNCTION(m_array_test)
{
   int argument_count;
   zval *array;
#if MG_PHP_VERSION >= 70000
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
#else
   zval **parameter_array[MG_MAXARG];
#endif

   /* cmtxxx */

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      WRONG_PARAM_COUNT;

   array = &parameter_array_a[0];
   ZVAL_DEREF(array);
   SEPARATE_ARRAY(array);

   add_assoc_string(array, "1", "value 1");
   add_assoc_string(array, "2", "value 2");
   add_assoc_string(array, "3", "value 3");

   RETVAL_NULL();
}


#if MG_PHP_VERSION >= 70000
int mg_type(zval * item)
#else
int mg_type(zval ** item)
#endif
{
   int result;

   if (!item)
      return -1;

#if MG_PHP_VERSION >= 70000
   if (Z_TYPE_P(item) == IS_ARRAY) {
#else
   if (Z_TYPE_P(*item) == IS_ARRAY) {
#endif
      result = MG_T_LIST;
   }
   else {
      result = MG_T_STRING;
   }

/*
      result = MG_T_INTEGER;
      result = MG_T_FLOAT;
      result = MG_T_LIST;
      result = MG_T_VAR;
*/
   return result;
}


#if MG_PHP_VERSION >= 70000
int mg_get_integer(zval * item)
#else
int mg_get_integer(zval ** item)
#endif
{
   int result;
   char *ps;

   convert_to_string_ex(item);
   ps = MG_Z_STRVAL(item);
   result = (int) strtol(ps, NULL, 10);

   return result;
}


#if MG_PHP_VERSION >= 70000
double mg_get_float(zval * item)
#else
double mg_get_float(zval ** item)
#endif
{
   double result;
   char *ps;

   convert_to_string_ex(item);
   ps = MG_Z_STRVAL(item);
   result = (double) strtod(ps, NULL);

   return result;
}


#if MG_PHP_VERSION >= 70000
char * mg_get_string(zval * item, zval * item_tmp, int * size)
#else
char * mg_get_string(zval ** item, zval ** item_tmp, int * size)
#endif
{
   char * result;

   result = NULL;

   convert_to_string_ex(item);
   result = MG_Z_STRVAL(item);
   *size = (int) strlen(result);

   return result;
}



int mg_php_error(MGPAGE *p_page, char *buffer)
{
   int n;

   if (!strncmp(buffer + 5, "ce", 2)) {
      for (n = MG_RECV_HEAD; buffer[n]; n ++) {
         if (buffer[n] == '%')
            buffer[n] = '^';
      }
      MG_ERROR2(buffer + MG_RECV_HEAD);
   }
   else
      return 0;
}


/* cmtxxx */
#if MG_PHP_VERSION >= 70000
int mg_get_input_arguments(int argument_count, zval *parameter_array[])
{
   int n;

   switch (argument_count) {
      case 1:
         n = zend_parse_parameters(argument_count, "z/", &parameter_array[0]);
         break;
      case 2:
         n = zend_parse_parameters(argument_count, "z/z/", &parameter_array[0], &parameter_array[1]);
         break;
      case 3:
         n = zend_parse_parameters(argument_count, "z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2]);
         break;
      case 4:
         n = zend_parse_parameters(argument_count, "z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3]);
         break;
      case 5:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4]);
         break;
      case 6:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5]);
         break;
      case 7:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6]);
         break;
      case 8:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7]);
         break;
      case 9:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8]);
         break;
      case 10:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9]);
         break;
      case 11:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10]);
         break;
      case 12:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10], &parameter_array[11]);
         break;
      case 13:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10], &parameter_array[11], &parameter_array[12]);
         break;
      case 14:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10], &parameter_array[11], &parameter_array[12], &parameter_array[13]);
         break;
      case 15:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10], &parameter_array[11], &parameter_array[12], &parameter_array[13], &parameter_array[14]);
         break;
      case 16:
         n = zend_parse_parameters(argument_count, "z/z/z/z/z/z/z/z/z/z/z/z/z/z/z/z/", &parameter_array[0], &parameter_array[1], &parameter_array[2], &parameter_array[3], &parameter_array[4], &parameter_array[5], &parameter_array[6], &parameter_array[7], &parameter_array[8], &parameter_array[9], &parameter_array[10], &parameter_array[11], &parameter_array[12], &parameter_array[13], &parameter_array[14], &parameter_array[15]);
         break;
      default:
         n = FAILURE;
         break;
   }
   return n;
}
#endif

static const char *mg_array_lookup_string(HashTable *ht, const char *idx)
{
#if MG_PHP_VERSION >= 50500
#if MG_PHP_VERSION >= 70000
   zval *pvalue;
   zend_string *key;
#else
   zval **pvalue;
#endif
#else
   pval **pvalue;
#endif

#if MG_PHP_VERSION >= 70000
   key = zend_string_init(idx, strlen(idx), 0);
   if (ht && (pvalue = zend_hash_find(ht, key))) {
      SEPARATE_ZVAL(pvalue);
      convert_to_string_ex(pvalue);
      zend_string_release(key);
      return MG_Z_STRVAL(pvalue);
   }
#else
   if (ht && zend_hash_find(ht, (char *) idx, strlen(idx)+1, (void **) &pvalue) == SUCCESS) {
      SEPARATE_ZVAL(pvalue);
      convert_to_string(*pvalue);
      return (*pvalue)->value.str.val;
   }
#endif

   return 0;
}



#if MG_PHP_VERSION >= 70000
int mg_array_add_record(zval *ppa, MGAREC * p_arec, int mode)
#else
int mg_array_add_record(zval **ppa, MGAREC * p_arec, int mode)
#endif
{
#if MG_PHP_VERSION >= 70000
   int kn;
   unsigned long index;
	zval *pk[32];
   zval *zv = NULL;
   zval *ppk[32];
   HashTable *ht[32];

   TSRMLS_FETCH();

/*
   kn = 0;
   p = record;
   k[kn ++] = p;
   for (;;) {
      p = strstr(p, MG_AK);
      if (!p)
         break;
      *p = '\0';
      p ++;
      k[kn ++] = p;
   }

   if (kn < 2)
      return 0;

   value = k[kn - 1];
   k[kn - 1] = NULL;
*/

   if (p_arec->kn < 1)
      return 0;

   p_arec->krec[p_arec->kn] = NULL;
   for (kn = 0; kn < p_arec->kn; kn ++) {
      p_arec->kz[kn] = *(p_arec->krec[kn] + p_arec->ksize[kn]);
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = '\0';
   }
   p_arec->vz = *(p_arec->vrec + p_arec->vsize);
   *(p_arec->vrec + p_arec->vsize) = '\0';
/*
{
   char buffer[2048];
   sprintf(buffer, "kn=%d; v=%s;  key= ", p_arec->kn, p_arec->vrec);
   for (kn = 0; kn < p_arec->kn; kn ++) {
      strcat(buffer, p_arec->krec[kn]);
      strcat(buffer, ";");
   }
   zend_printf("\r\nSTART mg_array_add_record: Array record: %s", buffer);
}
*/

   for (kn = 0; p_arec->krec[kn]; kn ++) {
      p_arec->krec_ex[kn] = zend_string_init(p_arec->krec[kn], strlen(p_arec->krec[kn]), 0);
   }

   ppk[0] = ppa;
   for (kn = 0; p_arec->krec[kn]; kn ++) {
      if (!p_arec->krec[kn + 1]) {

         add_assoc_string(ppk[kn], p_arec->krec[kn], p_arec->vrec);

         break;
      }

	   ht[kn] = HASH_OF(ppk[kn]);

      zv = NULL;
      if (!strcmp(p_arec->krec[kn], "0")) {
         index = 0;
         zv = zend_hash_index_find(ht[kn], index);
         if (zv == NULL) {
            zv = zend_hash_find(ht[kn], (zend_string *) p_arec->krec_ex[kn]);
         }
      }
      else {
         index = (int) strtol(p_arec->krec[kn], NULL, 10);
         if (index != 0) {
            zv = zend_hash_index_find(ht[kn], index);
            if (zv == NULL) {
               zv = zend_hash_find(ht[kn], (zend_string *) p_arec->krec_ex[kn]);
            }
         }
         else {
            zv = zend_hash_find(ht[kn], (zend_string *) p_arec->krec_ex[kn]);
         }
      }
/*
      zend_printf("\r\n*** NEW zend_hash_find: kn=%d; ht[kn]=%p; p_arec->krec[kn]=%s; ppk[kn]=%p; zv=%p", kn, ht[kn], p_arec->krec[kn], ppk[kn], zv);
*/
      if (ht[kn] && zv != NULL) {
         ppk[kn] = zv;
         if (Z_TYPE_P(ppk[kn]) == IS_ARRAY) {
            ppk[kn + 1] = ppk[kn];
         }
         else {
/*
	         MAKE_STD_ZVAL(pk[kn + 1]);
            ZVAL_NEW_EMPTY_REF(pk[kn + 1]);
*/
            pk[kn + 1] = emalloc(sizeof(zval));
            ppk[kn + 1] = pk[kn + 1];
	         array_init(ppk[kn + 1]);
            add_assoc_zval(ppk[kn], p_arec->krec[kn], ppk[kn + 1]);
         }
	   }
      else {
         pk[kn + 1] = emalloc(sizeof(zval));
         ppk[kn + 1] = pk[kn + 1];
	      array_init(ppk[kn + 1]);
         add_assoc_zval(ppk[kn], p_arec->krec[kn], ppk[kn + 1]);
      }

   }

   for (kn = 0; p_arec->krec[kn]; kn ++) {
      zend_string_release(p_arec->krec_ex[kn]);
   }

   for (kn = 0; kn < p_arec->kn; kn ++) {
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = p_arec->kz[kn];
   }
   *(p_arec->vrec + p_arec->vsize) = p_arec->vz;

/*
	MAKE_STD_ZVAL(months);
   p_months = &months;
	array_init(*p_months);
	add_assoc_zval(*parameter_array[n], "kmonths", *p_months);
   add_assoc_string(*p_months, "month s", "nxt lev", 1);
*/
/*
   add_assoc_string(*parameter_array[n], "m_key", "m_value x", 1);
*/

   return 1;

#else

   int kn, n;
   unsigned long index;
	zval *pk[32];
   int n_ex = 0;
   zval **ppk[32];

   HashTable *ht[32];

   TSRMLS_FETCH();

/*
   kn = 0;
   p = record;
   k[kn ++] = p;
   for (;;) {
      p = strstr(p, MG_AK);
      if (!p)
         break;
      *p = '\0';
      p ++;
      k[kn ++] = p;
   }

   if (kn < 2)
      return 0;

   value = k[kn - 1];
   k[kn - 1] = NULL;
*/

   if (p_arec->kn < 1)
      return 0;

   p_arec->krec[p_arec->kn] = NULL;
   for (kn = 0; kn < p_arec->kn; kn ++) {
      p_arec->kz[kn] = *(p_arec->krec[kn] + p_arec->ksize[kn]);
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = '\0';
   }
   p_arec->vz = *(p_arec->vrec + p_arec->vsize);
   *(p_arec->vrec + p_arec->vsize) = '\0';
/*
{
   char buffer[2048];
   sprintf(buffer, "kn=%d; v=%s;  key= ", p_arec->kn, p_arec->vrec);
   for (kn = 0; kn < p_arec->kn; kn ++) {
      strcat(buffer, p_arec->krec[kn]);
      strcat(buffer, ";");
   }
   mg_log_event(buffer, "Array record");
}
*/

   ppk[0] = ppa;

   for (kn = 0; p_arec->krec[kn]; kn ++) {
      if (!p_arec->krec[kn + 1]) {
         add_assoc_string(*ppk[kn], p_arec->krec[kn], p_arec->vrec, 1);
         break;
      }
	   ht[kn] = HASH_OF(*ppk[kn]);

      if (!strcmp(p_arec->krec[kn], "0")) {
         index = 0;
         n = zend_hash_index_find(ht[kn], index, (void**) &ppk[kn]);
         if (n != SUCCESS) {
            n = zend_hash_find(ht[kn], (char *) p_arec->krec[kn], (uint) strlen(p_arec->krec[kn])+1, (void**) &ppk[kn]);
         }
      }
      else {
         index = (int) strtol(p_arec->krec[kn], NULL, 10);
         if (index != 0) {
            n = zend_hash_index_find(ht[kn], index, (void**) &ppk[kn]);
            if (n != SUCCESS) {
               n = zend_hash_find(ht[kn], (char*) p_arec->krec[kn], (uint) strlen(p_arec->krec[kn])+1, (void**) &ppk[kn]);
            }
         }
         else {
            n = zend_hash_find(ht[kn], (char *) p_arec->krec[kn], (uint) strlen(p_arec->krec[kn])+1, (void**) &ppk[kn]);
         }
      }


      if (ht[kn] && n == SUCCESS) {
         if (Z_TYPE_P(*ppk[kn]) == IS_ARRAY) {
            ppk[kn + 1] = ppk[kn];
         }
         else {
	         MAKE_STD_ZVAL(pk[kn + 1]);
            ppk[kn + 1] = &pk[kn + 1];
	         array_init(*ppk[kn + 1]);
            add_assoc_zval(*ppk[kn], p_arec->krec[kn], *ppk[kn + 1]);
         }
	   }
      else {
	         MAKE_STD_ZVAL(pk[kn + 1]);
            ppk[kn + 1] = &pk[kn + 1];
	         array_init(*ppk[kn + 1]);
            add_assoc_zval(*ppk[kn], p_arec->krec[kn], *ppk[kn + 1]);
      }
   }
/*
xxx:
*/
   for (kn = 0; kn < p_arec->kn; kn ++) {
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = p_arec->kz[kn];
   }
   *(p_arec->vrec + p_arec->vsize) = p_arec->vz;

/*
	MAKE_STD_ZVAL(months);
   p_months = &months;
	array_init(*p_months);
	add_assoc_zval(*parameter_array[n], "kmonths", *p_months);
   add_assoc_string(*p_months, "month s", "nxt lev", 1);
*/
/*
   add_assoc_string(*parameter_array[n], "m_key", "m_value x", 1);
*/

   return 1;

#endif /* #if MG_PHP_VERSION >= 70000 */

}


#if MG_PHP_VERSION >= 70000
int mg_array_parse(MGPAGE *p_page, int chndle, zval *ppa, MGBUF *p_buf, int mode, short byref)
#else
int mg_array_parse(MGPAGE *p_page, int chndle, zval **ppa, MGBUF *p_buf, int mode, short byref)
#endif
{
#if MG_PHP_VERSION >= 70000

   /* uint  str_length; */
   ulong  num_key;		
   int    n, type, ptr, len;
   zval *current;
   char  *string_key = NULL;
   char  *string_data = NULL;
   zend_string *string_key_ex = NULL;
   char *data;
   char *k[32];
   char key[8192], buf[265];
   HashTable *ht[32];
   HashPosition hp[32];

   TSRMLS_FETCH();

   mg_request_add(p_page, chndle, p_buf, NULL, 0, byref, MG_TX_AREC);

   for (ptr = 0; ptr < 32; ptr ++) {
      k[ptr] = key + (256 * ptr);
      strcpy(k[ptr], "");
   }

   ptr = 0;

	ht[ptr] = HASH_OF(ppa);
   zend_hash_internal_pointer_reset_ex(ht[ptr], &hp[ptr]);

	for (;;) {

      current = zend_hash_get_current_data_ex(ht[ptr], &hp[ptr]);
      if (current == NULL) {
         if (ptr) {
            ptr --;
            zend_hash_move_forward_ex(ht[ptr], &hp[ptr]);
            continue;
         }
         else {
            break;
         }
      }

      if (Z_TYPE_P(current) == IS_ARRAY) {
         string_key_ex = NULL;
         string_key = NULL;

	      type = zend_hash_get_current_key_ex(ht[ptr], &string_key_ex, (zend_ulong *) &num_key, &hp[ptr]);
         buf[0] = '\0';
         if (string_key_ex) {
         string_key = ZSTR_VAL(string_key_ex);
            strcpy(buf, string_key);
         }
         else {
            char num[32];
            sprintf(num, "%lu", num_key);
            strcpy(buf, num);
         }
         strcpy(k[ptr], buf);
         ptr ++;
	      ht[ptr] = HASH_OF(current);
         zend_hash_internal_pointer_reset_ex(ht[ptr], &hp[ptr]);
         continue;
      }
      else {
		   SEPARATE_ZVAL(current);
         convert_to_string_ex(current);
         string_key_ex = NULL;
         string_key = NULL;
	      type = zend_hash_get_current_key_ex(ht[ptr], &string_key_ex, (zend_ulong *) &num_key, &hp[ptr]);
         string_data = MG_Z_STRVAL(current);
         buf[0] = '\0';
         if (string_key_ex) {
         string_key = ZSTR_VAL(string_key_ex);
            if (string_data) {
               strcpy(buf, string_key);
            }
         }
         else {
            char num[32];
            sprintf(num, "%lu", num_key);
            strcpy(buf, num);
         }

         strcpy(k[ptr], buf);
         strcpy(buf, "");
         for (n = 0; n <= ptr; n ++) {
            data = k[n];
            len = (int) strlen(data);
            mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_AKEY);
         }

         if (mode == 1)
            zend_printf("<br>%s = %s", buf, string_data);

         if (string_data) {
            data = string_data;
            len = (int) strlen(data);
            mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
         }
         zend_hash_move_forward_ex(ht[ptr], &hp[ptr]);
      }

   }

   mg_request_add(p_page, chndle, p_buf, NULL, 0, byref, MG_TX_EOD);

   return 1;

#else

   uint  str_length;
   ulong  num_key;		
   int    n, type, ptr, len;
   zval **current;
   char  *string_key = NULL;
   char  *string_data = NULL;
   char *data;
   char *k[32];
   char key[8192], buf[265];
   HashTable *ht[32];
   HashPosition hp[32];

   TSRMLS_FETCH();

   mg_request_add(p_page, chndle, p_buf, NULL, 0, byref, MG_TX_AREC);

   for (ptr = 0; ptr < 32; ptr ++) {
      k[ptr] = key + (256 * ptr);
      strcpy(k[ptr], "");
   }
   ptr = 0;

	ht[ptr] = HASH_OF(*ppa);

   zend_hash_internal_pointer_reset_ex(ht[ptr], &hp[ptr]);

	for (;;) {

      n = zend_hash_get_current_data_ex(ht[ptr], (void **) &current, &hp[ptr]);
      if (n != SUCCESS) {
         if (ptr) {
            ptr --;
            zend_hash_move_forward_ex(ht[ptr], &hp[ptr]);
            continue;
         }
         else {
            break;
         }
      }
      if (Z_TYPE_P(*current) == IS_ARRAY) {

         string_key = NULL;

	      type = zend_hash_get_current_key_ex(ht[ptr], &string_key, &str_length, &num_key, 0, &hp[ptr]);

         if (string_key)
            strcpy(buf, string_key);
         else {
            char num[32];
            sprintf(num, "%lu", num_key);
            strcpy(buf, num);
         }
         strcpy(k[ptr], buf);
         ptr ++;
	      ht[ptr] = HASH_OF(*current);
         zend_hash_internal_pointer_reset_ex(ht[ptr], &hp[ptr]);
         continue;
      }
      else {
		   SEPARATE_ZVAL(current);
         convert_to_string_ex(current);

         string_key = NULL;
	      type = zend_hash_get_current_key_ex(ht[ptr], &string_key, &str_length, &num_key, 0, &hp[ptr]);
         string_data = MG_Z_STRVAL(current);

         if (string_key)
            strcpy(buf, string_key);
         else {
            char num[32];
            sprintf(num, "%lu", num_key);
            strcpy(buf, num);
         }
         strcpy(k[ptr], buf);
         strcpy(buf, "");
         for (n = 0; n <= ptr; n ++) {

            data = k[n];
            len = (int) strlen(data);
            mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_AKEY);

         }

         if (mode == 1)
            zend_printf("<br>%s = %s", buf, string_data);

         if (string_data) {
            data = string_data;
            len = strlen(data);
            mg_request_add(p_page, chndle, p_buf, data, len, byref, MG_TX_DATA);
         }
         zend_hash_move_forward_ex(ht[ptr], &hp[ptr]);
      }

   }

   mg_request_add(p_page, chndle, p_buf, NULL, 0, byref, MG_TX_EOD);

   return 1;

#endif /* #if MG_PHP_VERSION >= 70000 */

}



int mg_open_connection(MGPAGE *p_page, int *p_chndle, short context)
{
   int n, free;
   LPCONX lp_connection;
   struct sockaddr_in cli_addr, srv_addr;

/*
   static struct sockaddr_in cli_addr, srv_addr;
*/

/*
{
   char buffer[256];
   sprintf(buffer, "p_buf->size=%d, p_buf->curr_size=%d, len=%d %s --- %d", p_buf->size, p_buf->curr_size, len, p_buf->p_buffer, strlen(p_buf->p_buffer));
   mg_log_event(buffer, "cmcmcm : mg_request_add");
}
*/

   p_page->error_mess[0] = '\0';

   free = -1;
   *p_chndle = -1;
   for (n = 0; n < MG_MAXCON; n ++) {
      if (p_page->pcon[n]) {
         if (!p_page->pcon[n]->in_use) {
            *p_chndle = n;
            p_page->pcon[*p_chndle]->in_use = 1;
            p_page->pcon[*p_chndle]->eod = 0;
            break;
         }
      }
      else {
         if (free == -1)
            free = n;
      }
   }

   if (*p_chndle != -1) {
      return 1;
   }

   if (free == -1)
      return 0;

   *p_chndle = free;
   p_page->pcon[*p_chndle] = (LPCONX) mg_malloc(sizeof(CONX));
   p_page->pcon[*p_chndle]->in_use = 1;
   p_page->pcon[*p_chndle]->keep_alive = 0;

   lp_connection = p_page->pcon[*p_chndle];
   strcpy(lp_connection->ip_address, MG_IPA);
   lp_connection->port = MG_PORT;


   lp_connection->eod = 0;
   strcpy(lp_connection->error, "");

#ifdef _WIN32
   VersionRequested = 0x101;
   n = WSAStartup(VersionRequested, &(lp_connection->wsadata));
   if (n != 0) {
      sprintf(lp_connection->error, "mg_php v%s: Microsoft WSAStartup Failed: error=%ld", MG_VERSION, mg_get_last_error(0));
      strcpy(p_page->error_mess, lp_connection->error);
      return 0;
   }
#endif

#ifndef _WIN32
   bzero((char *) &srv_addr, sizeof(srv_addr));
#endif

   srv_addr.sin_port = htons((unsigned short) lp_connection->port);
   srv_addr.sin_family = AF_INET;
   srv_addr.sin_addr.s_addr = inet_addr(lp_connection->ip_address);

   lp_connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (lp_connection->sockfd < 0) {
      sprintf(lp_connection->error, "mg_php v%s: Cannot open a connection to MGSI: error=%ld",  MG_VERSION, mg_get_last_error(0));
      strcpy(p_page->error_mess, lp_connection->error);

#ifdef _WIN32
      WSACleanup();
#endif

      return 0;
   }

#ifndef _WIN32
   bzero((char *) &cli_addr, sizeof(cli_addr));
#endif

   cli_addr.sin_family = AF_INET;
   cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   cli_addr.sin_port = htons(0);

   n = bind(lp_connection->sockfd, (struct sockaddr *) &cli_addr, sizeof(cli_addr));

#ifdef _WIN32
   if (n < 0) {
      sprintf(lp_connection->error, "mg_php v%s: Cannot bind to local address for MGSI access: error=%ld",  MG_VERSION, mg_get_last_error(0));
      strcpy(p_page->error_mess, lp_connection->error);
      mg_close_connection(p_page, *p_chndle, 0);
      return 0;
   }
#endif

   n = connect(lp_connection->sockfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr));

   if (n < 0) {
      sprintf(lp_connection->error, "mg_php v%s: Cannot connect to MGSI: error=%ld",  MG_VERSION, mg_get_last_error(0));
      strcpy(p_page->error_mess, lp_connection->error);
      mg_close_connection(p_page, *p_chndle, 0);
      return 0;
   }

   return 1;
}


int mg_close_connection(MGPAGE *p_page, int chndle, short context)
{
   LPCONX lp_connection;

   if (!p_page->pcon[chndle])
      return 0;

   if (context == 1 && p_page->pcon[chndle]->keep_alive) {
      p_page->pcon[chndle]->in_use = 0;
      return 1;
   }

   lp_connection = p_page->pcon[chndle];

#ifdef _WIN32

{
struct linger l;
l.l_onoff = 1;
l.l_linger = 0;
setsockopt(lp_connection->sockfd, SOL_SOCKET, SO_LINGER, (const char *) &l, sizeof(l));
}

   closesocket(lp_connection->sockfd);
   WSACleanup();
#else
   close(lp_connection->sockfd);
#endif

   mg_free((void *) p_page->pcon[chndle]);
   p_page->pcon[chndle] = NULL;

   return 1;
}



int mg_request_header(MGPAGE *p_page, MGBUF *p_buf, char *command)
{
   char buffer[256];
/*
   sprintf(buffer, "PHPz^P^%s#%s#0###%s#%d^%s^00000\n", p_page->server, p_page->uci, MG_VERSION, p_page->storage_mode, command);
*/

   sprintf(buffer, "PHPz^P^%s#%s#0#%d#%d#%s#%d^%s^00000\n", p_page->server, p_page->uci, p_page->timeout, p_page->no_retry, MG_VERSION, p_page->storage_mode, command);
/*
   printf("\r\n header=%s;", buffer);
*/
   p_page->header_len = (int) strlen(buffer);

#ifdef MGSI
   mg_buf_cpy(p_buf, buffer, (unsigned long) strlen(buffer));
#else
   strcpy(buffer, "GET /scripts/mgms32.dll?MGLPN=LOCAL&MGPHP=");
   mg_buf_cpy(p_buf, buffer, strlen(buffer));
#endif

   return 1;
}


int mg_request_add(MGPAGE *p_page, int chndle, MGBUF *p_buf, unsigned char *element, int size, short byref, short type)
{
   int hlen;
   unsigned char head[16];

   if (type == MG_TX_AREC_FORMATTED) {
      mg_buf_cat(p_buf, element, size);
      return 1;
   }
   hlen = mg_encode_item_header(head, size, byref, type);
   mg_buf_cat(p_buf, head, hlen);
   if (size) {
      mg_buf_cat(p_buf, element, size);
   }
   return 1;
}


int mg_pow(int n10, int power)
{
#ifdef _WIN32
   return (int) pow((double) n10, (double) power);
#else
   int n, result;
   if (power == 0)
      return 1;
   result = 1;
   for (n = 1; n <= power; n ++)
      result = result * n10;
   return result;
#endif
}

int mg_encode_size64(int n10)
{
   if (n10 >= 0 && n10 < 10)
      return (48 + n10);
   if (n10 >= 10 && n10 < 36)
      return (65 + (n10 - 10));
   if (n10 >= 36 && n10 < 62)
      return  (97 + (n10 - 36));

   return 0;
}


int mg_decode_size64(int nxx)
{
   if (nxx >= 48 && nxx < 58)
      return (nxx - 48);
   if (nxx >= 65 && nxx < 91)
      return ((nxx - 65) + 10);
   if (nxx >= 97 && nxx < 123)
      return ((nxx - 97) + 36);

   return 0;
}



int mg_encode_size(unsigned char *esize, int size, short base)
{
   if (base == 10) {
      sprintf(esize, "%d", size);
      return (int) strlen(esize);
   }
   else {
      int n, n1, x;
      char buffer[32];

      n1 = 31;
      buffer[n1 --] = '\0';
      buffer[n1 --] = mg_encode_size64(size  % base);

      for (n = 1;; n ++) {
         x = (size / mg_pow(base, n));
         if (!x)
            break;
         buffer[n1 --] = mg_encode_size64(x  % base);
      }
      n1 ++;
      strcpy((char *) esize, buffer + n1);
      return (int) strlen(esize);
   }
}


int mg_decode_size(unsigned char *esize, int len, short base)
{
   int size;
   unsigned char c;

   if (base == 10) {
      c = *(esize + len);
      *(esize + len) = '\0';
      size = (int) strtol(esize, NULL, 10);
      *(esize + len) = c;
   }
   else {
      int n, x;

      size = 0;
      for (n = len - 1; n >= 0; n --) {

         x = (int) esize[n];
         size = size + mg_decode_size64(x) * mg_pow(base, (len - (n + 1)));
      }
   }

   return size;
}


int mg_encode_item_header(unsigned char * head, int size, short byref, short type)
{
   int slen, hlen;
   unsigned int code;
   unsigned char esize[16];

   slen = mg_encode_size(esize, size, 10);

   code = slen + (type * 8) + (byref * 64);
   head[0] = (unsigned char) code;
   strncpy(head + 1, esize, slen);

   hlen = slen + 1;
   head[hlen] = '0';

   return hlen;
}


int mg_decode_item_header(unsigned char * head, int * size, short * byref, short * type)
{
   int slen, hlen;
   unsigned int code;

   code = (unsigned int) head[0];

   *byref = code / 64;
   *type = (code % 64) / 8;
   slen = code % 8;

   *size = mg_decode_size(head + 1, slen, 10);

   hlen = slen + 1;

   return hlen;
}


int mg_send_request(MGPAGE *p_page, int chndle, MGBUF *p_buf, int mode)
{
   int result, n, n1, lenx;
   unsigned long len, total;
   char *request;
   unsigned char esize[8];
   LPCONX lp_connection;

   result = 1;

   lp_connection = p_page->pcon[chndle];

   lp_connection->eod = 0;
/*
   if (mode) {
#ifdef MGSI
      mg_buf_cat(p_buf, MG_EOD);
#else
      mg_buf_cat(p_buf, " HTTP/1.0\r\n\r\n");
#endif
   }
*/

   if (mode) {
#ifdef MGSI
      len = mg_encode_size(esize, p_buf->curr_size - p_page->header_len, MG_CHUNK_SIZE_BASE);
      strncpy(p_buf->p_buffer + (p_page->header_len - 6) + (5 - len), esize, len);
#else
      mg_buf_cat(p_buf, " HTTP/1.0\r\n\r\n", 13);
#endif
   }

   request = p_buf->p_buffer;
   len = p_buf->curr_size;

   total = 0;

   n1= 0;
   for (;;) {

      if ((len - total) > 8192)
         lenx = 8192;
      else
         lenx = (len - total);

      n = send(lp_connection->sockfd, request + total, lenx, 0);
      if (n < 0) {
         sprintf(lp_connection->error, "mg_php v%s: Cannot send request to MGSI daemon: error=%ld",  MG_VERSION, mg_get_last_error(0));
         strcpy(p_page->error_mess, lp_connection->error);
         result = 0;
         break;
      }

      total += n;

      if (total == len) {
         break;
      }

      n1 ++;
      if (n1 > 100000)
         break;

   }

   return result;
}


int mg_read_response(MGPAGE *p_page, int chndle, MGBUF *p_buf, int size, int mode)
{
   int result, n;
   unsigned long len, total, ssize;
   char s_buffer[16], stype[4];
   char *p;
   LPCONX lp_connection;

   lp_connection = p_page->pcon[chndle];
/*
p_buf->p_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(char) * (128000 + 32));
p_buf->size = 128000;
*/

   p = NULL;
   result = 0;
   ssize = 0;
   s_buffer[0] = '\0';
   p_buf->p_buffer[0] = '\0';
   p_buf->curr_size = 0;

   if (lp_connection->eod) {
      lp_connection->eod = 0;
      return 0;
   }
   lp_connection->eod = 0;

   if (mode) { /* Stream mode: rip just header off first */
      total = 8;
      len = 0;
      for (;;) {
         n = recv(lp_connection->sockfd, p_buf->p_buffer + len, total - len, 0);
         if (n < 1) {
            len = n;
            break;
         }
         len += n;
         if (len == total)
            break;
      }

      if (len != total) {
         return 0;
      }
      ssize = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);

      stype[0] = p_buf->p_buffer[5];
      stype[1] = p_buf->p_buffer[6];
      stype[2] = '\0';
/*
{
   char buffer[256];
   sprintf(buffer, "ssize=%d; total=%d; stype=%s", ssize, total, stype);
   mg_log_event(buffer, "Read Section: STREAM MODE");
}
*/

      if (!ssize) {
         return 0;
      }
      total = ssize + MG_RECV_HEAD;

   }
   else {
      len = 0;
      total = p_buf->size;
   }
/*
   p_buf->p_buffer[0] = '\0';
   p_buf->curr_size = 0;
*/
   for (;;) {

      n = recv(lp_connection->sockfd, p_buf->p_buffer + len, total - len, 0);

      if (n < 0) {
         result = len;
         lp_connection->eod = 1;
         sprintf(lp_connection->error, "mg_php v%s: Cannot read response from MGSI daemon (1): error=%ld",  MG_VERSION, mg_get_last_error(0));
         strcpy(p_page->error_mess, lp_connection->error);
         break;
      }
      if (n < 1) {
         sprintf(lp_connection->error, "mg_php v%s: Cannot read response from MGSI daemon (2): error=%ld",  MG_VERSION, mg_get_last_error(0));
         strcpy(p_page->error_mess, lp_connection->error);
         result = len;
         lp_connection->eod = 1;
         break;
      }

      len += n;
      p_buf->curr_size += n;
      p_buf->p_buffer[len] = '\0';
      result = len;

      if (!ssize && p_buf->curr_size >= MG_RECV_HEAD) {
         ssize = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);

         stype[0] = p_buf->p_buffer[5];
         stype[1] = p_buf->p_buffer[6];
         stype[2] = '\0';
         total = ssize + MG_RECV_HEAD;
/*
{
   char buffer[256];
   sprintf(buffer, "ssize=%d; total=%d; stype=%s", ssize, total, stype);
   mg_log_event(buffer, "Read Section");
}
*/
         if (ssize && (p_buf->size < total)) {
            if (!mg_buf_resize(p_buf, ssize + MG_RECV_HEAD + 32)) {
               p_page->mem_error = 1;
               break;
            }
         }
      }
      if (!ssize || len >= total) {
         p_buf->p_buffer[len] = '\0';
         result = len;
         if (!mode)
            lp_connection->eod = 1;
         lp_connection->keep_alive = 1;

         break;
      }

   }
/*
if (mode) {
   char buffer[256];
   sprintf(buffer, "result=%d; data=%s", result, p_buf->p_buffer);
   mg_log_event(buffer, "Read Section: STREAM MODE DATA");
}
*/

   return result;
}


int mg_ucase(char *string)
{
#if defined(_WIN32) && defined(_UNICODE)

   CharUpperA(string);
   return 1;

#else

   int n, chr;

   n = 0;
   while (string[n] != '\0') {
      chr = (int) string[n];
      if (chr >= 97 && chr <= 122)
         string[n] = (char) (chr - 32);
      n ++;
   }
   return 1;

#endif
}


int mg_lcase(char *string)
{
#if defined(_WIN32) && defined(_UNICODE)

   CharLowerA(string);
   return 1;

#else

   int n, chr;

   n = 0;
   while (string[n] != '\0') {
      chr = (int) string[n];
      if (chr >= 65 && chr <= 90)
         string[n] = (char) (chr + 32);
      n ++;
   }
   return 1;

#endif
}


int mg_buf_init(MGBUF *p_buf)
{
   p_buf->p_buffer = p_buf->buffer;
   p_buf->size = MG_BUF - 20;
   p_buf->curr_size = 0;
   p_buf->p_buffer[0] = '\0';

   return 1;
}


int mg_buf_resize(MGBUF *p_buf, unsigned long size)
{
   if (size < MG_BUF)
      return 1;

   if (size < p_buf->size)
      return 1;

   if (p_buf->p_buffer == p_buf->buffer) {
      p_buf->p_buffer = (char *) mg_malloc(sizeof(char) * size);
      strncpy(p_buf->p_buffer, p_buf->buffer, p_buf->curr_size);
      p_buf->p_buffer[p_buf->curr_size] = '\0';
      p_buf->size = size;

/*
{
   char buffer[256];
   sprintf(buffer, "MG_BUF=%d, p_buf->size=%lu, p_buf->curr_size=%lu, p_buf->p_buffer=%p", MG_BUF, p_buf->size, p_buf->curr_size, p_buf->p_buffer);
   mg_log_event(buffer, "cmcmcm : mg_buf_resize 1");
}
*/

   }
   else {
      p_buf->p_buffer = (char *) mg_realloc((void *) p_buf->p_buffer, sizeof(char) * size);
      p_buf->size = size;
/*
{
   char buffer[256];
   sprintf(buffer, "p_buf->size=%lu, p_buf->curr_size=%lu, p_buf->p_buffer=%p", p_buf->size, p_buf->curr_size, p_buf->p_buffer);
   mg_log_event(buffer, "cmcmcm : mg_buf_resize 2");
}
*/

   }

   return 1;
}


int mg_buf_free(MGBUF *p_buf)
{
   if (!p_buf)
      return 0;

   if (p_buf->p_buffer != p_buf->buffer)
      mg_free((void *) p_buf->p_buffer);

   mg_buf_init(p_buf);

   return 1;
}


int mg_buf_cpy(MGBUF *p_buf, char *buffer, unsigned long size)
{
   unsigned long mem_req, old_size;

   mem_req = p_buf->curr_size + size + 1;
/*
{
   char buffer[256];
   sprintf(buffer, "p_buf->size=%d, p_buf->curr_size=%d, mem_req=%d; p_buf->p_buffer=%p; p_buf->buffer=%p;", p_buf->size, p_buf->curr_size, mem_req, p_buf->p_buffer, p_buf->buffer);
   mg_log_event(buffer, "cmcmcm : mg_buf_cat");
}
*/
   if (p_buf->size < mem_req) {
      old_size = p_buf->size;
      while (old_size < mem_req) {
         mem_req += MG_BUF;
      }
      if (!mg_buf_resize(p_buf, mem_req)) {
         return p_buf->curr_size;
      }
   }

   memcpy((void *) p_buf->p_buffer, (void *) buffer, size);
   p_buf->curr_size = size;
   return p_buf->curr_size;
}


int mg_buf_cat(MGBUF *p_buf, char *buffer, unsigned long size)
{
   unsigned long mem_req, new_size;

   mem_req = p_buf->curr_size + size + 1;

/*
{
   char buffer[256];
   sprintf(buffer, "MG_BUF=%u, p_buf->size=%lu, p_buf->curr_size=%lu, mem_req=%lu; p_buf->p_buffer=%p; p_buf->buffer=%p;", MG_BUF, p_buf->size, p_buf->curr_size, mem_req, p_buf->p_buffer, p_buf->buffer);
   mg_log_event(buffer, "cmcmcm : mg_buf_cat");
}
*/


   if (p_buf->size < mem_req) {

      new_size = p_buf->size;
      while (new_size < mem_req) {
         new_size += MG_BUF;
      }

      if (!mg_buf_resize(p_buf, new_size)) {
         return p_buf->curr_size;
      }
   }

   memcpy((void *) (p_buf->p_buffer + p_buf->curr_size), (void *) buffer, size);
   p_buf->curr_size += size;

   return p_buf->curr_size;
}


void * mg_malloc(unsigned long size)
{
   void *p;

   p = NULL;

#ifdef MG_EMALLOC
   p = emalloc(size);
#else
#ifdef _WIN32
   p = (void *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 32);
#else
   p =  malloc(size);
#endif
#endif
/*
{
   char buffer[256];
   sprintf(buffer, "p=%p; size=%ld;", p, size);
   mg_log_event(buffer, "cmcmcm : mg_malloc");
}
*/
   return p;
}


void * mg_realloc(void *p_buffer, unsigned long size)
{
   void *p;

   p = NULL;

#ifdef MG_EMALLOC
   p = erealloc((void *) p_buffer, size);
#else
#ifdef _WIN32
   p = (void *) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (LPVOID) p_buffer, size + 32);
#else
   p = realloc(p_buffer, size);
#endif
#endif
/*
{
   char buffer[256];
   sprintf(buffer, "p=%p; p_buffer=%p; size=%ld;", p, p_buffer, size);
   mg_log_event(buffer, "cmcmcm : mg_realloc");
}
*/
   return p;
}


int mg_free(void *p_buffer)
{
/*
{
   char buffer[256];
   sprintf(buffer, "p=%p;", p_buffer);
   mg_log_event(buffer, "cmcmcm : mg_free");
}
*/
#ifdef MG_EMALLOC
   efree((void *) p_buffer);
#else
#ifdef _WIN32
   HeapFree(GetProcessHeap(), 0, p_buffer);
#else
   free((void *) p_buffer);
#endif
#endif
   return 1;
}


unsigned long mg_get_last_error(int context)
{
   unsigned long error_code;

#if defined(_WIN32)
   if (context)
      error_code = (unsigned long) GetLastError();
   else 
      error_code = (unsigned long) WSAGetLastError();
#else
   error_code = (unsigned long) errno;
#endif
   return error_code;
}


int mg_pause(unsigned long msecs)
{

#if defined(_WIN32)
   Sleep((DWORD) msecs);
#else
   int secs;
   secs = msecs / 1000;
   if (secs == 0)
      secs = 1;
   sleep(secs);
#endif
   return 1;
}


int mg_log_event(char *event, char *title)
{
   int len, n;
   FILE *fp = NULL;
   char timestr[64], heading[256], buffer[2048];
   char *p_buffer;
   time_t now = 0;
#ifdef _WIN32
   HANDLE hLogfile = 0;
   DWORD dwPos = 0, dwBytesWritten = 0;
#endif

#ifdef _WIN32
__try {
#endif

   now = time(NULL);
   sprintf(timestr, "%s", ctime(&now));
   for (n = 0; timestr[n] != '\0'; n ++) {
      if ((unsigned int) timestr[n] < 32) {
         timestr[n] = '\0';
         break;
      }
   }


#ifdef _WIN32
   sprintf(heading, ">>> Time: %s; Build: %s", timestr, MG_VERSION);
#else
   sprintf(heading, ">>> PID=%lu; RN=%lu; Time: %s; Build: %s", (unsigned long) getpid(), request_no, timestr, MG_VERSION);
#endif

   len = (int) strlen(heading) + (int) strlen(title) + (int) strlen(event) + 20;

   if (len < 2000)
      p_buffer = buffer;
   else
      p_buffer = (char *) malloc(sizeof(char) * len);

   if (p_buffer == NULL)
      return 0;

   p_buffer[0] = '\0';
   strcpy(p_buffer, heading);
   strcat(p_buffer, "\r\n    ");
   strcat(p_buffer, title);
   strcat(p_buffer, "\r\n    ");
   strcat(p_buffer, event);
   len = (int) strlen(p_buffer) * sizeof(char);

#ifdef _WIN32

   strcat(p_buffer, "\r\n");
   len = len + (2 * sizeof(char));
   hLogfile = CreateFileA(MG_LOG_FILE, GENERIC_WRITE, FILE_SHARE_WRITE,
                         (LPSECURITY_ATTRIBUTES) NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL);
   dwPos = SetFilePointer(hLogfile, 0, (LPLONG) NULL, FILE_END);
   LockFile(hLogfile, dwPos, 0, dwPos + len, 0);
   WriteFile(hLogfile, (LPTSTR) p_buffer, len, &dwBytesWritten, NULL);
   UnlockFile(hLogfile, dwPos, 0, dwPos + len, 0);
   CloseHandle(hLogfile);

#else /* UNIX or VMS */

   strcat(p_buffer, "\n");
   fp = fopen(MG_LOG_FILE, "a");
   if (fp) {
      fputs(p_buffer, fp);
      fclose(fp);
   }

#endif

   if (p_buffer != buffer)
      free((void *) p_buffer);

   return 1;

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {
      return 0;
}

#endif

}


