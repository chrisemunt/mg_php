/*
   ----------------------------------------------------------------------------
   | mg_dba.so|dll                                                            |
   | Description: An abstraction of the InterSystems Cache/IRIS API           |
   |              and YottaDB API                                             |
   | Author:      Chris Munt cmunt@mgateway.com                               |
   |                         chris.e.munt@gmail.com                           |
   | Copyright (c) 2017-2021 M/Gateway Developments Ltd,                      |
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
   Development Diary (in brief):

Version 1.0.1 20 April 2017:
   First release.

Version 1.0.2 3 February 2018:
   Access to Cache Objects added.

Version 1.0.3 3 July 2019:
   Open Source release.

Version 1.1.4 10 September 2019:
   Network based connectivity option.

Version 1.1.5 8 January 2020:
   Add compatibility shim layer for the old MGWSI protocols.
   Provide an embedded option to allow this code to be used for mg_python et al.

Version 1.1.6 17 January 2020:
   Introduce option in the old %ZMGWSIS protocol to connecto the database via its C API
   - Interface function: ifc^%zmgsis(context, request, parameters) and IFC^%ZMGWSIS(context, request, parameters)

Version 1.1.7 10 February 2020:
   Add a universal event log facility.  Remove the old interactive 'debug' facility.

Version 1.2.8 15 February 2021:
   Introduce support for M transaction processing: tstart, $tlevel, tcommit, trollback.
   Correct a fault that occasionally led to failures when returning long strings to Node.js from the DB Server.
	- This fault only affected network based connectivity to the DB Server.
   Allow a DB Server response timeout to be set for network based connectivity.

Version 1.2.9 14 March 2021:
   Introduce support for YottaDB Transaction Processing over API based connectivity.
   - This functionality was previously only available over network-based connectivity to YottaDB.
*/


#include "mg_dbasys.h"
#include "mg_dba.h"

#if !defined(_WIN32)
extern int errno;
#endif

static NETXSOCK      netx_so        = {0, 0, 0, 0, 0, 0, 0, {'\0'}};
static DBXCON *      connection[DBX_MAXCONS];

MG_MALLOC            dbx_ext_malloc = NULL;
MG_REALLOC           dbx_ext_realloc = NULL;
MG_FREE              dbx_ext_free = NULL;

#if defined(_WIN32)
CRITICAL_SECTION  dbx_global_mutex;
#else
pthread_mutex_t   dbx_global_mutex  = PTHREAD_MUTEX_INITIALIZER;
#endif


#if defined(_WIN32) && defined(MG_DBA_DSO)
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
#endif /* #if defined(_WIN32) && defined(MG_DBA_DSO) */


DBX_EXTFUN(int) dbx_init()
{
   int n;

   for (n = 0; n < DBX_MAXCONS; n ++) {
      connection[n] = NULL;
   }

   return 0;
}


DBX_EXTFUN(int) dbx_version(int index, char *output, int output_len)
{
   DBXMETH *pmeth;
   DBXCON *pcon;

   pcon = NULL;
   if (index >= 0 && index < DBX_MAXCONS) {
      pcon = connection[index];
   }
   pmeth = (DBXMETH *) pcon->pmeth_base;

   sprintf((char *) output, "mg_dba:%s", DBX_VERSION);

   if (pcon) {
      if (pcon->p_zv->version[0]) {
         if (pcon->p_zv->product == DBX_DBTYPE_YOTTADB)
            strcat((char *) output, "; YottaDB:");
         else if (pcon->p_zv->product == DBX_DBTYPE_IRIS)
            strcat((char *) output, "; InterSystems IRIS:");
         else
            strcat((char *) output, "; InterSystems Cache:");
         strcat((char *) output, pcon->p_zv->version);
      }

      mg_create_string(pmeth, (void *) output, DBX_DTYPE_STR);
   }

   return 0;
}


DBX_EXTFUN(int) dbx_open(unsigned char *input, unsigned char *output)
{
   int rc, n, chndle, len, error_code;
   char buffer[1024], nstr[64];
   DBXMETH *pmeth;
   DBXCON *pcon;
   char *p, *p1, *p2;

   chndle = -1;
   pcon = (DBXCON *) mg_malloc(sizeof(DBXCON), 0);
   if (!pcon) {
      mg_set_error_message_ex(output ? output : input, "Unable to allocate memory for the connection");
      return 0;
   }
   memset((void *) pcon, 0, sizeof(DBXCON));
   pmeth = (DBXMETH *) mg_malloc(sizeof(DBXMETH), 0);
   if (!pmeth) {
      mg_free((void *) pcon, 0);
      mg_set_error_message_ex(output ? output : input, "Unable to allocate memory for the request");
      return 0;
   }
   memset((void *) pmeth, 0, sizeof(DBXMETH));
   pcon->pmeth_base = (void *) pmeth;
   pmeth->pcon = pcon;

   mg_enter_critical_section((void *) &dbx_global_mutex);
   for (n = 0; n < DBX_MAXCONS; n ++) {
      if (!connection[n]) {
         chndle = n;
         connection[chndle] = pcon;
         pcon->chndle = chndle;
         break;
      }
   }
   mg_leave_critical_section((void *) &dbx_global_mutex);

   if (chndle < 0) {
      mg_set_error_message_ex(output ? output : input, "Connection table full");
      mg_free(pcon, 0);
      return 0;
   }

   pcon->tlevel = 0;
   pcon->p_isc_so = NULL;
   pcon->p_ydb_so = NULL;
   pcon->p_srv = NULL;
   pcon->p_log = &pcon->log;
   pcon->p_db_mutex = &pcon->db_mutex;
   mg_mutex_create(pcon->p_db_mutex);
   pcon->p_zv = &pcon->zv;

   mg_log_init(pcon->p_log);

   pmeth->input_str.len_used = 0;

   pmeth->output_val.svalue.len_alloc = 1024;
   pmeth->output_val.svalue.len_used = 0;
   pmeth->output_val.offset = 0;
   pmeth->output_val.type = DBX_DTYPE_DBXSTR;

   for (n = 0; n < DBX_MAXARGS; n ++) {
      pmeth->args[n].type = DBX_DTYPE_NONE;
      pmeth->args[n].cvalue.pstr = NULL;
   }

   mg_unpack_header(input, output);
   mg_unpack_arguments(pmeth);
/*
   printf("\ndbx_open : pcon->p_isc_so->libdir=%s; pcon->p_isc_so->libnam=%s; pcon->p_isc_so->loaded=%d; pcon->p_isc_so->functions_enabled=%d; pcon->p_isc_so->merge_enabled=%d;\n", pcon->p_isc_so->libdir, pcon->p_isc_so->libnam, pcon->p_isc_so->loaded, pcon->p_isc_so->functions_enabled, pcon->p_isc_so->merge_enabled);
   printf("\ndbx_open : pcon->p_ydb_so->loaded=%d;\n", pcon->p_ydb_so->loaded);
*/


   pcon->pid = 0;

   error_code = 0;

   pcon->dbtype = 0;
   pcon->shdir[0] = '\0';
   pcon->ip_address[0] = '\0';
   pcon->port = 0;
   pcon->timeout = NETX_TIMEOUT;

   for (n = 0; n < pmeth->argc; n ++) {

      p = (char *) pmeth->args[n].svalue.buf_addr;
      len = pmeth->args[n].svalue.len_used;

      switch (n) {
         case 0:
            len = (len < 30) ? len : 30;
            strncpy(buffer, p, len);
            buffer[len] = '\0';
            mg_lcase(buffer);

            if (!strcmp(buffer, "cache"))
               pcon->dbtype = DBX_DBTYPE_CACHE;
            else if (!strcmp(buffer, "iris"))
               pcon->dbtype = DBX_DBTYPE_IRIS;
            else if (!strcmp(buffer, "yottadb"))
               pcon->dbtype = DBX_DBTYPE_YOTTADB;
            break;
         case 1:
            len = (len < 250) ? len : 250;
            strncpy(pcon->shdir, p, len);
            pcon->shdir[len] = '\0';
            break;
         case 2:
            len = (len < 120) ? len : 120;
            strncpy(pcon->ip_address, p, len);
            pcon->ip_address[len] = '\0';
            break;
         case 3:
            len = (len < 32) ? len : 32;
            strncpy(buffer, p, len);
            buffer[len] = '\0';
            pcon->port = (int) strtol(buffer, NULL, 10);
            break;
         case 4:
            len = (len < 60) ? len : 60;
            strncpy(pcon->username, p, len);
            pcon->username[len] = '\0';
            break;
         case 5:
            len = (len < 60) ? len : 60;
            strncpy(pcon->password, p, len);
            pcon->password[len] = '\0';
            break;
         case 6:
            len = (len < 60) ? len : 60;
            strncpy(pcon->nspace, p, len);
            pcon->nspace[len] = '\0';
            break;
         case 7:
            len = (len < 60) ? len : 60;
            strncpy(pcon->input_device, p, len);
            pcon->input_device[len] = '\0';
            break;
         case 8:
            len = (len < 60) ? len : 60;
            strncpy(pcon->output_device, p, len);
            pcon->output_device[len] = '\0';
            break;
         case 9:
            len = (len < 60) ? len : 60;
            strncpy(pcon->debug_str, p, len);
            if (len) {
               pcon->p_log->log_errors = 1;
            }
            break;
         case 10:
            len = (len < 1020) ? len : 1020;
            strncpy(buffer, p, len);
            break;
         case 11:
            len = (len < 60) ? len : 60;
            strncpy(pcon->server, p, len);
            pcon->server[len] = '\0';
            break;
         case 12:
            len = (len < 60) ? len : 60;
            strncpy(pcon->server_software, p, len);
            pcon->server_software[len] = '\0';
            mg_lcase(pcon->server_software);
            break;
         case 13:
            len = (len < 32) ? len : 32;
            strncpy(nstr, p, len);
            nstr[len] = '\0';
            pcon->timeout = (int) strtol(nstr, NULL, 10);
            if (pcon->timeout < 1) {
               pcon->timeout = NETX_TIMEOUT;
            }
            break;

         default:
            break;
      }
   }

   p = buffer;
   p2 = p;
   while ((p2 = strstr(p, "\n"))) {
      *p2 = '\0';
      p1 = strstr(p, "=");
      if (p1) {
         *p1 = '\0';
         p1 ++;
#if defined(_WIN32)
         SetEnvironmentVariable((LPCTSTR) p, (LPCTSTR) p1);
#else
         /* printf("\nLinux : environment variable p=%s p1=%s;", p, p1); */
         setenv(p, p1, 1);
#endif
      }
      else {
         break;
      }
      p = p2 + 1;
   }

   if (!pcon->dbtype) {
      strcpy(pcon->error, "Unable to determine the database type");
      rc = CACHE_NOCON;
      goto dbx_open_exit;
   }

   if (!pcon->shdir[0] && pcon->ip_address[0] && pcon->port) {

      if (strstr(pcon->server_software, "zmgwsi")) {
         pcon->p_srv = (void *) mg_malloc(sizeof(MGSRV), 0);
         memset((void *) pcon->p_srv, 0, sizeof(MGSRV));
         ((MGSRV *) pcon->p_srv)->mem_error = 0;
         ((MGSRV *) pcon->p_srv)->mode = 0;
         ((MGSRV *) pcon->p_srv)->storage_mode = 0;
         ((MGSRV *) pcon->p_srv)->timeout = NETX_TIMEOUT;

         if (pcon->server[0])
            strcpy(((MGSRV *) pcon->p_srv)->server, pcon->server);
         else
            strcpy(((MGSRV *) pcon->p_srv)->server, MG_SERVER);
         if (pcon->nspace[0])
            strcpy(((MGSRV *) pcon->p_srv)->uci, pcon->nspace);
         else
            strcpy(((MGSRV *) pcon->p_srv)->uci, MG_UCI);
         if (pcon->ip_address[0])
            strcpy(((MGSRV *) pcon->p_srv)->ip_address, pcon->ip_address);
         else
            strcpy(((MGSRV *) pcon->p_srv)->ip_address, MG_HOST);
         if (pcon->port)
            ((MGSRV *) pcon->p_srv)->port = pcon->port;
         else
            ((MGSRV *) pcon->p_srv)->port = MG_PORT;

         strcpy(((MGSRV *) pcon->p_srv)->username, "");
         strcpy(((MGSRV *) pcon->p_srv)->password, "");

         for (n = 0; n < MG_MAXCON; n ++) {
            ((MGSRV *) pcon->p_srv)->pcon[n] = NULL;
         }
         ((MGSRV *) pcon->p_srv)->pcon[chndle] = pcon;

         rc = netx_tcp_connect(pcon, 0);
         if (rc != CACHE_SUCCESS) {
            pcon->connected = 0;
            rc = CACHE_NOCON;
            mg_error_message(pmeth, rc);
            return rc;
         }

         pcon->connected = 2; /* network connection for old protocol */
         mg_create_string(pmeth, (void *) &rc, DBX_DTYPE_INT);

         return rc;
      }

      rc = netx_tcp_connect(pcon, 0);
      if (rc != CACHE_SUCCESS) {
         pcon->connected = 0;
         rc = CACHE_NOCON;
         mg_error_message(pmeth, rc);
         return rc;
      }
 
      rc = netx_tcp_handshake(pcon, 0);

      if (rc != CACHE_SUCCESS) {
         pcon->connected = 0;
         rc = CACHE_NOCON;
         mg_error_message(pmeth, rc);
         return rc;
      }

      pcon->connected = 2; /* network connection */
      mg_create_string(pmeth, (void *) &rc, DBX_DTYPE_INT);

      return 0;
   }

   if (!pcon->shdir[0]) {
      strcpy(pcon->error, "Unable to determine the path to the database installation");
      rc = CACHE_NOCON;
      goto dbx_open_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_open(pmeth);
   }
   else {
/*
      if (!pcon->nspace[0])
            strcpy(((MGSRV *) pcon->p_srv)->uci, pcon->nspace);
         else
            strcpy(((MGSRV *) pcon->p_srv)->uci, MG_UCI);
*/
      rc = isc_open(pmeth);
   }

dbx_open_exit:

   if (rc == CACHE_SUCCESS) {
      pcon->connected = 1;
      mg_create_string(pmeth, (void *) &rc, DBX_DTYPE_INT);
   }
   else {
      pcon->connected = 0;
      mg_error_message(pmeth, rc);
   }

   return 0;
}


DBX_EXTFUN(int) dbx_close(unsigned char *input, unsigned char *output)
{
   int rc, rc1, narg;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->chndle >= 0 && pcon->chndle < DBX_MAXCONS) {
      connection[pcon->chndle] = NULL;
   }

   narg = mg_unpack_arguments(pmeth);

   if (pcon->connected == 2) {
      if (pcon->p_srv) {
         rc = 0;
      }
      else {
         strcpy(pmeth->command, "*");
         rc = netx_tcp_command(pmeth, 0);
      }
      rc1 = netx_tcp_disconnect(pcon, 0);
      goto dbx_close_tcp;
   }

   rc1 = 0;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->p_ydb_so->loaded) {
         rc = pcon->p_ydb_so->p_ydb_exit();
         /* printf("\r\np_ydb_exit=%d\r\n", rc); */
      }

      strcpy(pcon->error, "");
      mg_create_string(pmeth, (void *) "1", DBX_DTYPE_STR);
/*
      mg_dso_unload(pcon->p_ydb_so->p_library); 
      pcon->p_ydb_so->p_library = NULL;
      pcon->p_ydb_so->loaded = 0;
*/
      strcpy(pcon->p_ydb_so->libdir, "");
      strcpy(pcon->p_ydb_so->libnam, "");

   }
   else {
      if (pcon->p_isc_so->loaded) {

         DBX_LOCK(rc, 0);

         rc = pcon->p_isc_so->p_CacheEnd();
         rc1 = rc;

         DBX_UNLOCK(rc);
      }

      strcpy(pcon->error, "");
      mg_create_string(pmeth, (void *) "1", DBX_DTYPE_STR);

      mg_dso_unload(pcon->p_isc_so->p_library); 

      pcon->p_isc_so->p_library = NULL;
      pcon->p_isc_so->loaded = 0;

      strcpy(pcon->p_isc_so->libdir, "");
      strcpy(pcon->p_isc_so->libnam, "");
   }

dbx_close_tcp:

   strcpy(pcon->p_zv->version, "");

   strcpy(pcon->shdir, "");
   strcpy(pcon->username, "");
   strcpy(pcon->password, "");
   strcpy(pcon->nspace, "");
   strcpy(pcon->input_device, "");
   strcpy(pcon->output_device, "");
   strcpy(pcon->debug_str, "");

   rc = mg_mutex_destroy(pcon->p_db_mutex);

   rc = CACHE_SUCCESS;
   if (rc == CACHE_SUCCESS) {
      mg_create_string(pmeth, (void *) &rc, DBX_DTYPE_INT);
   }
   else {
      mg_error_message(pmeth, rc);
   }

   return 0;
}


DBX_EXTFUN(int) dbx_set(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "S");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_set_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_set_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_set_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_set_ex(pmeth);
      }
   }
   else {
      rc = dbx_set_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_set_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_set_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_set_s(&(pmeth->args[0].svalue), pmeth->argc - 2, &pmeth->yargs[0], &(pmeth->args[pmeth->argc - 1].svalue));
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalSet(pmeth->argc - 2);
   }

   mg_create_string(pmeth, (void *) &rc, DBX_DTYPE_INT);

   return rc;
}


DBX_EXTFUN(int) dbx_get(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "G");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_get_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_get_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_get_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_get_ex(pmeth);
      }
   }
   else {
      rc = dbx_get_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_get_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_get_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      pmeth->output_val.svalue.len_used = 0;
      pmeth->output_val.svalue.buf_addr += 5;

      rc = pcon->p_ydb_so->p_ydb_get_s(&(pmeth->args[0].svalue), pmeth->argc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));

      pmeth->output_val.svalue.buf_addr -= 5;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->output_val.svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalGet(pmeth->argc - 1, 0); /* 1 for no 'undefined' */
      if (rc == CACHE_ERUNDEF) {
         mg_create_string(pmeth, (void *) "", DBX_DTYPE_STR);
         rc = CACHE_SUCCESS;
      }
      else if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
      }
      else {
         mg_create_string(pmeth, (void *) "", DBX_DTYPE_STR);
         rc = CACHE_FAILURE;
      }
   }

   return rc;
}


DBX_EXTFUN(int) dbx_next(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "O");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_next_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_next_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_next_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_next_ex(pmeth);
      }
   }
   else {
      rc = dbx_next_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_next_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_next_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      pmeth->output_val.svalue.len_used = 0;
      pmeth->output_val.svalue.buf_addr += 5;

      rc = pcon->p_ydb_so->p_ydb_subscript_next_s(&(pmeth->args[0].svalue), pmeth->argc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));

      pmeth->output_val.svalue.buf_addr -= 5;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->output_val.svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pmeth->argc - 1, 1, 0);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
      }
   }

   return rc;
}


DBX_EXTFUN(int) dbx_previous(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "P");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_previous_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_previous_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_previous_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_previous_ex(pmeth);
      }
   }
   else {
      rc = dbx_previous_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_previous_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_previous_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      pmeth->output_val.svalue.len_used = 0;
      pmeth->output_val.svalue.buf_addr += 5;

      rc = pcon->p_ydb_so->p_ydb_subscript_previous_s(&(pmeth->args[0].svalue), pmeth->argc - 1, &pmeth->yargs[0], &(pmeth->output_val.svalue));

      pmeth->output_val.svalue.buf_addr -= 5;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->output_val.svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalOrder(pmeth->argc - 1, -1, 0);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
      }
   }

   return rc;
}


DBX_EXTFUN(int) dbx_delete(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "K");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_delete_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_delete_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_delete_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_delete_ex(pmeth);
      }
   }
   else {
      rc = dbx_delete_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_delete_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_delete_ex(DBXMETH *pmeth)
{
   int rc, n;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_delete_s(&(pmeth->args[0].svalue), pmeth->argc - 1, &pmeth->yargs[0], YDB_DEL_TREE);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalKill(pmeth->argc - 1, 0);
   }

   if (rc == CACHE_SUCCESS) {
/*
      printf("\r\n defined  pmeth->output_val.offset=%lu;\r\n",  pmeth->output_val.offset);
*/
      pmeth->output_val.num.int32 = rc;
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", rc);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }

   return rc;
}


DBX_EXTFUN(int) dbx_defined(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "D");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_defined_exit;
   }

   pmeth->increment = 0;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_defined_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_defined_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_defined_ex(pmeth);
      }
   }
   else {
      rc = dbx_defined_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_defined_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_defined_ex(DBXMETH *pmeth)
{
   int rc, n;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = pcon->p_ydb_so->p_ydb_data_s(&(pmeth->args[0].svalue), pmeth->argc - 1, &pmeth->yargs[0], (unsigned int *) &n);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalData(pmeth->argc - 1, 0);
      if (rc == CACHE_SUCCESS) {
         pcon->p_isc_so->p_CachePopInt(&n);
      }
   }

   if (rc == CACHE_SUCCESS) {
      pmeth->output_val.num.int32 = n;
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", n);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }

   return rc;
}


DBX_EXTFUN(int) dbx_increment(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "I");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_increment_exit;
   }

   pmeth->increment = 1;
   rc = mg_global_reference(pmeth);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_increment_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_increment_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_increment_ex(pmeth);
      }
   }
   else {
      rc = dbx_increment_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_increment_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_increment_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      pmeth->output_val.svalue.len_used = 0;
      pmeth->output_val.svalue.buf_addr += 5;

      rc = pcon->p_ydb_so->p_ydb_incr_s(&(pmeth->args[0].svalue), pmeth->argc - 2, &pmeth->yargs[0], &(pmeth->args[pmeth->argc - 1].svalue), &(pmeth->output_val.svalue));

      pmeth->output_val.svalue.buf_addr -= 5;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->output_val.svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      rc = pcon->p_isc_so->p_CacheGlobalIncrement(pmeth->argc - 2);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
      }
   }

   return rc;
}


/* v1.2.8 */
DBX_EXTFUN(int) dbx_tstart(unsigned char *input, unsigned char *output)
{
   int rc, n;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   mg_unpack_arguments(pmeth);

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "a");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_tstart_exit;
   }

   rc = dbx_tstart_ex(pmeth);

   if (rc == CACHE_SUCCESS) {
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", rc);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_tstart_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_tstart_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_transaction(pmeth); /* v1.2.9 */
      pmeth->output_val.num.int32 = rc;

/*
      mg_set_error_message_ex(output ? output : input, "Transaction Processing only available over network based connectivity for YottaDB");
      rc = CACHE_FAILURE;
*/
   }
   else {
      rc = pcon->p_isc_so->p_CacheTStart();
      pmeth->output_val.num.int32 = rc;
   }

   return rc;
}


DBX_EXTFUN(int) dbx_tlevel(unsigned char *input, unsigned char *output)
{
   int rc, n;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   mg_unpack_arguments(pmeth);

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "b");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_tlevel_exit;
   }

   rc = dbx_tlevel_ex(pmeth);

   if (pmeth->output_val.num.int32 >= 0) {
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", pmeth->output_val.num.int32);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_tlevel_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_tlevel_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      if (pcon->tlevel > 0) {
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_TLEVEL);
      }
      else {
         rc = 0;
         pmeth->output_val.num.int32 = rc;
      }

/*
      mg_set_error_message_ex(output ? output : input, "Transaction Processing only available over network based connectivity for YottaDB");
      rc = CACHE_FAILURE;
*/
   }
   else {
      rc = pcon->p_isc_so->p_CacheTLevel();
      pmeth->output_val.num.int32 = rc;
   }

   return rc;
}


DBX_EXTFUN(int) dbx_tcommit(unsigned char *input, unsigned char *output)
{
   int rc, n;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   mg_unpack_arguments(pmeth);

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "c");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_tcommit_exit;
   }

   rc = dbx_tcommit_ex(pmeth);

   if (rc == CACHE_SUCCESS) {
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", rc);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_tcommit_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_tcommit_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_COMMIT);
         pmeth->output_val.num.int32 = rc;
      }
      else {
         rc = CACHE_FAILURE;
         pmeth->output_val.num.int32 = rc;
      }

/*
      mg_set_error_message_ex(output ? output : input, "Transaction Processing only available over network based connectivity for YottaDB");
      rc = CACHE_FAILURE;
*/
   }
   else {
      rc = pcon->p_isc_so->p_CacheTCommit();
      pmeth->output_val.num.int32 = rc;
   }

   return rc;
}


DBX_EXTFUN(int) dbx_trollback(unsigned char *input, unsigned char *output)
{
   int rc, n;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   mg_unpack_arguments(pmeth);

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "d");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_trollback_exit;
   }

   rc = dbx_trollback_ex(pmeth);

   if (rc == CACHE_SUCCESS) {
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, "%d", rc);
      n = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset);
      pmeth->output_val.svalue.len_used += n;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_trollback_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_trollback_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_ROLLBACK);
         pmeth->output_val.num.int32 = rc;
      }
      else {
         rc = CACHE_FAILURE;
         pmeth->output_val.num.int32 = rc;
      }

/*
      mg_set_error_message_ex(output ? output : input, "Transaction Processing only available over network based connectivity for YottaDB");
      rc = CACHE_FAILURE;
*/
   }
   else {
      rc = pcon->p_isc_so->p_CacheTRollback(0);
      pmeth->output_val.num.int32 = rc;
   }

   return rc;
}


DBX_EXTFUN(int) dbx_function(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXFUN fun;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   fun.rflag = 0;

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "X");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_function_exit;
   }

   rc = mg_function_reference(pmeth, &fun);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_function_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->tlevel > 0) {
         pmeth->p_dbxfun = (int (*) (struct tagDBXMETH * pmeth)) dbx_function_ex;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_DB);
      }
      else {
         rc = dbx_function_ex(pmeth);
      }
   }
   else {
      rc = dbx_function_ex(pmeth);
   }

   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
   }

