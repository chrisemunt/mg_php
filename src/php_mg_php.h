/*
   ----------------------------------------------------------------------------
   | mg_php                                                                   |
   | Description: PHP Extension for M/Cache/IRIS                              |
   | Author:      Chris Munt cmunt@mgateway.com                               |
   |                         chris.e.munt@gmail.com                           |
   | Copyright (c) 2019-2024 MGateway Ltd                                     |
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

#ifndef PHP_MG_PHP_H
#define PHP_MG_PHP_H

/* Define this symbol when building the old php_mgw DSOs */
/*
#define MG_PHP_MGW               1
*/

#if defined(_WIN32)
#include "php_version.h"
#endif

#if defined(_WIN32)
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

#if defined(MG_PHP_MGW)
#define PHP_MG_PHP_VERSION    "2.1.55"
#define MG_EXT_NAME           "m_php"
#if !defined(MG_DEFAULT_PORT)
#define MG_DEFAULT_PORT       7040
#endif
#else
#define PHP_MG_PHP_VERSION    "3.3.61"
#define MG_EXT_NAME           "mg_php"
#if !defined(MG_DEFAULT_PORT)
#define MG_DEFAULT_PORT       7041
#endif
#endif

#define MG_PHP_VERSION ((PHP_MAJOR_VERSION * 10000) + (PHP_MINOR_VERSION * 100) + PHP_RELEASE_VERSION)

extern zend_module_entry   mg_php_module_entry;
#define mg_php_module_ptr  &mg_php_module_entry
#define phpext_mg_php_ptr  mg_php_module_ptr

PHP_MINIT_FUNCTION(mg_php);
PHP_MSHUTDOWN_FUNCTION(mg_php);
PHP_RINIT_FUNCTION(mg_php);
PHP_RSHUTDOWN_FUNCTION(mg_php);
PHP_MINFO_FUNCTION(mg_php);

static PHP_FUNCTION(m_test);
static PHP_FUNCTION(m_dump_trace);
static PHP_FUNCTION(m_ext_version);
static PHP_FUNCTION(m_set_log_level);
static PHP_FUNCTION(m_set_error_mode);
static PHP_FUNCTION(m_set_storage_mode);
static PHP_FUNCTION(m_set_timeout);
static PHP_FUNCTION(m_set_no_retry);
static PHP_FUNCTION(m_set_host);
static PHP_FUNCTION(m_set_server);
static PHP_FUNCTION(m_set_uci);
#if !defined(MG_PHP_MGW)
static PHP_FUNCTION(m_bind_server_api);
static PHP_FUNCTION(m_release_server_api);
#endif
static PHP_FUNCTION(m_get_last_error);
static PHP_FUNCTION(m_set);
static PHP_FUNCTION(m_get);
static PHP_FUNCTION(m_delete);
static PHP_FUNCTION(m_kill);
static PHP_FUNCTION(m_defined);
static PHP_FUNCTION(m_data);
static PHP_FUNCTION(m_order);
static PHP_FUNCTION(m_previous);
static PHP_FUNCTION(m_increment);
static PHP_FUNCTION(m_tstart);
static PHP_FUNCTION(m_tlevel);
static PHP_FUNCTION(m_tcommit);
static PHP_FUNCTION(m_trollback);
static PHP_FUNCTION(m_sleep);
static PHP_FUNCTION(m_html);
static PHP_FUNCTION(m_html_method);
static PHP_FUNCTION(m_http);
static PHP_FUNCTION(m_function);
static PHP_FUNCTION(m_proc);
static PHP_FUNCTION(m_proc_ex);
static PHP_FUNCTION(m_proc_byref);
static PHP_FUNCTION(m_classmethod);
static PHP_FUNCTION(m_method);
static PHP_FUNCTION(m_method_byref);
static PHP_FUNCTION(m_merge_to_db);
static PHP_FUNCTION(m_merge_from_db);
static PHP_FUNCTION(m_return_to_applet);
static PHP_FUNCTION(m_return_to_client);
static PHP_FUNCTION(m_array_test);

#endif /* PHP_MG_PHP_H */
