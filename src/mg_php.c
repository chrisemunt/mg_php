/*
   ----------------------------------------------------------------------------
   | mg_php                                                                   |
   | Description: PHP Extension for M/Cache/IRIS                              |
   | Author:      Chris Munt cmunt@mgateway.com                               |
   |                         chris.e.munt@gmail.com                           |
   | Copyright (c) 2002-2023 M/Gateway Developments Ltd,                      |
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
   |                                                                          |
   ----------------------------------------------------------------------------
*/

/*
Version 1.0.14 15 November 2002:
   Initial implementation.

Version 1.1.15 16 November 2002:
   Introduce m_http() function.

Version 1.1.16 16 November 2002:
   Introduce support for multi-dimensional arrays.

Version 1.1.17 17 November 2002:
   Port to UNIX

Version 1.1.18 17 November 2002:
   Bug fixes

Version 1.1.21 18 November 2002:
   Introduce m_method() function.

Version 1.2.23 19 November 2002:
   Introduce passing input parameters by reference.

Version 1.4.24 3 February 2003:
   Introduce m_html_method() function.

Version 1.4.25 12 August 2003:
   Bug fixes.

Version 1.4.26 16 August 2004:
   Improved error trap.

Version 1.5.31 3 February 2005: 
   Large memory model; dynamic allocation.

Version 1.5.35 20 April 2005:
   Fix array error (numeric subscripts >= v5 php)

Version 1.6.36 13 September 2006:
   Fix trace buffer overrun.

Version 2.0.40 23 October 2009:
   Increase buffer sizes for response data

Version 2.0.42 26 December 2009:
   Try to reduce the impact of TIME_WAIT on close
   setsockopt(lp_connection->sockfd, SOL_SOCKET, SO_LINGER, (const char *) &l, sizeof(l));

Version 2.0.43 23 February 2010:
   Port to PHP v5.3.

Version 2.0.44 21 October 2010:
   Fix crash in m_proc if args passed by value.
   (PHP Fatal error: Exception caught in f:m_proc: c0000005|12 in C:\Inetpub\wwwroot\jk1.php on line 32)

Version 2.0.45 4 July 2014:
   Port to PHP v5.5.x

Version 2.1.46 7 August 2014:
   Introduce m_proc_byref() (etc) to cope with the withdrawal of passing arguments by reference in PHP (&$var).
   These *_byref functions will implicitly treat all arrays as 'passed by reference'.
   Increase the volume of data accepted by functions to 1MB and raise a PHP error if this limit is exceeded. Previous limit was 32K.

Version 2.1.47 18 September 2014:
   Port to PHP v5.6.x

Version 2.1.48 7 March 2016:
   Port to PHP v7.0.x

Version 2.1.49 17 March 2017:
   Fix issue with passing simple variables (non-array) by reference.

Version 2.1.50 22 april 2017:
   Mark the first and second arguments of m_proc_byref() as non-Pass-By-Reference
   Compiler warnings fixed on 14 September 2017.

Version 2.1.50 17 November 2018:
   Port to PHP v7.1.x and v7.2.x

Version 2.1.51 3 February 2019:
   Port to PHP v7.3.x

Version 2.1.52 19 September 2019:
   Remove all code relevant to pre-v7.0.0 versions of PHP.
   Introduce m_proc_ex() function.
   Implement mg_log_buffer() for recording binary data.

Version 2.1.53 4 October 2019:
   Fix a bug in the m_proc_ex() function.

Version 2.1.54 11 January 2020:
   Correct a problem that resulted in network connectivity errors not being reported to the application.
   Add an advanced logging (and debugging) facility. 

Version 2.1.55 7 February 2020:
   Port to PHP v7.4.x
   Improve the function for parsing input arguments passed as PHP arrays (mg_array_parse).
   - Remove the M 32 keys/subscripts limit (i.e. nested PHP array depth).  This is now 256 in accordance with similar functions.
   - Increase the aggregate M global key length from 256 to 512 Bytes (the limit imposed by InterSystems products).
   - Add a Windows error trap to record failures and report them as PHP errors.
   Upgrade Winsock protocol from v1.1 to v2.2.
   Add PHP-compliant function prototype and documentation text to the C source.
   - The {{{ comment syntax used by 'genfunclist' and 'genfuncsummary' scripts that are part of the PHP documentation project.
   Upgrade the Windows Winsock Protocol version to 2.2 (was 1.1).

Version 3.1.56 18 February 2020:
   Upgrade TCP functionality to use IPv6 infrastructure.
   Implement DNS look-ups for the Host.
   Converge code base with the standard mg_dba module.
   - Implement standard connectivity models: TCP to SIG; TCP to M; M API.
   Release as Open Source with the product name: 'mg_php'.

Version 3.2.57 18 February 2021:
   Introduce support for M transaction processing: tstart, $tlevel, tcommit, trollback.
   Introduce support for the M increment function.

Version 3.2.58 14 March 2021:
   Introduce support for YottaDB Transaction Processing over API based connectivity.
   - This functionality was previously only available over network-based connectivity to YottaDB.

Version 3.3.59 26 January 2023:
   Port to PHP v8.0.x, v8.1.x and v8.2.x

Version 3.3.60 26 March 2023:
   Properly terminate strings returned from the YottaDB API.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_mg_php.h"

#include "mg_dbasys.h"
#include "mg_dba.h"

#ifdef _WIN32
#define MG_LOG_FILE           "c:/temp/" MG_EXT_NAME ".log"
#else
#define MG_LOG_FILE           "/tmp/" MG_EXT_NAME ".log"
#endif

#define MG_PRODUCT            "z"

#define MG_MAXARG             32
#define MG_MAXKEY             256
#define MG_MAXKEYLEN          1024

#define MG_T_VAR              0
#define MG_T_STRING           1
#define MG_T_INTEGER          2
#define MG_T_FLOAT            3
#define MG_T_LIST             4

#ifdef ZTS
#define MG_EMALLOC            1
#endif

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

#define MG_RETURN_STRING(s, duplicate) \
   { \
      RETVAL_STRING(s); \
      return; \
   } \

#define MG_RETURN_LONG(i) \
   { \
      RETVAL_LONG(i); \
      return; \
   } \

#define MG_RETURN_STRING_AND_FREE_BUF(s, duplicate) \
   { \
      RETVAL_STRING(s); \
      mg_buf_free(p_buf); \
      return; \
   } \

#define MG_ERROR1(e) \
   if (p_page && p_page->p_log->log_errors) \
      mg_log_event(p_page->p_log, e, "Error Condition", 0); \
   if (p_page && p_page->p_srv->error_mode == 1) \
      php_error(E_USER_WARNING, "%s", e); \
   else if (p_page && p_page->p_srv->error_mode == 9) { \
      MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1); \
   } \
   else \
      php_error(E_USER_ERROR, "%s", e); \
   MG_RETURN_FALSE_AND_FREE_BUF; \


#define MG_ERROR2(e) \
   if (p_page && p_page->p_log->log_errors) \
      mg_log_event(p_page->p_log, e, "Error Condition", 0); \
   if (p_page && p_page->p_srv->error_mode == 1) \
      php_error(E_USER_WARNING, "%s", e); \
   else if (p_page && p_page->p_srv->error_mode == 9) { \
      return 2; \
   } \
   else \
      php_error(E_USER_ERROR, "%s", e); \
   return 1; \

#define MG_ERROR3(e) \
   if (p_page && p_page->p_log->log_errors) \
      mg_log_event(p_page->p_log, e, "Error Condition", 0); \


#define MG_MEMCHECK(e, c) \
   if (p_page && p_page->p_srv->mem_error == 1) { \
      if (p_page && p_page->p_log->log_errors) \
         mg_log_event(p_page->p_log, e, "Error Condition", 0); \
      mg_db_disconnect(p_page->p_srv, chndle, c); \
      if (p_page && p_page->p_srv->error_mode == 1) \
         php_error(E_USER_WARNING, e); \
      else if (p_page && p_page->p_srv->error_mode == 9) { \
         strcpy(p_page->p_srv->error_mess, e); \
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1); \
      } \
      else \
         php_error(E_USER_ERROR, e); \
      MG_RETURN_FALSE_AND_FREE_BUF; \
   } \


#if defined(_WIN32)
typedef DWORD           DBXTHID;
#else
typedef pthread_t       DBXTHID;
#endif

typedef struct tagMGPTYPE {
   short type;
   short byref;
} MGPTYPE;


typedef struct tagMGAREC {
   int            kn;
   unsigned char  kz[MG_MAXKEY];
   unsigned char  *krec[MG_MAXKEY];
   zend_string    *krec_ex[MG_MAXKEY];
   int            ksize[MG_MAXKEY];
   unsigned char  vz;
   unsigned char  *vrec;
   int            vsize;
} MGAREC;


typedef struct tagMGAKEY {
   int            kn;
   int            kmemsize[MG_MAXKEY];
   unsigned char  *krec[MG_MAXKEY];
   int            ksize[MG_MAXKEY];
   zval           *zval_subarray[MG_MAXKEY];
   zval           *zval_array;
} MGAKEY;


#define MG_MAXKEYLEN 1024

typedef struct tagMGAKEYX {
   int            kn;
   int            kmemoffs[MG_MAXKEY];
   unsigned char  *krec[MG_MAXKEY];
   int            ksize[MG_MAXKEY];
   HashTable      *ht[MG_MAXKEY];
   HashPosition   hp[MG_MAXKEY];
} MGAKEYX;


typedef struct tagMGPAGE {
   MGSRV       srv;
   MGSRV       *p_srv;
   DBXLOG      log;
   DBXLOG      *p_log;
   char        eod[4];
   char        server_base[64];
} MGPAGE;


ZEND_BEGIN_MODULE_GLOBALS(mg_php)
   unsigned long     req_no;
   unsigned long     fun_no;
   char              trace[32000];
   MGPAGE *          p_page;
ZEND_END_MODULE_GLOBALS(mg_php)

#ifdef ZTS
# define MG_PHP_GLOBAL(v) TSRMG(mg_php_globals_id, zend_mg_php_globals *, v)
#else
# define MG_PHP_GLOBAL(v) (mg_php_globals.v)
#endif

/* v3.3.59 */
#if PHP_MAJOR_VERSION >= 8