dbx_function_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_function_ex(DBXMETH *pmeth)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {

      pmeth->output_val.svalue.len_used = 0;
      pmeth->output_val.svalue.buf_addr += 5;

      rc = ydb_function(pmeth, pmeth->pfun);

      pmeth->output_val.svalue.buf_addr -= 5;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->output_val.svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      rc = pcon->p_isc_so->p_CacheExtFun(pmeth->pfun->rflag, pmeth->argc - 1);
      if (rc == CACHE_SUCCESS) {
         isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
      }
   }

   return rc;
}


DBX_EXTFUN(int) dbx_classmethod(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache objects are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "x");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_classmethod_exit;
   }

   rc = mg_class_reference(pmeth, 0);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_classmethod_exit;
   }

   rc = pcon->p_isc_so->p_CacheInvokeClassMethod(pmeth->argc - 2);

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_classmethod_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_method(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache objects are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "*");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_method_exit;
   }

   rc = mg_class_reference(pmeth, 1);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_method_exit;
   }

   rc = pcon->p_isc_so->p_CacheInvokeMethod(pmeth->argc - 2);

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_method_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_getproperty(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache objects are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "*");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_getproperty_exit;
   }

   rc = mg_class_reference(pmeth, 2);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_getproperty_exit;
   }

   rc = pcon->p_isc_so->p_CacheGetProperty();

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_getproperty_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_setproperty(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache objects are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "*");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_setproperty_exit;
   }

   rc = mg_class_reference(pmeth, 2);
   if (rc != CACHE_SUCCESS) {
      mg_error_message(pmeth, rc);
      goto dbx_setproperty_exit;
   }

   rc = pcon->p_isc_so->p_CacheSetProperty();

   if (rc == CACHE_SUCCESS) {
      mg_create_string(pmeth, (void *) "", DBX_DTYPE_STR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_setproperty_exit:

   DBX_UNLOCK(rc);

   return 0;
}



DBX_EXTFUN(int) dbx_closeinstance(unsigned char *input, unsigned char *output)
{
   int rc;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache objects are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "*");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_closeinstance_exit;
   }

   rc = mg_class_reference(pmeth, 3);

   if (rc == CACHE_SUCCESS) {
      isc_pop_value(pcon, &(pmeth->output_val), DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_closeinstance_exit:

   DBX_UNLOCK(rc);

   return 0;
}


DBX_EXTFUN(int) dbx_getnamespace(unsigned char *input, unsigned char *output)
{
   int rc;
   CACHE_ASTR retval;
   CACHE_ASTR expr;
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache Namespace operations are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   if (pcon->connected != 2 && pcon->p_isc_so && pcon->p_isc_so->p_CacheEvalA == NULL) {
      pcon->error_code = 4020;
      strcpy(pcon->error, "Cache Namespace operations are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   strcpy((char *) expr.str, "$Namespace");
   expr.len = (unsigned short) strlen((char *) expr.str);

   mg_unpack_arguments(pmeth);

   DBX_LOCK(rc, 0);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "gns");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_getnamespace_exit;
   }

   rc = pcon->p_isc_so->p_CacheEvalA(&expr);

   if (rc == CACHE_SUCCESS) {
      retval.len = 256;
      rc = pcon->p_isc_so->p_CacheConvert(CACHE_ASTRING, &retval);

      if (retval.len < pmeth->output_val.svalue.len_alloc) {

         strncpy((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, (char *) retval.str, retval.len);
         pmeth->output_val.svalue.buf_addr[retval.len + pmeth->output_val.offset] = '\0';
         pmeth->output_val.svalue.len_used += retval.len;
         mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) retval.len, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
      }
      else {
         rc = CACHE_BAD_NAMESPACE;
         mg_error_message(pmeth, rc);
      }
   }
   else {
      mg_error_message(pmeth, rc);
   }

dbx_getnamespace_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_setnamespace(unsigned char *input, unsigned char *output)
{
   int rc;
   char nspace[128];
   DBXMETH *pmeth;
   DBXCON *pcon;

   pmeth = mg_unpack_header(input, output);
   pcon = pmeth->pcon;

   if (!pcon || !pcon->connected) {
      mg_set_error_message_ex(output ? output : input, "No Database Connection");
      return 1;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      pcon->error_code = 2020;
      strcpy(pcon->error, "Cache Namespace operations are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   if (pcon->connected != 2 && pcon->p_isc_so && pcon->p_isc_so->p_CacheExecuteA == NULL) {
      pcon->error_code = 4020;
      strcpy(pcon->error, "Cache Namespace operations are not available with this platform");
      mg_set_error_message(pmeth);
      return 0;
   }

   mg_unpack_arguments(pmeth);

   if (pcon->connected == 2) {
      strcpy(pmeth->command, "sns");
      rc = netx_tcp_command(pmeth, 0);
      goto dbx_setnamespace_exit;
   }

   DBX_LOCK(rc, 0);

   *nspace = '\0';
   if (pmeth->argc > 0 && pmeth->args[0].svalue.len_used < 120) {
      strncpy(nspace, (char *) pmeth->args[0].svalue.buf_addr, pmeth->args[0].svalue.len_used);
      nspace[pmeth->args[0].svalue.len_used] = '\0';
   }

   rc = isc_change_namespace(pcon, nspace);

   if (rc == CACHE_SUCCESS) {
      strncpy((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.offset, (char *) nspace, pmeth->args[0].svalue.len_used);
      pmeth->output_val.svalue.buf_addr[pmeth->args[0].svalue.len_used + pmeth->output_val.offset] = '\0';
      pmeth->output_val.svalue.len_used += pmeth->args[0].svalue.len_used;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) pmeth->args[0].svalue.len_used, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);
   }
   else {
      mg_error_message(pmeth, rc);
   }


dbx_setnamespace_exit:

   DBX_UNLOCK(rc);

   mg_cleanup(pmeth);

   return 0;
}


DBX_EXTFUN(int) dbx_sleep(int period_ms)
{
   unsigned long msecs;

   msecs = (unsigned long) period_ms;

   mg_sleep(msecs);

   return 0;

}


DBX_EXTFUN(int) dbx_benchmark(unsigned char *inputstr, unsigned char *outputstr)
{
   strcpy((char *) inputstr, "Output String");
   return 0;
}


int isc_load_library(DBXCON *pcon)
{
   int n, len, result;
   char primlib[DBX_ERROR_SIZE], primerr[DBX_ERROR_SIZE];
   char verfile[256], fun[64];
   char *libnam[16];

   strcpy(pcon->p_isc_so->libdir, pcon->shdir);
   strcpy(pcon->p_isc_so->funprfx, "Cache");
   strcpy(pcon->p_isc_so->dbname, "Cache");

   strcpy(verfile, pcon->shdir);
   len = (int) strlen(pcon->p_isc_so->libdir);
   if (pcon->p_isc_so->libdir[len - 1] == '/' || pcon->p_isc_so->libdir[len - 1] == '\\') {
      pcon->p_isc_so->libdir[len - 1] = '\0';
      len --;
   }
   for (n = len - 1; n > 0; n --) {
      if (pcon->p_isc_so->libdir[n] == '/') {
         strcpy((pcon->p_isc_so->libdir + (n + 1)), "bin/");
         break;
      }
      else if (pcon->p_isc_so->libdir[n] == '\\') {
         strcpy((pcon->p_isc_so->libdir + (n + 1)), "bin\\");
         break;
      }
   }

   /* printf("version=%s;\n", pcon->p_zv->version); */

   n = 0;
   if (pcon->dbtype == DBX_DBTYPE_IRIS) {
#if defined(_WIN32)
      libnam[n ++] = (char *) DBX_IRIS_DLL;
      libnam[n ++] = (char *) DBX_CACHE_DLL;
#else
#if defined(MACOSX)
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
#else
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
#endif
#endif
   }
   else {
#if defined(_WIN32)
      libnam[n ++] = (char *) DBX_CACHE_DLL;
      libnam[n ++] = (char *) DBX_IRIS_DLL;
#else
#if defined(MACOSX)
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
#else
      libnam[n ++] = (char *) DBX_CACHE_SO;
      libnam[n ++] = (char *) DBX_CACHE_DYLIB;
      libnam[n ++] = (char *) DBX_IRIS_SO;
      libnam[n ++] = (char *) DBX_IRIS_DYLIB;
#endif
#endif
   }

   libnam[n ++] = NULL;
   strcpy(pcon->p_isc_so->libnam, pcon->p_isc_so->libdir);
   len = (int) strlen(pcon->p_isc_so->libnam);

   for (n = 0; libnam[n]; n ++) {
      strcpy(pcon->p_isc_so->libnam + len, libnam[n]);
      if (!n) {
         strcpy(primlib, pcon->p_isc_so->libnam);
      }

      pcon->p_isc_so->p_library = mg_dso_load(pcon->p_isc_so->libnam);

      if (pcon->p_isc_so->p_library) {
         if (strstr(libnam[n], "iris")) {
            pcon->p_isc_so->iris = 1;
            strcpy(pcon->p_isc_so->funprfx, "Iris");
            strcpy(pcon->p_isc_so->dbname, "IRIS");
         }
         strcpy(pcon->error, "");
         pcon->error_code = 0;
         break;
      }

      if (!n) {
         int len1, len2;
         char *p;
#if defined(_WIN32)
         DWORD errorcode;
         LPVOID lpMsgBuf;

         lpMsgBuf = NULL;
         errorcode = GetLastError();
         sprintf(pcon->error, "Error loading %s Library: %s; Error Code : %ld", pcon->p_isc_so->dbname, primlib, errorcode);
         len2 = (int) strlen(pcon->error);
         len1 = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorcode,
                        /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPTSTR) &lpMsgBuf,
                        0,
                        NULL 
                        );
         if (lpMsgBuf && len1 > 0 && (DBX_ERROR_SIZE - len2) > 30) {
            strncpy(primerr, (const char *) lpMsgBuf, DBX_ERROR_SIZE - 1);
            p = strstr(primerr, "\r\n");
            if (p)
               *p = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            p = strstr(primerr, "%1");
            if (p) {
               *p = 'I';
               *(p + 1) = 't';
            }
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
         if (lpMsgBuf)
            LocalFree(lpMsgBuf);
#else
         p = (char *) dlerror();
         sprintf(primerr, "Cannot load %s library: Error Code: %d", pcon->p_isc_so->dbname, errno);
         len2 = strlen(pcon->error);
         if (p) {
            strncpy(primerr, p, DBX_ERROR_SIZE - 1);
            primerr[DBX_ERROR_SIZE - 1] = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
#endif
      }
   }

   if (!pcon->p_isc_so->p_library) {
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sSetDir", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSetDir = (int (*) (char *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheSetDir) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sSecureStartA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSecureStartA = (int (*) (CACHE_ASTRP, CACHE_ASTRP, CACHE_ASTRP, unsigned long, int, CACHE_ASTRP, CACHE_ASTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheSecureStartA) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sEnd", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEnd = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheEnd) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sExStrNew", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNew = (unsigned char * (*) (CACHE_EXSTRP, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrNew) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sExStrNewW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNewW = (unsigned short * (*) (CACHE_EXSTRP, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrNewW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sExStrNewH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrNewH = (wchar_t * (*) (CACHE_EXSTRP, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
/*
   if (!pcon->p_isc_so->p_CacheExStrNewH) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
*/
   sprintf(fun, "%sPushExStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStr = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushExStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushExStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStrW = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushExStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushExStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushExStrH = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPopExStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStr = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopExStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopExStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStrW = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopExStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopExStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopExStrH = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sExStrKill", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExStrKill = (int (*) (CACHE_EXSTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheExStrKill) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStr = (int (*) (int, Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStrW = (int (*) (int, short *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushStrH = (int (*) (int, wchar_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPopStr", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStr = (int (*) (int *, Callin_char_t **)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopStr) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopStrW", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStrW = (int (*) (int *, short **)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopStrW) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopStrH", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopStrH = (int (*) (int *, wchar_t **)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sPushDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushDbl = (int (*) (double)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushIEEEDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushIEEEDbl = (int (*) (double)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushIEEEDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopDbl", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopDbl = (int (*) (double *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopDbl) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushInt", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushInt = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushInt) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopInt", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopInt = (int (*) (int *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopInt) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sPushInt64", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushInt64 = (int (*) (CACHE_INT64)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushInt64) {
      pcon->p_isc_so->p_CachePushInt64 = (int (*) (CACHE_INT64)) pcon->p_isc_so->p_CachePushInt;
   }
   sprintf(fun, "%sPushInt64", pcon->p_isc_so->funprfx);
   if (!pcon->p_isc_so->p_CachePushInt64) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPopInt64", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopInt64 = (int (*) (CACHE_INT64 *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePopInt64) {
      pcon->p_isc_so->p_CachePopInt64 = (int (*) (CACHE_INT64 *)) pcon->p_isc_so->p_CachePopInt;
   }
   if (!pcon->p_isc_so->p_CachePopInt64) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sPushGlobal", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushGlobal = (int (*) (int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushGlobal) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushGlobalX", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushGlobalX = (int (*) (int, const Callin_char_t *, int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushGlobalX) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalGet", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalGet = (int (*) (int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalGet) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalSet", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalSet = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalSet) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalData", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalData = (int (*) (int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalData) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalKill", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalKill = (int (*) (int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalKill) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalOrder", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalOrder = (int (*) (int, int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalOrder) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalQuery", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalQuery = (int (*) (int, int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalQuery) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalIncrement", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalIncrement = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalIncrement) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sGlobalRelease", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGlobalRelease = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheGlobalRelease) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sAcquireLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAcquireLock = (int (*) (int, int, int, int *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheAcquireLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sReleaseAllLocks", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheReleaseAllLocks = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheReleaseAllLocks) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sReleaseLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheReleaseLock = (int (*) (int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CacheReleaseLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }
   sprintf(fun, "%sPushLock", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushLock = (int (*) (int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   if (!pcon->p_isc_so->p_CachePushLock) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_isc_so->dbname, pcon->p_isc_so->libnam, fun);
      goto isc_load_library_exit;
   }

   sprintf(fun, "%sAddGlobal", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddGlobal = (int (*) (int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddGlobalDescriptor", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddGlobalDescriptor = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddSSVN", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddSSVN = (int (*) (int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sAddSSVNDescriptor", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheAddSSVNDescriptor = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sMerge", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheMerge = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   /* printf("pcon->p_isc_so->p_CacheAddGlobal=%p; pcon->p_isc_so->p_CacheAddGlobalDescriptor=%p; pcon->p_isc_so->p_CacheAddSSVN=%p; pcon->p_isc_so->p_CacheAddSSVNDescriptor=%p; pcon->p_isc_so->p_CacheMerge=%p;",  pcon->p_isc_so->p_CacheAddGlobal, pcon->p_isc_so->p_CacheAddGlobalDescriptor, pcon->p_isc_so->p_CacheAddSSVN, pcon->p_isc_so->p_CacheAddSSVNDescriptor, pcon->p_isc_so->p_CacheMerge); */

   if (pcon->p_isc_so->p_CacheAddGlobal && pcon->p_isc_so->p_CacheAddGlobalDescriptor && pcon->p_isc_so->p_CacheAddSSVN && pcon->p_isc_so->p_CacheAddSSVNDescriptor && pcon->p_isc_so->p_CacheMerge) {
      pcon->p_isc_so->merge_enabled = 1;
   }

   sprintf(fun, "%sPushFunc", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushFunc = (int (*) (unsigned int *, int, const Callin_char_t *, int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sExtFun", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExtFun = (int (*) (unsigned int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushRtn", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushRtn = (int (*) (unsigned int *, int, const Callin_char_t *, int, const Callin_char_t *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sDoFun", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheDoFun = (int (*) (unsigned int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sDoRtn", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheDoRtn = (int (*) (unsigned int, int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sCloseOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheCloseOref = (int (*) (unsigned int oref)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sIncrementCountOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheIncrementCountOref = (int (*) (unsigned int oref)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPopOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePopOref = (int (*) (unsigned int * orefp)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushOref", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushOref = (int (*) (unsigned int oref)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sInvokeMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheInvokeMethod = (int (*) (int narg)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushMethod = (int (*) (unsigned int oref, int mlen, const Callin_char_t * mptr, int flg)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sInvokeClassMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheInvokeClassMethod = (int (*) (int narg)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushClassMethod", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushClassMethod = (int (*) (int clen, const Callin_char_t * cptr, int mlen, const Callin_char_t * mptr, int flg)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sGetProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheGetProperty = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sSetProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheSetProperty = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sPushProperty", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CachePushProperty = (int (*) (unsigned int oref, int plen, const Callin_char_t * pptr)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sType", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheType = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sEvalA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEvalA = (int (*) (CACHE_ASTRP volatile)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sExecuteA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheExecuteA = (int (*) (CACHE_ASTRP volatile)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sConvert", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheConvert = (int (*) (unsigned long, void *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sErrorA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheErrorA = (int (*) (CACHE_ASTRP, CACHE_ASTRP, int *)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sErrxlateA", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheErrxlateA = (int (*) (int, CACHE_ASTRP)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   sprintf(fun, "%sEnableMultiThread", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheEnableMultiThread = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   /* v1.2.8 */
   sprintf(fun, "%sTStart", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheTStart = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sTLevel", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheTLevel = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sTCommit", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheTCommit = (int (*) (void)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);
   sprintf(fun, "%sTRollback", pcon->p_isc_so->funprfx);
   pcon->p_isc_so->p_CacheTRollback = (int (*) (int)) mg_dso_sym(pcon->p_isc_so->p_library, (char *) fun);

   pcon->pid = mg_current_process_id();

   pcon->p_isc_so->loaded = 1;

isc_load_library_exit:

   if (pcon->error[0]) {
      pcon->p_isc_so->loaded = 0;
      pcon->error_code = 1009;
      result = CACHE_NOCON;

      return result;
   }

   return CACHE_SUCCESS;
}


int isc_authenticate(DBXCON *pcon)
{
   unsigned char reopen;
   int termflag, timeout;
   char buffer[256];
	CACHESTR pin, *ppin;
	CACHESTR pout, *ppout;
	CACHESTR pusername;
	CACHESTR ppassword;
	CACHESTR pexename;
	int rc;

   reopen = 0;
   termflag = 0;
	timeout = 15;

isc_authenticate_reopen:

   pcon->error_code = 0;
   *pcon->error = '\0';
	strcpy((char *) pexename.str, "mg_dba");
	pexename.len = (unsigned short) strlen((char *) pexename.str);
/*
	strcpy((char *) pin.str, "//./nul");
	strcpy((char *) pout.str, "//./nul");
	pin.len = (unsigned short) strlen((char *) pin.str);
	pout.len = (unsigned short) strlen((char *) pout.str);
*/
   ppin = NULL;
   if (pcon->input_device[0]) {
	   strcpy(buffer, pcon->input_device);
      mg_lcase(buffer);
      if (!strcmp(buffer, "stdin")) {
	      strcpy((char *) pin.str, "");
         ppin = &pin;
      }
      else if (strcmp(pcon->input_device, DBX_NULL_DEVICE)) {
	      strcpy((char *) pin.str, pcon->input_device);
         ppin = &pin;
      }
      if (ppin)
	      ppin->len = (unsigned short) strlen((char *) ppin->str);
   }
   ppout = NULL;
   if (pcon->output_device[0]) {
	   strcpy(buffer, pcon->output_device);
      mg_lcase(buffer);
      if (!strcmp(buffer, "stdout")) {
	      strcpy((char *) pout.str, "");
         ppout = &pout;
      }
      else if (strcmp(pcon->output_device, DBX_NULL_DEVICE)) {
	      strcpy((char *) pout.str, pcon->output_device);
         ppout = &pout;
      }
      if (ppout)
	      ppout->len = (unsigned short) strlen((char *) ppout->str);
   }

   if (ppin && ppout) { 
      termflag = CACHE_TTALL|CACHE_PROGMODE;
   }
   else {
      termflag = CACHE_TTNEVER|CACHE_PROGMODE;
   }

	strcpy((char *) pusername.str, pcon->username);
	strcpy((char *) ppassword.str, pcon->password);

	pusername.len = (unsigned short) strlen((char *) pusername.str);
	ppassword.len = (unsigned short) strlen((char *) ppassword.str);

#if !defined(_WIN32)

   signal(SIGUSR1, SIG_IGN);

#endif

	rc = pcon->p_isc_so->p_CacheSecureStartA(&pusername, &ppassword, &pexename, termflag, timeout, ppin, ppout);

	if (rc != CACHE_SUCCESS) {
      pcon->error_code = rc;
	   if (rc == CACHE_ACCESSDENIED) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : Access Denied : Check the audit log for the real authentication error (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_CHANGEPASSWORD) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : Password Change Required (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_ALREADYCON) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : Already Connected (%d)\n", rc);
	      return 1;
	   }
	   if (rc == CACHE_CONBROKEN) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : Connection was formed and then broken by the server. (%d)\n", rc);
	      return 0;
	   }

	   if (rc == CACHE_FAILURE) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : An unexpected error has occurred. (%d)\n", rc);
	      return 0;
	   }
	   if (rc == CACHE_STRTOOLONG) {
	      sprintf(pcon->error, "Authentication: CacheSecureStart() : prinp or prout is too long. (%d)\n", rc);
	      return 0;
	   }
	   sprintf(pcon->error, "Authentication: CacheSecureStart() : Failed (%d)\n", rc);
	   return 0;
   }

   if (pcon->p_isc_so->p_CacheEvalA && pcon->p_isc_so->p_CacheConvert) {
      CACHE_ASTR retval;
      CACHE_ASTR expr;

      strcpy((char *) expr.str, "$ZVersion");
      expr.len = (unsigned short) strlen((char *) expr.str);
      rc = pcon->p_isc_so->p_CacheEvalA(&expr);

      if (rc == CACHE_CONBROKEN)
         reopen = 1;
      if (rc == CACHE_SUCCESS) {
         retval.len = 256;
         rc = pcon->p_isc_so->p_CacheConvert(CACHE_ASTRING, &retval);

         if (rc == CACHE_CONBROKEN)
            reopen = 1;
         if (rc == CACHE_SUCCESS) {
            isc_parse_zv((char *) retval.str, pcon->p_zv);
            sprintf(pcon->p_zv->version, "%d.%d.b%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->mg_build);
         }
      }
   }

   if (reopen) {
      goto isc_authenticate_reopen;
   }

   rc = isc_change_namespace(pcon, pcon->nspace);

   if (pcon->p_isc_so && pcon->p_isc_so->p_CacheEnableMultiThread) {
      rc = pcon->p_isc_so->p_CacheEnableMultiThread();
   }
   else {
      rc = -1;
   }

   return 1;
}


int isc_open(DBXMETH *pmeth)
{
   int rc, error_code, result;
   DBXCON *pcon = pmeth->pcon;

   error_code = 0;
   rc = CACHE_SUCCESS;

   if (!pcon->p_isc_so) {
      pcon->p_isc_so = (DBXISCSO *) mg_malloc(sizeof(DBXISCSO), 0);
      if (!pcon->p_isc_so) {
         strcpy(pcon->error, "No Memory");
         pcon->error_code = 1009; 
         result = CACHE_NOCON;
         return result;
      }
      memset((void *) pcon->p_isc_so, 0, sizeof(DBXISCSO));
      pcon->p_isc_so->loaded = 0;
   }

   if (pcon->p_isc_so->loaded == 2) {
      strcpy(pcon->error, "Cannot create multiple connections to the database");
      pcon->error_code = 1009; 
      strncpy(pcon->error, pcon->error, DBX_ERROR_SIZE - 1);
      pcon->error[DBX_ERROR_SIZE - 1] = '\0';
      rc = CACHE_NOCON;
      goto isc_open_exit;
   }

   if (!pcon->p_isc_so->loaded) {
      pcon->p_isc_so->merge_enabled = 0;
   }

   if (!pcon->p_isc_so->loaded) {
      rc = isc_load_library(pcon);
      sprintf((char *) pmeth->output_val.svalue.buf_addr, "%d", rc);
      pmeth->output_val.svalue.len_used = (unsigned int) strlen((char *) pmeth->output_val.svalue.buf_addr);
      if (rc != CACHE_SUCCESS) {
         goto isc_open_exit;
      }
   }

   rc = pcon->p_isc_so->p_CacheSetDir(pcon->shdir);

   if (!isc_authenticate(pcon)) {
      pcon->error_code = error_code;
      rc = CACHE_NOCON;
   }
   else {
      pcon->p_isc_so->loaded = 2;
      rc = CACHE_SUCCESS;
   }

isc_open_exit:

   return rc;
}


int isc_parse_zv(char *zv, DBXZV * p_isc_sv)
{
   int result;
   double mg_version;
   char *p, *p1, *p2;

   p_isc_sv->mg_version = 0;
   p_isc_sv->majorversion = 0;
   p_isc_sv->minorversion = 0;
   p_isc_sv->mg_build = 0;
   p_isc_sv->vnumber = 0;

   result = 0;

   p = strstr(zv, "Cache");
   if (p) {
      p_isc_sv->product = DBX_DBTYPE_CACHE;
   }
   else {
      p_isc_sv->product = DBX_DBTYPE_IRIS;
   }

   p = zv;
   mg_version = 0;
   while (*(++ p)) {
      if (*(p - 1) == ' ' && isdigit((int) (*p))) {
         mg_version = strtod(p, NULL);
         if (*(p + 1) == '.' && mg_version >= 1.0 && mg_version <= 5.2)
            break;
         else if (*(p + 4) == '.' && mg_version >= 2000.0)
            break;
         mg_version = 0;
      }
   }

   if (mg_version > 0) {
      p_isc_sv->mg_version = mg_version;
      p_isc_sv->majorversion = (int) strtol(p, NULL, 10);
      p1 = strstr(p, ".");
      if (p1) {
         p_isc_sv->minorversion = (int) strtol(p1 + 1, NULL, 10);
      }
      p2 = strstr(p, "Build ");
      if (p2) {
         p_isc_sv->mg_build = (int) strtol(p2 + 6, NULL, 10);
      }

      if (p_isc_sv->majorversion >= 2007)
         p_isc_sv->vnumber = (((p_isc_sv->majorversion - 2000) * 100000) + (p_isc_sv->minorversion * 10000) + p_isc_sv->mg_build);
      else
         p_isc_sv->vnumber = ((p_isc_sv->majorversion * 100000) + (p_isc_sv->minorversion * 10000) + p_isc_sv->mg_build);

      result = 1;
   }

   return result;
}


int isc_change_namespace(DBXCON *pcon, char *nspace)
{
   int rc, len;
   CACHE_ASTR expr;

   len = (int) strlen(nspace);
   if (len == 0 || len > 64) {
      return CACHE_ERNAMSP;
   }
   if (pcon->p_isc_so->p_CacheExecuteA == NULL) {
      return CACHE_ERNAMSP;
   }

   sprintf((char *) expr.str, "ZN \"%s\"", nspace); /* changes namespace */
   expr.len = (unsigned short) strlen((char *) expr.str);

   mg_mutex_lock(pcon->p_db_mutex, 0);

   rc = pcon->p_isc_so->p_CacheExecuteA(&expr);

   mg_mutex_unlock(pcon->p_db_mutex);

   return rc;
}


int isc_pop_value(DBXCON *pcon, DBXVAL *value, int required_type)
{
   int rc, rc1, ex, ctype, offset, oref;
   unsigned int n, max, len;
   char *pstr8, *p8, *outstr8;
   CACHE_EXSTR zstr;

   ex = 1;
   offset = 0;
   zstr.len = 0;
   zstr.str.ch = NULL;
   outstr8 = NULL;
   ctype = CACHE_ASTRING;

   if (pcon->p_isc_so->p_CacheType) {
      ctype = pcon->p_isc_so->p_CacheType();

      if (ctype == CACHE_OREF) {
         rc = pcon->p_isc_so->p_CachePopOref(&oref);

         value->type = DBX_DTYPE_OREF;
         value->num.oref = oref;
         sprintf((char *) value->svalue.buf_addr + value->offset, "%d", oref);
         value->svalue.len_used += (int) strlen((char *) value->svalue.buf_addr + value->offset);
         mg_add_block_size(&(value->svalue), 0, (unsigned long) value->svalue.len_used - value->offset, DBX_DSORT_DATA, DBX_DTYPE_OREF);

         return rc;
      }
   }
   else {
      ctype = CACHE_ASTRING;
   }

   if (ex) {
      rc = pcon->p_isc_so->p_CachePopExStr(&zstr);
      len = zstr.len;
      outstr8 = (char *) zstr.str.ch;
   }
   else {
      rc = pcon->p_isc_so->p_CachePopStr((int *) &len, (Callin_char_t **) &outstr8);
   }


   max = 0;
   if (value->svalue.len_alloc > 8) {
      max = (value->svalue.len_alloc - 2);
   }

   pstr8 = (char *) value->svalue.buf_addr;
   offset = value->offset;
   if (len >= max) {
      p8 = (char *) mg_malloc(sizeof(char) * (len + 2), 301);
      if (p8) {
         if (value->svalue.buf_addr)
            mg_free((void *) value->svalue.buf_addr, 301);
         value->svalue.buf_addr = (unsigned char *) p8;
         pstr8 = (char *) value->svalue.buf_addr;
         max = len;
      }
   }
   for (n = 0; n < len; n ++) {
      if (n > max)
         break;
      pstr8[n + offset] = (char) outstr8[n];
   }
   pstr8[n + offset] = '\0';

   value->svalue.len_used += n;

   mg_add_block_size(&(value->svalue), 0, (unsigned long) n, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);

   if (ex) {
      rc1 = pcon->p_isc_so->p_CacheExStrKill(&zstr);
   }

   return rc;
}


int isc_error_message(DBXMETH *pmeth, int error_code)
{
   int size, size1, len, rc;
   CACHE_ASTR *pcerror;
   DBXCON *pcon = pmeth->pcon;

   size = DBX_ERROR_SIZE;

   if (pcon) {
      if (error_code < 0) {
         pcon->error_code = 900 + (error_code * -1);
      }
      else {
         pcon->error_code = error_code;
      }
   }
   else {
      return 0;
   }

   if (pcon->error[0]) {
      goto isc_error_message_exit;
   }

   pcon->error[0] = '\0';

   size1 = size;

   if (pcon->p_isc_so && pcon->p_isc_so->p_CacheErrxlateA) {
      pcerror = (CACHE_ASTR *) mg_malloc(sizeof(CACHE_ASTR), 801);
      if (pcerror) {
         pcerror->str[0] = '\0';
         pcerror->len = 50;
         rc = pcon->p_isc_so->p_CacheErrxlateA(error_code, pcerror);

         pcerror->str[50] = '\0';

         if (pcerror->len > 0) {
            len = pcerror->len;
            if (len >= DBX_ERROR_SIZE) {
               len = DBX_ERROR_SIZE - 1;
            }
            strncpy(pcon->error, (char *) pcerror->str, len);
            pcon->error[len] = '\0';
         }
         mg_free((void *) pcerror, 801);
         size1 -= (int) strlen(pcon->error);
      }
   }

   switch (error_code) {
      case CACHE_SUCCESS:
         strncat(pcon->error, "Operation completed successfully!", size1 - 1);
         break;
      case CACHE_ACCESSDENIED:
         strncat(pcon->error, "Authentication has failed. Check the audit log for the real authentication error.", size1 - 1);
         break;
      case CACHE_ALREADYCON:
         strncat(pcon->error, "Connection already existed. Returned if you call CacheSecureStartH from a $ZF function.", size1 - 1);
         break;
      case CACHE_CHANGEPASSWORD:
         strncat(pcon->error, "Password change required. This return value is only returned if you are using Cach\xe7 authentication.", size1 - 1);
         break;
      case CACHE_CONBROKEN:
         strncat(pcon->error, "Connection was broken by the server. Check arguments for validity.", size1 - 1);
         break;
      case CACHE_FAILURE:
         strncat(pcon->error, "An unexpected error has occurred.", size1 - 1);
         break;
      case CACHE_STRTOOLONG:
         strncat(pcon->error, "String is too long.", size1 - 1);
         break;
      case CACHE_NOCON:
         strncat(pcon->error, "No connection has been established.", size1 - 1);
         break;
      case CACHE_ERSYSTEM:
         strncat(pcon->error, "Either the Cache engine generated a <SYSTEM> error, or callin detected an internal data inconsistency.", size1 - 1);
         break;
      case CACHE_ERARGSTACK:
         strncat(pcon->error, "Argument stack overflow.", size1 - 1);
         break;
      case CACHE_ERSTRINGSTACK:
         strncat(pcon->error, "String stack overflow.", size1 - 1);
         break;
      case CACHE_ERPROTECT:
         strncat(pcon->error, "Protection violation.", size1 - 1);
         break;
      case CACHE_ERUNDEF:
         strncat(pcon->error, "Global node is undefined", size1 - 1);
         break;
      case CACHE_ERUNIMPLEMENTED:
         strncat(pcon->error, "String is undefined OR feature is not implemented.", size1 - 1);
         break;
      case CACHE_ERSUBSCR:
         strncat(pcon->error, "Subscript error in Global node (subscript null/empty or too long)", size1 - 1);
         break;
      case CACHE_ERNOROUTINE:
         strncat(pcon->error, "Routine does not exist", size1 - 1);
         break;
      case CACHE_ERNOLINE:
         strncat(pcon->error, "Function does not exist in routine", size1 - 1);
         break;
      case CACHE_ERPARAMETER:
         strncat(pcon->error, "Function arguments error", size1 - 1);
         break;
      case CACHE_BAD_GLOBAL:
         strncat(pcon->error, "Invalid global name", size1 - 1);
         break;
      case CACHE_BAD_NAMESPACE:
         strncat(pcon->error, "Invalid NameSpace name", size1 - 1);
         break;
      case CACHE_BAD_FUNCTION:
         strncat(pcon->error, "Invalid function name", size1 - 1);
         break;
      case CACHE_BAD_CLASS:
         strncat(pcon->error, "Invalid class name", size1 - 1);
         break;
      case CACHE_BAD_METHOD:
         strncat(pcon->error, "Invalid method name", size1 - 1);
         break;
      case CACHE_ERNOCLASS:
         strncat(pcon->error, "Class does not exist", size1 - 1);
         break;
      case CACHE_ERBADOREF:
         strncat(pcon->error, "Invalid Object Reference", size1 - 1);
         break;
      case CACHE_ERNOMETHOD:
         strncat(pcon->error, "Method does not exist", size1 - 1);
         break;
      case CACHE_ERNOPROPERTY:
         strncat(pcon->error, "Property does not exist", size1 - 1);
         break;
      case CACHE_ETIMEOUT:
         strncat(pcon->error, "Operation timed out", size1 - 1);
         break;
      case CACHE_BAD_STRING:
         strncat(pcon->error, "Invalid string", size1 - 1);
         break;
      case CACHE_ERNAMSP:
         strncat(pcon->error, "Invalid Namespace", size1 - 1);
         break;
      default:
         strncat(pcon->error, "Database Server Error", size1 - 1);
         break;
   }
   pcon->error[size - 1] = '\0';

isc_error_message_exit:

   mg_set_error_message(pmeth);

   return 0;
}


int ydb_load_library(DBXCON *pcon)
{
   int n, len, result;
   char primlib[DBX_ERROR_SIZE], primerr[DBX_ERROR_SIZE];
   char verfile[256], fun[64];
   char *libnam[16];

   strcpy(pcon->p_ydb_so->libdir, pcon->shdir);
   strcpy(pcon->p_ydb_so->funprfx, "ydb");
   strcpy(pcon->p_ydb_so->dbname, "YottaDB");

   strcpy(verfile, pcon->shdir);
   len = (int) strlen(pcon->p_ydb_so->libdir);
   if (pcon->p_ydb_so->libdir[len - 1] != '/' && pcon->p_ydb_so->libdir[len - 1] != '\\') {
      pcon->p_ydb_so->libdir[len] = '/';
      len ++;
   }

   n = 0;
#if defined(_WIN32)
   libnam[n ++] = (char *) DBX_YDB_DLL;
#else
#if defined(MACOSX)
   libnam[n ++] = (char *) DBX_YDB_DYLIB;
   libnam[n ++] = (char *) DBX_YDB_SO;
#else
   libnam[n ++] = (char *) DBX_YDB_SO;
   libnam[n ++] = (char *) DBX_YDB_DYLIB;
#endif
#endif

   libnam[n ++] = NULL;
   strcpy(pcon->p_ydb_so->libnam, pcon->p_ydb_so->libdir);
   len = (int) strlen(pcon->p_ydb_so->libnam);

   for (n = 0; libnam[n]; n ++) {
      strcpy(pcon->p_ydb_so->libnam + len, libnam[n]);
      if (!n) {
         strcpy(primlib, pcon->p_ydb_so->libnam);
      }

      pcon->p_ydb_so->p_library = mg_dso_load(pcon->p_ydb_so->libnam);
      if (pcon->p_ydb_so->p_library) {
         break;
      }

      if (!n) {
         int len1, len2;
         char *p;
#if defined(_WIN32)
         DWORD errorcode;
         LPVOID lpMsgBuf;

         lpMsgBuf = NULL;
         errorcode = GetLastError();
         sprintf(pcon->error, "Error loading %s Library: %s; Error Code : %ld",  pcon->p_ydb_so->dbname, primlib, errorcode);
         len2 = (int) strlen(pcon->error);
         len1 = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorcode,
                        /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPTSTR) &lpMsgBuf,
                        0,
                        NULL 
                        );
         if (lpMsgBuf && len1 > 0 && (DBX_ERROR_SIZE - len2) > 30) {
            strncpy(primerr, (const char *) lpMsgBuf, DBX_ERROR_SIZE - 1);
            p = strstr(primerr, "\r\n");
            if (p)
               *p = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            p = strstr(primerr, "%1");
            if (p) {
               *p = 'I';
               *(p + 1) = 't';
            }
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
         if (lpMsgBuf)
            LocalFree(lpMsgBuf);
#else
         p = (char *) dlerror();
         sprintf(primerr, "Cannot load %s library: Error Code: %d", pcon->p_ydb_so->dbname, errno);
         len2 = strlen(pcon->error);
         if (p) {
            strncpy(primerr, p, DBX_ERROR_SIZE - 1);
            primerr[DBX_ERROR_SIZE - 1] = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
#endif
      }
   }

   if (!pcon->p_ydb_so->p_library) {
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_init", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_init = (int (*) (void)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_init) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_exit", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_exit = (int (*) (void)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_exit) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_malloc", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_malloc = (int (*) (size_t)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_malloc) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_free", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_free = (int (*) (void *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_free) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_data_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_data_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, unsigned int *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_data_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_delete_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_delete_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_delete_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_set_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_set_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_set_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_get_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_get_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_get_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_subscript_next_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_subscript_next_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_subscript_next_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_subscript_previous_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_subscript_previous_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_subscript_previous_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_node_next_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_node_next_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_node_next_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_node_previous_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_node_previous_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, int *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_node_previous_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_incr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_incr_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *, ydb_buffer_t *, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_incr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_ci", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_ci = (int (*) (const char *, ...)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_ci) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }
   sprintf(fun, "%s_cip", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_cip = (int (*) (ci_name_descriptor *, ...)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_cip) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_lock_incr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_lock_incr_s = (int (*) (unsigned long long, ydb_buffer_t *, int, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_lock_incr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_lock_decr_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_lock_decr_s = (int (*) (ydb_buffer_t *, int, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);
   if (!pcon->p_ydb_so->p_ydb_lock_decr_s) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_ydb_so->dbname, pcon->p_ydb_so->libnam, fun);
      goto ydb_load_library_exit;
   }

   sprintf(fun, "%s_zstatus", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_zstatus = (void (*) (ydb_char_t *, ydb_long_t)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);

   sprintf(fun, "%s_tp_s", pcon->p_ydb_so->funprfx);
   pcon->p_ydb_so->p_ydb_tp_s = (int (*) (ydb_tpfnptr_t, void *, const char *, int, ydb_buffer_t *)) mg_dso_sym(pcon->p_ydb_so->p_library, (char *) fun);


   pcon->pid = mg_current_process_id();

   pcon->p_ydb_so->loaded = 1;

ydb_load_library_exit:

   if (pcon->error[0]) {
      pcon->p_ydb_so->loaded = 0;
      pcon->error_code = 1009;
      result = CACHE_NOCON;
      return result;
   }

   return CACHE_SUCCESS;
}


int ydb_open(DBXMETH *pmeth)
{
   int rc, error_code, result;
   char buffer[256], buffer1[256];
   ydb_buffer_t zv, data;
   DBXCON *pcon = pmeth->pcon;

   error_code = 0;
   rc = CACHE_SUCCESS;

   if (!pcon->p_ydb_so) {
      pcon->p_ydb_so = (DBXYDBSO *) mg_malloc(sizeof(DBXYDBSO), 0);
      if (!pcon->p_ydb_so) {
         strcpy(pcon->error, "No Memory");
         pcon->error_code = 1009; 
         result = CACHE_NOCON;
         return result;
      }
      memset((void *) pcon->p_ydb_so, 0, sizeof(DBXYDBSO));
      pcon->p_ydb_so->loaded = 0;
   }

   if (pcon->p_ydb_so->loaded == 2) {
      strcpy(pcon->error, "Cannot create multiple connections to the database");
      pcon->error_code = 1009; 
      strncpy(pcon->error, pcon->error, DBX_ERROR_SIZE - 1);
      pcon->error[DBX_ERROR_SIZE - 1] = '\0';
      strcpy((char *) pmeth->output_val.svalue.buf_addr, "0");
      rc = CACHE_NOCON;
      goto ydb_open_exit;
   }

   if (!pcon->p_ydb_so->loaded) {
      rc = ydb_load_library(pcon);
      sprintf((char *) pmeth->output_val.svalue.buf_addr, "%d", rc);
      pmeth->output_val.svalue.len_used = (unsigned int) strlen((char *) pmeth->output_val.svalue.buf_addr);
      if (rc != CACHE_SUCCESS) {
         goto ydb_open_exit;
      }
   }

   rc = pcon->p_ydb_so->p_ydb_init();

   strcpy(buffer, "$zv");
   zv.buf_addr = buffer;
   zv.len_used = (int) strlen(buffer);
   zv.len_alloc = 255;

   data.buf_addr = buffer1;
   data.len_used = 0;
   data.len_alloc = 255;

   rc = pcon->p_ydb_so->p_ydb_get_s(&zv, 0, NULL, &data);

   if (data.len_used >= 0) {
      data.buf_addr[data.len_used] = '\0';
   }

   if (rc == CACHE_SUCCESS) {
      ydb_parse_zv(data.buf_addr, &(pcon->zv));
      sprintf(pcon->p_zv->version, "%d.%d.b%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->mg_build);
   }

ydb_open_exit:

   return rc;
}


int ydb_parse_zv(char *zv, DBXZV * p_ydb_sv)
{
   int result;
   double mg_version;
   char *p, *p1, *p2;

   p_ydb_sv->mg_version = 0;
   p_ydb_sv->majorversion = 0;
   p_ydb_sv->minorversion = 0;
   p_ydb_sv->mg_build = 0;
   p_ydb_sv->vnumber = 0;

   result = 0;
   /* GT.M V6.3-004 Linux x86_64 */

   p_ydb_sv->product = DBX_DBTYPE_YOTTADB;

   p = zv;
   mg_version = 0;
   while (*(++ p)) {
      if (*(p - 1) == 'V' && isdigit((int) (*p))) {
         mg_version = strtod(p, NULL);
         break;
      }
   }

   if (mg_version > 0) {
      p_ydb_sv->mg_version = mg_version;
      p_ydb_sv->majorversion = (int) strtol(p, NULL, 10);
      p1 = strstr(p, ".");
      if (p1) {
         p_ydb_sv->minorversion = (int) strtol(p1 + 1, NULL, 10);
      }
      p2 = strstr(p, "-");
      if (p2) {
         p_ydb_sv->mg_build = (int) strtol(p2 + 1, NULL, 10);
      }

      p_ydb_sv->vnumber = ((p_ydb_sv->majorversion * 100000) + (p_ydb_sv->minorversion * 10000) + p_ydb_sv->mg_build);

      result = 1;
   }
/*
   printf("\r\n ydb_parse_zv : p_ydb_sv->majorversion=%d; p_ydb_sv->minorversion=%d; p_ydb_sv->mg_build=%d; p_ydb_sv->mg_version=%f;", p_ydb_sv->majorversion, p_ydb_sv->minorversion, p_ydb_sv->mg_build, p_ydb_sv->mg_version);
*/
   return result;
}


int ydb_get_intsvar(DBXCON *pcon, char *svarname)
{
   int rc;
   char svarstr[32];
   ydb_buffer_t svar, data;

   svar.buf_addr = svarname;
   svar.len_used = (int) strlen(svarname);
   svar.len_alloc = svar.len_used;

   data.buf_addr = svarstr;
   data.len_used = 0;
   data.len_alloc = sizeof(svarstr);

   rc = pcon->p_ydb_so->p_ydb_get_s(&svar, 0, NULL, &data);
   if (rc == YDB_OK) {
      svarstr[data.len_used] = '\0';
      rc = (int) strtol(svarstr, NULL, 10);
   }

   return rc;
}


int ydb_error_message(DBXMETH *pmeth, int error_code)
{
   int rc;
   char buffer[256], buffer1[256];
   ydb_buffer_t zstatus, data;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->p_ydb_so && pcon->p_ydb_so->p_ydb_get_s) {
      strcpy(buffer, "$zstatus");
      zstatus.buf_addr = buffer;
      zstatus.len_used = (int) strlen(buffer);
      zstatus.len_alloc = 255;

      data.buf_addr = buffer1;
      data.len_used = 0;
      data.len_alloc = 255;

      rc = pcon->p_ydb_so->p_ydb_get_s(&zstatus, 0, NULL, &data);

      if (data.len_used >= 0) {
         data.buf_addr[data.len_used] = '\0';
      }

      strcpy(pcon->error, data.buf_addr);
   }
   else {
      if (!pcon->error[0]) {
         strcpy(pcon->error, "No connection has been established");
      }
   }

   mg_set_error_message(pmeth);

   return rc;
}


int ydb_function(DBXMETH *pmeth, DBXFUN *pfun)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   pmeth->output_val.svalue.len_used = 0;
   pmeth->output_val.svalue.buf_addr[0] = '\0';

   switch (pmeth->argc) {
      case 1:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pmeth->output_val.svalue));
         break;
      case 2:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pmeth->output_val.svalue), &(pmeth->args[1].svalue));
         break;
      case 3:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pmeth->output_val.svalue), &(pmeth->args[1].svalue), &(pmeth->args[2].svalue));
         break;
      case 4:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pmeth->output_val.svalue), &(pmeth->args[1].svalue), &(pmeth->args[2].svalue), &(pmeth->args[3].svalue));
         break;
      default:
         rc = CACHE_SUCCESS;
         break;
   }

   pmeth->output_val.svalue.len_used = (int) strlen(pmeth->output_val.svalue.buf_addr);

   return rc;
}


int ydb_function_ex(DBXMETH *pmeth, DBXFUN *pfun)
{
   int rc;
   DBXCON *pcon = pmeth->pcon;

   switch (pfun->argc) {
      case 1:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pfun->out));
         break;
      case 2:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pfun->out), &(pfun->in[1]));
         break;
      case 3:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pfun->out), &(pfun->in[1]), &(pfun->in[2]));
         break;
      case 4:
         rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, &(pfun->out), &(pfun->in[1]), &(pfun->in[2]), &(pfun->in[3]));
         break;
      default:
         rc = CACHE_SUCCESS;
         pfun->out.length = 0;
         break;
   }

   return rc;
}


/* v1.2.9 */
int ydb_transaction_cb(void *pargs)
{
#if defined(_WIN32)
   return 0;
#else
   int rc, init, context, trestart;
   struct timespec   ts;
   struct timeval    tp;
   DBXTHRT *pthrt;
   DBXMETH *pmeth;

/*
   YDB_OK
   YDB_TP_RESTART
   YDB_TP_ROLLBACK
*/
   rc = YDB_OK;
   pthrt = (DBXTHRT *) pargs;
   pmeth = pthrt->pmeth;

   trestart = ydb_get_intsvar(pthrt->pmeth->pcon, (char *) "$trestart");
   if (trestart > 0) {
      rc = YDB_TP_ROLLBACK;
      return rc;
   }
/*
   printf("\r\n*** ydb_transaction_cb tid=%lu; tlevel=%d; ...", (unsigned long) mg_current_thread_id(), ydb_get_intsvar(pthrt->pmeth->pcon, "$tlevel"));
*/

   init = 0;
   pthread_mutex_lock(&(pthrt->req_cv_mutex));
   while (1) {
      if (init == 0) {
         init ++;
         pthrt->done = 1;
         pthread_cond_broadcast(&(pthrt->res_cv));
      }
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec;
      ts.tv_nsec = tp.tv_usec * 1000;
      ts.tv_sec += 3;
      rc = pthread_cond_timedwait(&(pthrt->req_cv), &(pthrt->req_cv_mutex), &ts);

      pmeth = pthrt->pmeth;

      if (pthrt->context) {
         context = pthrt->context;
         pthrt->context = 0;
         if (context == YDB_TPCTX_DB) {
            rc = pmeth->p_dbxfun(pmeth);
         }
         else if (context == YDB_TPCTX_FUN) {
            rc = ydb_function_ex(pmeth, pmeth->pfun);
         }
         else if (context == YDB_TPCTX_QUERY) {
            if (pmeth->pfun->dir == 1) {
               pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_node_next_s(pmeth->pfun->global, pmeth->pfun->in_nkeys, pmeth->pfun->in_keys, pmeth->pfun->out_nkeys, pmeth->pfun->out_keys);
            }
            else {
               pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_node_previous_s(pmeth->pfun->global, pmeth->pfun->in_nkeys, pmeth->pfun->in_keys, pmeth->pfun->out_nkeys, pmeth->pfun->out_keys);
            }
            if (pmeth->pfun->getdata && pmeth->pfun->rc == YDB_OK && *(pmeth->pfun->out_nkeys) != YDB_NODE_END) {
               pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_get_s(pmeth->pfun->global, *(pmeth->pfun->out_nkeys), pmeth->pfun->out_keys, pmeth->pfun->data);
            }
         }
         else if (context == YDB_TPCTX_ORDER) {
            if (pmeth->pfun->dir == 1) {
               pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_subscript_next_s(pmeth->pfun->global, pmeth->pfun->in_nkeys, pmeth->pfun->in_keys, pmeth->pfun->out_keys);
            }
            else {
               pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_subscript_previous_s(pmeth->pfun->global, pmeth->pfun->in_nkeys, pmeth->pfun->in_keys, pmeth->pfun->out_keys);
            }
            if (pmeth->pfun->rc == CACHE_SUCCESS && pmeth->pfun->out_keys->len_used > 0) {
               strcpy((pmeth->pfun->in_keys + (pmeth->pfun->in_nkeys - 1))->buf_addr, pmeth->pfun->out_keys->buf_addr);
               (pmeth->pfun->in_keys + (pmeth->pfun->in_nkeys - 1))->len_used = pmeth->pfun->out_keys->len_used;
               if (pmeth->pfun->getdata) {
                  pmeth->pfun->rc = pmeth->pcon->p_ydb_so->p_ydb_get_s(pmeth->pfun->global, pmeth->pfun->in_nkeys, pmeth->pfun->in_keys, pmeth->pfun->data);
               }
            }
            else {
               (pmeth->pfun->in_keys + (pmeth->pfun->in_nkeys - 1))->len_used = 0;
            }
         }
         else if (context == YDB_TPCTX_TLEVEL) {
            pmeth->output_val.num.int32 = ydb_get_intsvar(pmeth->pcon, (char *) "$tlevel");
         }
         pthrt->done = 1;
         pthread_cond_broadcast(&(pthrt->res_cv));

         if (context == YDB_TPCTX_COMMIT) {
            rc = YDB_OK;
            break;
         }
         else if (context == YDB_TPCTX_ROLLBACK) {
            rc = YDB_TP_ROLLBACK;
            break;
         }
      }
   }
   pthread_mutex_unlock(&(pthrt->req_cv_mutex));
/*
   printf("\r\n*** ydb_transaction_cb EXIT tid=%lu ...", (unsigned long) mg_current_thread_id());
*/
   return rc;
#endif
}


#if defined(_WIN32)
LPTHREAD_START_ROUTINE ydb_transaction_thread(LPVOID pargs)
#else
void * ydb_transaction_thread(void *pargs)
#endif
{
   ydb_buffer_t vnames[DBX_MAXARGS];
   DBXTHRT *pthrt;

   pthrt = (DBXTHRT *) pargs;
/*
   printf("\r\n*** ydb_transaction_thread tid=%lu; tlevel=%d; ...", (unsigned long) dbx_current_thread_id(), ydb_get_intsvar(pthrt->pmeth->pcon, "$tlevel"));
*/
   vnames[0].buf_addr = NULL;
   vnames[0].len_alloc = 0;
   vnames[0].len_used = 0;

   pthrt->pmeth->pcon->p_ydb_so->p_ydb_tp_s((ydb_tpfnptr_t) ydb_transaction_cb, (void *) pthrt, (const char *) "mg-dbx", 0, &vnames[0]);
/*
   printf("\r\n*** ydb_transaction_thread EXIT tid=%lu ...", (unsigned long) dbx_current_thread_id());
*/

#if defined(_WIN32)
   return 0;
#else
   return NULL;
#endif
}


/* v1.2.9 */
int ydb_transaction(DBXMETH *pmeth)
{
#if defined(_WIN32)
   return 0;
#else
   int rc;
   DBXTHRT *pthrt;
   pthread_attr_t attr;
   size_t stacksize, newstacksize;
   struct timespec   ts;
   struct timeval    tp;

   pthrt = (DBXTHRT *) mg_malloc(sizeof(DBXTHRT), 0);
   pthrt->pmeth = pmeth;
   pthrt->context = 0;
   pthrt->done = 0;

   pthread_mutex_init(&(pthrt->req_cv_mutex), NULL);
   pthread_cond_init(&(pthrt->req_cv), NULL);
   pthread_mutex_init(&(pthrt->res_cv_mutex), NULL);
   pthread_cond_init(&(pthrt->res_cv), NULL);

   mg_enter_critical_section((void *) &dbx_global_mutex);
   pmeth->pcon->tlevel ++;
   pmeth->pcon->pthrt[pmeth->pcon->tlevel] = (void *) pthrt;
   mg_leave_critical_section((void *) &dbx_global_mutex);

   pthread_attr_init(&attr);

   stacksize = 0;
   pthread_attr_getstacksize(&attr, &stacksize);

   newstacksize = DBX_THREAD_STACK_SIZE;

   pthread_attr_setstacksize(&attr, newstacksize);
/*
   printf("Thread: default stack=%lu; new stack=%lu;\n", (unsigned long) stacksize, (unsigned long) newstacksize);
*/
   rc = pthread_create(&(pthrt->tp_tid), &attr, ydb_transaction_thread, (void *) pthrt);
   if (rc) {
      printf("failed to create thread, errno = %d\n",errno);
   }

   pthread_mutex_lock(&(pthrt->res_cv_mutex));
   while (!pthrt->done) {
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec;
      ts.tv_nsec = tp.tv_usec * 1000;
      ts.tv_sec += 3;
      pthread_cond_timedwait(&(pthrt->res_cv), &(pthrt->res_cv_mutex), &ts);
   }
   pthread_mutex_unlock(&(pthrt->res_cv_mutex));

   return YDB_OK;

#endif
}


int ydb_transaction_task(DBXMETH *pmeth, int context)
{
#if defined(_WIN32)
   return 0;
#else
   int rc;
   DBXTHRT *pthrt;
   struct timespec   ts;
   struct timeval    tp;

   rc = YDB_OK;
   pthrt = (DBXTHRT *) pmeth->pcon->pthrt[pmeth->pcon->tlevel];
   pthrt->context = context;
   pthrt->pmeth = pmeth;

   pthread_mutex_lock(&(pthrt->req_cv_mutex));
   pthread_cond_signal(&(pthrt->req_cv));
   pthread_mutex_unlock(&(pthrt->req_cv_mutex));

   pthrt->done = 0;

   pthread_mutex_lock(&(pthrt->res_cv_mutex));
   while (!pthrt->done) {
      gettimeofday(&tp, NULL);
      ts.tv_sec  = tp.tv_sec;
      ts.tv_nsec = tp.tv_usec * 1000;
      ts.tv_sec += 3;
      pthread_cond_timedwait(&(pthrt->res_cv), &(pthrt->res_cv_mutex), &ts);
   }
   pthread_mutex_unlock(&(pthrt->res_cv_mutex));

   if (context == YDB_TPCTX_COMMIT || context == YDB_TPCTX_ROLLBACK) {
      mg_enter_critical_section((void *) &dbx_global_mutex);
      pmeth->pcon->pthrt[pmeth->pcon->tlevel] = (void *) NULL;
      pmeth->pcon->tlevel --;
      mg_leave_critical_section((void *) &dbx_global_mutex);
      rc = pthread_join(pthrt->tp_tid, NULL);
      mg_free((void *) pthrt, 0);
   }
   return rc;
#endif
}


int gtm_load_library(DBXCON *pcon)
{
   int n, len, result;
   char primlib[DBX_ERROR_SIZE], primerr[DBX_ERROR_SIZE];
   char verfile[256], fun[64];
   char *libnam[16];

   strcpy(pcon->p_gtm_so->libdir, pcon->shdir);
   strcpy(pcon->p_gtm_so->funprfx, "gtm");
   strcpy(pcon->p_gtm_so->dbname, "GT.M");

   strcpy(verfile, pcon->shdir);
   len = (int) strlen(pcon->p_gtm_so->libdir);
   if (pcon->p_gtm_so->libdir[len - 1] != '/' && pcon->p_gtm_so->libdir[len - 1] != '\\') {
      pcon->p_gtm_so->libdir[len] = '/';
      len ++;
   }

   n = 0;
#if defined(_WIN32)
   libnam[n ++] = (char *) DBX_GTM_DLL;
#else
#if defined(MACOSX)
   libnam[n ++] = (char *) DBX_GTM_DYLIB;
   libnam[n ++] = (char *) DBX_GTM_SO;
#else
   libnam[n ++] = (char *) DBX_GTM_SO;
   libnam[n ++] = (char *) DBX_GTM_DYLIB;
#endif
#endif

   libnam[n ++] = NULL;
   strcpy(pcon->p_gtm_so->libnam, pcon->p_gtm_so->libdir);
   len = (int) strlen(pcon->p_gtm_so->libnam);

   for (n = 0; libnam[n]; n ++) {
      strcpy(pcon->p_gtm_so->libnam + len, libnam[n]);
      if (!n) {
         strcpy(primlib, pcon->p_gtm_so->libnam);
      }

      pcon->p_gtm_so->p_library = mg_dso_load(pcon->p_gtm_so->libnam);
      if (pcon->p_gtm_so->p_library) {
         break;
      }

      if (!n) {
         int len1, len2;
         char *p;
#if defined(_WIN32)
         DWORD errorcode;
         LPVOID lpMsgBuf;

         lpMsgBuf = NULL;
         errorcode = GetLastError();
         sprintf(pcon->error, "Error loading %s Library: %s; Error Code : %ld",  pcon->p_gtm_so->dbname, primlib, errorcode);
         len2 = (int) strlen(pcon->error);
         len1 = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorcode,
                        /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        (LPTSTR) &lpMsgBuf,
                        0,
                        NULL 
                        );
         if (lpMsgBuf && len1 > 0 && (DBX_ERROR_SIZE - len2) > 30) {
            strncpy(primerr, (const char *) lpMsgBuf, DBX_ERROR_SIZE - 1);
            p = strstr(primerr, "\r\n");
            if (p)
               *p = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            p = strstr(primerr, "%1");
            if (p) {
               *p = 'I';
               *(p + 1) = 't';
            }
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
         if (lpMsgBuf)
            LocalFree(lpMsgBuf);
#else
         p = (char *) dlerror();
         sprintf(primerr, "Cannot load %s library: Error Code: %d", pcon->p_gtm_so->dbname, errno);
         len2 = strlen(pcon->error);
         if (p) {
            strncpy(primerr, p, DBX_ERROR_SIZE - 1);
            primerr[DBX_ERROR_SIZE - 1] = '\0';
            len1 = (DBX_ERROR_SIZE - (len2 + 10));
            if (len1 < 1)
               len1 = 0;
            primerr[len1] = '\0';
            strcat(pcon->error, " (");
            strcat(pcon->error, primerr);
            strcat(pcon->error, ")");
         }
#endif
      }
   }

   if (!pcon->p_gtm_so->p_library) {
      goto gtm_load_library_exit;
   }

   sprintf(fun, "%s_init", pcon->p_gtm_so->funprfx);
   pcon->p_gtm_so->p_gtm_init = (int (*) (void)) mg_dso_sym(pcon->p_gtm_so->p_library, (char *) fun);
   if (!pcon->p_gtm_so->p_gtm_init) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_gtm_so->dbname, pcon->p_gtm_so->libnam, fun);
      goto gtm_load_library_exit;
   }
   sprintf(fun, "%s_exit", pcon->p_gtm_so->funprfx);
   pcon->p_gtm_so->p_gtm_exit = (int (*) (void)) mg_dso_sym(pcon->p_gtm_so->p_library, (char *) fun);
   if (!pcon->p_gtm_so->p_gtm_exit) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_gtm_so->dbname, pcon->p_gtm_so->libnam, fun);
      goto gtm_load_library_exit;
   }

   sprintf(fun, "%s_ci", pcon->p_gtm_so->funprfx);
   pcon->p_gtm_so->p_gtm_ci = (int (*) (const char *, ...)) mg_dso_sym(pcon->p_gtm_so->p_library, (char *) fun);
   if (!pcon->p_gtm_so->p_gtm_ci) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_gtm_so->dbname, pcon->p_gtm_so->libnam, fun);
      goto gtm_load_library_exit;
   }

   sprintf(fun, "%s_zstatus", pcon->p_gtm_so->funprfx);
   pcon->p_gtm_so->p_gtm_ci = (int (*) (const char *, ...)) mg_dso_sym(pcon->p_gtm_so->p_library, (char *) fun);
   if (!pcon->p_gtm_so->p_gtm_ci) {
      sprintf(pcon->error, "Error loading %s library: %s; Cannot locate the following function : %s", pcon->p_gtm_so->dbname, pcon->p_gtm_so->libnam, fun);
      goto gtm_load_library_exit;
   }

   pcon->pid = mg_current_process_id();

   pcon->p_gtm_so->loaded = 1;

gtm_load_library_exit:

   if (pcon->error[0]) {
      pcon->p_gtm_so->loaded = 0;
      pcon->error_code = 1009;
      result = CACHE_NOCON;
      return result;
   }

   return CACHE_SUCCESS;
}


int gtm_open(DBXMETH *pmeth)
{
   int rc, error_code, result;
   char buffer[256];
   DBXCON *pcon = pmeth->pcon;

   error_code = 0;
   rc = CACHE_SUCCESS;

   if (!pcon->p_gtm_so) {
      pcon->p_gtm_so = (DBXGTMSO *) mg_malloc(sizeof(DBXGTMSO), 0);
      if (!pcon->p_gtm_so) {
         strcpy(pcon->error, "No Memory");
         pcon->error_code = 1009; 
         result = CACHE_NOCON;
         return result;
      }
      memset((void *) pcon->p_gtm_so, 0, sizeof(DBXGTMSO));
      pcon->p_gtm_so->loaded = 0;
   }

   if (pcon->p_gtm_so->loaded == 2) {
      strcpy(pcon->error, "Cannot create multiple connections to the database");
      pcon->error_code = 1009; 
      strncpy(pcon->error, pcon->error, DBX_ERROR_SIZE - 1);
      pcon->error[DBX_ERROR_SIZE - 1] = '\0';
      strcpy((char *) pmeth->output_val.svalue.buf_addr, "0");
      rc = CACHE_NOCON;
      goto gtm_open_exit;
   }

   if (!pcon->p_gtm_so->loaded) {
      rc = gtm_load_library(pcon);
      sprintf((char *) pmeth->output_val.svalue.buf_addr, "%d", rc);
      pmeth->output_val.svalue.len_used = (unsigned int) strlen((char *) pmeth->output_val.svalue.buf_addr);
      if (rc != CACHE_SUCCESS) {
         goto gtm_open_exit;
      }
   }

   rc = pcon->p_gtm_so->p_gtm_init();

   rc = (int) pcon->p_gtm_so->p_gtm_ci("ifc_zmgsis", buffer, "0", "", "$zv");

   if (rc == CACHE_SUCCESS) {
      gtm_parse_zv(buffer, &(pcon->zv));
      sprintf(pcon->p_zv->version, "%d.%d.b%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->mg_build);
   }

gtm_open_exit:

   return rc;
}


int gtm_parse_zv(char *zv, DBXZV * p_gtm_sv)
{
   int result;
   double mg_version;
   char *p, *p1, *p2;

   p_gtm_sv->mg_version = 0;
   p_gtm_sv->majorversion = 0;
   p_gtm_sv->minorversion = 0;
   p_gtm_sv->mg_build = 0;
   p_gtm_sv->vnumber = 0;

   result = 0;
   /* GT.M V6.3-004 Linux x86_64 */

   p_gtm_sv->product = DBX_DBTYPE_YOTTADB;

   p = zv;
   mg_version = 0;
   while (*(++ p)) {
      if (*(p - 1) == 'V' && isdigit((int) (*p))) {
         mg_version = strtod(p, NULL);
         break;
      }
   }

   if (mg_version > 0) {
      p_gtm_sv->mg_version = mg_version;
      p_gtm_sv->majorversion = (int) strtol(p, NULL, 10);
      p1 = strstr(p, ".");
      if (p1) {
         p_gtm_sv->minorversion = (int) strtol(p1 + 1, NULL, 10);
      }
      p2 = strstr(p, "-");
      if (p2) {
         p_gtm_sv->mg_build = (int) strtol(p2 + 1, NULL, 10);
      }

      p_gtm_sv->vnumber = ((p_gtm_sv->majorversion * 100000) + (p_gtm_sv->minorversion * 10000) + p_gtm_sv->mg_build);

      result = 1;
   }
/*
   printf("\r\n gtm_parse_zv : p_gtm_sv->majorversion=%d; p_gtm_sv->minorversion=%d; p_gtm_sv->mg_build=%d; p_gtm_sv->mg_version=%f;", p_gtm_sv->majorversion, p_gtm_sv->minorversion, p_gtm_sv->mg_build, p_gtm_sv->mg_version);
*/
   return result;
}


int gtm_error_message(DBXMETH *pmeth, int error_code)
{
   int rc;
   char buffer[256];
   DBXCON *pcon = pmeth->pcon;

   if (pcon->p_gtm_so && pcon->p_gtm_so->p_gtm_zstatus) {
      pcon->p_gtm_so->p_gtm_zstatus(buffer, 255);
      strcpy(pcon->error, buffer);
      rc = CACHE_SUCCESS;
   }
   else {
      if (!pcon->error[0]) {
         strcpy(pcon->error, "No connection has been established");
      }
      rc = CACHE_NOCON;
   }

   mg_set_error_message(pmeth);

   return rc;
}


DBXMETH * mg_unpack_header(unsigned char *input, unsigned char *output)
{
   int len, output_bsize, offset, index;
   DBXCON *pcon;
   DBXMETH *pmeth;

   offset = 0;

   len = (int) mg_get_size(input + offset);
   offset += 5;

   output_bsize = (int) mg_get_size(input + offset);
   offset += 5;

   index = (int) mg_get_size(input + offset);
   offset += 5;

/*
   printf("\r\n mg_unpack_header : input=%p; output=%p; len=%d; output_bsize=%d; index=%d;", input, output, len, output_bsize, index);
*/
   if (index >= 0 && index < DBX_MAXCONS) {
      pcon = connection[index];
   }
   if (!pcon) {
      return NULL;
   }
   pmeth = (DBXMETH *) pcon->pmeth_base;
   pmeth->pcon = pcon;

   pmeth->argc = 0;
   pmeth->input_str.buf_addr = (char *) input;
   pmeth->input_str.len_used = len;

   if (output) {
      pmeth->output_val.svalue.buf_addr = (char *) output;
   }
   else {
      pmeth->output_val.svalue.buf_addr = (char *) input;
   }

   pmeth->output_val.svalue.len_alloc = output_bsize;
/*
   memset((void *) pmeth->output_val.svalue.buf_addr, 0, 5);
*/
   pmeth->output_val.offset = 5;
   pmeth->output_val.svalue.len_used = 5;

   pmeth->offset = offset;

   return pmeth;
}


int mg_unpack_arguments(DBXMETH *pmeth)
{
   int len, dsort, dtype;

   for (;;) {
      len = (int) mg_get_block_size(&(pmeth->input_str), pmeth->offset, &dsort, &dtype);
      pmeth->offset += 5;
/*
      printf("\r\nn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", pmeth->argc, len, pmeth->offset, dsort, dtype, pmeth->input_str.str + offset);
*/
      if (dsort == DBX_DSORT_EOD) {
         break;
      }

      pmeth->args[pmeth->argc].type = dtype;
      pmeth->args[pmeth->argc].svalue.len_used = len;
      pmeth->args[pmeth->argc].svalue.buf_addr = (char *) (pmeth->input_str.buf_addr + pmeth->offset);
      pmeth->offset += len;
      pmeth->argc ++;

      if (pmeth->argc > (DBX_MAXARGS - 1))
         break;
   }

   return pmeth->argc;
}


int mg_global_reference(DBXMETH *pmeth)
{
   int n, rc, len, dsort, dtype;
   unsigned int ne;
   char *p;
   DBXCON *pcon = pmeth->pcon;

   rc = CACHE_SUCCESS;
   for (;;) {
      len = (int) mg_get_block_size(&(pmeth->input_str), pmeth->offset, &dsort, &dtype);
      pmeth->offset += 5;
/*
      printf("\r\nn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", pmeth->argc, len, pmeth->offset, dsort, dtype, pmeth->input_str.buf_addr + pmeth->offset);
*/
      if (dsort == DBX_DSORT_EOD) {
         break;
      }
      p = (char *) (pmeth->input_str.buf_addr + pmeth->offset);

      pmeth->args[pmeth->argc].type = dtype;
      pmeth->args[pmeth->argc].svalue.len_used = len;
      pmeth->args[pmeth->argc].svalue.len_alloc = len;
      pmeth->args[pmeth->argc].svalue.buf_addr = (char *) (pmeth->input_str.buf_addr + pmeth->offset);
      pmeth->offset += len;
      n = pmeth->argc;
      pmeth->argc ++;
      if (pmeth->argc > (DBX_MAXARGS - 1)) {
         break;
      }
      if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
         if (n > 0) {
            pmeth->yargs[n - 1].len_used = pmeth->args[n].svalue.len_used;
            pmeth->yargs[n - 1].len_alloc = pmeth->args[n].svalue.len_alloc;
            pmeth->yargs[n - 1].buf_addr = (char *) pmeth->args[n].svalue.buf_addr;
         }
         continue;
      }

      if (n == 0) {
         if (pmeth->args[n].svalue.buf_addr[0] == '^')
            rc = pcon->p_isc_so->p_CachePushGlobal((int) pmeth->args[n].svalue.len_used - 1, (Callin_char_t *) pmeth->args[n].svalue.buf_addr + 1);
         else
            rc = pcon->p_isc_so->p_CachePushGlobal((int) pmeth->args[n].svalue.len_used, (Callin_char_t *) pmeth->args[n].svalue.buf_addr);
      }
      else {
         if (pmeth->increment && n == (pmeth->argc - 1)) {
            char buffer[32];
            if (pmeth->args[n].svalue.len_used < 32) {
               strncpy(buffer, pmeth->args[n].svalue.buf_addr, pmeth->args[n].svalue.len_used);
               pmeth->args[n].svalue.buf_addr[pmeth->args[n].svalue.len_used] = '\0';
            }
            else {
               buffer[0] = '1';
               buffer[1] = '\0';
            }
            pmeth->args[n].type = DBX_DTYPE_DOUBLE;
            pmeth->args[n].num.real = (double) strtod(buffer, NULL);
         }

         if (pmeth->args[n].type == DBX_DTYPE_INT) {
            rc = pcon->p_isc_so->p_CachePushInt(pmeth->args[n].num.int32);
         }
         else if (pmeth->args[n].type == DBX_DTYPE_DOUBLE) {
            rc = pcon->p_isc_so->p_CachePushDbl(pmeth->args[n].num.real);
         }
         else {
            if (pmeth->args[n].svalue.len_used < DBX_MAXSIZE) {
               rc = pcon->p_isc_so->p_CachePushStr(pmeth->args[n].svalue.len_used, (Callin_char_t *) pmeth->args[n].svalue.buf_addr);
            }
            else {
               pmeth->args[n].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr), pmeth->args[n].svalue.len_used + 1);
               for (ne = 0; ne < pmeth->args[n].svalue.len_used; ne ++) {
                  pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) pmeth->args[n].svalue.buf_addr[ne];
               }
               pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) 0;
               pmeth->args[n].cvalue.zstr.len = pmeth->args[n].svalue.len_used;

               rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr));
            }
         }
      }
      if (rc != CACHE_SUCCESS) {
         break;
      }
   }

   return rc;
}


int mg_class_reference(DBXMETH *pmeth, short context)
{
   int n, rc, len, dsort, dtype, flags;
   unsigned int ne;
   char *p;
   DBXCON *pcon = pmeth->pcon;

   rc = CACHE_SUCCESS;
   for (;;) {
      len = (int) mg_get_block_size(&(pmeth->input_str), pmeth->offset, &dsort, &dtype);
      pmeth->offset += 5;
/*
      printf("\r\nn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", pmeth->argc, len, pmeth->offset, dsort, dtype, pmeth->input_str.buf_addr + pmeth->offset);
*/
      if (dsort == DBX_DSORT_EOD) {
         break;
      }
      p = (char *) (pmeth->input_str.buf_addr + pmeth->offset);

      pmeth->args[pmeth->argc].type = dtype;
      pmeth->args[pmeth->argc].svalue.len_used = len;
      pmeth->args[pmeth->argc].svalue.len_alloc = len;
      pmeth->args[pmeth->argc].svalue.buf_addr = (char *) (pmeth->input_str.buf_addr + pmeth->offset);
      pmeth->offset += len;
      n = pmeth->argc;
      pmeth->argc ++;
      if (pmeth->argc > (DBX_MAXARGS - 1)) {
         break;
      }
      if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
         if (n > 0) {
            pmeth->yargs[n - 1].len_used = pmeth->args[n].svalue.len_used;
            pmeth->yargs[n - 1].len_alloc = pmeth->args[n].svalue.len_alloc;
            pmeth->yargs[n - 1].buf_addr = (char *) pmeth->args[n].svalue.buf_addr;
         }
         continue;
      }

      if (context == 0) { /* classmethod */
         if (n == 0) {
            continue;
         }
         else if (n == 1) {
            flags = 1;
            rc = pcon->p_isc_so->p_CachePushClassMethod(pmeth->args[0].svalue.len_used, (const Callin_char_t *) pmeth->args[0].svalue.buf_addr, pmeth->args[1].svalue.len_used, (const Callin_char_t *) pmeth->args[1].svalue.buf_addr, flags);
            continue;
         }
      }
      else if (context == 1) { /* method */
         if (n == 0) {
            continue;
         }
         else if (n == 1) {
            flags = 1;
            rc = pcon->p_isc_so->p_CachePushMethod((int) strtol(pmeth->args[0].svalue.buf_addr, NULL, 10), pmeth->args[1].svalue.len_used, (const Callin_char_t *) pmeth->args[1].svalue.buf_addr, flags);
            continue;
         }
      }
      else if (context == 2) { /* property */
         if (n == 0) {
            continue;
         }
         else if (n == 1) {
            flags = 1;
            rc = pcon->p_isc_so->p_CachePushProperty((int) strtol(pmeth->args[0].svalue.buf_addr, NULL, 10), pmeth->args[1].svalue.len_used, (const Callin_char_t *) pmeth->args[1].svalue.buf_addr);
            continue;
         }
      }
      else if (context == 2) { /* close instance */
         if (n == 0) {
            rc = pcon->p_isc_so->p_CacheCloseOref((int) strtol(pmeth->args[0].svalue.buf_addr, NULL, 10));
            break;
         }
      }

      if (pmeth->args[n].type == DBX_DTYPE_INT) {
         rc = pcon->p_isc_so->p_CachePushInt(pmeth->args[n].num.int32);
      }
      else if (pmeth->args[n].type == DBX_DTYPE_DOUBLE) {
         rc = pcon->p_isc_so->p_CachePushDbl(pmeth->args[n].num.real);
      }
      else {
         if (pmeth->args[n].svalue.len_used < DBX_MAXSIZE) {
            rc = pcon->p_isc_so->p_CachePushStr(pmeth->args[n].svalue.len_used, (Callin_char_t *) pmeth->args[n].svalue.buf_addr);
         }
         else {
            pmeth->args[n].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr), pmeth->args[n].svalue.len_used + 1);
            for (ne = 0; ne < pmeth->args[n].svalue.len_used; ne ++) {
               pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) pmeth->args[n].svalue.buf_addr[ne];
            }
            pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) 0;
            pmeth->args[n].cvalue.zstr.len = pmeth->args[n].svalue.len_used;

            rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr));
         }
      }
      if (rc != CACHE_SUCCESS) {
         break;
      }
   }

   return rc;
}


int mg_function_reference(DBXMETH *pmeth, DBXFUN *pfun)
{
   int n, rc, len, dsort, dtype;
   unsigned int ne;
   char *p;
   DBXCON *pcon = pmeth->pcon;

   rc = CACHE_SUCCESS;
   for (;;) {
      len = (int) mg_get_block_size(&(pmeth->input_str), pmeth->offset, &dsort, &dtype);
      pmeth->offset += 5;
/*
      printf("\r\nn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", pmeth->argc, len, pmeth->offset, dsort, dtype, pmeth->input_str.buf_addr + pmeth->offset);
*/
      if (dsort == DBX_DSORT_EOD) {
         break;
      }
      p = (char *) (pmeth->input_str.buf_addr + pmeth->offset);

      pmeth->args[pmeth->argc].type = dtype;
      pmeth->args[pmeth->argc].svalue.len_used = len;
      pmeth->args[pmeth->argc].svalue.len_alloc = len;
      pmeth->args[pmeth->argc].svalue.buf_addr = (char *) (pmeth->input_str.buf_addr + pmeth->offset);
      pmeth->offset += len;
      n = pmeth->argc;
      pmeth->argc ++;
      if (pmeth->argc > (DBX_MAXARGS - 1)) {
         break;
      }
      if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
         if (n > 0) {
            pmeth->yargs[n - 1].len_used = pmeth->args[n].svalue.len_used;
            pmeth->yargs[n - 1].len_alloc = pmeth->args[n].svalue.len_alloc;
            pmeth->yargs[n - 1].buf_addr = (char *) pmeth->args[n].svalue.buf_addr;
         }
      }

      if (n == 0) {
         strncpy(pfun->buffer, pmeth->args[n].svalue.buf_addr, pmeth->args[n].svalue.len_used);
         pfun->buffer[pmeth->args[n].svalue.len_used] = '\0';
         pfun->label = pfun->buffer;
         pfun->routine = strstr(pfun->buffer, "^");
         *pfun->routine = '\0';
         pfun->routine ++;
         pfun->label_len = (int) strlen(pfun->label);
         pfun->routine_len = (int) strlen(pfun->routine);

         if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
            rc = pcon->p_isc_so->p_CachePushFunc(&(pfun->rflag), (int) pfun->label_len, (const Callin_char_t *) pfun->label, (int) pfun->routine_len, (const Callin_char_t *) pfun->routine);
         }

      }
      else {

         if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
            continue;
         }

         if (pmeth->args[n].type == DBX_DTYPE_INT) {
            rc = pcon->p_isc_so->p_CachePushInt(pmeth->args[n].num.int32);
         }
         else if (pmeth->args[n].type == DBX_DTYPE_DOUBLE) {
            rc = pcon->p_isc_so->p_CachePushDbl(pmeth->args[n].num.real);
         }
         else {
            if (pmeth->args[n].svalue.len_used < DBX_MAXSIZE) {
               rc = pcon->p_isc_so->p_CachePushStr(pmeth->args[n].svalue.len_used, (Callin_char_t *) pmeth->args[n].svalue.buf_addr);
            }
            else {
               pmeth->args[n].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr), pmeth->args[n].svalue.len_used + 1);
               for (ne = 0; ne < pmeth->args[n].svalue.len_used; ne ++) {
                  pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) pmeth->args[n].svalue.buf_addr[ne];
               }
               pmeth->args[n].cvalue.zstr.str.ch[ne] = (char) 0;
               pmeth->args[n].cvalue.zstr.len = pmeth->args[n].svalue.len_used;

               rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pmeth->args[n].cvalue.zstr));
            }
         }
      }
      if (rc != CACHE_SUCCESS) {
         break;
      }
   }

   return rc;
}