ZEND_BEGIN_ARG_INFO(m_noargs_ainfo, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_onearg_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_set_log_level_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
   ZEND_ARG_INFO(0, input3)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_set_error_mode_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_set_host_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
   ZEND_ARG_INFO(0, input3)
   ZEND_ARG_INFO(0, input4)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_bind_server_api_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
   ZEND_ARG_INFO(0, input3)
   ZEND_ARG_INFO(0, input4)
   ZEND_ARG_INFO(0, input5)
   ZEND_ARG_INFO(0, input6)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_global_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
   ZEND_ARG_INFO(0, input3)
   ZEND_ARG_INFO(0, input4)
   ZEND_ARG_INFO(0, input5)
   ZEND_ARG_INFO(0, input6)
   ZEND_ARG_INFO(0, input7)
   ZEND_ARG_INFO(0, input8)
   ZEND_ARG_INFO(0, input9)
   ZEND_ARG_INFO(0, input10)
   ZEND_ARG_INFO(0, input11)
   ZEND_ARG_INFO(0, input12)
   ZEND_ARG_INFO(0, input13)
   ZEND_ARG_INFO(0, input14)
   ZEND_ARG_INFO(0, input15)
   ZEND_ARG_INFO(0, input16)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_varargs_ainfo, 1)
   ZEND_ARG_INFO(0, input1)
   ZEND_ARG_INFO(0, input2)
   ZEND_ARG_INFO(0, input3)
   ZEND_ARG_INFO(0, input4)
   ZEND_ARG_INFO(0, input5)
   ZEND_ARG_INFO(0, input6)
   ZEND_ARG_INFO(0, input7)
   ZEND_ARG_INFO(0, input8)
   ZEND_ARG_INFO(0, input9)
   ZEND_ARG_INFO(0, input10)
   ZEND_ARG_INFO(0, input11)
   ZEND_ARG_INFO(0, input12)
   ZEND_ARG_INFO(0, input13)
   ZEND_ARG_INFO(0, input14)
   ZEND_ARG_INFO(0, input15)
   ZEND_ARG_INFO(0, input16)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_proc_byref_ainfo, 1)
   ZEND_ARG_INFO(0, input1) /* first argument can be a literal if zero */
   ZEND_ARG_INFO(0, input2) /* second argument can be a literal if zero */
   ZEND_ARG_INFO(1, input3)
   ZEND_ARG_INFO(1, input4)
   ZEND_ARG_INFO(1, input5)
   ZEND_ARG_INFO(1, input6)
   ZEND_ARG_INFO(1, input7)
   ZEND_ARG_INFO(1, input8)
   ZEND_ARG_INFO(1, input9)
   ZEND_ARG_INFO(1, input10)
   ZEND_ARG_INFO(1, input11)
   ZEND_ARG_INFO(1, input12)
   ZEND_ARG_INFO(1, input13)
   ZEND_ARG_INFO(1, input14)
   ZEND_ARG_INFO(1, input15)
   ZEND_ARG_INFO(1, input16)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_method_byref_ainfo, 1)
   ZEND_ARG_INFO(0, input1) /* first argument can be a literal if zero */
   ZEND_ARG_INFO(0, input2) /* second argument can be a literal if zero */
   ZEND_ARG_INFO(1, input3)
   ZEND_ARG_INFO(1, input4)
   ZEND_ARG_INFO(1, input5)
   ZEND_ARG_INFO(1, input6)
   ZEND_ARG_INFO(1, input7)
   ZEND_ARG_INFO(1, input8)
   ZEND_ARG_INFO(1, input9)
   ZEND_ARG_INFO(1, input10)
   ZEND_ARG_INFO(1, input11)
   ZEND_ARG_INFO(1, input12)
   ZEND_ARG_INFO(1, input13)
   ZEND_ARG_INFO(1, input14)
   ZEND_ARG_INFO(1, input15)
   ZEND_ARG_INFO(1, input16)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_array_test_ainfo, 1)
   ZEND_ARG_INFO(1, input1)
   ZEND_ARG_INFO(1, input2)
   ZEND_ARG_INFO(1, input3)
   ZEND_ARG_INFO(1, input4)
   ZEND_ARG_INFO(1, input5)
   ZEND_ARG_INFO(1, input6)
   ZEND_ARG_INFO(1, input7)
   ZEND_ARG_INFO(1, input8)
   ZEND_ARG_INFO(1, input9)
   ZEND_ARG_INFO(1, input10)
   ZEND_ARG_INFO(1, input11)
   ZEND_ARG_INFO(1, input12)
   ZEND_ARG_INFO(1, input13)
   ZEND_ARG_INFO(1, input14)
   ZEND_ARG_INFO(1, input15)
   ZEND_ARG_INFO(1, input16)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(m_merge_from_db_byref_ainfo, 1)
   ZEND_ARG_INFO(0, input1) /* first argument can be a literal if zero */
   ZEND_ARG_INFO(0, input2) /* second argument can be a literal if zero */
   ZEND_ARG_INFO(1, input3)
   ZEND_ARG_INFO(1, input4)
   ZEND_ARG_INFO(1, input5)
   ZEND_ARG_INFO(1, input6)
   ZEND_ARG_INFO(1, input7)
   ZEND_ARG_INFO(1, input8)
   ZEND_ARG_INFO(1, input9)
   ZEND_ARG_INFO(1, input10)
   ZEND_ARG_INFO(1, input11)
   ZEND_ARG_INFO(1, input12)
   ZEND_ARG_INFO(1, input13)
   ZEND_ARG_INFO(1, input14)
   ZEND_ARG_INFO(1, input15)
   ZEND_ARG_INFO(1, input16)
   ZEND_END_ARG_INFO()

static const zend_function_entry mg_functions[] =
{
    PHP_FE(m_test, m_noargs_ainfo)
    PHP_FE(m_dump_trace, m_noargs_ainfo)
    PHP_FE(m_ext_version, m_noargs_ainfo)
    PHP_FE(m_set_log_level, m_set_log_level_ainfo)
    PHP_FE(m_set_error_mode, m_set_error_mode_ainfo)
    PHP_FE(m_set_storage_mode, m_onearg_ainfo)
    PHP_FE(m_set_timeout, m_onearg_ainfo)
    PHP_FE(m_set_no_retry, m_onearg_ainfo)
    PHP_FE(m_set_host, m_set_host_ainfo)
    PHP_FE(m_set_server, m_onearg_ainfo)
    PHP_FE(m_set_uci, m_onearg_ainfo)
#if !defined(MG_PHP_MGW)
    PHP_FE(m_bind_server_api, m_bind_server_api_ainfo)
    PHP_FE(m_release_server_api, m_noargs_ainfo)
#endif
    PHP_FE(m_get_last_error, m_noargs_ainfo)
    PHP_FE(m_set, m_global_ainfo)
    PHP_FE(m_get, m_global_ainfo)
    PHP_FE(m_delete, m_global_ainfo)
    PHP_FE(m_kill, m_global_ainfo)
    PHP_FE(m_defined, m_global_ainfo)
    PHP_FE(m_data, m_global_ainfo)
    PHP_FE(m_order, m_global_ainfo)
    PHP_FE(m_previous, m_global_ainfo)
    PHP_FE(m_increment, m_global_ainfo)
    PHP_FE(m_tstart, m_onearg_ainfo)
    PHP_FE(m_tlevel, m_onearg_ainfo)
    PHP_FE(m_tcommit, m_onearg_ainfo)
    PHP_FE(m_trollback, m_onearg_ainfo)
    PHP_FE(m_sleep, m_onearg_ainfo)
    PHP_FE(m_html, m_varargs_ainfo)
    PHP_FE(m_html_method, m_varargs_ainfo)
    PHP_FE(m_http, m_varargs_ainfo)
    PHP_FE(m_function, m_varargs_ainfo)
    PHP_FE(m_proc, m_varargs_ainfo)
    PHP_FE(m_proc_ex, m_varargs_ainfo)
    PHP_FE(m_proc_byref, m_proc_byref_ainfo)
    PHP_FE(m_classmethod, m_varargs_ainfo)
    PHP_FE(m_method, m_varargs_ainfo)
    PHP_FE(m_method_byref, m_method_byref_ainfo)
    PHP_FE(m_merge_to_db, m_varargs_ainfo)
    PHP_FE(m_merge_from_db, m_merge_from_db_byref_ainfo)
    PHP_FE(m_return_to_applet, m_varargs_ainfo)
    PHP_FE(m_return_to_client, m_varargs_ainfo)
    PHP_FE(m_array_test, m_array_test_ainfo)
    {NULL, NULL, NULL}
};

#else

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

static const zend_function_entry mg_functions[] =
{
    PHP_FE(m_test, NULL)
    PHP_FE(m_dump_trace, NULL)
    PHP_FE(m_ext_version, NULL)
    PHP_FE(m_set_log_level, NULL)
    PHP_FE(m_set_error_mode, NULL)
    PHP_FE(m_set_storage_mode, NULL)
    PHP_FE(m_set_timeout, NULL)
    PHP_FE(m_set_no_retry, NULL)
    PHP_FE(m_set_host, NULL)
    PHP_FE(m_set_server, NULL)
    PHP_FE(m_set_uci, NULL)
#if !defined(MG_PHP_MGW)
    PHP_FE(m_bind_server_api, NULL)
    PHP_FE(m_release_server_api, NULL)
#endif
    PHP_FE(m_get_last_error, NULL)
    PHP_FE(m_set, NULL)
    PHP_FE(m_get, NULL)
    PHP_FE(m_delete, NULL)
    PHP_FE(m_kill, NULL)
    PHP_FE(m_defined, NULL)
    PHP_FE(m_data, NULL)
    PHP_FE(m_order, NULL)
    PHP_FE(m_previous, NULL)
    PHP_FE(m_increment, NULL)
    PHP_FE(m_tstart, NULL)
    PHP_FE(m_tlevel, NULL)
    PHP_FE(m_tcommit, NULL)
    PHP_FE(m_trollback, NULL)
    PHP_FE(m_sleep, NULL)
    PHP_FE(m_html, NULL)
    PHP_FE(m_html_method, NULL)
    PHP_FE(m_http, NULL)
    PHP_FE(m_function, NULL)
    PHP_FE(m_proc, NULL)
    PHP_FE(m_proc_ex, NULL)
    PHP_FE(m_proc_byref, m_proc_byref_ainfo)
    PHP_FE(m_classmethod, NULL)
    PHP_FE(m_method, NULL)
    PHP_FE(m_method_byref, m_method_byref_ainfo)
    PHP_FE(m_merge_to_db, NULL)
    PHP_FE(m_merge_from_db, m_merge_from_db_byref_ainfo)
    PHP_FE(m_return_to_applet, NULL)
    PHP_FE(m_return_to_client, NULL)
    PHP_FE(m_array_test, m_array_test_ainfo)
    {NULL, NULL, NULL}
};

#endif /* #if PHP_MAJOR_VERSION >= 8 */


/* compiled module information */
zend_module_entry mg_php_module_entry =
{
   STANDARD_MODULE_HEADER,
   MG_EXT_NAME,
   mg_functions,
	PHP_MINIT(mg_php),
	PHP_MSHUTDOWN(mg_php),
	PHP_RINIT(mg_php),
	PHP_RSHUTDOWN(mg_php),
	PHP_MINFO(mg_php),
   NO_VERSION_YET,
   STANDARD_MODULE_PROPERTIES
};


ZEND_DECLARE_MODULE_GLOBALS(mg_php)

/* implement standard "stub" routine to introduce ourselves to Zend */
#if defined(COMPILE_DL_MG_PHP)
ZEND_GET_MODULE(mg_php)
#endif


static unsigned long request_no  = 0;
static char minit[256]           = {'\0'};

int                  mg_type                    (zval *item);
int                  mg_get_integer             (zval *item);
double               mg_get_float               (zval *item);
char *               mg_get_string              (zval *item, zval *item_tmp, int *size);
int                  mg_php_error               (MGPAGE *p_page, char *buffer);
int                  mg_get_input_arguments     (int argument_count, zval *parameter_array[]);
static const char *  mg_array_lookup_string     (HashTable *ht, const char *idx);
int                  mg_array_add_record        (zval *ppa, MGAREC *arec, int mode);
int                  mg_akey_init               (MGAKEY *p_akey);
int                  mg_akey_free               (MGAKEY *p_akey);
zval *               mg_array_subarray_zval     (MGAKEY *p_akey, MGAREC *p_arec);
int                  mg_array_terminate_strings (MGAREC *p_arec);
int                  mg_array_reset_strings     (MGAREC *p_arec);
int                  mg_array_parse             (MGPAGE *p_page, int chndle, zval *ppa, MGBUF *p_buf, int mode, short byref);
int                  mg_request_header_ex       (MGPAGE *p_page, MGBUF *p_buf, char *command, char *product, zval *parg0);
void *               mg_ext_malloc              (unsigned long size);
void *               mg_ext_realloc             (void *p_buffer, unsigned long size);
int                  mg_ext_free                (void *p_buffer);
int                  mg_log_request             (MGPAGE *p_page, char *function);


#if defined(_WIN32) && defined(COMPILE_DL_MG_PHP)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
   switch (fdwReason)
   { 
      case DLL_PROCESS_ATTACH:
         mg_init_critical_section((void *) &dbx_global_mutex);
         break;
      case DLL_THREAD_ATTACH:
         break;
      case DLL_THREAD_DETACH:
         break;
      case DLL_PROCESS_DETACH:
         mg_delete_critical_section((void *) &dbx_global_mutex);
         break;
   }
   return TRUE;
}
#endif


/**
 * php_mg_php_init_globals
 */
static void php_mg_php_init_globals(zend_mg_php_globals *mg_php_globals)
{
	mg_php_globals->req_no = 0;
	mg_php_globals->fun_no = 0;
   *(mg_php_globals->trace) = '\0';
   mg_php_globals->p_page = NULL;

}


/**
 * PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mg_php)
{
   int n;
   time_t now;
   char buffer[256];

#if defined(_WIN32) && !defined(COMPILE_DL_MG_PHP)
   InitializeCriticalSection(&dbx_global_mutex);
#endif

#ifdef MG_EMALLOC
   dbx_ext_malloc = (MG_MALLOC) mg_ext_malloc;
   dbx_ext_realloc = (MG_REALLOC) mg_ext_realloc;
   dbx_ext_free = (MG_FREE) mg_ext_free;
#endif

   ZEND_INIT_MODULE_GLOBALS(mg_php, php_mg_php_init_globals, NULL);

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

   dbx_init();

	return SUCCESS;
}


/**
 * PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mg_php)
{

#if defined(_WIN32) && !defined(COMPILE_DL_MG_PHP)
   DeleteCriticalSection(&dbx_global_mutex);
#endif
	return SUCCESS;
}


/**
 * PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mg_php)
{
   int n;
   unsigned long req_no;

   mg_enter_critical_section((void *) &dbx_global_mutex);
   request_no ++;
   req_no = request_no;
   mg_leave_critical_section((void *) &dbx_global_mutex);

   MG_PHP_GLOBAL(req_no) = req_no;

   MG_PHP_GLOBAL(fun_no) = 0;
   strcpy(MG_PHP_GLOBAL(trace), minit);

   MG_PHP_GLOBAL(p_page) = (MGPAGE *) mg_malloc(sizeof(MGPAGE), 0);

   MG_PHP_GLOBAL(p_page)->p_srv = &(MG_PHP_GLOBAL(p_page)->srv);
   MG_PHP_GLOBAL(p_page)->p_log = &(MG_PHP_GLOBAL(p_page)->log);

   mg_log_init(MG_PHP_GLOBAL(p_page)->p_log);
   strcpy(MG_PHP_GLOBAL(p_page)->p_log->log_file, MG_LOG_FILE);
   strcpy(MG_PHP_GLOBAL(p_page)->p_log->product, MG_PRODUCT);
   strcpy(MG_PHP_GLOBAL(p_page)->p_log->product_version, PHP_MG_PHP_VERSION);

   MG_PHP_GLOBAL(p_page)->p_srv->mem_error = 0;
   MG_PHP_GLOBAL(p_page)->p_srv->storage_mode = 0;
   MG_PHP_GLOBAL(p_page)->p_srv->timeout = 0;
   MG_PHP_GLOBAL(p_page)->p_srv->no_retry = 0;

   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->ip_address, MG_HOST);
   MG_PHP_GLOBAL(p_page)->p_srv->port = MG_DEFAULT_PORT;
   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->server, MG_SERVER);
   strcpy(MG_PHP_GLOBAL(p_page)->server_base, MG_SERVER);
   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->uci, MG_UCI);

   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->username, "");
   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->password, "");

   strcpy(MG_PHP_GLOBAL(p_page)->p_srv->product, MG_PRODUCT);

   for (n = 0; n < MG_MAXCON; n ++) {
      MG_PHP_GLOBAL(p_page)->p_srv->pcon[n] = NULL;
   }

	return SUCCESS;
}


/**
 * PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mg_php)
{
   int n;

   if (MG_PHP_GLOBAL(p_page) != NULL) {

      for (n = 0; n < MG_MAXCON; n ++) {
         if (MG_PHP_GLOBAL(p_page)->p_srv->pcon[n] != NULL) {
            mg_db_disconnect(MG_PHP_GLOBAL(p_page)->p_srv, n, 0);
         }
      }
      mg_free((void *) MG_PHP_GLOBAL(p_page), 0);
   }

	return SUCCESS;
}


/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mg_php)
{

	php_info_print_table_start();

   php_info_print_table_header(2, "Property", "Value");

	php_info_print_table_row(2, "Version", PHP_MG_PHP_VERSION);

	php_info_print_table_end();

}


/* {{{ proto mixed m_test(mixed data...)
   Internal test function */
ZEND_FUNCTION(m_test)
{
   char test[64];

   strcpy(test, "test");

   MG_RETURN_STRING(test, 1);
}
/* }}} */


/* {{{ proto string m_dump_trace()
   Dump the trace file to the client */
ZEND_FUNCTION(m_dump_trace)
{
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   mg_log_request(p_page, "m_dump_trace");

   zend_write(MG_PHP_GLOBAL(trace), (unsigned int) strlen(MG_PHP_GLOBAL(trace)));

   MG_RETURN_STRING(MG_PHP_GLOBAL(trace), 1);
}
/* }}} */


/* {{{ proto string m_ext_version()
   Return the version of mg_php */
ZEND_FUNCTION(m_ext_version)
{
   long parameter = 0;
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
   zval parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle = 0;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_ext_version");

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

      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);

   }

   sprintf(p_buf->p_buffer, "M/Gateway Developments Ltd. - " MG_EXT_NAME ": PHP Gateway to M - Version %s", PHP_MG_PHP_VERSION);
   p_buf->data_size = (unsigned long) strlen(p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}
/* }}} */


/* {{{ proto bool m_set_log_level(string filename, string loglevel, string logfilter)
   Enable logging */
ZEND_FUNCTION(m_set_log_level)
{
   char buffer[1024];
   int argument_count, n;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   p_page->p_srv->error_mode = 0;
   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (3 arguments) */
   if (argument_count < 3)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 120);
   buffer[120] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_log->log_file, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[1]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[1]), Z_STRLEN_P(&parameter_array[1])), 7);
   buffer[7] = '\0';
   if (buffer[0]) {
      mg_lcase(buffer);
      strcpy(p_page->p_log->log_level, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[2]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[2]), Z_STRLEN_P(&parameter_array[2])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      mg_lcase(buffer);
      strcpy(p_page->p_log->log_filter, ",");
      strcat(p_page->p_log->log_filter, buffer);
      strcat(p_page->p_log->log_filter, ",");
   }

   mg_log_request(p_page, "m_set_log_level");

   if (p_page->p_log->log_level[0]) {
      sprintf(buffer, "Log File: %s; Log Level: %s; Log Filter: %s;", p_page->p_log->log_file, p_page->p_log->log_level, p_page->p_log->log_filter);
      mg_log_event(p_page->p_log, buffer, "Logging Active", 0);
   }

   MG_RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool m_set_error_mode(int errormode[, string errocode])
   Set the error mode */
ZEND_FUNCTION(m_set_error_mode)
{
   char buffer[128];
   int argument_count, n, mode;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_error_mode");

   p_page->p_srv->error_mode = 0;
   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


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
      convert_to_string_ex(&(parameter_array[n]));
      mg_log_event(p_page, (char *) &(parameter_array[n]), "cmcmcm0");
   }
*/

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 100);
   buffer[100] = '\0';
   mode = (int) strtol(buffer, NULL, 10);

   if (mode == 0) {
      p_page->p_srv->error_mode = 0;
      strcpy(p_page->p_srv->error_code, "");
      strcpy(p_page->p_srv->error_mess, "");
   }
   else if (mode == 1) {
      p_page->p_srv->error_mode = 1;
      strcpy(p_page->p_srv->error_code, "");
      strcpy(p_page->p_srv->error_mess, "");
   }
   else if (mode == 9) {
      p_page->p_srv->error_mode = 9;
      strcpy(p_page->p_srv->error_code, "-1");
      strcpy(p_page->p_srv->error_mess, "");

      if (argument_count > 1) {
         convert_to_string_ex(&(parameter_array[1]));
         strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[1]), Z_STRLEN_P(&parameter_array[1])), 100);

         buffer[100] = '\0';
         strcpy(p_page->p_srv->error_code, buffer);
      }
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;

}
/* }}} */