int mg_add_block_size(DBXSTR *block, unsigned long offset, unsigned long data_len, int dsort, int dtype)
{
   mg_set_size((unsigned char *) block->buf_addr + offset, data_len);
   block->buf_addr[offset + 4] = (unsigned char) ((dsort * 20) + dtype);

   return 1;
}


unsigned long mg_get_block_size(DBXSTR *block, unsigned long offset, int *dsort, int *dtype)
{
   unsigned long data_len;
   unsigned char uc;

   data_len = 0;
   uc = (unsigned char) block->buf_addr[offset + 4];
   *dtype = uc % 20;
   *dsort = uc / 20;
   if (*dsort != DBX_DSORT_STATUS) {
      data_len = mg_get_size((char *) block->buf_addr + offset);
   }

   /* printf("\r\n mg_get_block_size %x:%x:%x:%x dlen=%lu; offset=%lu; type=%d (%x);\r\n", block->str[offset + 0], block->str[offset + 1], block->str[offset + 2], block->str[offset + 3], data_len, offset, *type, block->str[offset + 4]); */

   if (!DBX_DSORT_ISVALID(*dsort)) {
      *dsort = DBX_DSORT_INVALID;
   }

   return data_len;
}


int mg_set_size(unsigned char *str, unsigned long data_len)
{
   str[0] = (unsigned char) (data_len >> 0);
   str[1] = (unsigned char) (data_len >> 8);
   str[2] = (unsigned char) (data_len >> 16);
   str[3] = (unsigned char) (data_len >> 24);

   return 0;
}


unsigned long mg_get_size(unsigned char *str)
{
   unsigned long size;

   size = ((unsigned char) str[0]) | (((unsigned char) str[1]) << 8) | (((unsigned char) str[2]) << 16) | (((unsigned char) str[3]) << 24);
   return size;
}


int mg_buf_init(MGBUF *p_buf, int size, int increment_size)
{
   int result;

   p_buf->p_buffer = (unsigned char *) mg_malloc(sizeof(char) * (size + 1), 0);
   if (p_buf->p_buffer) {
      *(p_buf->p_buffer) = '\0';
      result = 1;
   }
   else {
      result = 0;
      p_buf->p_buffer = (unsigned char *) mg_malloc(sizeof(char), 0);
      if (p_buf->p_buffer) {
         *(p_buf->p_buffer) = '\0';
         size = 1;
      }
      else
         size = 0;
   }

   p_buf->size = size;
   p_buf->increment_size = increment_size;
   p_buf->data_size = 0;

   return result;
}


int mg_buf_resize(MGBUF *p_buf, unsigned long size)
{
   if (size < MG_BUFSIZE)
      return 1;

   if (size < p_buf->size)
      return 1;

   p_buf->p_buffer = (unsigned char *) mg_realloc((void *) p_buf->p_buffer, 0, sizeof(char) * size, 0);
   p_buf->size = size;

   return 1;
}