/* {{{ proto bool m_set_storage_mode(int storagemode)
   Set the storage mode */
ZEND_FUNCTION(m_set_storage_mode)
{
   char buffer[128];
   int argument_count, n, mode;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_storage_mode");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 100);
   buffer[100] = '\0';
   mode = (int) strtol(buffer, NULL, 10);

   if (mode == 0) {
      p_page->p_srv->storage_mode = 0;
   }
   else if (mode == 1) {
      p_page->p_srv->storage_mode = 1;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool m_set_timeout(int timeout)
   Set the request timeout */
ZEND_FUNCTION(m_set_timeout)
{
   char buffer[128];
   int argument_count, n, timeout;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_timeout");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 100);
   buffer[100] = '\0';
   timeout = (int) strtol(buffer, NULL, 10);

   if (timeout >= 0) {
      p_page->p_srv->timeout = timeout;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool m_set_no_retry(int noretryflag)
   Set the request 'no retry' flag */
ZEND_FUNCTION(m_set_no_retry)
{
   char buffer[128];
   int argument_count, n, no_retry;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_no_retry");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 100);
   buffer[100] = '\0';
   no_retry = (int) strtol(buffer, NULL, 10);

   if (no_retry == 0) {
      p_page->p_srv->no_retry = 0;
   }
   else if (no_retry == 1) {
      p_page->p_srv->no_retry = 1;
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;

}
/* }}} */


/* {{{ proto bool m_set_host(string ipaddress, int port, string username, string password)
   Set the host.  Either the M server or the 'Service Integration Gateway' (if used). */
ZEND_FUNCTION(m_set_host)
{
   char buffer[128];
   int argument_count, n;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_host");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (4 argument) */
   if (argument_count < 4)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->ip_address, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[1]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[1]), Z_STRLEN_P(&parameter_array[1])), 8);
   buffer[8] = '\0';
   if (buffer[0]) {
      p_page->p_srv->port = (int) strtol(buffer, NULL, 10);
      if (p_page->p_srv->port == 0) {
         p_page->p_srv->port = MG_DEFAULT_PORT;
      }
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[2]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[2]), Z_STRLEN_P(&parameter_array[2])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->username, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[3]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[3]), Z_STRLEN_P(&parameter_array[3])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->password, buffer);
   }

   MG_RETURN_TRUE;

}
/* }}} */


/* {{{ proto bool m_set_server(string server)
   Set the server name as defined in the 'Service Integration Gateway' (if used) */
ZEND_FUNCTION(m_set_server)
{
   char buffer[128];
   int argument_count, n;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_server");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 31);
   buffer[31] = '\0';

   strcpy(p_page->p_srv->server, buffer);
   strcpy(p_page->server_base, buffer);

   MG_RETURN_TRUE;

}
/* }}} */


/* {{{ proto bool m_set_uci(string uci)
   Set the M server uci name or InterSystems Namespace */
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

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_set_uci");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 31);
   buffer[31] = '\0';

   strcpy(p_page->p_srv->uci, buffer);

   MG_RETURN_TRUE;

}
/* }}} */

#if !defined(MG_PHP_MGW)
/* {{{ proto string m_bind_server_api(string dbtype_name, string path, string username, string password, string env_variables, string parameters)
   Bind to the server's API */
ZEND_FUNCTION(m_bind_server_api)
{
   char buffer[128];
   int argument_count, n, result;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;
   MGBUF *p_buf;

   p_buf = NULL;
   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_bind_server_api");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (6 argument) */
   if (argument_count < 6)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 30);
   buffer[30] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->dbtype_name, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[1]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[1]), Z_STRLEN_P(&parameter_array[1])), 120);
   buffer[120] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->shdir, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[2]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[2]), Z_STRLEN_P(&parameter_array[2])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->username, buffer);
   }

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[3]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[3]), Z_STRLEN_P(&parameter_array[3])), 60);
   buffer[60] = '\0';
   if (buffer[0]) {
      strcpy(p_page->p_srv->password, buffer);
   }

   p_page->p_srv->p_env = (MGBUF *) mg_malloc(sizeof(MGBUF), 0);
   mg_buf_init(p_page->p_srv->p_env, MG_BUFSIZE, MG_BUFSIZE);

   convert_to_string_ex(&(parameter_array[4]));
   mg_buf_cpy(p_page->p_srv->p_env, estrndup(Z_STRVAL_P(&parameter_array[4]), Z_STRLEN_P(&parameter_array[4])), (unsigned long) Z_STRLEN_P(&parameter_array[4]));

   *buffer = '\0';
   convert_to_string_ex(&(parameter_array[5]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[5]), Z_STRLEN_P(&parameter_array[5])), 60);
   buffer[60] = '\0';

   result = mg_bind_server_api(p_page->p_srv, 0);

   if (!result) {
      if (!strlen(p_page->p_srv->error_mess)) {
         strcpy(p_page->p_srv->error_mess, "The server API is not available on this host");
      }
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   MG_RETURN_STRING_AND_FREE_BUF("", 1);

}
/* }}} */


/* {{{ proto string m_release_server_api()
   Release binding to the server's API */
ZEND_FUNCTION(m_release_server_api)
{
   int argument_count, result;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;
   MGBUF *p_buf;

   p_buf = NULL;
   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_release_server_api");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   result = mg_release_server_api(p_page->p_srv, 0);

   MG_RETURN_STRING_AND_FREE_BUF("", 1);
}
/* }}} */
#endif

/* {{{ proto string m_get_last_error()
   Get the last error message (if any)*/
ZEND_FUNCTION(m_get_last_error)
{
   char buffer[256];
   int argument_count, n;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }


   mg_log_request(p_page, "m_get_last_error");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   for (n = 0; n < argument_count; n ++) {

      convert_to_string_ex(&(parameter_array[n]));

   }

   strcpy(buffer, p_page->p_srv->error_mess);

   MG_RETURN_STRING(buffer, 1);
}
/* }}} */


/* {{{ proto string m_set([string servername, ]string globalname, mixed keys ..., mixed data)
   Set an M global node */
ZEND_FUNCTION(m_set)
{
   long parameter = 0;
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_set");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "S", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);
   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   parameter ++;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}
/* }}} */


/* {{{ proto string m_get([string servername, ]string globalname, mixed keys ...)
   Get the data associated with an M global node */
ZEND_FUNCTION(m_get)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_get");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "G", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_delete([string servername, ]string globalname, mixed keys ...)
   Delete an M global node */
ZEND_FUNCTION(m_delete)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_delete");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "K", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_kill([string servername, ]string globalname, mixed keys ...)
   Delete an M global node */
ZEND_FUNCTION(m_kill)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_kill");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "K", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_defined([string servername, ]string globalname, mixed keys ...)
   Determine if an an M global node is defined */
ZEND_FUNCTION(m_defined)
{
   MGBUF mgbuf, *p_buf;   
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_defined");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "D", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_data([string servername, ]string globalname, mixed keys ...)
   Determine if an an M global node is defined */
ZEND_FUNCTION(m_data)
{
   MGBUF mgbuf, *p_buf;   
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_data");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "D", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_order([string servername, ]string globalname, mixed keys ...)
   Get the next trailing key value for an M global node */
ZEND_FUNCTION(m_order)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_order");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "O", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_previous([string servername, ]string globalname, mixed keys ...)
   Get the previous trailing key value for an M global node */
ZEND_FUNCTION(m_previous)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_previous");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "P", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_increment([string servername, ]string globalname, mixed keys ...)
   Increment the value of an M global node and return the next value */
ZEND_FUNCTION(m_increment)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_increment");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "I", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_tstart([string servername])
   Start a transaction */
ZEND_FUNCTION(m_tstart)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_tstart");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   if (argument_count > 0) {
      if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
          MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "a", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_tlevel([string servername])
   Return the current transaction level */
ZEND_FUNCTION(m_tlevel)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_tlevel");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   if (argument_count > 0) {
      if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
          MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "b", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_tcommit([string servername])
   Commit a transaction */
ZEND_FUNCTION(m_tcommit)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_tcommit");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   if (argument_count > 0) {
      if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
          MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "c", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_trollback([string servername])
   Rollback a transaction */
ZEND_FUNCTION(m_trollback)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_trollback");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   if (argument_count > 0) {
      if (zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
          MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "d", MG_PRODUCT, &(parameter_array[0]));

   for (n = offset; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
   }

   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto bool m_sleep(int msecs)
   Sleep for the number of milliseconds specified */
ZEND_FUNCTION(m_sleep)
{
   char buffer[128];
   int argument_count, n, msecs;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);
   if (!p_page) {
      MG_RETURN_FALSE;
   }

   mg_log_request(p_page, "m_sleep");

   strcpy(p_page->p_srv->error_code, "");
   strcpy(p_page->p_srv->error_mess, "");


   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT;

   n = 0;

   convert_to_string_ex(&(parameter_array[0]));
   strncpy(buffer, estrndup(Z_STRVAL_P(&parameter_array[0]), Z_STRLEN_P(&parameter_array[0])), 100);
   buffer[100] = '\0';
   msecs = (int) strtol(buffer, NULL, 10);

   if (msecs >= 0) {
      mg_sleep(msecs);
   }
   else {
      MG_RETURN_FALSE;
   }

   MG_RETURN_TRUE;
}
/* }}} */


/* {{{ proto string m_html([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function to return a block of HTML text */
ZEND_FUNCTION(m_html)
{
   MGBUF mgbuf, *p_buf;
   int rc, argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_html");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "H", MG_PRODUCT, &(parameter_array[0]));

   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, 0);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_html() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   while ((n = mg_db_receive(p_page->p_srv, chndle, p_buf, 2048, 1))) {
/*
      if (n > (MG_RECV_HEAD + 4) && !strncmp((p_buf->p_buffer + (n - 4)), "\x01\x02\x01\x0a", 4)) {
         n -= 4;
         p_buf->p_buffer[n] = '\0';
         zend_write(p_buf->p_buffer, n);
      }
      else {
         zend_write(p_buf->p_buffer + MG_RECV_HEAD, n - MG_RECV_HEAD);
      }
*/
      zend_write(p_buf->p_buffer + MG_RECV_HEAD, n - MG_RECV_HEAD);
   }

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}
/* }}} */