int mg_buf_free(MGBUF *p_buf)
{
   if (!p_buf)
      return 0;

   if (p_buf->p_buffer)
      mg_free((void *) p_buf->p_buffer, 0);

   p_buf->p_buffer = NULL;
   p_buf->size = 0;
   p_buf->increment_size = 0;
   p_buf->data_size = 0;

   return 1;
}


int mg_buf_cpy(LPMGBUF p_buf, char *buffer, unsigned long size)
{
   unsigned long  result, req_size, csize, increment_size;

   result = 1;

   if (size == 0)
      size = (unsigned long) strlen(buffer);

   if (size == 0) {
      p_buf->data_size = 0;
      p_buf->p_buffer[p_buf->data_size] = '\0';
      return result;
   }

   req_size = size;
   if (req_size > p_buf->size) {
      csize = p_buf->size;
      increment_size = p_buf->increment_size;
      while (req_size > csize)
         csize = csize + p_buf->increment_size;
      mg_buf_free(p_buf);
      result = mg_buf_init(p_buf, (int) size, (int) increment_size);
   }
   if (result) {
      memcpy((void *) p_buf->p_buffer, (void *) buffer, size);
      p_buf->data_size = req_size;
      p_buf->p_buffer[p_buf->data_size] = '\0';
   }

   return result;
}


int mg_buf_cat(LPMGBUF p_buf, char *buffer, unsigned long size)
{
   unsigned long int result, req_size, csize, tsize, increment_size;
   unsigned char *p_temp;

   result = 1;

   if (size == 0)
      size = (unsigned long ) strlen(buffer);

   if (size == 0)
      return result;

   p_temp = NULL;
   req_size = (size + p_buf->data_size);
   tsize = p_buf->data_size;
   if (req_size > p_buf->size) {
      csize = p_buf->size;
      increment_size = p_buf->increment_size;
      while (req_size > csize)
         csize = csize + p_buf->increment_size;
      p_temp = p_buf->p_buffer;
      result = mg_buf_init(p_buf, (int) csize, (int) increment_size);
      if (result) {
         if (p_temp) {
            memcpy((void *) p_buf->p_buffer, (void *) p_temp, tsize);
            p_buf->data_size = tsize;
            mg_free((void *) p_temp, 0);
         }
      }
      else
         p_buf->p_buffer = p_temp;
   }
   if (result) {
      memcpy((void *) (p_buf->p_buffer + tsize), (void *) buffer, size);
      p_buf->data_size = req_size;
      p_buf->p_buffer[p_buf->data_size] = '\0';
   }

   return result;
}


void * mg_realloc(void *p, int curr_size, int new_size, short id)
{
   if (dbx_ext_realloc) {
      p = (void *) dbx_ext_realloc((void *) p, (unsigned long) new_size);
   }
   else {
      if (new_size >= curr_size) {
         if (p) {
            mg_free((void *) p, 0);
         }

#if defined(_WIN32)
         p = (void *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, new_size + 32);
#else
         p = (void *) mg_malloc(new_size, id);
#endif
      }
   }

   /* printf("\r\n curr_size=%d; new_size=%d;\r\n", curr_size, new_size); */

   return p;
}


void * mg_malloc(int size, short id)
{
   void *p;

   if (dbx_ext_malloc) {
      p = (void *) dbx_ext_malloc((unsigned long) size);
   }
   else {
#if defined(_WIN32)
      p = (void *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 32);
#else
      p = (void *) malloc(size);
#endif
   }

   /* printf("\nmg_malloc: size=%d; id=%d; p=%p;", size, id, p); */

   return p;
}


int mg_free(void *p, short id)
{
   /* printf("\nmg_free: id=%d; p=%p;", id, p); */

   if (dbx_ext_free) {
      dbx_ext_free((void *) p);
   }
   else {
#if defined(_WIN32)
      HeapFree(GetProcessHeap(), 0, p);
#else
      free((void *) p);
#endif
   }

   return 0;
}


int mg_lcase(char *string)
{
#ifdef _UNICODE

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


int mg_create_string(DBXMETH *pmeth, void *data, short type)
{
   int len;
   DBXSTR *pstrobj_in;

   if (!data) {
      return -1;
   }

   len = 0;

   if (type == DBX_DTYPE_DBXSTR) {
      pstrobj_in = (DBXSTR *) data;
      type = DBX_DTYPE_STR;
      data = (void *) pstrobj_in->buf_addr;
   }

   if (type == DBX_DTYPE_STR) { 
      strcpy((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.svalue.len_used, (char *) data);
      len = (int) strlen ((char *) data);
   }
   else if (type == DBX_DTYPE_INT) {
      sprintf((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.svalue.len_used, "%d", (int) *((int *) data));
   }
   else {
      pmeth->output_val.svalue.buf_addr[pmeth->output_val.svalue.len_used] = '\0';
   }

   len = (int) strlen((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.svalue.len_used);

   pmeth->output_val.svalue.len_used += len;
   mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) len, DBX_DSORT_DATA, DBX_DTYPE_DBXSTR);

   return (int) len;
}


int mg_log_init(DBXLOG *p_log)
{
   p_log->log_errors = 0;
   p_log->log_functions = 0;
   p_log->log_transmissions = 0;
   p_log->log_file[0] = '\0';
   p_log->log_level[0] = '\0';
   p_log->log_filter[0] = '\0';
   p_log->fun_no = 0;
   p_log->req_no = 0;

   p_log->product[0] = '\0';
   p_log->product_version[0] = '\0';

   return 0;
}


int mg_log_event(DBXLOG *p_log, char *message, char *title, int level)
{
   int len, n;
   char timestr[64], heading[256], buffer[2048];
   char *p_buffer;
   time_t now = 0;
#if defined(_WIN32)
   HANDLE hLogfile = 0;
   DWORD dwPos = 0, dwBytesWritten = 0;
#else
   FILE *fp = NULL;
   struct flock lock;
#endif

   now = time(NULL);
   sprintf(timestr, "%s", ctime(&now));
   for (n = 0; timestr[n] != '\0'; n ++) {
      if ((unsigned int) timestr[n] < 32) {
         timestr[n] = '\0';
         break;
      }
   }

   sprintf(heading, ">>> Time: %s; Build: %s pid=%lu;tid=%lu;req_no=%lu;fun_no=%lu", timestr, DBX_VERSION, (unsigned long) mg_current_process_id(), (unsigned long) mg_current_thread_id(), p_log->req_no, p_log->fun_no);

   len = (int) strlen(heading) + (int) strlen(title) + (int) strlen(message) + 20;

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
   strcat(p_buffer, message);
   len = (int) strlen(p_buffer) * sizeof(char);

#if defined(_WIN32)

   strcat(p_buffer, "\r\n");
   len = len + (2 * sizeof(char));
   hLogfile = CreateFileA(p_log->log_file, GENERIC_WRITE, FILE_SHARE_WRITE,
                         (LPSECURITY_ATTRIBUTES) NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL);
   dwPos = SetFilePointer(hLogfile, 0, (LPLONG) NULL, FILE_END);
   LockFile(hLogfile, dwPos, 0, dwPos + len, 0);
   WriteFile(hLogfile, (LPTSTR) p_buffer, len, &dwBytesWritten, NULL);
   UnlockFile(hLogfile, dwPos, 0, dwPos + len, 0);
   CloseHandle(hLogfile);

#else /* UNIX or VMS */

   strcat(p_buffer, "\n");
   fp = fopen(p_log->log_file, "a");
   if (fp) {

      lock.l_type = F_WRLCK;
      lock.l_start = 0;
      lock.l_whence = SEEK_SET;
      lock.l_len = 0;
      n = fcntl(fileno(fp), F_SETLKW, &lock);

      fputs(p_buffer, fp);
      fclose(fp);

      lock.l_type = F_UNLCK;
      lock.l_start = 0;
      lock.l_whence = SEEK_SET;
      lock.l_len = 0;
      n = fcntl(fileno(fp), F_SETLK, &lock);
   }

#endif

   if (p_buffer != buffer)
      free((void *) p_buffer);

   return 1;
}


int mg_log_buffer(DBXLOG *p_log, char *buffer, int buffer_len, char *title, int level)
{
   unsigned int c, len, strt;
   int n, n1, nc, size;
   char tmp[16];
   char *p;

   for (n = 0, nc = 0; n < buffer_len; n ++) {
      c = (unsigned int) buffer[n];
      if (c < 32 || c > 126)
         nc ++;
   }

   size = buffer_len + (nc * 4) + 32;
   p = (char *) malloc(sizeof(char) * size);
   if (!p)
      return 0;

   if (nc) {

      for (n = 0, nc = 0; n < buffer_len; n ++) {
         c = (unsigned int) buffer[n];
         if (c < 32 || c > 126) {
            sprintf((char *) tmp, "%02x", c);
            len = (int) strlen(tmp);
            if (len > 2)
               strt = len - 2;
            else
               strt = 0;
            p[nc ++] = '\\';
            p[nc ++] = 'x';
            for (n1 = strt; tmp[n1]; n1 ++)
               p[nc ++] = tmp[n1];
         }
         else
            p[nc ++] = buffer[n];
      }
      p[nc] = '\0';
   }
   else {
      strncpy(p, buffer, buffer_len);
      p[buffer_len] = '\0';
   }

   mg_log_event(p_log, (char *) p, title, level);

   free((void *) p);

   return 1;
}


int mg_pause(int msecs)
{
#if defined(_WIN32)

   Sleep((DWORD) msecs);

#else

#if 1
   unsigned int secs, msecs_rem;

   secs = (unsigned int) (msecs / 1000);
   msecs_rem = (unsigned int) (msecs % 1000);

   /* printf("\n   ===> msecs=%ld; secs=%ld; msecs_rem=%ld", msecs, secs, msecs_rem); */

   if (secs > 0) {
      sleep(secs);
   }
   if (msecs_rem > 0) {
      usleep((useconds_t) (msecs_rem * 1000));
   }

#else
   unsigned int secs;

   secs = (unsigned int) (msecs / 1000);
   if (secs == 0)
      secs = 1;
   sleep(secs);

#endif

#endif

   return 0;
}


DBXPLIB mg_dso_load(char * library)
{
   DBXPLIB p_library;

#if defined(_WIN32)
   p_library = LoadLibraryA(library);
#else
   p_library = dlopen(library, RTLD_NOW);
#endif

   return p_library;
}


DBXPROC mg_dso_sym(DBXPLIB p_library, char * symbol)
{
   DBXPROC p_proc;

#if defined(_WIN32)
   p_proc = GetProcAddress(p_library, symbol);
#else
   p_proc  = (void *) dlsym(p_library, symbol);
#endif

   return p_proc;
}



int mg_dso_unload(DBXPLIB p_library)
{

#if defined(_WIN32)
   FreeLibrary(p_library);
#else
   dlclose(p_library); 
#endif

   return 1;
}


DBXTHID mg_current_thread_id(void)
{
#if defined(_WIN32)
   return (DBXTHID) GetCurrentThreadId();
#else
   return (DBXTHID) pthread_self();
#endif
}


unsigned long mg_current_process_id(void)
{
#if defined(_WIN32)
   return (unsigned long) GetCurrentProcessId();
#else
   return ((unsigned long) getpid());
#endif
}


int mg_error_message(DBXMETH *pmeth, int error_code)
{
   int rc;

   if (pmeth->pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_error_message(pmeth, error_code);
   }
   else {
      rc = isc_error_message(pmeth, error_code);
   }

   return rc;
}


int mg_set_error_message(DBXMETH *pmeth)
{
   int len;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->p_srv) {
      strcpy(((MGSRV *) pcon->p_srv)->error_mess, pcon->error);
   }
   else {
      len = (int) strlen(pcon->error);
      strcpy((char *) pmeth->output_val.svalue.buf_addr + pmeth->output_val.svalue.len_used, pcon->error   );
      pmeth->output_val.svalue.len_used += len;
      mg_add_block_size(&(pmeth->output_val.svalue), 0, (unsigned long) len, DBX_DSORT_ERROR, DBX_DTYPE_DBXSTR);
   }

   return 0;
}


int mg_set_error_message_ex(unsigned char *output, char *error_message)
{
   mg_set_size((unsigned char *) output, (int) strlen(error_message));

   output[4] = (unsigned char) ((DBX_DSORT_ERROR * 20) + DBX_DTYPE_DBXSTR);
   strcpy((char *) output + 5, error_message);

   return 0;
}


int mg_cleanup(DBXMETH *pmeth)
{
   int n, rc;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->connected == 2) {
      return 0;
   }
   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      return 0;
   }

   for (n = 0; n < DBX_MAXARGS; n ++) {
      if (pmeth->args[n].cvalue.pstr) {
         /* printf("\r\nmg_cleanup %d &zstr=%p; pstr=%p;", n, &(pcon->cargs[n].zstr), pcon->cargs[n].pstr); */
         rc = pcon->p_isc_so->p_CacheExStrKill(&(pmeth->args[n].cvalue.zstr));

         pmeth->args[n].cvalue.pstr = NULL;
      }
   }
   return 1;
}


int mg_mutex_create(DBXMUTEX *p_mutex)
{
   int result;

   result = 0;
   if (p_mutex->created) {
      return result;
   }

#if defined(_WIN32)
   p_mutex->h_mutex = CreateMutex(NULL, FALSE, NULL);
   result = 0;
#else
   result = pthread_mutex_init(&(p_mutex->h_mutex), NULL);
#endif

   p_mutex->created = 1;
   p_mutex->stack = 0;
   p_mutex->thid = 0;

   return result;
}



int mg_mutex_lock(DBXMUTEX *p_mutex, int timeout)
{
   int result;
   DBXTHID tid;
#ifdef _WIN32
   DWORD result_wait;
#endif

   result = 0;

   if (!p_mutex->created) {
      return -1;
   }

   tid = mg_current_thread_id();
   if (p_mutex->thid == tid) {
      p_mutex->stack ++;
      /* printf("\r\n thread already owns lock : thid=%lu; stack=%d;\r\n", (unsigned long) tid, p_mutex->stack); */
      return 0; /* success - thread already owns lock */
   }

#if defined(_WIN32)
   if (timeout == 0) {
      result_wait = WaitForSingleObject(p_mutex->h_mutex, INFINITE);
   }
   else {
      result_wait = WaitForSingleObject(p_mutex->h_mutex, (timeout * 1000));
   }

   if (result_wait == WAIT_OBJECT_0) { /* success */
      result = 0;
   }
   else if (result_wait == WAIT_ABANDONED) {
      printf("\r\nmg_mutex_lock: Returned WAIT_ABANDONED state");
      result = -1;
   }
   else if (result_wait == WAIT_TIMEOUT) {
      printf("\r\nmg_mutex_lock: Returned WAIT_TIMEOUT state");
      result = -1;
   }
   else if (result_wait == WAIT_FAILED) {
      printf("\r\nmg_mutex_lock: Returned WAIT_FAILED state: Error Code: %d", GetLastError());
      result = -1;
   }
   else {
      printf("\r\nmg_mutex_lock: Returned Unrecognized state: %d", result_wait);
      result = -1;
   }
#else
   result = pthread_mutex_lock(&(p_mutex->h_mutex));
#endif

   p_mutex->thid = tid;
   p_mutex->stack = 0;

   return result;
}


int mg_mutex_unlock(DBXMUTEX *p_mutex)
{
   int result;
   DBXTHID tid;

   result = 0;

   if (!p_mutex->created) {
      return -1;
   }

   tid = mg_current_thread_id();
   if (p_mutex->thid == tid && p_mutex->stack) {
      /* printf("\r\n thread has stacked locks : thid=%lu; stack=%d;\r\n", (unsigned long) tid, p_mutex->stack); */
      p_mutex->stack --;
      return 0;
   }
   p_mutex->thid = 0;
   p_mutex->stack = 0;

#if defined(_WIN32)
   ReleaseMutex(p_mutex->h_mutex);
   result = 0;
#else
   result = pthread_mutex_unlock(&(p_mutex->h_mutex));
#endif /* #if defined(_WIN32) */

   return result;
}


int mg_mutex_destroy(DBXMUTEX *p_mutex)
{
   int result;

   if (!p_mutex->created) {
      return -1;
   }

#if defined(_WIN32)
   CloseHandle(p_mutex->h_mutex);
   result = 0;
#else
   result = pthread_mutex_destroy(&(p_mutex->h_mutex));
#endif

   p_mutex->created = 0;

   return result;
}


int mg_init_critical_section(void *p_crit)
{
#if defined(_WIN32)
   InitializeCriticalSection((LPCRITICAL_SECTION) p_crit);
#endif

   return 0;
}


int mg_delete_critical_section(void *p_crit)
{
#if defined(_WIN32)
   DeleteCriticalSection((LPCRITICAL_SECTION) p_crit);
#endif

   return 0;
}


int mg_enter_critical_section(void *p_crit)
{
   int result;

#if defined(_WIN32)
   EnterCriticalSection((LPCRITICAL_SECTION) p_crit);
   result = 0;
#else
   result = pthread_mutex_lock((pthread_mutex_t *) p_crit);
#endif
   return result;
}


int mg_leave_critical_section(void *p_crit)
{
   int result;

#if defined(_WIN32)
   LeaveCriticalSection((LPCRITICAL_SECTION) p_crit);
   result = 0;
#else
   result = pthread_mutex_unlock((pthread_mutex_t *) p_crit);
#endif
   return result;
}


int mg_sleep(unsigned long msecs)
{
#if defined(_WIN32)

   Sleep((DWORD) msecs);

#else

#if 1
   unsigned int secs, msecs_rem;

   secs = (unsigned int) (msecs / 1000);
   msecs_rem = (unsigned int) (msecs % 1000);

   /* printf("\n   ===> msecs=%ld; secs=%ld; msecs_rem=%ld", msecs, secs, msecs_rem); */

   if (secs > 0) {
      sleep(secs);
   }
   if (msecs_rem > 0) {
      usleep((useconds_t) (msecs_rem * 1000));
   }

#else
   unsigned int secs;

   secs = (unsigned int) (msecs / 1000);
   if (secs == 0)
      secs = 1;
   sleep(secs);

#endif

#endif

   return 0;
}

int netx_load_winsock(DBXCON *pcon, int context)
{
#if defined(_WIN32)
   int result, mem_locked;
   char buffer[1024];

   result = 0;
   mem_locked = 0;
   *buffer = '\0';
   netx_so.version_requested = 0;

   if (netx_so.load_attempted) {
      return result;
   }

   if (netx_so.load_attempted) {
      goto netx_load_winsock_no_so;
   }

   netx_so.sock = 0;

   /* Try to Load the Winsock 2 library */

   netx_so.winsock = 2;
   strcpy(netx_so.libnam, "WS2_32.DLL");

   netx_so.plibrary = mg_dso_load(netx_so.libnam);

   if (!netx_so.plibrary) {
      netx_so.winsock = 1;
      strcpy(netx_so.libnam, "WSOCK32.DLL");
      netx_so.plibrary = mg_dso_load(netx_so.libnam);

      if (!netx_so.plibrary) {
         goto netx_load_winsock_no_so;
      }
   }

   netx_so.p_WSASocket             = (MG_LPFN_WSASOCKET)              mg_dso_sym(netx_so.plibrary, "WSASocketA");
   netx_so.p_WSAGetLastError       = (MG_LPFN_WSAGETLASTERROR)        mg_dso_sym(netx_so.plibrary, "WSAGetLastError");
   netx_so.p_WSAStartup            = (MG_LPFN_WSASTARTUP)             mg_dso_sym(netx_so.plibrary, "WSAStartup");
   netx_so.p_WSACleanup            = (MG_LPFN_WSACLEANUP)             mg_dso_sym(netx_so.plibrary, "WSACleanup");
   netx_so.p_WSAFDIsSet            = (MG_LPFN_WSAFDISSET)             mg_dso_sym(netx_so.plibrary, "__WSAFDIsSet");
   netx_so.p_WSARecv               = (MG_LPFN_WSARECV)                mg_dso_sym(netx_so.plibrary, "WSARecv");
   netx_so.p_WSASend               = (MG_LPFN_WSASEND)                mg_dso_sym(netx_so.plibrary, "WSASend");

#if defined(NETX_IPV6)
   netx_so.p_WSAStringToAddress    = (MG_LPFN_WSASTRINGTOADDRESS)     mg_dso_sym(netx_so.plibrary, "WSAStringToAddressA");
   netx_so.p_WSAAddressToString    = (MG_LPFN_WSAADDRESSTOSTRING)     mg_dso_sym(netx_so.plibrary, "WSAAddressToStringA");
   netx_so.p_getaddrinfo           = (MG_LPFN_GETADDRINFO)            mg_dso_sym(netx_so.plibrary, "getaddrinfo");
   netx_so.p_freeaddrinfo          = (MG_LPFN_FREEADDRINFO)           mg_dso_sym(netx_so.plibrary, "freeaddrinfo");
   netx_so.p_getnameinfo           = (MG_LPFN_GETNAMEINFO)            mg_dso_sym(netx_so.plibrary, "getnameinfo");
   netx_so.p_getpeername           = (MG_LPFN_GETPEERNAME)            mg_dso_sym(netx_so.plibrary, "getpeername");
   netx_so.p_inet_ntop             = (MG_LPFN_INET_NTOP)              mg_dso_sym(netx_so.plibrary, "InetNtop");
   netx_so.p_inet_pton             = (MG_LPFN_INET_PTON)              mg_dso_sym(netx_so.plibrary, "InetPton");
#else
   netx_so.p_WSAStringToAddress    = NULL;
   netx_so.p_WSAAddressToString    = NULL;
   netx_so.p_getaddrinfo           = NULL;
   netx_so.p_freeaddrinfo          = NULL;
   netx_so.p_getnameinfo           = NULL;
   netx_so.p_getpeername           = NULL;
   netx_so.p_inet_ntop             = NULL;
   netx_so.p_inet_pton             = NULL;
#endif

   netx_so.p_closesocket           = (MG_LPFN_CLOSESOCKET)            mg_dso_sym(netx_so.plibrary, "closesocket");
   netx_so.p_gethostname           = (MG_LPFN_GETHOSTNAME)            mg_dso_sym(netx_so.plibrary, "gethostname");
   netx_so.p_gethostbyname         = (MG_LPFN_GETHOSTBYNAME)          mg_dso_sym(netx_so.plibrary, "gethostbyname");
   netx_so.p_getservbyname         = (MG_LPFN_GETSERVBYNAME)          mg_dso_sym(netx_so.plibrary, "getservbyname");
   netx_so.p_gethostbyaddr         = (MG_LPFN_GETHOSTBYADDR)          mg_dso_sym(netx_so.plibrary, "gethostbyaddr");
   netx_so.p_htons                 = (MG_LPFN_HTONS)                  mg_dso_sym(netx_so.plibrary, "htons");
   netx_so.p_htonl                 = (MG_LPFN_HTONL)                  mg_dso_sym(netx_so.plibrary, "htonl");
   netx_so.p_ntohl                 = (MG_LPFN_NTOHL)                  mg_dso_sym(netx_so.plibrary, "ntohl");
   netx_so.p_ntohs                 = (MG_LPFN_NTOHS)                  mg_dso_sym(netx_so.plibrary, "ntohs");
   netx_so.p_connect               = (MG_LPFN_CONNECT)                mg_dso_sym(netx_so.plibrary, "connect");
   netx_so.p_inet_addr             = (MG_LPFN_INET_ADDR)              mg_dso_sym(netx_so.plibrary, "inet_addr");
   netx_so.p_inet_ntoa             = (MG_LPFN_INET_NTOA)              mg_dso_sym(netx_so.plibrary, "inet_ntoa");

   netx_so.p_socket                = (MG_LPFN_SOCKET)                 mg_dso_sym(netx_so.plibrary, "socket");
   netx_so.p_setsockopt            = (MG_LPFN_SETSOCKOPT)             mg_dso_sym(netx_so.plibrary, "setsockopt");
   netx_so.p_getsockopt            = (MG_LPFN_GETSOCKOPT)             mg_dso_sym(netx_so.plibrary, "getsockopt");
   netx_so.p_getsockname           = (MG_LPFN_GETSOCKNAME)            mg_dso_sym(netx_so.plibrary, "getsockname");

   netx_so.p_select                = (MG_LPFN_SELECT)                 mg_dso_sym(netx_so.plibrary, "select");
   netx_so.p_recv                  = (MG_LPFN_RECV)                   mg_dso_sym(netx_so.plibrary, "recv");
   netx_so.p_send                  = (MG_LPFN_SEND)                   mg_dso_sym(netx_so.plibrary, "send");
   netx_so.p_shutdown              = (MG_LPFN_SHUTDOWN)               mg_dso_sym(netx_so.plibrary, "shutdown");
   netx_so.p_bind                  = (MG_LPFN_BIND)                   mg_dso_sym(netx_so.plibrary, "bind");
   netx_so.p_listen                = (MG_LPFN_LISTEN)                 mg_dso_sym(netx_so.plibrary, "listen");
   netx_so.p_accept                = (MG_LPFN_ACCEPT)                 mg_dso_sym(netx_so.plibrary, "accept");

   if (   (netx_so.p_WSASocket              == NULL && netx_so.winsock == 2)
       ||  netx_so.p_WSAGetLastError        == NULL
       ||  netx_so.p_WSAStartup             == NULL
       ||  netx_so.p_WSACleanup             == NULL
       ||  netx_so.p_WSAFDIsSet             == NULL
       || (netx_so.p_WSARecv                == NULL && netx_so.winsock == 2)
       || (netx_so.p_WSASend                == NULL && netx_so.winsock == 2)

#if defined(NETX_IPV6)
       || (netx_so.p_WSAStringToAddress     == NULL && netx_so.winsock == 2)
       || (netx_so.p_WSAAddressToString     == NULL && netx_so.winsock == 2)
       ||  netx_so.p_getpeername            == NULL
#endif

       ||  netx_so.p_closesocket            == NULL
       ||  netx_so.p_gethostname            == NULL
       ||  netx_so.p_gethostbyname          == NULL
       ||  netx_so.p_getservbyname          == NULL
       ||  netx_so.p_gethostbyaddr          == NULL
       ||  netx_so.p_htons                  == NULL
       ||  netx_so.p_htonl                  == NULL
       ||  netx_so.p_ntohl                  == NULL
       ||  netx_so.p_ntohs                  == NULL
       ||  netx_so.p_connect                == NULL
       ||  netx_so.p_inet_addr              == NULL
       ||  netx_so.p_inet_ntoa              == NULL
       ||  netx_so.p_socket                 == NULL
       ||  netx_so.p_setsockopt             == NULL
       ||  netx_so.p_getsockopt             == NULL
       ||  netx_so.p_getsockname            == NULL
       ||  netx_so.p_select                 == NULL
       ||  netx_so.p_recv                   == NULL
       ||  netx_so.p_send                   == NULL
       ||  netx_so.p_shutdown               == NULL
       ||  netx_so.p_bind                   == NULL
       ||  netx_so.p_listen                 == NULL
       ||  netx_so.p_accept                 == NULL
      ) {

      sprintf(buffer, "Cannot use Winsock library (WSASocket=%p; WSAGetLastError=%p; WSAStartup=%p; WSACleanup=%p; WSAFDIsSet=%p; WSARecv=%p; WSASend=%p; WSAStringToAddress=%p; WSAAddressToString=%p; closesocket=%p; gethostname=%p; gethostbyname=%p; getservbyname=%p; gethostbyaddr=%p; getaddrinfo=%p; freeaddrinfo=%p; getnameinfo=%p; getpeername=%p; htons=%p; htonl=%p; ntohl=%p; ntohs=%p; connect=%p; inet_addr=%p; inet_ntoa=%p; socket=%p; setsockopt=%p; getsockopt=%p; getsockname=%p; select=%p; recv=%p; p_send=%p; shutdown=%p; bind=%p; listen=%p; accept=%p;)",
            netx_so.p_WSASocket,
            netx_so.p_WSAGetLastError,
            netx_so.p_WSAStartup,
            netx_so.p_WSACleanup,
            netx_so.p_WSAFDIsSet,
            netx_so.p_WSARecv,
            netx_so.p_WSASend,

            netx_so.p_WSAStringToAddress,
            netx_so.p_WSAAddressToString,

            netx_so.p_closesocket,
            netx_so.p_gethostname,
            netx_so.p_gethostbyname,
            netx_so.p_getservbyname,
            netx_so.p_gethostbyaddr,

            netx_so.p_getaddrinfo,
            netx_so.p_freeaddrinfo,
            netx_so.p_getnameinfo,
            netx_so.p_getpeername,

            netx_so.p_htons,
            netx_so.p_htonl,
            netx_so.p_ntohl,
            netx_so.p_ntohs,
            netx_so.p_connect,
            netx_so.p_inet_addr,
            netx_so.p_inet_ntoa,
            netx_so.p_socket,
            netx_so.p_setsockopt,
            netx_so.p_getsockopt,
            netx_so.p_getsockname,
            netx_so.p_select,
            netx_so.p_recv,
            netx_so.p_send,
            netx_so.p_shutdown,
            netx_so.p_bind,
            netx_so.p_listen,
            netx_so.p_accept
            );
      mg_dso_unload((DBXPLIB) netx_so.plibrary);
   }
   else {
      netx_so.sock = 1;
   }

   if (netx_so.sock)
      result = 0;
   else
      result = -1;

   netx_so.load_attempted = 1;

   if (netx_so.p_getaddrinfo == NULL ||  netx_so.p_freeaddrinfo == NULL ||  netx_so.p_getnameinfo == NULL)
      netx_so.ipv6 = 0;

netx_load_winsock_no_so:

   if (result == 0) {

      if (netx_so.winsock == 2)
         netx_so.version_requested = MAKEWORD(2, 2);
      else
         netx_so.version_requested = MAKEWORD(1, 1);

      netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));

      if (netx_so.wsastartup != 0 && netx_so.winsock == 2) {
         netx_so.version_requested = MAKEWORD(2, 0);
         netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));
         if (netx_so.wsastartup != 0) {
            netx_so.winsock = 1;
            netx_so.version_requested = MAKEWORD(1, 1);
            netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));
         }
      }
      if (netx_so.wsastartup == 0) {
         if ((netx_so.winsock == 2 && LOBYTE(netx_so.wsadata.wVersion) != 2)
               || (netx_so.winsock == 1 && (LOBYTE(netx_so.wsadata.wVersion) != 1 || HIBYTE(netx_so.wsadata.wVersion) != 1))) {
  
            sprintf(pcon->error, "Initialization Error: Wrong version of Winsock library (%s) (%d.%d)", netx_so.libnam, LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            NETX_WSACLEANUP();
            netx_so.wsastartup = -1;
         }
         else {
            if (strlen(netx_so.libnam))
               sprintf(pcon->info, "Initialization: Windows Sockets library loaded (%s) Version: %d.%d", netx_so.libnam, LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            else
               sprintf(pcon->info, "Initialization: Windows Sockets library Version: %d.%d", LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            netx_so.winsock_ready = 1;
         }
      }
      else {
         strcpy(pcon->error, "Initialization Error: Unusable Winsock library");
      }
   }

   return result;

#else

   return 1;

#endif /* #if defined(_WIN32) */

}


int netx_tcp_connect(DBXCON *pcon, int context)
{
   short physical_ip, ipv6, connected, getaddrinfo_ok;
   int n, errorno;
   unsigned long inetaddr;
   DWORD spin_count;
   char ansi_ip_address[64];
   struct sockaddr_in srv_addr, cli_addr;
   struct hostent *hp;
   struct in_addr **pptr;

   pcon->connected = 0;
   pcon->error_no = 0;
   connected = 0;
   getaddrinfo_ok = 0;
   spin_count = 0;

   ipv6 = 1;
#if !defined(NETX_IPV6)
   ipv6 = 0;
#endif

   strcpy(ansi_ip_address, (char *) pcon->ip_address);

#if defined(_WIN32)

   if (!netx_so.load_attempted) {
      n = netx_load_winsock(pcon, 0);
      if (n != 0) {
         return CACHE_NOCON;
      }
   }
   if (!netx_so.winsock_ready) {
      strcpy(pcon->error, (char *) "DLL Load Error: Unusable Winsock Library");
      return CACHE_NOCON;
   }

   n = netx_so.wsastartup;
   if (n != 0) {
      strcpy(pcon->error, (char *) "DLL Load Error: Unusable Winsock Library");
      return n;
   }

#endif /* #if defined(_WIN32) */

#if defined(NETX_IPV6)

   if (ipv6) {
      short mode;
      struct addrinfo hints, *res;
      struct addrinfo *ai;
      char port_str[32];

      res = NULL;
      sprintf(port_str, "%d", pcon->port);
      connected = 0;
      pcon->error_no = 0;

      for (mode = 0; mode < 3; mode ++) {

         if (res) {
            NETX_FREEADDRINFO(res);
            res = NULL;
         }

         memset(&hints, 0, sizeof hints);
         hints.ai_family = AF_UNSPEC;     /* Use IPv4 or IPv6 */
         hints.ai_socktype = SOCK_STREAM;
         /* hints.ai_flags = AI_PASSIVE; */
         if (mode == 0)
            hints.ai_flags = AI_NUMERICHOST | AI_CANONNAME;
         else if (mode == 1)
            hints.ai_flags = AI_CANONNAME;
         else if (mode == 2) {
            /* Apparently an error can occur with AF_UNSPEC (See RJW1564) */
            /* This iteration will return IPV6 addresses if any */
            hints.ai_flags = AI_CANONNAME;
            hints.ai_family = AF_INET6;
         }
         else
            break;

         n = NETX_GETADDRINFO(ansi_ip_address, port_str, &hints, &res);

         if (n != 0) {
            continue;
         }

         getaddrinfo_ok = 1;
         spin_count = 0;
         for (ai = res; ai != NULL; ai = ai->ai_next) {

            spin_count ++;

	         if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) {
               continue;
            }

	         /* Open a socket with the correct address family for this address. */
	         pcon->cli_socket = NETX_SOCKET(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

            /* NETX_BIND(pcon->cli_socket, ai->ai_addr, (int) (ai->ai_addrlen)); */
            /* NETX_CONNECT(pcon->cli_socket, ai->ai_addr, (int) (ai->ai_addrlen)); */

            if (netx_so.nagle_algorithm == 0) {

               int flag = 1;
               int result;

               result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));

               if (result < 0) {
                  strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");
               }

            }

            pcon->error_no = 0;
            n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) ai->ai_addr, (socklen_netx) (ai->ai_addrlen), pcon->timeout);
            if (n == -2) {
               pcon->error_no = n;
               n = -737;
               continue;
            }
            if (SOCK_ERROR(n)) {
               errorno = (int) netx_get_last_error(0);
               pcon->error_no = errorno;
               netx_tcp_disconnect(pcon, 0);
               continue;
            }
            else {
               connected = 1;
               break;
            }
         }
         if (connected)
            break;
      }

      if (pcon->error_no) {
         char message[256];
         netx_get_error_message(pcon->error_no, message, 250, 0);
         sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->ip_address, pcon->port, pcon->error_no, message);
         n = -5;
      }

      if (res) {
         NETX_FREEADDRINFO(res);
         res = NULL;
      }
   }
#endif

   if (ipv6) {
      if (connected) {
         pcon->connected = 1;
         return 0;
      }
      else {
         if (getaddrinfo_ok) {
            netx_tcp_disconnect(pcon, 0);
            return -5;
         }
         else {
            char message[256];

            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Cannot identify Server: Error Code: %d (%s)", errorno, message);
            netx_tcp_disconnect(pcon, 0);
            return -5;
         }
      }
   }

   ipv6 = 0;
   inetaddr = NETX_INET_ADDR(ansi_ip_address);

   physical_ip = 0;
   if (isdigit(ansi_ip_address[0])) {
      char *p;

      if ((p = strstr(ansi_ip_address, "."))) {
         if (isdigit(*(++ p))) {
            if ((p = strstr(p, "."))) {
               if (isdigit(*(++ p))) {
                  if ((p = strstr(p, "."))) {
                     if (isdigit(*(++ p))) {
                        physical_ip = 1;
                     }
                  }
               }
            }
         }
      }
   }

   if (inetaddr == INADDR_NONE || !physical_ip) {

      hp = NETX_GETHOSTBYNAME((const char *) ansi_ip_address);

      if (hp == NULL) {
         n = -2;
         strcpy(pcon->error, "Connection Error: Invalid Host");
         return n;
      }

      pptr = (struct in_addr **) hp->h_addr_list;
      connected = 0;

      spin_count = 0;

      for (; *pptr != NULL; pptr ++) {

         spin_count ++;

         pcon->cli_socket = NETX_SOCKET(AF_INET, SOCK_STREAM, 0);

         if (INVALID_SOCK(pcon->cli_socket)) {
            char message[256];

            n = -2;
            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Invalid Socket: Context=1: Error Code: %d (%s)", errorno, message);
            break;
         }

#if !defined(_WIN32)
         BZERO((char *) &cli_addr, sizeof(cli_addr));
         BZERO((char *) &srv_addr, sizeof(srv_addr));
#endif

         cli_addr.sin_family = AF_INET;
         srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->port);

         cli_addr.sin_addr.s_addr = NETX_HTONL(INADDR_ANY);
         cli_addr.sin_port = NETX_HTONS(0);

         n = NETX_BIND(pcon->cli_socket, (xLPSOCKADDR) &cli_addr, sizeof(cli_addr));

         if (SOCK_ERROR(n)) {
            char message[256];

            n = -3;
            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Cannot bind to Socket: Error Code: %d (%s)", errorno, message);

            break;
         }

         if (netx_so.nagle_algorithm == 0) {

            int flag = 1;
            int result;

            result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));
            if (result < 0) {
               strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");
            }
         }

         srv_addr.sin_family = AF_INET;
         srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->port);

         NETX_MEMCPY(&srv_addr.sin_addr, *pptr, sizeof(struct in_addr));

         n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) &srv_addr, sizeof(srv_addr), pcon->timeout);

         if (n == -2) {
            pcon->error_no = n;
            n = -737;

            continue;
         }

         if (SOCK_ERROR(n)) {
            char message[256];

            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);

            pcon->error_no = errorno;
            sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->ip_address, pcon->port, errorno, message);
            n = -5;
            netx_tcp_disconnect(pcon, 0);
            continue;
         }
         else {
            connected = 1;
            break;
         }
      }
      if (!connected) {

         netx_tcp_disconnect(pcon, 0);

         strcpy(pcon->error, "Connection Error: Failed to find the Server via a DNS Lookup");

         return n;
      }
   }
   else {

      pcon->cli_socket = NETX_SOCKET(AF_INET, SOCK_STREAM, 0);

      if (INVALID_SOCK(pcon->cli_socket)) {
         char message[256];

         n = -2;
         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);
         sprintf(pcon->error, "Connection Error: Invalid Socket: Context=2: Error Code: %d (%s)", errorno, message);

         return n;
      }

#if !defined(_WIN32)
      BZERO((char *) &cli_addr, sizeof(cli_addr));
      BZERO((char *) &srv_addr, sizeof(srv_addr));
#endif

      cli_addr.sin_family = AF_INET;
      cli_addr.sin_addr.s_addr = NETX_HTONL(INADDR_ANY);
      cli_addr.sin_port = NETX_HTONS(0);

      n = NETX_BIND(pcon->cli_socket, (xLPSOCKADDR) &cli_addr, sizeof(cli_addr));

      if (SOCK_ERROR(n)) {
         char message[256];

         n = -3;

         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);

         sprintf(pcon->error, "Connection Error: Cannot bind to Socket: Error Code: %d (%s)", errorno, message);

         netx_tcp_disconnect(pcon, 0);

         return n;
      }

      if (netx_so.nagle_algorithm == 0) {

         int flag = 1;
         int result;

         result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));

         if (result < 0) {
            strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");

         }
      }

      srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->port);
      srv_addr.sin_family = AF_INET;
      srv_addr.sin_addr.s_addr = NETX_INET_ADDR(ansi_ip_address);

      n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) &srv_addr, sizeof(srv_addr), pcon->timeout);
      if (n == -2) {
         pcon->error_no = n;
         n = -737;

         netx_tcp_disconnect(pcon, 0);

         return n;
      }

      if (SOCK_ERROR(n)) {
         char message[256];

         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);
         pcon->error_no = errorno;
         sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->ip_address, pcon->port, errorno, message);
         n = -5;
         netx_tcp_disconnect(pcon, 0);
         return n;
      }
   }

   pcon->connected = 1;

   return 0;
}


int netx_tcp_handshake(DBXCON *pcon, int context)
{
   int len;
   char buffer[256];

   sprintf(buffer, "dbx1~%s\n", pcon->nspace);
   len = (int) strlen(buffer);

   netx_tcp_write(pcon, (unsigned char *) buffer, len);
   len = netx_tcp_read(pcon, (unsigned char *) buffer, 5, pcon->timeout, 1); /* v1.2.8 */

   len = mg_get_size((unsigned char *) buffer);

   netx_tcp_read(pcon, (unsigned char *) buffer, len, pcon->timeout, 1); /* v1.2.8 */
   if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
      isc_parse_zv(buffer, pcon->p_zv);
   }
   else {
     ydb_parse_zv(buffer, pcon->p_zv);
   }

   return 0;
}

/*

 i buf="xDBC" g main^%mgsqln
 i buf?1u.e1"HTTP/"1n1"."1n1c s buf=buf_$c(10) g main^%mgsqlw
 i $e(buf,1,4)="dbx1" g dbx^%zmgsis

dbx ; new wire protocol for access to M
 s res=$zv
 s res=$$esize256($l(res))_"0"_res
 w res,*-3
dbx1 ; test
 r head#4
 s len=$$esize256(head)
 s ^%cm($i(^%cm))=head
 r data#len
 s ^%cm($i(^%cm))=data
 s res="1"
 s res=$$esize256($l(res))_"0"_res
 w res,*-3
 q
 ;

*/

int netx_tcp_command(DBXMETH *pmeth, int context)
{
   int len;
   DBXCON *pcon = pmeth->pcon;

   if (pcon->p_srv) {
      return mg_db_command(pmeth, context);
   }

   netx_tcp_write(pcon, (unsigned char *) pmeth->input_str.buf_addr, pmeth->input_str.len_used);
   netx_tcp_read(pcon, (unsigned char *) pmeth->output_val.svalue.buf_addr, 5, pcon->timeout, 1); /* v1.2.8 */
   pmeth->output_val.svalue.buf_addr[5] = '\0';

   len = mg_get_size((unsigned char *) pmeth->output_val.svalue.buf_addr);

   if (len > 0) {
      netx_tcp_read(pcon, (unsigned char *) pmeth->output_val.svalue.buf_addr + 5, len, pcon->timeout, 1); /* v1.2.8 */
   }

   pmeth->output_val.svalue.len_used = len;

   return 0;
}


int netx_tcp_connect_ex(DBXCON *pcon, xLPSOCKADDR p_srv_addr, socklen_netx srv_addr_len, int timeout)
{
#if defined(_WIN32)
   int n;
#else
   int flags, n, error;
   socklen_netx len;
   fd_set rset, wset;
   struct timeval tval;
#endif

#if defined(SOLARIS) && BIT64PLAT
   timeout = 0;
#endif

   /* It seems that BIT64PLAT is set to 0 for 64-bit Solaris:  So, to be safe .... */

#if defined(SOLARIS)
   timeout = 0;
#endif

   if (timeout != 0) {

#if defined(_WIN32)

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      return n;

#else
      flags = fcntl(pcon->cli_socket, F_GETFL, 0);
      n = fcntl(pcon->cli_socket, F_SETFL, flags | O_NONBLOCK);

      error = 0;

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      if (n < 0) {

         if (errno != EINPROGRESS) {

#if defined(SOLARIS)

            if (errno != 2 && errno != 146) {
               sprintf((char *) pcon->error, "Diagnostic: Solaris: Initial Connection Error errno=%d; EINPROGRESS=%d", errno, EINPROGRESS);
               return -1;
            }
#else
            return -1;
#endif

         }
      }

      if (n != 0) {

         FD_ZERO(&rset);
         FD_SET(pcon->cli_socket, &rset);

         wset = rset;
         tval.tv_sec = timeout;
         tval.tv_usec = timeout;

         n = NETX_SELECT((int) (pcon->cli_socket + 1), &rset, &wset, NULL, &tval);

         if (n == 0) {
            close(pcon->cli_socket);
            errno = ETIMEDOUT;

            return (-2);
         }
         if (NETX_FD_ISSET(pcon->cli_socket, &rset) || NETX_FD_ISSET(pcon->cli_socket, &wset)) {

            len = sizeof(error);
            if (NETX_GETSOCKOPT(pcon->cli_socket, SOL_SOCKET, SO_ERROR, (void *) &error, &len) < 0) {

               sprintf((char *) pcon->error, "Diagnostic: Solaris: Pending Error %d", errno);

               return (-1);   /* Solaris pending error */
            }
         }
         else {
            ;
         }
      }

      fcntl(pcon->cli_socket, F_SETFL, flags);      /* Restore file status flags */

      if (error) {

         close(pcon->cli_socket);
         errno = error;
         return (-1);
      }

      return 1;

#endif

   }
   else {

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      return n;
   }

}


int netx_tcp_disconnect(DBXCON *pcon, int context)
{
   int n;

   if (!pcon) {
      return 0;
   }

   if (pcon->cli_socket != (SOCKET) 0) {

#if defined(_WIN32)
      n = NETX_CLOSESOCKET(pcon->cli_socket);
/*
      NETX_WSACLEANUP();
*/

#else
      n = close(pcon->cli_socket);
#endif

   }

   pcon->connected = 0;

   return 0;

}