/* {{{ proto string m_html_method([string servername, ]string classname, string classmethodname, mixed arguments ...)
   Invoke an InterSystems ClassMethod to return a block of HTML text */
ZEND_FUNCTION(m_html_method)
{
   MGBUF mgbuf, *p_buf;
   int rc, argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_html_method");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "y", MG_PRODUCT, &(parameter_array[0]));

   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, 0);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_html_method() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   while ((n = mg_db_receive(p_page->p_srv, chndle, p_buf, 2048, 1))) {
      zend_write(p_buf->p_buffer + MG_RECV_HEAD, n - MG_RECV_HEAD);
   }

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}
/* }}} */


/* {{{ proto string m_http([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function to stream HTTP compliant content */
ZEND_FUNCTION(m_http)
{
   MGBUF mgbuf, *p_buf;
   int rc, argument_count, offset, n, len;
   char *key = NULL, *p, *p1, *p2;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_http");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 2 || argument_count > 3)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   if (Z_TYPE_P(&(parameter_array[argument_count - 2])) != IS_ARRAY) {
      strcpy(p_page->p_srv->error_mess, "The penultimate argument to the 'm_http()' function must be an array");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "h", MG_PRODUCT, &(parameter_array[0]));

   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, 0);
         if (!rc) {
            break;
         }
      }
      else if (n == (argument_count - 1)) {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   n = 0;
   while (mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 1)) {
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

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }

}
/* }}} */