int netx_tcp_write(DBXCON *pcon, unsigned char *data, int size)
{
   int n = 0, errorno = 0, char_sent = 0;
   int total;
   char errormessage[512];

   *errormessage = '\0';

   if (pcon->connected == 0) {
      strcpy(pcon->error, "TCP Write Error: Socket is Closed");
      return -1;
   }

   total = 0;
   for (;;) {
      n = NETX_SEND(pcon->cli_socket, (xLPSENDBUF) (data + total), size - total, 0);

      if (SOCK_ERROR(n)) {

         errorno = (int) netx_get_last_error(0);

         if (NOT_BLOCKING(errorno) && errorno != 0) {

            char message[256];

            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "TCP Write Error: Cannot Write Data: Error Code: %d (%s)", errorno, message);

            char_sent = -1;
            break;
         }
      }
      else {

         total += n;
         if (total == size) {
            break;
         }
      }
   }

   if (char_sent < 0)
      return char_sent;
   else
      return size;

}



int netx_tcp_read(DBXCON *pcon, unsigned char *data, int size, int timeout, int context)
{
   int result, n;
   int len;
   fd_set rset, eset;
   struct timeval tval;
   unsigned long spin_count;


   if (!pcon) {
      return NETX_READ_ERROR;
   }

   result = 0;

   tval.tv_sec = timeout;
   tval.tv_usec = 0;

   spin_count = 0;
   len = 0;
   for (;;) {
      spin_count ++;

      FD_ZERO(&rset);
      FD_ZERO(&eset);
      FD_SET(pcon->cli_socket, &rset);
      FD_SET(pcon->cli_socket, &eset);

      n = NETX_SELECT((int) (pcon->cli_socket + 1), &rset, NULL, &eset, &tval);

      if (n == 0) {
         sprintf(pcon->error, "TCP Read Error: Server did not respond within the timeout period (%d seconds)", timeout);
         result = NETX_READ_TIMEOUT;
         break;
      }

      if (n < 0 || !NETX_FD_ISSET(pcon->cli_socket, &rset)) {
          strcpy(pcon->error, "TCP Read Error: Server closed the connection without having returned any data");
          result = NETX_READ_ERROR;
         break;
      }

      n = NETX_RECV(pcon->cli_socket, (char *) data + len, size - len, 0);

      if (n < 1) {
         if (n == 0) {
            result = NETX_READ_EOF;
            pcon->connected = 0;
            pcon->eof = 1;
         }
         else {
            result = NETX_READ_ERROR;
            len = 0;
            pcon->connected = 0;
         }
         break;
      }

      len += n;
      if (context) { /* Must read length requested */
         if (len == size) {
            break;
         }
      }
      else {
         break;
      }
   }

   if (len) {
      result = len;
   }
   return result;
}



int netx_get_last_error(int context)
{
   int error_code;

#if defined(_WIN32)
   if (context)
      error_code = (int) GetLastError();
   else
      error_code = (int) NETX_WSAGETLASTERROR();
#else
   error_code = (int) errno;
#endif

   return error_code;
}


int netx_get_error_message(int error_code, char *message, int size, int context)
{
   *message = '\0';

#if defined(_WIN32)

   if (context == 0) {
      short ok;
      int len;
      char *p;
      LPVOID lpMsgBuf;

      ok = 0;
      lpMsgBuf = NULL;
      len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL,
                           error_code,
                           /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                           MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                           (LPTSTR) &lpMsgBuf,
                           0,
                           NULL 
                           );
      if (len && lpMsgBuf) {
         strncpy(message, (const char *) lpMsgBuf, size);
         p = strstr(message, "\r\n");
         if (p)
            *p = '\0';
         ok = 1;
      }
      if (lpMsgBuf)
         LocalFree(lpMsgBuf);

      if (!ok) {
         switch (error_code) {
            case EXCEPTION_ACCESS_VIOLATION:
               strncpy(message, "The thread attempted to read from or write to a virtual address for which it does not have the appropriate access.", size);
               break;
            case EXCEPTION_BREAKPOINT:
               strncpy(message, "A breakpoint was encountered.", size); 
               break;
            case EXCEPTION_DATATYPE_MISALIGNMENT:
               strncpy(message, "The thread attempted to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries, 32-bit values on 4-byte boundaries, and so on.", size);
               break;
            case EXCEPTION_SINGLE_STEP:
               strncpy(message, "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.", size);
               break;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
               strncpy(message, "The thread attempted to access an array element that is out of bounds, and the underlying hardware supports bounds checking.", size);
               break;
            case EXCEPTION_FLT_DENORMAL_OPERAND:
               strncpy(message, "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.", size);
               break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
               strncpy(message, "The thread attempted to divide a floating-point value by a floating-point divisor of zero.", size);
               break;
            case EXCEPTION_FLT_INEXACT_RESULT:
               strncpy(message, "The result of a floating-point operation cannot be represented exactly as a decimal fraction.", size);
               break;
            case EXCEPTION_FLT_INVALID_OPERATION:
               strncpy(message, "This exception represents any floating-point exception not included in this list.", size);
               break;
            case EXCEPTION_FLT_OVERFLOW:
               strncpy(message, "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.", size);
               break;
            case EXCEPTION_FLT_STACK_CHECK:
               strncpy(message, "The stack overflowed or underflowed as the result of a floating-point operation.", size);
               break;
            case EXCEPTION_FLT_UNDERFLOW:
               strncpy(message, "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.", size);
               break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
               strncpy(message, "The thread attempted to divide an integer value by an integer divisor of zero.", size);
               break;
            case EXCEPTION_INT_OVERFLOW:
               strncpy(message, "The result of an integer operation caused a carry out of the most significant bit of the result.", size);
               break;
            case EXCEPTION_PRIV_INSTRUCTION:
               strncpy(message, "The thread attempted to execute an instruction whose operation is not allowed in the current machine mode.", size);
               break;
            case EXCEPTION_NONCONTINUABLE_EXCEPTION:
               strncpy(message, "The thread attempted to continue execution after a noncontinuable exception occurred.", size);
               break;
            default:
               strncpy(message, "Unrecognised system or hardware error.", size);
            break;
         }
      }
   }

#else

   if (context == 0) {
#if defined(_GNU_SOURCE)
      char *p;
#endif
      strcpy(message, "");
#if defined(LINUX) || defined(AIX) || defined(OSF1) || defined(MACOSX)
#if defined(_GNU_SOURCE)
      p = strerror_r(error_code, message, (size_t) size);
      if (p && p != message) {
         strncpy(message, p, size - 1);
         message[size - 1] = '\0';
      }
#else
      strerror_r(error_code, message, (size_t) size);
#endif
      size = (int) strlen(message);
#else
      netx_get_std_error_message(error_code, message, size, context);
      size = (int) strlen(message);
#endif
   }

#endif

   message[size - 1] = '\0';

   return (int) strlen(message);
}


int netx_get_std_error_message(int error_code, char *message, int size, int context)
{

   strcpy(message, "");

#if !defined(_WIN32)
   switch (error_code) {
      case E2BIG:
         strncpy(message, "Argument list too long.", size);
         break;
      case EACCES:
         strncpy(message, "Permission denied.", size);
         break;
      case EADDRINUSE:
         strncpy(message, "Address in use.", size);
         break;
      case EADDRNOTAVAIL:
         strncpy(message, "Address not available.", size);
         break;
      case EAFNOSUPPORT:
         strncpy(message, "Address family not supported.", size);
         break;
      case EAGAIN:
         strncpy(message, "Resource unavailable, try again.", size);
         break;
      case EALREADY:
         strncpy(message, "Connection already in progress.", size);
         break;
      case EBADF:
         strncpy(message, "Bad file descriptor.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case EBADMSG:
         strncpy(message, "Bad message.", size);
         break;
#endif
      case EBUSY:
         strncpy(message, "Device or resource busy.", size);
         break;
      case ECANCELED:
         strncpy(message, "Operation canceled.", size);
         break;
      case ECHILD:
         strncpy(message, "No child processes.", size);
         break;
      case ECONNABORTED:
         strncpy(message, "Connection aborted.", size);
         break;
      case ECONNREFUSED:
         strncpy(message, "Connection refused.", size);
         break;
      case ECONNRESET:
         strncpy(message, "Connection reset.", size);
         break;
      case EDEADLK:
         strncpy(message, "Resource deadlock would occur.", size);
         break;
      case EDESTADDRREQ:
         strncpy(message, "Destination address required.", size);
         break;
      case EDOM:
         strncpy(message, "Mathematics argument out of domain of function.", size);
         break;
      case EDQUOT:
         strncpy(message, "Reserved.", size);
         break;
      case EEXIST:
         strncpy(message, "File exists.", size);
         break;
      case EFAULT:
         strncpy(message, "Bad address.", size);
         break;
      case EFBIG:
         strncpy(message, "File too large.", size);
         break;
      case EHOSTUNREACH:
         strncpy(message, "Host is unreachable.", size);
         break;
      case EIDRM:
         strncpy(message, "Identifier removed.", size);
         break;
      case EILSEQ:
         strncpy(message, "Illegal byte sequence.", size);
         break;
      case EINPROGRESS:
         strncpy(message, "Operation in progress.", size);
         break;
      case EINTR:
         strncpy(message, "Interrupted function.", size);
         break;
      case EINVAL:
         strncpy(message, "Invalid argument.", size);
         break;
      case EIO:
         strncpy(message, "I/O error.", size);
         break;
      case EISCONN:
         strncpy(message, "Socket is connected.", size);
         break;
      case EISDIR:
         strncpy(message, "Is a directory.", size);
         break;
      case ELOOP:
         strncpy(message, "Too many levels of symbolic links.", size);
         break;
      case EMFILE:
         strncpy(message, "Too many open files.", size);
         break;
      case EMLINK:
         strncpy(message, "Too many links.", size);
         break;
      case EMSGSIZE:
         strncpy(message, "Message too large.", size);
         break;
#if !defined(MACOSX) && !defined(OSF1) && !defined(FREEBSD)
      case EMULTIHOP:
         strncpy(message, "Reserved.", size);
         break;
#endif
      case ENAMETOOLONG:
         strncpy(message, "Filename too long.", size);
         break;
      case ENETDOWN:
         strncpy(message, "Network is down.", size);
         break;
      case ENETRESET:
         strncpy(message, "Connection aborted by network.", size);
         break;
      case ENETUNREACH:
         strncpy(message, "Network unreachable.", size);
         break;
      case ENFILE:
         strncpy(message, "Too many files open in system.", size);
         break;
      case ENOBUFS:
         strncpy(message, "No buffer space available.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENODATA:
         strncpy(message, "[XSR] [Option Start] No message is available on the STREAM head read queue. [Option End]", size);
         break;
#endif
      case ENODEV:
         strncpy(message, "No such device.", size);
         break;
      case ENOENT:
         strncpy(message, "No such file or directory.", size);
         break;
      case ENOEXEC:
         strncpy(message, "Executable file format error.", size);
         break;
      case ENOLCK:
         strncpy(message, "No locks available.", size);
         break;
#if !defined(MACOSX) && !defined(OSF1) && !defined(FREEBSD)
      case ENOLINK:
         strncpy(message, "Reserved.", size);
         break;
#endif
      case ENOMEM:
         strncpy(message, "Not enough space.", size);
         break;
      case ENOMSG:
         strncpy(message, "No message of the desired type.", size);
         break;
      case ENOPROTOOPT:
         strncpy(message, "Protocol not available.", size);
         break;
      case ENOSPC:
         strncpy(message, "No space left on device.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENOSR:
         strncpy(message, "[XSR] [Option Start] No STREAM resources. [Option End]", size);
         break;
#endif
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENOSTR:
         strncpy(message, "[XSR] [Option Start] Not a STREAM. [Option End]", size);
         break;
#endif
      case ENOSYS:
         strncpy(message, "Function not supported.", size);
         break;
      case ENOTCONN:
         strncpy(message, "The socket is not connected.", size);
         break;
      case ENOTDIR:
         strncpy(message, "Not a directory.", size);
         break;
#if !defined(AIX) && !defined(AIX5)
      case ENOTEMPTY:
         strncpy(message, "Directory not empty.", size);
         break;
#endif
      case ENOTSOCK:
         strncpy(message, "Not a socket.", size);
         break;
      case ENOTSUP:
         strncpy(message, "Not supported.", size);
         break;
      case ENOTTY:
         strncpy(message, "Inappropriate I/O control operation.", size);
         break;
      case ENXIO:
         strncpy(message, "No such device or address.", size);
         break;
#if !defined(LINUX) && !defined(MACOSX) && !defined(FREEBSD)
      case EOPNOTSUPP:
         strncpy(message, "Operation not supported on socket.", size);
         break;
#endif
#if !defined(OSF1)
      case EOVERFLOW:
         strncpy(message, "Value too large to be stored in data type.", size);
         break;
#endif
      case EPERM:
         strncpy(message, "Operation not permitted.", size);
         break;
      case EPIPE:
         strncpy(message, "Broken pipe.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case EPROTO:
         strncpy(message, "Protocol error.", size);
         break;
#endif
      case EPROTONOSUPPORT:
         strncpy(message, "Protocol not supported.", size);
         break;
      case EPROTOTYPE:
         strncpy(message, "Protocol wrong type for socket.", size);
         break;
      case ERANGE:
         strncpy(message, "Result too large.", size);
         break;
      case EROFS:
         strncpy(message, "Read-only file system.", size);
         break;
      case ESPIPE:
         strncpy(message, "Invalid seek.", size);
         break;
      case ESRCH:
         strncpy(message, "No such process.", size);
         break;
      case ESTALE:
         strncpy(message, "Reserved.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ETIME:
         strncpy(message, "[XSR] [Option Start] Stream ioctl() timeout. [Option End]", size);
         break;
#endif
      case ETIMEDOUT:
         strncpy(message, "Connection timed out.", size);
         break;
      case ETXTBSY:
         strncpy(message, "Text file busy.", size);
         break;
#if !defined(LINUX) && !defined(AIX) && !defined(AIX5) && !defined(MACOSX) && !defined(OSF1) && !defined(SOLARIS) && !defined(FREEBSD)
      case EWOULDBLOCK:
         strncpy(message, "Operation would block.", size);
         break;
#endif
      case EXDEV:
         strncpy(message, "Cross-device link.", size);
         break;
      default:
         strcpy(message, "");
      break;
   }
#endif

   return (int) strlen(message);
}



/* Old MGWSI protocol */

int mg_db_command(DBXMETH *pmeth, int context)
{
   MGBUF mgbuf, *p_buf;
   int rc, n, len, dsort, dtype, chndle;
   int ifc[4];
   char *p;
   MGSRV *p_srv;
   DBXCON *pcon = pmeth->pcon;

   p_srv = (MGSRV *) pcon->p_srv;
   chndle = pcon->chndle;

   if (!strcmp(pmeth->command, "gns")) {
      dtype = 1;
      dsort = 1;
      strcpy(pmeth->output_val.svalue.buf_addr + 5, p_srv->uci);
      len = (int) strlen(p_srv->uci);
      if (len > 0) {
         mg_set_size((unsigned char *) pmeth->output_val.svalue.buf_addr, len);
      }
      pmeth->output_val.svalue.buf_addr[4] = (dsort * 20) + dtype;
      return 0;
   }
   else if (!strcmp(pmeth->command, "sns")) {
      dtype = 1;
      dsort = 1;
      len = 0;
      if (pmeth->argc > 0 && pmeth->args[0].svalue.len_used > 0 && pmeth->args[0].svalue.len_used < 120) {
         strncpy(p_srv->uci, (char *) pmeth->args[0].svalue.buf_addr, pmeth->args[0].svalue.len_used);
         p_srv->uci[pmeth->args[0].svalue.len_used] = '\0';
         len = pmeth->args[0].svalue.len_used;
      }
      strcpy(pmeth->output_val.svalue.buf_addr + 5, p_srv->uci);
      len = (int) strlen(p_srv->uci);
      if (len > 0) {
         mg_set_size((unsigned char *) pmeth->output_val.svalue.buf_addr, len);
      }
      pmeth->output_val.svalue.buf_addr[4] = (dsort * 20) + dtype;
      return 0;
   }
   else if (!strcmp(pmeth->command, "*")) {
      dtype = 1;
      dsort = 11;
      memset((void *) pmeth->output_val.svalue.buf_addr, 0, 5);
      strcpy(pmeth->output_val.svalue.buf_addr + 5, "Not Implemented");
      len = (int) strlen(pmeth->output_val.svalue.buf_addr + 5);
      mg_set_size((unsigned char *) pmeth->output_val.svalue.buf_addr, len);
      pmeth->output_val.svalue.buf_addr[4] = (dsort * 20) + dtype;
      return 0;
   }

/*
   printf("\r\n mg_db_command p_srv=%p; chndle=%d; server=%s; ip_address=%s; port=%d; offset=%d; len_alloc=%d; len_used=%d; ...\r\n", p_srv, chndle, p_srv->server, p_srv->ip_address, p_srv->port, pmeth->offset, pmeth->input_str.len_alloc, pmeth->input_str.len_used);
*/


   p_buf = &mgbuf;
   mg_buf_init(p_buf, MG_BUFSIZE, MG_BUFSIZE);

   mg_request_header(p_srv, p_buf, pmeth->command, p_srv->product);

   ifc[0] = 0;
   ifc[1] = MG_TX_DATA;

   rc = CACHE_SUCCESS;
   for (;;) {
      len = (int) mg_get_block_size(&(pmeth->input_str), pmeth->offset, &dsort, &dtype);
      pmeth->offset += 5;
/*
      printf("\r\nn=%d; len=%d; offset=%d; sort=%d; type=%d; str=%s;", pmeth->argc, len, pmeth->offset, dsort, dtype, pmeth->input_str.buf_addr + pmeth->offset);
*/
      if (dsort == DBX_DSORT_EOD) {
         break;
      }
      p = (char *) (pmeth->input_str.buf_addr + pmeth->offset);

      mg_request_add(p_srv, chndle, p_buf, (unsigned char *) p, len, (short) ifc[0], (short) ifc[1]);
      pmeth->offset += len;
   }

   mg_db_send(p_srv, chndle, p_buf, 1);

   mg_db_receive(p_srv, chndle, p_buf, MG_BUFSIZE, 0);

   if ((n = mg_get_error(p_srv, (char *) p_buf->p_buffer))) {
      dtype = 1;
      dsort = 11;
      memset((void *) pmeth->output_val.svalue.buf_addr, 0, 5);
      len = p_buf->data_size - MG_RECV_HEAD;
      mg_set_size((unsigned char *) pmeth->output_val.svalue.buf_addr, len);
      memcpy(pmeth->output_val.svalue.buf_addr + 5, p_buf->p_buffer + MG_RECV_HEAD, len);
      pmeth->output_val.svalue.buf_addr[4] = (dsort * 20) + dtype;
      mg_buf_free(p_buf);
      return 0;
   }

   memset((void *) pmeth->output_val.svalue.buf_addr, 0, 5);
   len = p_buf->data_size - MG_RECV_HEAD;

   if (len > 0) {
      mg_set_size((unsigned char *) pmeth->output_val.svalue.buf_addr, len);
      memcpy(pmeth->output_val.svalue.buf_addr + 5, p_buf->p_buffer + MG_RECV_HEAD, len);
   }

   mg_buf_free(p_buf);

   return 0;
}


int mg_db_connect(MGSRV *p_srv, int *p_chndle, short context)
{
   int rc, n, free;
   DBXCON *pcon;
   DBXMETH *pmeth;

   if (p_srv->mode == 2) {
      return 1;
   }

   free = -1;
   *p_chndle = -1;
   for (n = 0; n < MG_MAXCON; n ++) {
      if (connection[n]) {
         if (!connection[n]->in_use) {
            *p_chndle = n;
            connection[*p_chndle]->in_use = 1;
            connection[*p_chndle]->eod = 0;
            break;
         }
      }
      else {
         if (free == -1) {
            free = n;
         }
      }
   }

   if (*p_chndle != -1) {
      p_srv->pcon[*p_chndle] = connection[*p_chndle];
      return 1;
   }

   if (free == -1) {
      return 0;
   }

   *p_chndle = free;

   pcon = (PDBXCON) mg_malloc(sizeof(DBXCON), 0);
   if (pcon == NULL) {
      return 0;
   }
   memset((void *) pcon, 0, sizeof(DBXCON));
   pmeth = (PDBXMETH) mg_malloc(sizeof(DBXMETH), 0);
   if (pmeth == NULL) {
      mg_free((void *) pcon, 0);
      return 0;
   }
   memset((void *) pmeth, 0, sizeof(DBXMETH));
   pcon->pmeth_base = (void *) pmeth;
   pmeth->pcon = pcon;

   connection[*p_chndle] = pcon;
   pcon->chndle = *p_chndle;
   p_srv->pcon[*p_chndle] = connection[*p_chndle];

   pcon->tlevel = 0;
   pcon->p_isc_so = NULL;
   pcon->p_ydb_so = NULL;
   pcon->p_srv = p_srv;

   pcon->p_log = &pcon->log;
   pcon->p_db_mutex = &pcon->db_mutex;
   pcon->p_zv = &pcon->zv;
   pcon->timeout = p_srv->timeout; /* v1.2.8 */

   mg_log_init(pcon->p_log);

   pcon->in_use = 1;
   pcon->keep_alive = 0;

   strcpy(pcon->ip_address, p_srv->ip_address);
   pcon->port = p_srv->port;

   pcon->eod = 0;
   strcpy(p_srv->error_mess, "");

   rc = netx_tcp_connect(pcon, 0);

   if (rc != CACHE_SUCCESS) {
      pcon->connected = 0;
      rc = CACHE_NOCON;
      mg_error_message(pmeth, rc);
      return 0;
   }

   return 1;

}


int mg_db_disconnect(MGSRV *p_srv, int chndle, short context)
{
   DBXCON *pcon;

   if (p_srv->mode == 2) {
      return 1;
   }

   if (!p_srv->pcon[chndle])
      return 0;

   if (p_srv->mode == 1) {
      p_srv->pcon[chndle]->in_use = 0;
      return 1;
   }

   if (context == 1 && p_srv->pcon[chndle]->keep_alive) {
      p_srv->pcon[chndle]->in_use = 0;
      return 1;
   }

   pcon = p_srv->pcon[chndle];

#if defined(_WIN32)
   NETX_CLOSESOCKET(pcon->cli_socket);
   NETX_WSACLEANUP();
#else
   close(pcon->cli_socket);
#endif

   mg_free((void *) p_srv->pcon[chndle], 0);
   p_srv->pcon[chndle] = NULL;

   return 1;
}

int mg_db_send(MGSRV *p_srv, int chndle, MGBUF *p_buf, int mode)
{
   int result, n, n1, len, total;
   char *request;
   unsigned char esize[8];
   DBXCON *pcon;

   result = 1;

   if (p_srv->p_log && p_srv->p_log->log_transmissions) {
      char buffer[64];

      sprintf(buffer, "Transmission: Send to Host (size=%lu)", p_buf->data_size);
      mg_log_buffer(p_srv->p_log, p_buf->p_buffer, p_buf->data_size, buffer, 0);
   }

   if (mode) {
      len = mg_encode_size(esize, p_buf->data_size - p_srv->header_len, MG_CHUNK_SIZE_BASE);
      strncpy((char *) (p_buf->p_buffer + (p_srv->header_len - 6) + (5 - len)), (char *) esize, len);
   }

   if (p_srv->mode == 2) {
      return 1;
   }

   pcon = p_srv->pcon[chndle];

   pcon->eod = 0;

   request = (char *) p_buf->p_buffer;
   len = p_buf->data_size;

   total = 0;

   n1= 0;
   for (;;) {
      n = NETX_SEND(pcon->cli_socket, request + total, len - total, 0);
      if (n < 0) {
         result = 0;
         break;
      }

      total += n;

      if (total == len)
         break;

      n1 ++;
      if (n1 > 100000)
         break;

   }

   return result;
}


int mg_db_receive(MGSRV *p_srv, int chndle, MGBUF *p_buf, int size, int mode)
{
   int result, n;
   unsigned long len, total, ssize;
   char s_buffer[16], stype[4];
   char *p;
   fd_set rset, eset;
   struct timeval tval;
   DBXCON *pcon;
   unsigned long spin_count;

   if (p_srv->mode == 2) {
      return mg_invoke_server_api(p_srv, chndle, p_buf, size, mode);
   }

   pcon = p_srv->pcon[chndle];

   p = NULL;
   result = 0;
   ssize = 0;
   s_buffer[0] = '\0';
   p_buf->p_buffer[0] = '\0';
   p_buf->data_size = 0;

   /* v1.2.8 */
   pcon->timeout = p_srv->timeout;
   tval.tv_sec = pcon->timeout;
   tval.tv_usec = 0;

   if (pcon->eod) {
      pcon->eod = 0;
      return 0;
   }
   pcon->eod = 0;

   len = 0;

   if (mode)
      total = size;
   else
      total = p_buf->size;

   spin_count = 0;
   for (;;) {
      spin_count ++;

      if (pcon->timeout) {
         FD_ZERO(&rset);
         FD_ZERO(&eset);
         FD_SET(pcon->cli_socket, &rset);
         FD_SET(pcon->cli_socket, &eset);

         n = NETX_SELECT((int) (pcon->cli_socket + 1), &rset, NULL, &eset, &tval);

         if (n == 0) {
            sprintf(pcon->error, "TCP Read Error: Server did not respond within the timeout period (%d seconds)", pcon->timeout);
            result = NETX_READ_TIMEOUT;
            pcon->eod = 1;
            break;
         }

         if (n < 0 || !NETX_FD_ISSET(pcon->cli_socket, &rset)) {
            strcpy(pcon->error, "TCP Read Error: Server closed the connection without having returned any data");
            result = NETX_READ_ERROR;
            pcon->eod = 1;
            break;
         }
      }

      n = NETX_RECV(pcon->cli_socket, p_buf->p_buffer + len, total - len, 0);

      if (n < 0) {
         result = len;
         pcon->eod = 1;
         break;
      }
      if (n < 1) {

         result = len;
         pcon->eod = 1;
         break;
      }

      len += n;
      p_buf->data_size += n;
      p_buf->p_buffer[len] = '\0';
      result = len;

      if (!ssize && p_buf->data_size >= MG_RECV_HEAD) {
         ssize = mg_decode_size(p_buf->p_buffer, 5, MG_CHUNK_SIZE_BASE);

         stype[0] = p_buf->p_buffer[5];
         stype[1] = p_buf->p_buffer[6];
         stype[2] = '\0';
         total = ssize + MG_RECV_HEAD;

         if (ssize && (ssize + MG_RECV_HEAD) > total) {
            if (!mg_buf_resize(p_buf, ssize + MG_RECV_HEAD + 32)) {
               p_srv->mem_error = 1;
               break;
            }
         }
      }
      if (!ssize || len >= total) {
         p_buf->p_buffer[len] = '\0';
         result = len;
         pcon->eod = 1;
         pcon->keep_alive = 1;

         break;
      }

   }

   if (p_srv->p_log && p_srv->p_log->log_transmissions) {
      char buffer[64];
      sprintf(buffer, "Transmission: Received from Host (size=%lu)", p_buf->data_size);
      mg_log_buffer(p_srv->p_log, p_buf->p_buffer, p_buf->data_size, buffer, 0);
   }

   return result;
}


int mg_db_connect_init(MGSRV *p_srv, int chndle)
{
   int result, n, len, buffer_actual_size, child_port;
   char buffer[1024], buffer1[256];
   char *p, *p1;
   MGBUF request;

   if (p_srv->mode == 2) {
      return 1;
   }

   result = 0;
   len = 0;

   p_srv->pcon[chndle]->child_port = 0;

   mg_buf_init(&request, 1024, 1024);

   sprintf(buffer, "^S^version=%s&timeout=%d&nls=%s&uci=%s\n", DBX_VERSION, 0, "", p_srv->uci);

   mg_buf_cpy(&request, buffer, (int) strlen(buffer));

   n = mg_db_send(p_srv, chndle, &request, 0);

   strcpy(buffer, "");
   buffer_actual_size = 0;
   n = mg_db_receive(p_srv, chndle, &request, 1024, 0);

   if (n > 0) {
      buffer_actual_size = n;
      request.p_buffer[buffer_actual_size] = '\0';

      strcpy(buffer, (char *) request.p_buffer);

      p = strstr(buffer, "pid=");
      if (!p) {
         return 2;
      }
      if (p) {
         result = 1;
         p +=4;
         p1 = strstr(p, "&");
         if (p1)
            *p1 = '\0';
         strcpy(p_srv->pcon[chndle]->mpid, p);
         if (p1)
            *p1 = '&';
      }
      p = strstr(buffer, "uci=");
      if (p) {
         p +=4;
         p1 = strstr(p, "&");
         if (p1)
            *p1 = '\0';
         if (p1)
            *p1 = '&';
      }
      p = strstr(buffer, "server_type=");
      if (p) {
         p +=12;
         p1 = strstr(p, "&");
         if (p1)
            *p1 = '\0';


         mg_lcase(p);

         if (!strcmp(p, "cache"))
            p_srv->pcon[chndle]->dbtype = DBX_DBTYPE_CACHE;
         else if (!strcmp(p, "iris"))
            p_srv->pcon[chndle]->dbtype = DBX_DBTYPE_IRIS;
         else if (!strcmp(p, "yottadb"))
            p_srv->pcon[chndle]->dbtype = DBX_DBTYPE_YOTTADB;
         else
            p_srv->pcon[chndle]->dbtype = DBX_DBTYPE_CACHE;

         if (p1)
            *p1 = '&';
      }
      p = strstr(buffer, "version=");
      if (p) {
         p +=8;
         p1 = strstr(p, "&");
         if (p1)
            *p1 = '\0';
         strcpy(buffer1, p);
         if (p1)
            *p1 = '&';
         strcpy(p_srv->pcon[chndle]->zmgsi_version, buffer1);
      }
      p = strstr(buffer, "child_port=");
      if (p) {
         p +=11;
         p1 = strstr(p, "&");
         if (p1)
            *p1 = '\0';
         strcpy(buffer1, p);
         if (p1)
            *p1 = '&';
         child_port = (int) strtol(buffer1, NULL, 10);

         if (child_port == 1)
            child_port = 0;

         if (child_port) {
            p_srv->pcon[chndle]->child_port = child_port;
            result = -120;
         }
      }
   }

   return result;
}


int mg_db_ayt(MGSRV *p_srv, int chndle)
{
   int result, n, len, buffer_actual_size;
   char buffer[512];
   MGBUF request;

   if (p_srv->mode == 2) {
      return 1;
   }

   result = 0;
   len = 0;
   buffer_actual_size = 0;

   mg_buf_init(&request, 1024, 1024);

   strcpy(buffer, "^A^A0123456789^^^^^\n");
   mg_buf_cpy(&request, buffer, (int) strlen(buffer));

   n = mg_db_send(p_srv, chndle, &request, 1);

   strcpy(buffer, "");

   n = mg_db_receive(p_srv, chndle, &request, 1024, 0);

   if (n > 0)
      buffer_actual_size += n;

   strcpy(buffer, (char *) request.p_buffer);
   buffer[buffer_actual_size] = '\0';

   if (buffer_actual_size > 0)
      result = 1;

   return result;
}


int mg_db_get_last_error(int context)
{
   int error_code;

#if defined(_WIN32)
   if (context)
      error_code = (int) GetLastError();
   else
      error_code = (int) NETX_WSAGETLASTERROR();
#else
   error_code = (int) errno;
#endif

   return error_code;
}


int mg_request_header(MGSRV *p_srv, MGBUF *p_buf, char *command, char *product)
{
   char buffer[256];

   sprintf(buffer, "PHP%s^P^%s#%s#0#%d#%d#%s#%d^%s^00000\n", product, p_srv->server, p_srv->uci, p_srv->timeout, p_srv->no_retry, DBX_VERSION, p_srv->storage_mode, command);

   p_srv->header_len = (int) strlen(buffer);

   mg_buf_cpy(p_buf, buffer, (int) strlen(buffer));

   return 1;
}


int mg_request_add(MGSRV *p_srv, int chndle, MGBUF *p_buf, unsigned char *element, int size, short byref, short type)
{
#if 1
   int hlen;
   unsigned char head[16];

   if (type == MG_TX_AREC_FORMATTED) {
      mg_buf_cat(p_buf, (char *) element, size);
      return 1;
   }
   hlen = mg_encode_item_header(head, size, byref, type);
   mg_buf_cat(p_buf, (char *) head, hlen);
   if (size)
      mg_buf_cat(p_buf, (char *) element, size);
   return 1;
#else
   unsigned long len;
   char *p;

   len = (int) strlen((char *) element);

   if ((len + p_buf->data_size) < p_buf->size) {
      strcpy((char *) (p_buf->p_buffer + p_buf->data_size), (char *) element);
      p_buf->data_size += len;
   }
   else {
      mg_db_send(p_srv, chndle, p_buf, 0);
      p_buf->data_size = 0;

      if (len > (MG_BUFSIZE / 2)) {

         p = p_buf->p_buffer;

         p_buf->p_buffer = element;
         p_buf->data_size = len;

         mg_db_send(p_srv, chndle, p_buf, 0);

         p_buf->p_buffer = p;
         p_buf->data_size = 0;
      }
      else {
         mg_buf_cat(p_buf, (char *) element);
      }

   }

   return 1;
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
      sprintf((char *) esize, "%d", size);
      return (int) strlen((char *) esize);
   }
   else {
      int n, n1, x;
      char buffer[32];

      n1 = 31;
      buffer[n1 --] = '\0';
      buffer[n1 --] = mg_encode_size64(size  % base);

      for (n = 1;; n ++) {
         x = (size / ((int) pow(base, n)));
         if (!x)
            break;
         buffer[n1 --] = mg_encode_size64(x  % base);
      }
      n1 ++;
      strcpy((char *) esize, buffer + n1);
      return (int) strlen((char *) esize);
   }
}


int mg_decode_size(unsigned char *esize, int len, short base)
{
   int size;
   unsigned char c;

   if (base == 10) {
      c = *(esize + len);
      *(esize + len) = '\0';
      size = (int) strtol((char *) esize, NULL, 10);
      *(esize + len) = c;
   }
   else {
      int n, x;

      size = 0;
      for (n = len - 1; n >= 0; n --) {

         x = (int) esize[n];
         size = size + mg_decode_size64(x) * ((int) pow((double) base, ((double) (len - (n + 1)))));
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
   strncpy((char *) (head + 1), (char *) esize, slen);

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


int mg_get_error(MGSRV *p_srv, char *buffer)
{
   int n;

   if (!strncmp(buffer + 5, "ce", 2)) {
      for (n = MG_RECV_HEAD; buffer[n]; n ++) {
         if (buffer[n] == '%')
            buffer[n] = '^';
      }
      return 1;
   }
   else
      return 0;
}


int mg_extract_substrings(MGSTR * records, char * buffer, int tsize, char delim, int offset, int no_tail, short type)
{
   int n;
   char *p;

   if (!buffer)
      return 0;

   n = offset;
   p = buffer;

   if (type == MG_ES_DELIM) {
      records[n].ps = (unsigned char *) p;
      records[n].size = (int) strlen((char *) records[n].ps);

      for (;;) {
         p = strchr(p, delim);
         if (!p) {
         records[n].size = (int) strlen((char *) records[n].ps);
            break;
         }

         *p = '\0';
         records[n].size = (int) strlen((char *) records[n].ps);
         n ++;
         records[n].ps = (unsigned char *) (++ p);
      }
      n ++;
      records[n].ps = NULL;
      records[n].size = 0;
      if (no_tail == 1 && n > 0)
         n --;

      records[n].ps = NULL;
      records[n].size = 0;
   }
   else {
      short byref, type;
      int size, hlen, rlen, i;
      
      rlen = 0;
      for (i = 0;; i ++) {
         hlen = mg_decode_item_header((unsigned char *) p, &size, &byref, &type);

         *p = '\0';
         rlen += hlen;
         if ((rlen + size) > tsize)
            break;
         records[n].ps = (unsigned char *) (p + hlen);
         records[n].size = size;
         n ++;
         p += (hlen + size);
         rlen += size;
         if (rlen >= tsize)
            break;
      }
      records[n].ps = NULL;
      records[n].size = 0;
   }

   return (n - offset);
}


int mg_compare_keys(MGSTR * key, MGSTR * rkey, int max)
{
   int n, result;

   result = 0;
   for (n = 1; n <= max; n ++) {
      result = strcmp((char *) key[n].ps, (char *) rkey[n].ps);
      if (result)
         break;
   }

   return result;
}


int mg_replace_substrings(char * tbuffer, char *fbuffer, char * replace, char * with)
{
   int len, wlen, rlen;
   char *pf1, *pf2, *pt1;
   char temp[32000];

   rlen = (int) strlen(replace);
   wlen = (int) strlen(with);

   pt1 = tbuffer;
   pf1 = fbuffer;
   if (pt1 == pf1) {
      pf1 = temp;
      strcpy(pf1, pt1);
   }
   while ((pf2 = strstr(pf1, replace))) {
      len = (int) (pf2 - pf1);
      strncpy(pt1, pf1, len);
      pt1 += len;
      strncpy(pt1, with, wlen);
      pt1 += wlen;
      pf1 = (pf2 + rlen);
   }
   strcpy(pt1, pf1);

   return 1;
}


int mg_bind_server_api(MGSRV *p_srv, short context)
{
   int rc, chndle, result;
   char *p, *p1, *p2;
   char buffer[256];
   DBXMETH *pmeth;
   DBXCON *pcon;

   chndle = 0;
   result = 0;

   if (!p_srv->pcon[chndle]) {
      p_srv->pcon[chndle] = (DBXCON *) mg_malloc(sizeof(DBXCON), 0);
      memset(p_srv->pcon[chndle], 0, sizeof(DBXCON));
      p_srv->pcon[chndle]->chndle = chndle;

      pmeth = (DBXMETH *) mg_malloc(sizeof(DBXMETH), 0);
      memset(pmeth, 0, sizeof(DBXMETH));
      p_srv->pcon[chndle]->pmeth_base = (void *) pmeth;
   }

   pcon = p_srv->pcon[chndle];
   pmeth = (DBXMETH *) pcon->pmeth_base;
   pmeth->pcon = pcon;

   pcon->tlevel = 0;
   pcon->p_isc_so = NULL;
   pcon->p_ydb_so = NULL;
   pcon->p_gtm_so = NULL;

   p_srv->mode = 2;
   pcon->p_srv = (void *) p_srv;
   pcon->p_log = &pcon->log;
   pcon->p_db_mutex = &pcon->db_mutex;
   mg_mutex_create(pcon->p_db_mutex);
   pcon->p_zv = &pcon->zv;

   mg_log_init(pcon->p_log);

#if defined(MG_DBA_DSO)
   strcpy(pcon->p_log->product, MG_LOG_FILE);
   strcpy(pcon->p_log->product, MG_PRODUCT);
#endif

   strcpy(pcon->p_log->product_version, DBX_VERSION);

   strcpy(buffer, p_srv->dbtype_name);
   mg_lcase(buffer);

   pcon->dbtype = 0;
   if (!strcmp(buffer, "cache"))
      pcon->dbtype = DBX_DBTYPE_CACHE;
   else if (!strcmp(buffer, "iris"))
      pcon->dbtype = DBX_DBTYPE_IRIS;
   else if (!strcmp(buffer, "yottadb"))
      pcon->dbtype = DBX_DBTYPE_YOTTADB;
   else if (!strcmp(buffer, "gtm") || !strcmp(buffer, "gt.m"))
      pcon->dbtype = DBX_DBTYPE_GTM;

   if (!pcon->dbtype) {
      strcpy(p_srv->error_mess, "Unrecognised Server Type");
      return 0;
   }

   strcpy(pcon->shdir, p_srv->shdir);
   strcpy(pcon->username, p_srv->username);
   strcpy(pcon->password, p_srv->password);
   strcpy(pcon->nspace, p_srv->uci);
   pcon->input_device[0] = '\0';
   pcon->output_device[0] = '\0';

   p = (char *) p_srv->p_env->p_buffer;
   p2 = p;
   while ((p2 = strstr(p, "\n"))) {
      *p2 = '\0';
      p1 = strstr(p, "=");
      if (p1) {
         *p1 = '\0';
         p1 ++;
#if defined(_WIN32)
         SetEnvironmentVariable((LPCTSTR) p, (LPCTSTR) p1);
#else
         /* printf("\nLinux : environment variable p=%s p1=%s;", p, p1); */
         setenv(p, p1, 1);
#endif
         *p1 = '=';
      }
      else {
         break;
      }
      *p2 = '\n';
      p = p2 + 1;
   }

   if (!pcon->shdir[0]) {
      strcpy(pcon->error, "Unable to determine the path to the database installation");
      rc = CACHE_NOCON;
      goto mg_bind_server_api_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      rc = ydb_open(pmeth);
   }
   else if (pcon->dbtype == DBX_DBTYPE_GTM) {
      rc = gtm_open(pmeth);
   }
   else {
      rc = isc_open(pmeth);
   }

mg_bind_server_api_exit:

   if (rc == CACHE_SUCCESS) {
      pcon->connected = 1;
      result = 1;
   }
   else {
      pcon->connected = 0;
      result = 0;
      strcpy(p_srv->error_mess, pcon->error);
   }

   return result;

}


int mg_release_server_api(MGSRV *p_srv, short context)
{
   int result, chndle, rc, rc1;
   char buffer[256];
   DBXCON *pcon;

   result = 1;
   chndle = 0;

   pcon = p_srv->pcon[chndle];

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (pcon->p_ydb_so->loaded) {
         rc = pcon->p_ydb_so->p_ydb_exit();
         /* printf("\r\np_ydb_exit=%d\r\n", rc); */
      }

      strcpy(pcon->error, "");
/*
      mg_dso_unload(pcon->p_ydb_so->p_library); 
      pcon->p_ydb_so->p_library = NULL;
      pcon->p_ydb_so->loaded = 0;
*/
      strcpy(pcon->p_ydb_so->libdir, "");
      strcpy(pcon->p_ydb_so->libnam, "");

   }
   else if (pcon->dbtype == DBX_DBTYPE_GTM) {
      if (pcon->p_gtm_so->loaded) {
         rc = (int) pcon->p_gtm_so->p_gtm_exit();
         /* printf("\r\np_gtm_exit=%d\r\n", rc); */
         if (rc != 0) {
            pcon->p_gtm_so->p_gtm_zstatus(buffer, 255);
            strcpy(p_srv->error_mess, buffer);
            result = 0;
         }
      }

      strcpy(pcon->error, "");
/*
      mg_dso_unload(pcon->p_gtm_so->p_library); 
      pcon->p_gtm_so->p_library = NULL;
      pcon->p_gtm_so->loaded = 0;
*/
      strcpy(pcon->p_gtm_so->libdir, "");
      strcpy(pcon->p_gtm_so->libnam, "");
   }
   else {
      if (pcon->p_isc_so->loaded) {

         DBX_LOCK(rc, 0);

         rc = pcon->p_isc_so->p_CacheEnd();
         rc1 = rc;

         DBX_UNLOCK(rc);

      }

      strcpy(pcon->error, "");

      mg_dso_unload(pcon->p_isc_so->p_library); 

      pcon->p_isc_so->p_library = NULL;
      pcon->p_isc_so->loaded = 0;

      strcpy(pcon->p_isc_so->libdir, "");
      strcpy(pcon->p_isc_so->libnam, "");
   }

   return result;

}


int mg_invoke_server_api(MGSRV *p_srv, int chndle, MGBUF *p_buf, int size, int mode)
{
   int result, rc, rc1, ne, ex;
   unsigned int n, max, len;
   char *outstr8, *p;
   char buffer[256], buf1[32], buf3[32];
   DBXFUN fun, *pfun;
   DBXMETH *pmeth;
   DBXCON *pcon;
   CACHE_EXSTR zstr;

   result = 0;
   chndle = 0;

   pcon = p_srv->pcon[chndle];
   pmeth = (DBXMETH *) pcon->pmeth_base;
   pfun = &fun;

   p = strstr(p_buf->p_buffer, "\n");
   if (p) {
      p -= 7;
      pmeth->command[0] = *p;
      pmeth->command[1] = '\0';
   }

   if (pmeth->command[0] == 'a') {
      rc = dbx_tstart_ex(pmeth);
      if (rc == CACHE_SUCCESS) {
         result = 1;
      }
      sprintf(p_buf->p_buffer, "00000cv\n%d", pmeth->output_val.num.int32);
      p_buf->data_size = (int) strlen((char *) p_buf->p_buffer);
      goto mg_invoke_server_api_exit;
   }
   else if (pmeth->command[0] == 'b') {
      rc = dbx_tlevel_ex(pmeth);
      if (rc >= 0) {
         result = 1;
      }
      sprintf(p_buf->p_buffer, "00000cv\n%d", pmeth->output_val.num.int32);
      p_buf->data_size = (int) strlen((char *) p_buf->p_buffer);
      goto mg_invoke_server_api_exit;
   }
   else if (pmeth->command[0] == 'c') {
      rc = dbx_tcommit_ex(pmeth);
      if (rc == CACHE_SUCCESS) {
         result = 1;
      }
      sprintf(p_buf->p_buffer, "00000cv\n%d", pmeth->output_val.num.int32);
      p_buf->data_size = (int) strlen((char *) p_buf->p_buffer);
      goto mg_invoke_server_api_exit;
   }
   else if (pmeth->command[0] == 'd') {
      rc = dbx_trollback_ex(pmeth);
      if (rc == CACHE_SUCCESS) {
         result = 1;
      }
      sprintf(p_buf->p_buffer, "00000cv\n%d", pmeth->output_val.num.int32);
      p_buf->data_size = (int) strlen((char *) p_buf->p_buffer);
      goto mg_invoke_server_api_exit;
   }

   if (pcon->dbtype == DBX_DBTYPE_YOTTADB) {
      if (!pcon->p_ydb_so->loaded || !pcon->p_ydb_so || !pcon->p_ydb_so->p_ydb_ci) {
         result = 0;
         strcpy(p_srv->error_mess, "YottaDB server API not bound");
         goto mg_invoke_server_api_exit;
      }
      pfun->rflag = 0;
      pfun->label = "ifc_zmgsis";
      pfun->label_len = 10;
      pfun->routine = "";
      pfun->routine_len = 0;
      pmeth->argc = 3;

/*
      rc = pcon->p_ydb_so->p_ydb_ci(pfun->label, p_buf->p_buffer, "0", p_buf->p_buffer, "");
      p_buf->data_size = (unsigned long) strlen(p_buf->p_buffer);
      result = 1;
*/

      pfun->out.address = (char *) p_buf->p_buffer;
      pfun->out.length = (unsigned long) p_buf->size;

      strcpy(buf1, "0");
      pfun->in[1].address = buf1;
      pfun->in[1].length = 1;

      pfun->in[2].address = (char *)  p_buf->p_buffer;
      pfun->in[2].length = (unsigned long)  p_buf->data_size;

      strcpy(buf3, "");
      pfun->in[3].address = buf3;
      pfun->in[3].length = 0;

      pfun->argc = 4;
      if (pcon->tlevel > 0) {
         pmeth->pfun = pfun;
         rc = ydb_transaction_task(pmeth, YDB_TPCTX_FUN);
      }
      else {
         rc = ydb_function_ex(pmeth, pfun);
      }
      result = 1;
   }
   else if (pcon->dbtype == DBX_DBTYPE_GTM) {
      if (!pcon->p_gtm_so->loaded || !pcon->p_gtm_so || !pcon->p_gtm_so->p_gtm_ci) {
         result = 0;
         strcpy(p_srv->error_mess, "GT.M server API not bound");
         goto mg_invoke_server_api_exit;
      }
      pfun->rflag = 0;
      pfun->label = "ifc_zmgsis";
      pfun->label_len = 10;
      pfun->routine = "";
      pfun->routine_len = 0;
      pmeth->argc = 3;

      rc = (int) pcon->p_gtm_so->p_gtm_ci(pfun->label, p_buf->p_buffer, "0", p_buf->p_buffer, "");
      if (rc != 0) {
         pcon->p_gtm_so->p_gtm_zstatus(buffer, 255);
         strcpy(p_srv->error_mess, buffer);
         result = 0;
         goto mg_invoke_server_api_exit;
      }

      p_buf->data_size = (unsigned long) strlen(p_buf->p_buffer);
      result = 1;
   }
   else {
      if (!pcon->p_isc_so->loaded || !pcon->p_isc_so || !pcon->p_isc_so->p_CachePushFunc) {
         result = 0;
         strcpy(p_srv->error_mess, "InterSystems server API not bound");
         goto mg_invoke_server_api_exit;
      }
      ex = 1;
      zstr.len = 0;
      zstr.str.ch = NULL;
      outstr8 = NULL;

      pfun->rflag = 0;
      pfun->label = "ifc";
      pfun->label_len = 3;
      pfun->routine = "%zmgsis";
      pfun->routine_len = 7;
      pmeth->argc = 3;
      rc = pcon->p_isc_so->p_CachePushFunc(&(pfun->rflag), (int) pfun->label_len, (const Callin_char_t *) pfun->label, (int) pfun->routine_len, (const Callin_char_t *) pfun->routine);

      rc1 = 0;
      rc = pcon->p_isc_so->p_CachePushInt(rc1);

      if (p_buf->data_size < DBX_MAXSIZE) {
         rc = pcon->p_isc_so->p_CachePushStr(p_buf->data_size, (Callin_char_t *) p_buf->p_buffer);
      }
      else {
         pmeth->args[0].cvalue.pstr = (void *) pcon->p_isc_so->p_CacheExStrNew((CACHE_EXSTRP) &(pmeth->args[0].cvalue.zstr), p_buf->data_size + 1);
         for (ne = 0; ne < (int) p_buf->data_size; ne ++) {
            pmeth->args[0].cvalue.zstr.str.ch[ne] = (char) p_buf->p_buffer[ne];
         }
         pmeth->args[0].cvalue.zstr.str.ch[ne] = (char) 0;
         pmeth->args[0].cvalue.zstr.len = p_buf->data_size;
         rc = pcon->p_isc_so->p_CachePushExStr((CACHE_EXSTRP) &(pmeth->args[0].cvalue.zstr));
      }
      *buffer = '\0';
      rc = pcon->p_isc_so->p_CachePushStr(0, (Callin_char_t *) buffer);
      rc = pcon->p_isc_so->p_CacheExtFun(pfun->rflag, pmeth->argc);

      if (rc == CACHE_SUCCESS) {
         if (ex) {
            rc = pcon->p_isc_so->p_CachePopExStr(&zstr);
            len = zstr.len;
            outstr8 = (char *) zstr.str.ch;
         }
         else {
            rc = pcon->p_isc_so->p_CachePopStr((int *) &len, (Callin_char_t **) &outstr8);
         }
         max = p_buf->size - 1;
         for (n = 0; n < len; n ++) {
            if (n > max)
               break;
            p_buf->p_buffer[n] = (char) outstr8[n];
         }
         p_buf->p_buffer[n] = '\0';
         p_buf->data_size = len;

         if (ex) {
            rc1 = pcon->p_isc_so->p_CacheExStrKill(&zstr);
         }
         result = 1;
      }
      else {
         result = 0;
         strcpy(p_srv->error_mess, "InterSystems server error - unable to invoke function");
         goto mg_invoke_server_api_exit;
      }
   }

mg_invoke_server_api_exit:

   if (!result) {
      sprintf(p_buf->p_buffer, "00000ce\n%s", buffer);
      p_buf->data_size = (int) strlen(p_buf->p_buffer);
   }

   return result;
}