/* {{{ proto string m_function([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function */
ZEND_FUNCTION(m_function)
{
   short phase;
   int rc, res_open, res_send, res_recv, attempt_no;
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
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

   p_page = MG_PHP_GLOBAL(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   phase = 3;

   mg_log_request(p_page, "m_function");

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
         if (!strlen(p_page->p_srv->error_mess))
            sprintf(p_page->p_srv->error_mess, MG_EXT_NAME " v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", PHP_MG_PHP_VERSION, res_open, res_send, res_recv);

         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   attempt_no ++;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   res_open = n;
   if (!n) {
      goto m_proc_retry;
   }

   phase = 5;

   offset = mg_request_header_ex(p_page, p_buf, "X", MG_PRODUCT, &(parameter_array[0]));

   phase = 6;

   byref = 0;
   rc = 1;
   for (n = offset; n < argument_count; n ++) {

      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }

   phase = 7;

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_function() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!res_send) {
      MG_ERROR3(p_page->p_srv->error_mess);
   }

   if (!res_send) {
      goto m_proc_retry;
   }
   phase = 9;

   res_recv = mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   if (!res_recv) {
      goto m_proc_retry;
   }

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

   data = p_buf->p_buffer + MG_RECV_HEAD;

   phase = 99;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;
   char buffer[256];

   __try {
      code = GetExceptionCode();
      sprintf(buffer, "Exception caught in f:m_function: %x|%d", code, phase);
      if (p_page) {
         strcpy(p_page->p_srv->error_mess, buffer);
         MG_ERROR1(p_page->p_srv->error_mess);
      }

   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif

}
/* }}} */


/* {{{ proto string m_proc([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function */
ZEND_FUNCTION(m_proc)
{
   short phase;
   int rc, res_open, res_send, res_recv, attempt_no;
   MGBUF mgbuf, *p_buf;
   short byref;
   int argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
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

   p_page = MG_PHP_GLOBAL(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   phase = 3;

   mg_log_request(p_page, "m_proc");

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
         if (!strlen(p_page->p_srv->error_mess))
            sprintf(p_page->p_srv->error_mess, MG_EXT_NAME " v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", PHP_MG_PHP_VERSION, res_open, res_send, res_recv);

         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   attempt_no ++;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   res_open = n;
   if (!n) {
      goto m_proc_retry;
   }

   phase = 5;

   offset = mg_request_header_ex(p_page, p_buf, "X", MG_PRODUCT, &(parameter_array[0]));

   phase = 6;

   byref = 0;
   rc = 1;
   for (n = offset; n < argument_count; n ++) {

      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }

   phase = 7;

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_proc() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!res_send) {
      MG_ERROR3(p_page->p_srv->error_mess);
   }

   if (!res_send) {
      goto m_proc_retry;
   }
   phase = 9;

   res_recv = mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   if (!res_recv) {
      goto m_proc_retry;
   }

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

   data = p_buf->p_buffer + MG_RECV_HEAD;

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
      if (p_page) {
         strcpy(p_page->p_srv->error_mess, buffer);
         MG_ERROR1(p_page->p_srv->error_mess);
      }

   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif

}
/* }}} */


/* {{{ proto array m_proc_ex([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function with all (possibly) modified input arguments returned as an array */
ZEND_FUNCTION(m_proc_ex)
{
   short phase;
   int rc, res_open, res_send, res_recv, attempt_no, array_init;
   MGBUF mgbuf, *p_buf;
   short byref, type, stop, anybyref;
   int argument_count, offset, argc, n, n1, rn, len, hlen, clen, rlen, size, size0, rec_len;
   char *key, *data, *data0, *parg, *par;
   char stype[4];
/*
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
*/
   zval	*parameter_array[MG_MAXARG];
   int chndle;
   MGPAGE *p_page;
   MGAREC arec;

   phase = 0;
   res_open = -9;
   res_send = -9;
   res_recv = -9;
   p_buf = NULL;
   p_page = NULL;
   key = NULL;

#ifdef _WIN32
__try {
#endif

   phase = 1;

   p_page = MG_PHP_GLOBAL(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   phase = 3;

   mg_log_request(p_page, "m_proc_ex");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */

   n = mg_get_input_arguments(argument_count, parameter_array);
   if (n == FAILURE) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   phase = 4;

   attempt_no = 0;

m_proc_ex_retry:

   if (attempt_no) {
      mg_pause(1000 * attempt_no);
      if (attempt_no > 10) {
         if (!strlen(p_page->p_srv->error_mess))
            sprintf(p_page->p_srv->error_mess, MG_EXT_NAME " v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", PHP_MG_PHP_VERSION, res_open, res_send, res_recv);
         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   attempt_no ++;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   res_open = n;
   if (!n) {
      goto m_proc_ex_retry;
   }

   phase = 5;

   offset = mg_request_header_ex(p_page, p_buf, "X", MG_PRODUCT, parameter_array[0]);

   phase = 6;

   rc = 1;
   for (n = offset; n < argument_count; n ++) {

      if (n > offset) {
         byref = 1;
      }
      else {
         byref = 0;
      }

      if (Z_TYPE_P(parameter_array[n]) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, parameter_array[n], p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(parameter_array[n], NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }
   offset ++;

   phase = 7;

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_proc_ex() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!res_send) {
      MG_ERROR3(p_page->p_srv->error_mess);
   }

   if (!res_send) {
      goto m_proc_ex_retry;
   }
   phase = 9;

   res_recv = mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   if (!res_recv) {
      goto m_proc_ex_retry;
   }

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

   data = p_buf->p_buffer + MG_RECV_HEAD;

   phase = 12;

   stop = 0;
   offset ++;
   parg = (p_buf->p_buffer + MG_RECV_HEAD);

   clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
   stype[0] = p_buf->p_buffer[5];
   stype[1] = p_buf->p_buffer[6];
   stype[2] = '\0';

   array_init(return_value);

   if (!strcmp(stype, "cv")) {
      anybyref = 0;
      size0 = p_buf->data_size - MG_RECV_HEAD;
   }
   else {
      zval *subarray;
      MGAKEY akey;

      mg_akey_init(&akey);

      rlen = 0;
      argc = 0;
      anybyref = 0;
      for (n = 0;; n ++) {

         mg_akey_free(&akey);
         mg_akey_init(&akey);

         array_init = 0;
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

         if (type == MG_TX_AREC) {
            par = parg;
            rn = 0;
            rec_len = 0;
            arec.kn = 0;

            parg += size;
            rlen += size;

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

               if (type == MG_TX_DATA) {

                  arec.vrec = parg;
                  arec.vsize = size;

                  if (argc > offset && (argc - offset) > 0) {
                     anybyref = 1;

                     mg_array_terminate_strings(&arec);

                     if (akey.zval_array == NULL) {
                        akey.zval_array = (zval *) emalloc(sizeof(zval));
                        array_init(akey.zval_array);
                        add_index_zval(return_value, argc - offset, akey.zval_array);
                        akey.zval_subarray[0] = akey.zval_array;
                        /*
                        zend_printf("\r\n ===> Create top level array : %p;", akey.zval_array);
                        */
                     }
                     subarray = akey.zval_array;
                     if (arec.kn > 1) {
                        subarray = mg_array_subarray_zval(&akey, &arec);
                     }
/*
                     {
                        int n;
                        char buffer[1024];

                        sprintf(buffer, "argc=%d; arec.kn=%d; ", argc-offset, arec.kn);
                        for (n = 0; n < arec.kn; n ++) {
                           strcat(buffer, arec.krec[n]);
                           strcat(buffer, ",");
                        }
                        strcat(buffer, ",,");
                        strcat(buffer, arec.vrec);
                        zend_printf("\r\n ===> ARRAY NODE : (%p) %s;", subarray, buffer);
                     }
*/
                     add_assoc_string(subarray, arec.krec[arec.kn - 1], arec.vrec);

                     mg_array_reset_strings(&arec);
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

            if (argc > offset && (argc - offset) > 0) {
               anybyref = 1;
               c = *(parg + size);
               *(parg + size) = '\0';

               add_index_string(return_value, argc - offset, parg);
/*
               zend_printf("\r\n ===> argc=%d; offset=%d; size=%d; parg=%s;", argc, offset, size, parg);
*/
               *(parg + size) = c;

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

      mg_akey_free(&akey);
   }

   if (stop) {
      strcpy(p_page->p_srv->error_mess, "m_proc_ex: Bad return data");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   if (anybyref) {
      data = data0;
      *(data + size0) = '\0';
   }

   phase = 99;

   add_index_string(return_value, 0, data);

   mg_buf_free(p_buf);

   return;

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;

   __try {
      code = GetExceptionCode();
      if (p_page) {
         sprintf(p_page->p_srv->error_mess, "Exception caught in f:m_proc_ex: %x|%d", code, phase);
         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif
}
/* }}} */


/* {{{ proto string m_proc_byref([string servername, ]string functionname, mixed arguments ...)
   Invoke an M function with input arguments (onwards from the third argument) passed by reference */
ZEND_FUNCTION(m_proc_byref)
{
   short phase;
   int rc, res_open, res_send, res_recv, attempt_no;
   MGBUF mgbuf, *p_buf;
   short byref, type, stop, anybyref;
   int argument_count, offset, argc, n, n1, rn, len, hlen, clen, rlen, size, size0, rec_len;
   char *key, *data, *data0, *parg, *par;
   char stype[4];
/*
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
*/
   zval	*parameter_array[MG_MAXARG];
   int chndle;
   MGPAGE *p_page;
   MGAREC arec;

   phase = 0;
   res_open = -9;
   res_send = -9;
   res_recv = -9;
   p_buf = NULL;
   p_page = NULL;
   key = NULL;

#ifdef _WIN32
__try {
#endif

   phase = 1;

   p_page = MG_PHP_GLOBAL(p_page);

   phase = 2;

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   phase = 3;

   mg_log_request(p_page, "m_proc_byref");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */

   n = mg_get_input_arguments(argument_count, parameter_array);
   if (n == FAILURE) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   phase = 4;

   attempt_no = 0;

m_proc_byref_retry:

   if (attempt_no) {
      mg_pause(1000 * attempt_no);
      if (attempt_no > 10) {
         if (!strlen(p_page->p_srv->error_mess))
            sprintf(p_page->p_srv->error_mess, MG_EXT_NAME " v%s: General error: res_open=%d; res_send=%d; res_recv=%d;", PHP_MG_PHP_VERSION, res_open, res_send, res_recv);
         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   attempt_no ++;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   res_open = n;
   if (!n) {
      goto m_proc_byref_retry;
   }

   phase = 5;

   offset = mg_request_header_ex(p_page, p_buf, "X", MG_PRODUCT, parameter_array[0]);

   phase = 6;

   rc = 1;
   for (n = offset; n < argument_count; n ++) {

      if (n > 1) {
         byref = 1;
      }
      else {
         byref = 0;
      }

      if (Z_TYPE_P(parameter_array[n]) == IS_ARRAY) {
         /* zend_printf("\r\narg %d is array byref=%d", n, byref); */
         rc = mg_array_parse(p_page, chndle, parameter_array[n], p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         /* zend_printf("\r\narg %d is string byref=%d", n, byref); */
         data = mg_get_string(parameter_array[n], NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }

   phase = 7;

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_proc_byref() function has been exceeded (1MB)", 1);
   }

   phase = 8;

   res_send = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!res_send) {
      MG_ERROR3(p_page->p_srv->error_mess);
   }

   if (!res_send) {
      goto m_proc_byref_retry;
   }
   phase = 9;

   res_recv = mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   if (!res_recv) {
      goto m_proc_byref_retry;
   }

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   phase = 10;

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   phase = 11;

   data = p_buf->p_buffer + MG_RECV_HEAD;

   phase = 12;

   stop = 0;
   offset = 2;
   parg = (p_buf->p_buffer + MG_RECV_HEAD);

   clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
   stype[0] = p_buf->p_buffer[5];
   stype[1] = p_buf->p_buffer[6];
   stype[2] = '\0';

   if (!strcmp(stype, "cv")) {
      anybyref = 0;
      size0 = p_buf->data_size - MG_RECV_HEAD;
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
         if (type == MG_TX_AREC) {
            par = parg;
            rn = 0;
            rec_len = 0;
            arec.kn = 0;

            parg += size;
            rlen += size;
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

               if (type == MG_TX_DATA) {

                  arec.vrec = parg;
                  arec.vsize = size;

                  if (argc >= offset && (argc - offset) < argument_count) {
                     anybyref = 1;
                     mg_array_add_record(parameter_array[argc - offset], &arec, 0);
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

            if (argc > offset && (argc - offset) < argument_count) {
               anybyref = 1;
               c = *(parg + size);
               *(parg + size) = '\0';

               ZVAL_STRING(parameter_array[argc - offset], parg);

               *(parg + size) = c;

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
      strcpy(p_page->p_srv->error_mess, "m_proc_byref: Bad return data");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   if (anybyref) {
      data = data0;
      *(data + size0) = '\0';
   }

   phase = 99;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;

   __try {
      code = GetExceptionCode();
      if (p_page) {
         sprintf(p_page->p_srv->error_mess, "Exception caught in f:m_proc_byref: %x|%d", code, phase);
         MG_ERROR1(p_page->p_srv->error_mess);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   MG_RETURN_STRING("", 1);
}
#endif
}
/* }}} */


/* {{{ proto string m_classmethod([string servername, ]string classname, string classmethodname, mixed arguments ...)
   Invoke an InterSystems ClassMethod */
ZEND_FUNCTION(m_classmethod)
{
   MGBUF mgbuf, *p_buf;
   short byref;
   int rc, argument_count, offset, n, len;
   char *key, *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   key = NULL;
   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_classmethod");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "x", MG_PRODUCT, &(parameter_array[0]));

   byref = 0;
   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_classmethod() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   data = p_buf->p_buffer + MG_RECV_HEAD;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);
}
/* }}} */


/* {{{ proto string m_method([string servername, ]string classname, string classmethodname, mixed arguments ...)
   Invoke an InterSystems ClassMethod */
ZEND_FUNCTION(m_method)
{
   MGBUF mgbuf, *p_buf;
   short byref;
   int rc, argument_count, offset, n, len;
   char *key, *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   key = NULL;
   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_method");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "x", MG_PRODUCT, &(parameter_array[0]));

   byref = 0;
   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (Z_TYPE_P(&(parameter_array[n])) == IS_ARRAY) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_method() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   data = p_buf->p_buffer + MG_RECV_HEAD;

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);
}
/* }}} */


/* {{{ proto string m_method_byref([string servername, ]string classname, string classmethodname, mixed arguments ...)
   Invoke an InterSystems ClassMethod with input arguments (onwards from the fourth argument) passed by reference */
ZEND_FUNCTION(m_method_byref)
{
   MGBUF mgbuf, *p_buf;
   short byref, type, stop, anybyref;;
   int rc, argument_count, offset, argc, n, n1, rn, len, hlen, clen, rlen, size, size0, rec_len;
   char *key, *data, *data0, *parg, *par;
   char stype[4];
/*
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
*/
   zval	*parameter_array[MG_MAXARG];
   int chndle;
   MGPAGE *p_page;
   MGAREC arec;

   key = NULL;
   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_method_byref");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */

   n = mg_get_input_arguments(argument_count, parameter_array);
   if (n == FAILURE) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "x", MG_PRODUCT, parameter_array[0]);

   rc = 1;
   for (n = offset; n < argument_count; n ++) {

      if (n > 1) {
         byref = 1;
      }
      else {
         byref = 0;
      }

      if (Z_TYPE_P(parameter_array[n]) == IS_ARRAY) {
/*
         zend_printf("\r\narg %d is array byref=%d", n, byref);
*/
         rc = mg_array_parse(p_page, chndle, parameter_array[n], p_buf, 0, byref);
         if (!rc) {
            break;
         }
      }
      else {
/*
         zend_printf("\r\narg %d is string byref=%d", n, byref);
*/
         data = mg_get_string(parameter_array[n], NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_method_byref() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

   data = p_buf->p_buffer + MG_RECV_HEAD;

   stop = 0;
   offset = 2;
   parg = (p_buf->p_buffer + MG_RECV_HEAD);

   clen = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);
   stype[0] = p_buf->p_buffer[5];
   stype[1] = p_buf->p_buffer[6];
   stype[2] = '\0';

   if (!strcmp(stype, "cv")) {
      anybyref = 0;
      size0 = p_buf->data_size - MG_RECV_HEAD;
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

         if (type == MG_TX_AREC) {
            par = parg;
            rn = 0;
            rec_len = 0;
            arec.kn = 0;

            parg += size;
            rlen += size;

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

               if (type == MG_TX_DATA) {

                  arec.vrec = parg;
                  arec.vsize = size;

                  if (argc >= offset && (argc - offset) < argument_count) {
                     anybyref = 1;
                     mg_array_add_record((zval *) &(parameter_array[argc - offset]), &arec, 0);
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
               anybyref = 1;
               c = *(parg + size);
               *(parg + size) = '\0';
               ZVAL_STRING(parameter_array[argc - offset], parg);
               *(parg + size) = c;
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
      strcpy(p_page->p_srv->error_mess, "m_method_byref: Bad return data");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   if (anybyref) {
      data = data0;
      *(data + size0) = '\0';
   }

   MG_RETURN_STRING_AND_FREE_BUF(data, 1);
}
/* }}} */


/* {{{ proto string m_merge_to_db([string servername, ]string globalname, mixed keys ..., array phpdata, string parameters)
   Merge the contents of a PHP array into an M global node */
ZEND_FUNCTION(m_merge_to_db)
{
   MGBUF mgbuf, *p_buf;
   int rc, argument_count, offset, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_merge_to_db");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 3)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   if (Z_TYPE_P(&(parameter_array[argument_count - 1])) != IS_STRING) {
      strcpy(p_page->p_srv->error_mess, "The last (options) argument to the 'm_merge_to_db()' function must be a string");
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   if (Z_TYPE_P(&(parameter_array[argument_count - 2])) != IS_ARRAY) {
      strcpy(p_page->p_srv->error_mess, "The penultimate argument to the 'm_merge_to_db()' function must be an array");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "M", MG_PRODUCT, &(parameter_array[0]));

   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
         rc = mg_array_parse(p_page, chndle, &(parameter_array[n]), p_buf, 0, 0);
         if (!rc) {
            break;
         }
      }
      else if (n == (argument_count - 1)) {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_lcase(data);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);
   if (p_buf->data_size > 1000000) {
      p_page->p_srv->mem_error = 1;
      MG_MEMCHECK("The data limit for the m_merge_to_db() function has been exceeded (1MB)", 1);
   }

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }
   else {
      MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
   }
}
/* }}} */


/* {{{ proto string m_merge_from_db([string servername, ]string globalname, mixed keys ..., array phpdata, string parameters)
   Merge the contents of a a M global node into a PHP array */
ZEND_FUNCTION(m_merge_from_db)
{
   MGBUF mgbuf, *p_buf;
   short byref, type, stop;
   int rc, argument_count, offset, n, n1, rn, len, hlen, size, clen, rlen, argc, rec_len;
   char *key, *data;
   unsigned char *parg, *par;
   zval *parameter_array_d[MG_MAXARG];
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle;
   MGPAGE *p_page;
   MGAREC arec;

   key = NULL;
   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_merge_from_db");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (3 arguments) */
   if (argument_count < 3) {
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;
   }

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   for (n = 0; n < argument_count; n ++) {
      parameter_array_d[n] = &parameter_array_a[n];
      ZVAL_DEREF(parameter_array_d[n]);
      if (Z_TYPE_P(parameter_array_d[n]) == IS_ARRAY) {
         SEPARATE_ARRAY(parameter_array_d[n]);
      }
   }
   if (Z_TYPE_P(parameter_array_d[argument_count - 1]) != IS_STRING) {
      strcpy(p_page->p_srv->error_mess, "The last (options) argument to the 'm_merge_from_db()' function must be a string");
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   if (Z_TYPE_P(parameter_array_d[argument_count - 2]) != IS_ARRAY   ) {
      strcpy(p_page->p_srv->error_mess, "The penultimate argument to the 'm_merge_from_db()' function must be an array");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   n = mg_db_connect(p_page->p_srv, &chndle, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   offset = mg_request_header_ex(p_page, p_buf, "m", MG_PRODUCT, &(parameter_array[0]));

   rc = 1;
   for (n = offset; n < argument_count; n ++) {
      if (n == (argument_count - 2)) {
         rc = mg_array_parse(p_page, chndle, parameter_array_d[n], p_buf, 0, 1);
         if (!rc) {
            break;
         }
      }
      else {
         data = mg_get_string(&(parameter_array[n]), NULL, &len);
         mg_request_add(p_page->p_srv, chndle, p_buf, data, len, 0, MG_TX_DATA);
      }
   }

   if (!rc) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   MG_MEMCHECK("Insufficient memory to process request", 1);

   n = mg_db_send(p_page->p_srv, chndle, p_buf, 1);
   if (!n) {
      MG_ERROR1(p_page->p_srv->error_mess);
   }
   mg_db_receive(p_page->p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   MG_MEMCHECK("Insufficient memory to process response", 0);

   mg_db_disconnect(p_page->p_srv, chndle, 1);

   if ((n = mg_php_error(p_page, p_buf->p_buffer))) {
      if (n == 2) {
         MG_RETURN_STRING_AND_FREE_BUF(p_page->p_srv->error_code, 1);
      }
      MG_RETURN_FALSE_AND_FREE_BUF;
   }

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

      parg += size;
      rlen += size;
      if (type == MG_TX_AREC) {
         par = parg;
         rn = 0;
         rec_len = 0;
         arec.kn = 0;

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

            if (type == MG_TX_DATA) {

               arec.vrec = parg;
               arec.vsize = size;

               mg_array_add_record(parameter_array_d[argument_count - 2], &arec, 0);

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
      strcpy(p_page->p_srv->error_mess, "m_merge_from_db: Bad return data");
      MG_ERROR1(p_page->p_srv->error_mess);
   }

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer + MG_RECV_HEAD, 1);
}
/* }}} */


/* {{{ proto string m_return_to_applet(string content)
   Send content to the client (e.g. AJAX/XMLHTTP) */
ZEND_FUNCTION(m_return_to_applet)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle = 0;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_return_to_applet");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

    mg_buf_cpy(p_buf, "\x07", 1);

   for (n = 0; n < argument_count; n ++) {

      data = mg_get_string(&(parameter_array[n]), NULL, &len);

      mg_buf_cat(p_buf, data, len);

   }

   p_buf->p_buffer[p_buf->data_size] = '\0';

   zend_printf("%s", p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}
/* }}} */


/* {{{ proto string m_return_to_client(string content)
   Send content to the client (e.g. AJAX/XMLHTTP) */
ZEND_FUNCTION(m_return_to_client)
{
   MGBUF mgbuf, *p_buf;
   int argument_count, n, len;
   char *key = NULL;
   char *data;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;
   int chndle = 0;
   MGPAGE *p_page;

   p_page = MG_PHP_GLOBAL(p_page);

   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_log_request(p_page, "m_return_to_client");

   /* get the number of arguments */
   argument_count = ZEND_NUM_ARGS();

   /* see if it satisfies our minimal request (1 argument) */
   if (argument_count < 1)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

   /* argument count is correct, now retrieve arguments */
   if(zend_get_parameters_array_ex(argument_count, parameter_array) != SUCCESS)
      MG_WRONG_PARAM_COUNT_AND_FREE_BUF;

    mg_buf_cpy(p_buf, "\x07", 1);

   for (n = 0; n < argument_count; n ++) {
      data = mg_get_string(&(parameter_array[n]), NULL, &len);
      mg_buf_cat(p_buf, data, len);
   }

   p_buf->p_buffer[p_buf->data_size] = '\0';

   zend_printf("%s", p_buf->p_buffer);

   MG_RETURN_STRING_AND_FREE_BUF(p_buf->p_buffer, 1);
}
/* }}} */


/* {{{ proto mixed m_array_test(mixed data...)
   Internal test function */
ZEND_FUNCTION(m_array_test)
{
   int argument_count;
   zval *array;
   zval	parameter_array_a[MG_MAXARG] = {0}, *parameter_array = parameter_array_a;

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
/* }}} */


int mg_type(zval * item)
{
   int result;

   if (!item)
      return -1;

   if (Z_TYPE_P(item) == IS_ARRAY) {
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


int mg_get_integer(zval * item)
{
   int result;
   char *ps;

   convert_to_string_ex(item);
   ps = Z_STRVAL_P(item);
   result = (int) strtol(ps, NULL, 10);

   return result;
}


double mg_get_float(zval * item)
{
   double result;
   char *ps;

   convert_to_string_ex(item);
   ps = Z_STRVAL_P(item);
   result = (double) strtod(ps, NULL);

   return result;
}


char * mg_get_string(zval * item, zval * item_tmp, int * size)
{
   char * result;

   result = NULL;
   *size = 0;

   convert_to_string_ex(item);
   result = Z_STRVAL_P(item);
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
      strcpy(p_page->p_srv->error_mess, buffer + MG_RECV_HEAD);
      MG_ERROR2(p_page->p_srv->error_mess);
   }
   else
      return 0;
}



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


static const char *mg_array_lookup_string(HashTable *ht, const char *idx)
{
   zval *pvalue;
   zend_string *key;

   key = zend_string_init(idx, strlen(idx), 0);
   if (ht && (pvalue = zend_hash_find(ht, key))) {
      SEPARATE_ZVAL(pvalue);
      convert_to_string_ex(pvalue);
      zend_string_release(key);
      return Z_STRVAL_P(pvalue);
   }

   return 0;
}


int mg_array_add_record(zval *ppa, MGAREC * p_arec, int mode)
{
   int kn;
   unsigned long index;
	zval *pk[MG_MAXKEY];
   zval *zv = NULL;
   zval *ppk[MG_MAXKEY];
   HashTable *ht[MG_MAXKEY];

/* v3.3.59 */
#if PHP_MAJOR_VERSION >= 8
#else
   TSRMLS_FETCH();
#endif

   if (p_arec->kn < 1) {
      return 0;
   }

   mg_array_terminate_strings(p_arec);

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

   mg_array_reset_strings(p_arec);

   return 1;

}


int mg_akey_init(MGAKEY *p_akey)
{
   int n;

   for (n = 0; n < MG_MAXKEY; n ++) {
      p_akey->krec[n] = NULL;
      p_akey->ksize[n] = 0;
      p_akey->kmemsize[n] = 0;
      p_akey->zval_subarray[n] = NULL;
   }
   p_akey->kn = 0;
   p_akey->zval_array = NULL;
   return 0;
}


int mg_akey_free(MGAKEY *p_akey)
{
   int n;

   for (n = 0; n < MG_MAXKEY; n ++) {
      if (p_akey->krec[n]) {
         mg_free((void *) p_akey->krec[n], 0);
      }
      p_akey->krec[n] = NULL;
      p_akey->ksize[n] = 0;
      p_akey->kmemsize[n] = 0;
   }

   return 0;
}


zval * mg_array_subarray_zval(MGAKEY *p_akey, MGAREC * p_arec)
{
   int n, kn, newsub;

   if (p_arec->kn < 2) {
      return p_akey->zval_array;
   }

   for (kn = 0; kn < (p_arec->kn - 1); kn ++) {
      newsub = 0;
      if (p_akey->zval_subarray[kn] == NULL) {
         newsub = 1;
      }
      else if (p_akey->ksize[kn] == 0) {
         newsub = 1;
      }
      else if (p_akey->ksize[kn] != p_arec->ksize[kn] || strcmp(p_akey->krec[kn], p_arec->krec[kn])) {
         /*
         zend_printf("\r\n   ===> Difference at kn=%d; p_akey->ksize[kn]=%d; p_arec->ksize[kn]=%d; p_akey->krec[kn]=%s; p_arec->krec[kn]=%s;", kn, p_akey->ksize[kn], p_arec->ksize[kn], p_akey->krec[kn], p_arec->krec[kn]);
         */
         newsub = 1;
      }

      if (newsub) {

         p_akey->zval_subarray[kn + 1] = (zval *) emalloc(sizeof(zval));
         array_init(p_akey->zval_subarray[kn + 1]);

         add_assoc_zval(p_akey->zval_subarray[kn], p_arec->krec[kn], p_akey->zval_subarray[kn + 1]);
         /*
         zend_printf("\r\n      ===> New PHP array at kn=%d; zval=%p; name=%s; new_array=%p;", kn, p_akey->zval_subarray[kn], p_arec->krec[kn], p_akey->zval_subarray[kn + 1]);
         */
         /*
         zend_printf("\r\n      ===> New PHP array at kn=%d; p_akey->ksize[kn]=%d; p_arec->ksize[kn]=%d; p_akey->krec[kn]=%s; p_arec->krec[kn]=%s;", kn, p_akey->ksize[kn], p_arec->ksize[kn], p_akey->krec[kn], p_arec->krec[kn]);
         */
         for (n = kn + 1; n < MG_MAXKEY; n ++) {
            p_akey->ksize[n] = 0;
         }

         if (p_akey->kmemsize[kn] < (p_arec->ksize[kn] + 32)) {
            p_akey->kmemsize[kn] = p_arec->ksize[kn] + 126;
            p_akey->krec[kn] = (unsigned char *) mg_malloc(p_akey->kmemsize[kn] * sizeof(char), 0);
            if (!p_akey->krec[kn]) {
               return NULL;
            }
         }

         memcpy(p_akey->krec[kn], p_arec->krec[kn], p_arec->ksize[kn]);
         p_akey->krec[kn][p_arec->ksize[kn]] = '\0';
         p_akey->ksize[kn] = p_arec->ksize[kn];
      }
   }

   /*
   zend_printf("\r\n =========> sub array p_arec->kn=%d; p_arec->krec[kn-1]=%s; subarray=%p", p_arec->kn, p_arec->krec[kn-1], p_akey->zval_subarray[p_arec->kn - 1]); 
   */

   return p_akey->zval_subarray[p_arec->kn - 1];
}


int mg_array_terminate_strings(MGAREC * p_arec)
{
   int kn;

   p_arec->krec[p_arec->kn] = NULL;
   for (kn = 0; kn < p_arec->kn; kn ++) {
      p_arec->kz[kn] = *(p_arec->krec[kn] + p_arec->ksize[kn]);
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = '\0';
   }
   p_arec->vz = *(p_arec->vrec + p_arec->vsize);
   *(p_arec->vrec + p_arec->vsize) = '\0';

   return 0;
}


int mg_array_reset_strings(MGAREC * p_arec)
{
   int kn;

   for (kn = 0; kn < p_arec->kn; kn ++) {
      *(p_arec->krec[kn] + p_arec->ksize[kn]) = p_arec->kz[kn];
   }
   *(p_arec->vrec + p_arec->vsize) = p_arec->vz;

   return 0;
}


int mg_array_parse(MGPAGE *p_page, int chndle, zval *ppa, MGBUF *p_buf, int mode, short byref)
{
   short phase;
   unsigned long num_key;		
   int rc, n, type, len, string_key_len;
   zval *current;
   char *string_key = NULL;
   char *string_data = NULL;
   zend_string *string_key_ex = NULL;
   char *data;
   char num[32];
   MGAKEYX *p_keyx;

   phase = 0;

#ifdef _WIN32
__try {
#endif

   rc = 1;
   p_keyx = (MGAKEYX *) mg_malloc(sizeof(MGAKEYX) + MG_MAXKEYLEN + 32, 0);
   if (!p_keyx) {
      return 0;
   }
   memset((void *) p_keyx, 0, (sizeof(MGAKEYX) + MG_MAXKEYLEN + 32));
   p_keyx->krec[0] = ((unsigned char *) p_keyx) + sizeof(MGAKEYX);
   p_keyx->kmemoffs[0] = 0;

/* v3.3.59 */
#if PHP_MAJOR_VERSION >= 8
#else
   TSRMLS_FETCH();
#endif

   phase = 1;
   mg_request_add(p_page->p_srv, chndle, p_buf, NULL, 0, byref, MG_TX_AREC);

   p_keyx->kn = 0;
   p_keyx->ht[p_keyx->kn] = HASH_OF(ppa);
   phase = 2;
   zend_hash_internal_pointer_reset_ex(p_keyx->ht[p_keyx->kn], &(p_keyx->hp[p_keyx->kn]));

   phase = 3;
   for (;;) {
      phase = 4;
      current = zend_hash_get_current_data_ex(p_keyx->ht[p_keyx->kn], &(p_keyx->hp[p_keyx->kn]));
      if (current == NULL) {
         if (p_keyx->kn) {
            p_keyx->kn --;
            phase = 5;
            zend_hash_move_forward_ex(p_keyx->ht[p_keyx->kn], &(p_keyx->hp[p_keyx->kn]));
            continue;
         }
         else {
            break;
         }
      }

      if (Z_TYPE_P(current) == IS_ARRAY) {
         string_key_ex = NULL;
         string_key = NULL;

         phase = 6;
	      type = zend_hash_get_current_key_ex(p_keyx->ht[p_keyx->kn], &string_key_ex, (zend_ulong *) &num_key, &(p_keyx->hp[p_keyx->kn]));
         if (string_key_ex) {
            string_key = ZSTR_VAL(string_key_ex);
            string_key_len = (int) strlen(string_key);
         }
         else {
            sprintf(num, "%lu", num_key);
            string_key = num;
            string_key_len = (int) strlen(string_key);
         }
         /* zend_printf("\r\np_keyx->kn=%d; string_key=%s;", p_keyx->kn, string_key); */
         if (p_keyx->kn > 0) {
            p_keyx->krec[p_keyx->kn] = p_keyx->krec[0] + p_keyx->kmemoffs[p_keyx->kn];
         }
         p_keyx->kmemoffs[p_keyx->kn + 1] = p_keyx->kmemoffs[p_keyx->kn] + string_key_len + 1;
         if ((p_keyx->kmemoffs[p_keyx->kn] + string_key_len + 32) > MG_MAXKEYLEN) {
            sprintf(p_page->p_srv->error_mess, "Total length of keys may not exceed %d Bytes", MG_MAXKEYLEN - 32);
            rc = 0;
            break;
         }
         if ((p_keyx->kn + 2) > MG_MAXKEY) {
            sprintf(p_page->p_srv->error_mess, "Total number of keys (nested arrays) may not exceed %d", MG_MAXKEY - 2);
            rc = 0;
            break;
         }
         strcpy(p_keyx->krec[p_keyx->kn], string_key);
         p_keyx->ksize[p_keyx->kn] = string_key_len;
         p_keyx->kn ++;
	      p_keyx->ht[p_keyx->kn] = HASH_OF(current);

         phase = 7;
         zend_hash_internal_pointer_reset_ex(p_keyx->ht[p_keyx->kn], &(p_keyx->hp[p_keyx->kn]));
         continue;
      }
      else {
		   SEPARATE_ZVAL(current);
         convert_to_string_ex(current);
         string_key_ex = NULL;
         string_key = NULL;
         phase = 8;
	      type = zend_hash_get_current_key_ex(p_keyx->ht[p_keyx->kn], &string_key_ex, (zend_ulong *) &num_key, &(p_keyx->hp[p_keyx->kn]));
         string_data = Z_STRVAL_P(current);
         if (string_key_ex) {
            string_key = ZSTR_VAL(string_key_ex);
            if (string_data) {
               string_key_len = (int) strlen(string_key);
            }
            else {
               strcpy(num, "");
               string_key = num;
               string_key_len = (int) strlen(string_key);
            }
         }
         else {
            sprintf(num, "%lu", num_key);
            string_key = num;
            string_key_len = (int) strlen(string_key);
         }

         if (p_keyx->kn > 0) {
            p_keyx->krec[p_keyx->kn] = p_keyx->krec[0] + p_keyx->kmemoffs[p_keyx->kn];
         }
         p_keyx->kmemoffs[p_keyx->kn + 1] = p_keyx->kmemoffs[p_keyx->kn] + string_key_len + 1;
         if ((p_keyx->kmemoffs[p_keyx->kn] + string_key_len + 32) > MG_MAXKEYLEN) {
            sprintf(p_page->p_srv->error_mess, "Total length of keys may not exceed %d Bytes", MG_MAXKEYLEN - 32);
            rc = 0;
            break;
         }
         strcpy(p_keyx->krec[p_keyx->kn], string_key);
         p_keyx->ksize[p_keyx->kn] = string_key_len;

         for (n = 0; n <= p_keyx->kn; n ++) {
            data = p_keyx->krec[n];
            len = p_keyx->ksize[n];

            mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_AKEY);
         }

         if (string_data) {
            data = string_data;
            len = (int) strlen(data);
            mg_request_add(p_page->p_srv, chndle, p_buf, data, len, byref, MG_TX_DATA);
         }
         phase = 9;
         zend_hash_move_forward_ex(p_keyx->ht[p_keyx->kn], &(p_keyx->hp[p_keyx->kn]));
      }

   }

   phase = 99;
   mg_request_add(p_page->p_srv, chndle, p_buf, NULL, 0, byref, MG_TX_EOD);

   if (p_keyx) {
      mg_free((void *) p_keyx, 0);
   }

   return rc;

#ifdef _WIN32
}
__except (EXCEPTION_EXECUTE_HANDLER ) {

   DWORD code;

   __try {
      code = GetExceptionCode();
      if (p_page) {
         sprintf(p_page->p_srv->error_mess, "Exception caught in f:mg_array_parse: %x|%d", code, phase);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER ) {
      ;
   }

   return 0;
}
#endif

}


int mg_request_header_ex(MGPAGE *p_page, MGBUF *p_buf, char *command, char *product, zval *parg0)
{
   int offset;
#if !defined(MG_PHP_MGW)
   int len;
   char *data;
#endif

   offset = 0;

   /* for mg_php */
#if !defined(MG_PHP_MGW)
   if (Z_TYPE_P(parg0) == IS_STRING) {
      data = mg_get_string(parg0, NULL, &len);
      if (data && len) {
         if (!strstr(data, "^") && !strstr(data, "$") && !strstr(data,".")) {
            offset = 1;
            strcpy(p_page->p_srv->server, data);
         }
      }
   }
#endif

   mg_request_header(p_page->p_srv, p_buf, command, MG_PRODUCT);
   strcpy(p_page->p_srv->server, p_page->server_base);

   return offset;
}


void * mg_ext_malloc(unsigned long size)
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

   return p;
}


void * mg_ext_realloc(void *p_buffer, unsigned long size)
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
   return p;
}


int mg_ext_free(void *p_buffer)
{
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


int mg_log_request(MGPAGE *p_page, char *function)
{
   char *p;
   char buffer[32];

   MG_PHP_GLOBAL(fun_no) ++;
   p_page->p_log->req_no = MG_PHP_GLOBAL(req_no);
   p_page->p_log->fun_no = MG_PHP_GLOBAL(fun_no);
   p_page->p_log->log_errors = 0;
   p_page->p_log->log_functions = 0;
   p_page->p_log->log_transmissions = 0;
   if (!p_page->p_log->log_level[0]) {
      return 0;
   }

   if (p_page->p_log->log_filter[0] && !strstr(p_page->p_log->log_filter, "*")) {
      sprintf(buffer, ",%s,", function);
      if (!strstr(p_page->p_log->log_filter, buffer)) {
         return 0;
      }
   }
   p = p_page->p_log->log_level;
   while (*p) {
      if (*p == 'e')
         p_page->p_log->log_errors = 1;
      else if (*p == 'f')
         p_page->p_log->log_functions = 1;
      else if (*p == 't')
         p_page->p_log->log_transmissions = 1;
      p ++;
   }

   p_page->p_srv->p_log = p_page->p_log;

   if (p_page->p_log->log_functions) {  
      mg_log_event(p_page->p_log, function, "function called", 0);
   }
   return 1;
}

