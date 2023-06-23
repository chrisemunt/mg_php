/*
   ----------------------------------------------------------------------------
   | mg_dba.so|dll                                                            |
   | Description: An abstraction of the InterSystems Cache/IRIS API           |
   |              and YottaDB API                                             |
   | Author:      Chris Munt cmunt@mgateway.com                               |
   |                         chris.e.munt@gmail.com                           |
   | Copyright (c) 2019-2023 MGateway Ltd                                     |
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

#ifndef MG_DBX_H
#define MG_DBX_H

#if defined(_WIN32)

#define BUILDING_NODE_EXTENSION     1
#if defined(_MSC_VER)
/* Check for MS compiler later than VC6 */
#if (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE    1
#define _CRT_NONSTDC_NO_DEPRECATE   1
#endif
#endif

#elif defined(__linux__) || defined(__linux) || defined(linux)

#if !defined(LINUX)
#define LINUX                       1
#endif

#elif defined(__APPLE__)

#if !defined(MACOSX)
#define MACOSX                      1
#endif

#endif

#if defined(SOLARIS)
#ifndef __GNUC__
#  define  __attribute__(x)
#endif
#endif

#if defined(_WIN32)

#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__MINGW32__)
#include <math.h>
#endif

#else

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#if !defined(HPUX) && !defined(HPUX10) && !defined(HPUX11)
#include <sys/select.h>
#endif
#if defined(SOLARIS)
#include <sys/filio.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <dlfcn.h>
#include <math.h>

#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Cache/IRIS */

#define CACHE_MAXSTRLEN	32767
#define CACHE_MAXLOSTSZ	3641144

typedef char		Callin_char_t;
#define CACHE_INT64 long long
#define CACHESTR	CACHE_ASTR

typedef struct {
   unsigned short len;
   Callin_char_t  str[CACHE_MAXSTRLEN];
} CACHE_ASTR, *CACHE_ASTRP;

typedef struct {
   unsigned int	len;
   union {
      Callin_char_t * ch;
      unsigned short *wch;
      unsigned short *lch;
   } str;
} CACHE_EXSTR, *CACHE_EXSTRP;

#define CACHE_TTALL     1
#define CACHE_TTNEVER   8
#define CACHE_PROGMODE  32

#define CACHE_INT	      1
#define CACHE_DOUBLE	   2
#define CACHE_ASTRING   3

#define CACHE_CHAR      4
#define CACHE_INT2      5
#define CACHE_INT4      6
#define CACHE_INT8      7
#define CACHE_UCHAR     8
#define CACHE_UINT2     9
#define CACHE_UINT4     10
#define CACHE_UINT8     11
#define CACHE_FLOAT     12
#define CACHE_HFLOAT    13
#define CACHE_UINT      14
#define CACHE_WSTRING   15
#define CACHE_OREF      16
#define CACHE_LASTRING  17
#define CACHE_LWSTRING  18
#define CACHE_IEEE_DBL  19
#define CACHE_HSTRING   20
#define CACHE_UNDEF     21

#define CACHE_CHANGEPASSWORD  -16
#define CACHE_ACCESSDENIED    -15
#define CACHE_EXSTR_INUSE     -14
#define CACHE_NORES	         -13
#define CACHE_BADARG	         -12
#define CACHE_NOTINCACHE      -11
#define CACHE_RETTRUNC 	      -10
#define CACHE_ERUNKNOWN	      -9	
#define CACHE_RETTOOSMALL     -8	
#define CACHE_NOCON 	         -7
#define CACHE_INTERRUPT       -6
#define CACHE_CONBROKEN       -4
#define CACHE_STRTOOLONG      -3
#define CACHE_ALREADYCON      -2
#define CACHE_FAILURE	      -1
#define CACHE_SUCCESS 	      0

#define CACHE_ERMXSTR         5
#define CACHE_ERNOLINE        8
#define CACHE_ERUNDEF         9
#define CACHE_ERSYSTEM        10
#define CACHE_ERSUBSCR        16
#define CACHE_ERNOROUTINE     17
#define CACHE_ERSTRINGSTACK   20
#define CACHE_ERUNIMPLEMENTED 22
#define CACHE_ERARGSTACK      25
#define CACHE_ERPROTECT       27
#define CACHE_ERPARAMETER     40
#define CACHE_ERNAMSP         83
#define CACHE_ERWIDECHAR      89
#define CACHE_ERNOCLASS       122
#define CACHE_ERBADOREF       119
#define CACHE_ERNOMETHOD      120
#define CACHE_ERNOPROPERTY    121

#define CACHE_ETIMEOUT        -100
#define CACHE_BAD_STRING      -101
#define CACHE_BAD_NAMESPACE   -102
#define CACHE_BAD_GLOBAL      -103
#define CACHE_BAD_FUNCTION    -104
#define CACHE_BAD_CLASS       -105
#define CACHE_BAD_METHOD      -106

#define CACHE_INCREMENTAL_LOCK   1

/* End of Cache/IRIS */


/* YottaDB */

#define YDB_OK       0
#define YDB_FAILURE -1
#define YDB_DEL_TREE 1

typedef int                ydb_int_t;
typedef unsigned int       ydb_uint_t;
typedef long               ydb_long_t;
typedef unsigned long      ydb_ulong_t;
typedef float              ydb_float_t;
typedef double             ydb_double_t;
typedef char               ydb_char_t;
typedef int                (*ydb_tpfnptr_t) (void *tpfnparm);  

#define YDB_INT_MAX        ((int)0x7fffffff)
#define YDB_TP_RESTART     (YDB_INT_MAX - 1)
#define YDB_TP_ROLLBACK    (YDB_INT_MAX - 2)
#define YDB_NODE_END       (YDB_INT_MAX - 3)
#define YDB_LOCK_TIMEOUT   (YDB_INT_MAX - 4)
#define YDB_NOTOK          (YDB_INT_MAX - 5)

#define YDB_MAX_TP         32
#define YDB_TPCTX_DB       1
#define YDB_TPCTX_TLEVEL   2
#define YDB_TPCTX_COMMIT   3
#define YDB_TPCTX_ROLLBACK 4
#define YDB_TPCTX_FUN      10
#define YDB_TPCTX_QUERY    11
#define YDB_TPCTX_ORDER    12

typedef struct {
   unsigned int   len_alloc;
   unsigned int   len_used;
   char		      *buf_addr;
} ydb_buffer_t;

typedef struct {
   unsigned long  length;
   char		      *address;
} ydb_string_t;

typedef struct {
   ydb_string_t   rtn_name;
   void		      *handle;
} ci_name_descriptor;

typedef ydb_buffer_t DBXSTR;
typedef char         ydb_char_t;
typedef long         ydb_long_t;
typedef int          (*ydb_tpfnptr_t) (void *tpfnparm);  


/* End of YottaDB */

/* GT.M call-in interface */

typedef int    gtm_status_t;
typedef char   gtm_char_t;
typedef int    xc_status_t;

/* End of GT.M call-in interface */


#define DBX_DBTYPE_CACHE         1
#define DBX_DBTYPE_IRIS          2
#define DBX_DBTYPE_YOTTADB       5
#define DBX_DBTYPE_GTM           11

#define DBX_MAXCONS              32
#define DBX_MAXARGS              64

#define DBX_ERROR_SIZE           512

#define DBX_THREAD_STACK_SIZE    0xf0000

#define DBX_DSORT_INVALID        0
#define DBX_DSORT_DATA           1
#define DBX_DSORT_SUBSCRIPT      2
#define DBX_DSORT_GLOBAL         3
#define DBX_DSORT_EOD            9
#define DBX_DSORT_STATUS         10
#define DBX_DSORT_ERROR          11

/* v1.3.12 */
#define DBX_DSORT_ISVALID(a)     ((a == DBX_DSORT_GLOBAL) || (a == DBX_DSORT_SUBSCRIPT) || (a == DBX_DSORT_DATA) || (a == DBX_DSORT_EOD) || (a == DBX_DSORT_STATUS) || (a == DBX_DSORT_ERROR))

#define DBX_DTYPE_NONE           0
#define DBX_DTYPE_DBXSTR         1
#define DBX_DTYPE_STR            2
#define DBX_DTYPE_INT            4
#define DBX_DTYPE_INT64          5
#define DBX_DTYPE_DOUBLE         6
#define DBX_DTYPE_OREF           7
#define DBX_DTYPE_NULL           10

#define DBX_CMND_OPEN            1
#define DBX_CMND_CLOSE           2
#define DBX_CMND_NSGET           3
#define DBX_CMND_NSSET           4

#define DBX_CMND_GSET            11
#define DBX_CMND_GGET            12
#define DBX_CMND_GNEXT           13
#define DBX_CMND_GNEXTDATA       131
#define DBX_CMND_GPREVIOUS       14
#define DBX_CMND_GPREVIOUSDATA   141
#define DBX_CMND_GDELETE         15
#define DBX_CMND_GDEFINED        16
#define DBX_CMND_GINCREMENT      17
/* v1.3.13 */
#define DBX_CMND_GLOCK           18
#define DBX_CMND_GUNLOCK         19
#define DBX_CMND_GMERGE          20

#define DBX_CMND_GNNODE          21
#define DBX_CMND_GNNODEDATA      211
#define DBX_CMND_GPNODE          22
#define DBX_CMND_GPNODEDATA      221

#define DBX_CMND_FUNCTION        31

#define DBX_CMND_CCMETH          41
#define DBX_CMND_CGETP           42
#define DBX_CMND_CSETP           43
#define DBX_CMND_CMETH           44
#define DBX_CMND_CCLOSE          45

#define DBX_CMND_TSTART          61
#define DBX_CMND_TLEVEL          62
#define DBX_CMND_TCOMMIT         63
#define DBX_CMND_TROLLBACK       64

#define DBX_MAXSIZE              32767
#define DBX_BUFFER               32768

#define DBX_LS_MAXSIZE           3641144
#define DBX_LS_BUFFER            3641145

/* v1.3.14 */
#define DBX_YDB_MAXSIZE          1048576
#define DBX_YDB_BUFFER           1048577

#if defined(MAX_PATH) && (MAX_PATH>511)
#define DBX_MAX_PATH             MAX_PATH
#else
#define DBX_MAX_PATH             512
#endif

#if defined(_WIN32)
#define DBX_NULL_DEVICE          "//./nul"
#else
#define DBX_NULL_DEVICE          "/dev/null/"
#endif

#if defined(_WIN32)
#define DBX_CACHE_DLL            "cache.dll"
#define DBX_IRIS_DLL             "irisdb.dll"
#define DBX_YDB_DLL              "yottadb.dll"
#define DBX_GTM_DLL              "gtmshr.dll"
#else
#define DBX_CACHE_SO             "libcache.so"
#define DBX_CACHE_DYLIB          "libcache.dylib"
#define DBX_IRIS_SO              "libirisdb.so"
#define DBX_IRIS_DYLIB           "libirisdb.dylib"
#define DBX_YDB_SO               "libyottadb.so"
#define DBX_YDB_DYLIB            "libyottadb.dylib"
#define DBX_GTM_SO               "libgtmshr.so"
#define DBX_GTM_DYLIB            "libgtmshr.dylib"
#endif

#if defined(__linux__) || defined(linux) || defined(LINUX)
#define DBX_MEMCPY(a,b,c)           memmove(a,b,c)
#else
#define DBX_MEMCPY(a,b,c)           memcpy(a,b,c)
#endif

#define DBX_LOCK(RC, TIMEOUT) \
   if (pcon->use_db_mutex) { \
      RC = mg_mutex_lock(pcon->p_db_mutex, TIMEOUT); \
   } \

#define DBX_UNLOCK(RC) \
   if (pcon->use_db_mutex) { \
      RC = mg_mutex_unlock(pcon->p_db_mutex); \
   } \


#define NETX_TIMEOUT             30
#define NETX_IPV6                1
#define NETX_READ_EOF            0
#define NETX_READ_NOCON          -1
#define NETX_READ_ERROR          -2
#define NETX_READ_TIMEOUT        -3
#define NETX_RECV_BUFFER         32768

#if defined(LINUX)
#define NETX_MEMCPY(a,b,c)       memmove(a,b,c)
#else
#define NETX_MEMCPY(a,b,c)       memcpy(a,b,c)
#endif


typedef void * (* MG_MALLOC)     (unsigned long size);
typedef void * (* MG_REALLOC)    (void *p, unsigned long size);
typedef int    (* MG_FREE)       (void *p);


#if defined(_WIN32)

#if defined(MG_DBA_DSO)
#define DBX_EXTFUN(a)    __declspec(dllexport) a __cdecl
#else
#define DBX_EXTFUN(a)    a
#endif

#define NETX_WSASOCKET               netx_so.p_WSASocket
#define NETX_WSAGETLASTERROR         netx_so.p_WSAGetLastError
#define NETX_WSASTARTUP              netx_so.p_WSAStartup
#define NETX_WSACLEANUP              netx_so.p_WSACleanup
#define NETX_WSAFDISET               netx_so.p_WSAFDIsSet
#define NETX_WSARECV                 netx_so.p_WSARecv
#define NETX_WSASEND                 netx_so.p_WSASend

#define NETX_WSASTRINGTOADDRESS      netx_so.p_WSAStringToAddress
#define NETX_WSAADDRESSTOSTRING      netx_so.p_WSAAddressToString
#define NETX_GETADDRINFO             netx_so.p_getaddrinfo
#define NETX_FREEADDRINFO            netx_so.p_freeaddrinfo
#define NETX_GETNAMEINFO             netx_so.p_getnameinfo
#define NETX_GETPEERNAME             netx_so.p_getpeername
#define NETX_INET_NTOP               netx_so.p_inet_ntop
#define NETX_INET_PTON               netx_so.p_inet_pton

#define NETX_CLOSESOCKET             netx_so.p_closesocket
#define NETX_GETHOSTNAME             netx_so.p_gethostname
#define NETX_GETHOSTBYNAME           netx_so.p_gethostbyname
#define NETX_SETSERVBYNAME           netx_so.p_getservbyname
#define NETX_GETHOSTBYADDR           netx_so.p_gethostbyaddr
#define NETX_HTONS                   netx_so.p_htons
#define NETX_HTONL                   netx_so.p_htonl
#define NETX_NTOHL                   netx_so.p_ntohl
#define NETX_NTOHS                   netx_so.p_ntohs
#define NETX_CONNECT                 netx_so.p_connect
#define NETX_INET_ADDR               netx_so.p_inet_addr
#define NETX_INET_NTOA               netx_so.p_inet_ntoa
#define NETX_SOCKET                  netx_so.p_socket
#define NETX_SETSOCKOPT              netx_so.p_setsockopt
#define NETX_GETSOCKOPT              netx_so.p_getsockopt
#define NETX_GETSOCKNAME             netx_so.p_getsockname
#define NETX_SELECT                  netx_so.p_select
#define NETX_RECV                    netx_so.p_recv
#define NETX_SEND                    netx_so.p_send
#define NETX_SHUTDOWN                netx_so.p_shutdown
#define NETX_BIND                    netx_so.p_bind
#define NETX_LISTEN                  netx_so.p_listen
#define NETX_ACCEPT                  netx_so.p_accept

#define  NETX_FD_ISSET(fd, set)              netx_so.p_WSAFDIsSet((SOCKET)(fd), (fd_set *)(set))

typedef int (WINAPI * MG_LPFN_WSAFDISSET)       (SOCKET, fd_set *);

typedef DWORD           DBXTHID;
typedef HINSTANCE       DBXPLIB;
typedef FARPROC         DBXPROC;

typedef LPSOCKADDR      xLPSOCKADDR;
typedef u_long          *xLPIOCTL;
typedef const char      *xLPSENDBUF;
typedef char            *xLPRECVBUF;

#ifdef _WIN64
typedef int             socklen_netx;
#else
typedef size_t          socklen_netx;
#endif

#define SOCK_ERROR(n)   (n == SOCKET_ERROR)
#define INVALID_SOCK(n) (n == INVALID_SOCKET)
#define NOT_BLOCKING(n) (n != WSAEWOULDBLOCK)

#define BZERO(b,len) (memset((b), '\0', (len)), (void) 0)

#else /* #if defined(_WIN32) */

#define DBX_EXTFUN(a)    a

#define NETX_WSASOCKET               WSASocket
#define NETX_WSAGETLASTERROR         WSAGetLastError
#define NETX_WSASTARTUP              WSAStartup
#define NETX_WSACLEANUP              WSACleanup
#define NETX_WSAFDIsSet              WSAFDIsSet
#define NETX_WSARECV                 WSARecv
#define NETX_WSASEND                 WSASend

#define NETX_WSASTRINGTOADDRESS      WSAStringToAddress
#define NETX_WSAADDRESSTOSTRING      WSAAddressToString
#define NETX_GETADDRINFO             getaddrinfo
#define NETX_FREEADDRINFO            freeaddrinfo
#define NETX_GETNAMEINFO             getnameinfo
#define NETX_GETPEERNAME             getpeername
#define NETX_INET_NTOP               inet_ntop
#define NETX_INET_PTON               inet_pton

#define NETX_CLOSESOCKET             closesocket
#define NETX_GETHOSTNAME             gethostname
#define NETX_GETHOSTBYNAME           gethostbyname
#define NETX_SETSERVBYNAME           getservbyname
#define NETX_GETHOSTBYADDR           gethostbyaddr
#define NETX_HTONS                   htons
#define NETX_HTONL                   htonl
#define NETX_NTOHL                   ntohl
#define NETX_NTOHS                   ntohs
#define NETX_CONNECT                 connect
#define NETX_INET_ADDR               inet_addr
#define NETX_INET_NTOA               inet_ntoa
#define NETX_SOCKET                  socket
#define NETX_SETSOCKOPT              setsockopt
#define NETX_GETSOCKOPT              getsockopt
#define NETX_GETSOCKNAME             getsockname
#define NETX_SELECT                  select
#define NETX_RECV                    recv
#define NETX_SEND                    send
#define NETX_SHUTDOWN                shutdown
#define NETX_BIND                    bind
#define NETX_LISTEN                  listen
#define NETX_ACCEPT                  accept

#define NETX_FD_ISSET(fd, set) FD_ISSET(fd, set)

typedef pthread_t       DBXTHID;
typedef void            *DBXPLIB;
typedef void            *DBXPROC;

typedef unsigned long   DWORD;
typedef unsigned long   WORD;
typedef int             WSADATA;
typedef int             SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr * LPSOCKADDR;
typedef struct hostent  HOSTENT;
typedef struct hostent  * LPHOSTENT;
typedef struct servent  SERVENT;
typedef struct servent  * LPSERVENT;

#ifdef NETX_BS_GEN_PTR
typedef const void      * xLPSOCKADDR;
typedef void            * xLPIOCTL;
typedef const void      * xLPSENDBUF;
typedef void            * xLPRECVBUF;
#else
typedef LPSOCKADDR      xLPSOCKADDR;
typedef char            * xLPIOCTL;
typedef const char      * xLPSENDBUF;
typedef char            * xLPRECVBUF;
#endif /* #ifdef NETX_BS_GEN_PTR */

#if defined(OSF1) || defined(HPUX) || defined(HPUX10) || defined(HPUX11)
typedef int             socklen_netx;
#elif defined(LINUX) || defined(AIX) || defined(AIX5) || defined(MACOSX)
typedef socklen_t       socklen_netx;
#else
typedef size_t          socklen_netx;
#endif

#ifndef INADDR_NONE
#define INADDR_NONE     -1
#endif

#define SOCK_ERROR(n)   (n < 0)
#define INVALID_SOCK(n) (n < 0)
#define NOT_BLOCKING(n) (n != EWOULDBLOCK && n != 2)

#define BZERO(b, len)   (bzero(b, len))

#endif /* #if defined(_WIN32) */

#if defined(__MINGW32__)

typedef
INT
(WSAAPI * LPFN_INET_PTONA)(
    __in                                INT             Family,
    __in                                PCSTR           pszAddrString,
    __out_bcount(sizeof(IN6_ADDR))      PVOID           pAddrBuf
    );

#define LPFN_INET_PTON          LPFN_INET_PTONA

typedef
PCSTR
(WSAAPI * LPFN_INET_NTOPA)(
    __in                                INT             Family,
    __in                                PVOID           pAddr,
    __out_ecount(StringBufSize)         PSTR            pStringBuf,
    __in                                size_t          StringBufSize
    );

#define LPFN_INET_NTOP          LPFN_INET_NTOPA

#endif


#if defined(_WIN32)
#if defined(MG_USE_MS_TYPEDEFS)

typedef LPFN_WSASOCKET                MG_LPFN_WSASOCKET;
typedef LPFN_WSAGETLASTERROR          MG_LPFN_WSAGETLASTERROR; 
typedef LPFN_WSASTARTUP               MG_LPFN_WSASTARTUP;
typedef LPFN_WSACLEANUP               MG_LPFN_WSACLEANUP;
typedef LPFN_WSARECV                  MG_LPFN_WSARECV;
typedef LPFN_WSASEND                  MG_LPFN_WSASEND;

#if defined(NETX_IPV6)
typedef LPFN_WSASTRINGTOADDRESS       MG_LPFN_WSASTRINGTOADDRESS;
typedef LPFN_WSAADDRESSTOSTRING       MG_LPFN_WSAADDRESSTOSTRING;
typedef LPFN_GETADDRINFO              MG_LPFN_GETADDRINFO;
typedef LPFN_FREEADDRINFO             MG_LPFN_FREEADDRINFO;
typedef LPFN_GETNAMEINFO              MG_LPFN_GETNAMEINFO;
typedef LPFN_GETPEERNAME              MG_LPFN_GETPEERNAME;
typedef LPFN_INET_NTOP                MG_LPFN_INET_NTOP;
typedef LPFN_INET_PTON                MG_LPFN_INET_PTON;
#endif

typedef LPFN_CLOSESOCKET              MG_LPFN_CLOSESOCKET;
typedef LPFN_GETHOSTNAME              MG_LPFN_GETHOSTNAME;
typedef LPFN_GETHOSTBYNAME            MG_LPFN_GETHOSTBYNAME;
typedef LPFN_GETHOSTBYADDR            MG_LPFN_GETHOSTBYADDR;
typedef LPFN_GETSERVBYNAME            MG_LPFN_GETSERVBYNAME;

typedef LPFN_HTONS                    MG_LPFN_HTONS;
typedef LPFN_HTONL                    MG_LPFN_HTONL;
typedef LPFN_NTOHL                    MG_LPFN_NTOHL;
typedef LPFN_NTOHS                    MG_LPFN_NTOHS;
typedef LPFN_CONNECT                  MG_LPFN_CONNECT;
typedef LPFN_INET_ADDR                MG_LPFN_INET_ADDR;
typedef LPFN_INET_NTOA                MG_LPFN_INET_NTOA;

typedef LPFN_SOCKET                   MG_LPFN_SOCKET;
typedef LPFN_SETSOCKOPT               MG_LPFN_SETSOCKOPT;
typedef LPFN_GETSOCKOPT               MG_LPFN_GETSOCKOPT;
typedef LPFN_GETSOCKNAME              MG_LPFN_GETSOCKNAME;
typedef LPFN_SELECT                   MG_LPFN_SELECT;
typedef LPFN_RECV                     MG_LPFN_RECV;
typedef LPFN_SEND                     MG_LPFN_SEND;
typedef LPFN_SHUTDOWN                 MG_LPFN_SHUTDOWN;
typedef LPFN_BIND                     MG_LPFN_BIND;
typedef LPFN_LISTEN                   MG_LPFN_LISTEN;
typedef LPFN_ACCEPT                   MG_LPFN_ACCEPT;

#else

typedef int                   (WSAAPI * MG_LPFN_WSASTARTUP)          (WORD wVersionRequested, LPWSADATA lpWSAData);
typedef int                   (WSAAPI * MG_LPFN_WSACLEANUP)          (void);
typedef int                   (WSAAPI * MG_LPFN_WSAGETLASTERROR)     (void);
typedef SOCKET                (WSAAPI * MG_LPFN_WSASOCKET)           (int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo, GROUP g, DWORD dwFlags);
typedef int                   (WSAAPI * MG_LPFN_WSARECV)             (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int                   (WSAAPI * MG_LPFN_WSASEND)             (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef INT                   (WSAAPI * MG_LPFN_WSASTRINGTOADDRESS)  (LPSTR AddressString, INT AddressFamily, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSOCKADDR lpAddress, LPINT lpAddressLength);
typedef INT                   (WSAAPI * MG_LPFN_WSAADDRESSTOSTRING)  (LPSOCKADDR lpsaAddress, DWORD dwAddressLength, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSTR lpszAddressString, LPDWORD lpdwAddressStringLength);
typedef INT                   (WSAAPI * MG_LPFN_GETADDRINFO)         (PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA * pHints, PADDRINFOA * ppResult);
typedef VOID                  (WSAAPI * MG_LPFN_FREEADDRINFO)        (PADDRINFOA pAddrInfo);
typedef int                   (WSAAPI * MG_LPFN_GETNAMEINFO)         (const SOCKADDR * pSockaddr, socklen_t SockaddrLength, PCHAR pNodeBuffer, DWORD NodeBufferSize, PCHAR pServiceBuffer, DWORD ServiceBufferSize, INT Flags);
typedef int                   (WSAAPI * MG_LPFN_GETPEERNAME)         (SOCKET s, struct sockaddr FAR * name, int FAR * namelen);
typedef PCSTR                 (WSAAPI * MG_LPFN_INET_NTOP)           (INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
typedef INT                   (WSAAPI * MG_LPFN_INET_PTON)           (INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
typedef int                   (WSAAPI * MG_LPFN_CLOSESOCKET)         (SOCKET s);
typedef int                   (WSAAPI * MG_LPFN_GETHOSTNAME)         (char FAR * name, int namelen);
typedef struct hostent FAR *  (WSAAPI * MG_LPFN_GETHOSTBYNAME)       (const char FAR * name);
typedef struct hostent FAR *  (WSAAPI * MG_LPFN_GETHOSTBYADDR)       (const char FAR * addr, int len, int type);
typedef struct servent FAR *  (WSAAPI * MG_LPFN_GETSERVBYNAME)       (const char FAR * name, const char FAR * proto);
typedef u_short               (WSAAPI * MG_LPFN_HTONS)               (u_short hostshort);
typedef u_long                (WSAAPI * MG_LPFN_HTONL)               (u_long hostlong);
typedef u_long                (WSAAPI * MG_LPFN_NTOHL)               (u_long netlong);
typedef u_short               (WSAAPI * MG_LPFN_NTOHS)               (u_short netshort);
typedef char FAR *            (WSAAPI * MG_LPFN_INET_NTOA)           (struct in_addr in);
typedef int                   (WSAAPI * MG_LPFN_CONNECT)             (SOCKET s, const struct sockaddr FAR * name, int namelen);
typedef unsigned long         (WSAAPI * MG_LPFN_INET_ADDR)           (const char FAR * cp);
typedef SOCKET                (WSAAPI * MG_LPFN_SOCKET)              (int af, int type, int protocol);
typedef int                   (WSAAPI * MG_LPFN_SETSOCKOPT)          (SOCKET s, int level, int optname, const char FAR * optval, int optlen);
typedef int                   (WSAAPI * MG_LPFN_GETSOCKOPT)          (SOCKET s, int level, int optname, char FAR * optval, int FAR * optlen);
typedef int                   (WSAAPI * MG_LPFN_GETSOCKNAME)         (SOCKET s, struct sockaddr FAR * name, int FAR * namelen);
typedef int                   (WSAAPI * MG_LPFN_SELECT)              (int nfds, fd_set FAR * readfds, fd_set FAR * writefds, fd_set FAR *exceptfds, const struct timeval FAR * timeout);
typedef int                   (WSAAPI * MG_LPFN_RECV)                (SOCKET s, char FAR * buf, int len, int flags);
typedef int                   (WSAAPI * MG_LPFN_SEND)                (SOCKET s, const char FAR * buf, int len, int flags);
typedef int                   (WSAAPI * MG_LPFN_SHUTDOWN)            (SOCKET s, int how);
typedef int                   (WSAAPI * MG_LPFN_BIND)                (SOCKET s, const struct sockaddr FAR * name, int namelen);
typedef int                   (WSAAPI * MG_LPFN_LISTEN)              (SOCKET s, int backlog);
typedef SOCKET                (WSAAPI * MG_LPFN_ACCEPT)              (SOCKET s, struct sockaddr FAR * addr, int FAR * addrlen);

#endif
#endif /* #if defined(_WIN32) */

typedef struct tagNETXSOCK {

   unsigned char                 winsock_ready;
   short                         sock;
   short                         load_attempted;
   short                         nagle_algorithm;
   short                         winsock;
   short                         ipv6;
   DBXPLIB                       plibrary;

   char                          libnam[256];

#if defined(_WIN32)
   WSADATA                       wsadata;
   int                           wsastartup;
   WORD                          version_requested;
   MG_LPFN_WSASOCKET             p_WSASocket;
   MG_LPFN_WSAGETLASTERROR       p_WSAGetLastError; 
   MG_LPFN_WSASTARTUP            p_WSAStartup;
   MG_LPFN_WSACLEANUP            p_WSACleanup;
   MG_LPFN_WSAFDISSET            p_WSAFDIsSet;
   MG_LPFN_WSARECV               p_WSARecv;
   MG_LPFN_WSASEND               p_WSASend;

#if defined(NETX_IPV6)
   MG_LPFN_WSASTRINGTOADDRESS    p_WSAStringToAddress;
   MG_LPFN_WSAADDRESSTOSTRING    p_WSAAddressToString;
   MG_LPFN_GETADDRINFO           p_getaddrinfo;
   MG_LPFN_FREEADDRINFO          p_freeaddrinfo;
   MG_LPFN_GETNAMEINFO           p_getnameinfo;
   MG_LPFN_GETPEERNAME           p_getpeername;
   MG_LPFN_INET_NTOP             p_inet_ntop;
   MG_LPFN_INET_PTON             p_inet_pton;
#else
   LPVOID                        p_WSAStringToAddress;
   LPVOID                        p_WSAAddressToString;
   LPVOID                        p_getaddrinfo;
   LPVOID                        p_freeaddrinfo;
   LPVOID                        p_getnameinfo;
   LPVOID                        p_getpeername;
   LPVOID                        p_inet_ntop;
   LPVOID                        p_inet_pton;
#endif

   MG_LPFN_CLOSESOCKET           p_closesocket;
   MG_LPFN_GETHOSTNAME           p_gethostname;
   MG_LPFN_GETHOSTBYNAME         p_gethostbyname;
   MG_LPFN_GETHOSTBYADDR         p_gethostbyaddr;
   MG_LPFN_GETSERVBYNAME         p_getservbyname;

   MG_LPFN_HTONS                 p_htons;
   MG_LPFN_HTONL                 p_htonl;
   MG_LPFN_NTOHL                 p_ntohl;
   MG_LPFN_NTOHS                 p_ntohs;
   MG_LPFN_CONNECT               p_connect;
   MG_LPFN_INET_ADDR             p_inet_addr;
   MG_LPFN_INET_NTOA             p_inet_ntoa;

   MG_LPFN_SOCKET                p_socket;
   MG_LPFN_SETSOCKOPT            p_setsockopt;
   MG_LPFN_GETSOCKOPT            p_getsockopt;
   MG_LPFN_GETSOCKNAME           p_getsockname;
   MG_LPFN_SELECT                p_select;
   MG_LPFN_RECV                  p_recv;
   MG_LPFN_SEND                  p_send;
   MG_LPFN_SHUTDOWN              p_shutdown;
   MG_LPFN_BIND                  p_bind;
   MG_LPFN_LISTEN                p_listen;
   MG_LPFN_ACCEPT                p_accept;
#endif /* #if defined(_WIN32) */

} NETXSOCK, *PNETXSOCK;


typedef struct tagDBXLOG {
   char log_file[128];
   char log_level[8];
   char log_filter[64];
   short log_errors;
   short log_functions;
   short log_transmissions;
   unsigned long req_no;
   unsigned long fun_no;
   char product[4];
   char product_version[16];
} DBXLOG, *PDBXLOG;


typedef struct tagDBXZV {
   unsigned char  product;
   double         mg_version;
   int            majorversion;
   int            minorversion;
   int            mg_build;
   unsigned long  vnumber; /* yymbbbb */
   char           version[64];
} DBXZV, *PDBXZV;


typedef struct tagDBXMUTEX {
   unsigned char     created;
   int               stack;
#if defined(_WIN32)
   HANDLE            h_mutex;
#else
   pthread_mutex_t   h_mutex;
#endif /* #if defined(_WIN32) */
   DBXTHID           thid;
} DBXMUTEX, *PDBXMUTEX;


typedef struct tagDBXCVAL {
   void           *pstr;
   CACHE_EXSTR    zstr;
} DBXCVAL, *PDBXCVAL;


typedef struct tagDBXVAL {
   short          type;
   short          sort; /* v1.3.16 */
   short          realloc;
   union {
      int            int32;
      long long      int64;
      double         real;
      unsigned int   oref;
   } num;
   unsigned long  offset;
   DBXSTR         svalue;
   DBXCVAL cvalue;
} DBXVAL, *PDBXVAL;


typedef struct tagDBXFUN {
   unsigned int   rflag;
   int            label_len;
   char *         label;
   int            routine_len;
   char *         routine;
   char           buffer[128];
   /* v1.2.9 */
   int            argc;
   ydb_string_t   in[DBX_MAXARGS];
   ydb_string_t   out;
   ydb_buffer_t * global;
   int            in_nkeys;
   ydb_buffer_t * in_keys;
   int          * out_nkeys;
   ydb_buffer_t * out_keys;
   int            getdata;
   ydb_buffer_t * data;
   int            dir;
   int            rc;
} DBXFUN, *PDBXFUN;


typedef struct tagDBXISCSO {

   short             loaded;
   short             iris;
   short             merge_enabled;
   char              funprfx[8];
   char              libdir[256];
   char              libnam[256];
   char              dbname[32];
   DBXPLIB           p_library;

   int               (* p_CacheSetDir)                   (char * dir);
   int               (* p_CacheSecureStartA)             (CACHE_ASTRP username, CACHE_ASTRP password, CACHE_ASTRP exename, unsigned long flags, int tout, CACHE_ASTRP prinp, CACHE_ASTRP prout);
   int               (* p_CacheEnd)                      (void);

   unsigned char *   (* p_CacheExStrNew)                 (CACHE_EXSTRP zstr, int size);
   unsigned short *  (* p_CacheExStrNewW)                (CACHE_EXSTRP zstr, int size);
   wchar_t *         (* p_CacheExStrNewH)                (CACHE_EXSTRP zstr, int size);
   int               (* p_CachePushExStr)                (CACHE_EXSTRP sptr);
   int               (* p_CachePushExStrW)               (CACHE_EXSTRP sptr);
   int               (* p_CachePushExStrH)               (CACHE_EXSTRP sptr);
   int               (* p_CachePopExStr)                 (CACHE_EXSTRP sstrp);
   int               (* p_CachePopExStrW)                (CACHE_EXSTRP sstrp);
   int               (* p_CachePopExStrH)                (CACHE_EXSTRP sstrp);
   int               (* p_CacheExStrKill)                (CACHE_EXSTRP obj);
   int               (* p_CachePushStr)                  (int len, Callin_char_t * ptr);
   int               (* p_CachePushStrW)                 (int len, short * ptr);
   int               (* p_CachePushStrH)                 (int len, wchar_t * ptr);
   int               (* p_CachePopStr)                   (int * lenp, Callin_char_t ** strp);
   int               (* p_CachePopStrW)                  (int * lenp, short ** strp);
   int               (* p_CachePopStrH)                  (int * lenp, wchar_t ** strp);
   int               (* p_CachePushDbl)                  (double num);
   int               (* p_CachePushIEEEDbl)              (double num);
   int               (* p_CachePopDbl)                   (double * nump);
   int               (* p_CachePushInt)                  (int num);
   int               (* p_CachePopInt)                   (int * nump);
   int               (* p_CachePushInt64)                (CACHE_INT64 num);
   int               (* p_CachePopInt64)                 (CACHE_INT64 * nump);

   int               (* p_CachePushGlobal)               (int nlen, const Callin_char_t * nptr);
   int               (* p_CachePushGlobalX)              (int nlen, const Callin_char_t * nptr, int elen, const Callin_char_t * eptr);
   int               (* p_CacheGlobalGet)                (int narg, int flag);
   int               (* p_CacheGlobalSet)                (int narg);
   int               (* p_CacheGlobalData)               (int narg, int valueflag);
   int               (* p_CacheGlobalKill)               (int narg, int nodeonly);
   int               (* p_CacheGlobalOrder)              (int narg, int dir, int valueflag);
   int               (* p_CacheGlobalQuery)              (int narg, int dir, int valueflag);
   int               (* p_CacheGlobalIncrement)          (int narg);
   int               (* p_CacheGlobalRelease)            (void);

   int               (* p_CacheAcquireLock)              (int nsub, int flg, int tout, int * rval);
   int               (* p_CacheReleaseAllLocks)          (void);
   int               (* p_CacheReleaseLock)              (int nsub, int flg);
   int               (* p_CachePushLock)                 (int nlen, const Callin_char_t * nptr);

   int               (* p_CacheAddGlobal)                (int num, const Callin_char_t * nptr);
   int               (* p_CacheAddGlobalDescriptor)      (int num);
   int               (* p_CacheAddSSVN)                  (int num, const Callin_char_t * nptr);
   int               (* p_CacheAddSSVNDescriptor)        (int num);
   int               (* p_CacheMerge)                    (void);

   int               (* p_CachePushFunc)                 (unsigned int * rflag, int tlen, const Callin_char_t * tptr, int nlen, const Callin_char_t * nptr);
   int               (* p_CacheExtFun)                   (unsigned int flags, int narg);
   int               (* p_CachePushRtn)                  (unsigned int * rflag, int tlen, const Callin_char_t * tptr, int nlen, const Callin_char_t * nptr);
   int               (* p_CacheDoFun)                    (unsigned int flags, int narg);
   int               (* p_CacheDoRtn)                    (unsigned int flags, int narg);

   int               (* p_CacheCloseOref)                (unsigned int oref);
   int               (* p_CacheIncrementCountOref)       (unsigned int oref);
   int               (* p_CachePopOref)                  (unsigned int * orefp);
   int               (* p_CachePushOref)                 (unsigned int oref);
   int               (* p_CacheInvokeMethod)             (int narg);
   int               (* p_CachePushMethod)               (unsigned int oref, int mlen, const Callin_char_t * mptr, int flg);
   int               (* p_CacheInvokeClassMethod)        (int narg);
   int               (* p_CachePushClassMethod)          (int clen, const Callin_char_t * cptr, int mlen, const Callin_char_t * mptr, int flg);
   int               (* p_CacheGetProperty)              (void);
   int               (* p_CacheSetProperty)              (void);
   int               (* p_CachePushProperty)             (unsigned int oref, int plen, const Callin_char_t * pptr);

   int               (* p_CacheType)                     (void);

   int               (* p_CacheEvalA)                    (CACHE_ASTRP volatile expr);
   int               (* p_CacheExecuteA)                 (CACHE_ASTRP volatile cmd);
   int               (* p_CacheConvert)                  (unsigned long type, void * rbuf);

   int               (* p_CacheErrorA)                   (CACHE_ASTRP, CACHE_ASTRP, int *);
   int               (* p_CacheErrxlateA)                (int, CACHE_ASTRP);

   int               (* p_CacheEnableMultiThread)        (void);

   int               (* p_CacheTStart)                   (void);
   int               (* p_CacheTLevel)                   (void);
   int               (* p_CacheTCommit)                  (void);
   int               (* p_CacheTRollback)                (int nlev);

} DBXISCSO, *PDBXISCSO;


typedef struct tagDBXYDBSO {
   short             loaded;
   char              libdir[256];
   char              libnam[256];
   char              funprfx[8];
   char              dbname[32];
   DBXPLIB           p_library;

   int               (* p_ydb_init)                      (void);
   int               (* p_ydb_exit)                      (void);
   int               (* p_ydb_malloc)                    (size_t size);
   int               (* p_ydb_free)                      (void *ptr);
   int               (* p_ydb_data_s)                    (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, unsigned int *ret_value);
   int               (* p_ydb_delete_s)                  (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int deltype);
   int               (* p_ydb_set_s)                     (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *value);
   int               (* p_ydb_get_s)                     (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_subscript_next_s)          (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_subscript_previous_s)      (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
   int               (* p_ydb_node_next_s)               (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
   int               (* p_ydb_node_previous_s)           (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used, ydb_buffer_t *ret_subsarray);
   int               (* p_ydb_incr_s)                    (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *increment, ydb_buffer_t *ret_value);
   int               (* p_ydb_ci)                        (const char *c_rtn_name, ...);
   int               (* p_ydb_cip)                       (ci_name_descriptor *ci_info, ...);
   int               (* p_ydb_lock_incr_s)               (unsigned long long timeout_nsec, ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray);
   int               (* p_ydb_lock_decr_s)               (ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray);
   void              (* p_ydb_zstatus)                   (ydb_char_t* msg_buffer, ydb_long_t buf_len);
   int               (* p_ydb_tp_s)                      (ydb_tpfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, ydb_buffer_t *varnames);

} DBXYDBSO, *PDBXYDBSO;


typedef struct tagDBXGTMSO {
   short             loaded;
   char              libdir[256];
   char              libnam[256];
   char              funprfx[8];
   char              dbname[32];
   DBXPLIB           p_library;
   xc_status_t       (* p_gtm_ci)       (const char *c_rtn_name, ...);
   xc_status_t       (* p_gtm_init)     (void);
   xc_status_t       (* p_gtm_exit)     (void);
   void              (* p_gtm_zstatus)  (char* msg, int len);
} DBXGTMSO, *PDBXGTMSO;


typedef struct tagDBXCON {
   short          dbtype;
   unsigned long  pid;
   char           shdir[256];
   char           username[64];
   char           password[64];
   char           nspace[64];
   char           input_device[64];
   char           output_device[64];
   char           debug_str[64];
   int            error_code;
   char           error[DBX_ERROR_SIZE];
   short          use_db_mutex;
   DBXMUTEX       *p_db_mutex;
   DBXMUTEX       db_mutex;
   DBXZV          *p_zv;
   DBXZV          zv;
   DBXLOG         *p_log;
   DBXLOG         log;
   DBXISCSO       *p_isc_so;
   DBXYDBSO       *p_ydb_so;
   DBXGTMSO       *p_gtm_so;

   short          connected;
   int            port;
   char           ip_address[128];
   int            error_no;
   int            timeout;
   int            eof;
   SOCKET         cli_socket;
   char           info[256];

   /* v1.2.9 */
   void           *pmeth_base;
   int            tlevel;
   void *         pthrt[YDB_MAX_TP];

   /* Old MGWSI protocol */

   short          eod;
   short          keep_alive;
   short          in_use;
   int            chndle;
   int            base_port;
   int            child_port;
   char           mpid[128];
   char           server[64];
   char           server_software[64];
   char           zmgsi_version[8];
   void *         p_srv;

} DBXCON, *PDBXCON;


/* v1.2.9 */
typedef struct tagDBXMETH {
   short          done;
   short          lock;
   short          increment;
   short          merge;
   short          getdata; /* v1.3.13 */
   int            binary;
   int            argc;
   DBXSTR         input_str;
   DBXVAL         output_val;
   int            offset;
   DBXVAL         args[DBX_MAXARGS];
   ydb_buffer_t   yargs[DBX_MAXARGS];
   char           command[4];
   int            (* p_dbxfun) (struct tagDBXMETH * pmeth);
   DBXCON         *pcon;
   DBXFUN         *pfun;
} DBXMETH, *PDBXMETH;


/* v1.2.9 */
typedef struct tagDBXTHRT {
   int               context;
   int               done;
#if !defined(_WIN32)
   pthread_t         parent_tid;
   pthread_t         tp_tid;
   pthread_mutex_t   req_cv_mutex;
   pthread_cond_t    req_cv;
   pthread_mutex_t   res_cv_mutex;
   pthread_cond_t    res_cv;
#endif
   int               task_id;
   DBXMETH           *pmeth;
} DBXTHRT, *PDBXTHRT;


#define MG_HOST                  "127.0.0.1"
#if defined(MG_DEFAULT_PORT)
#define MG_PORT                  MG_DEFAULT_PORT
#else
#define MG_PORT                  7041
#endif
#define MG_SERVER                "LOCAL"
#define MG_UCI                   "USER"

#if defined(MG_DBA_DSO)
#define MG_EXT_NAME              "mg_dba"
#ifdef _WIN32
#define MG_LOG_FILE              "c:/temp/" MG_EXT_NAME ".log"
#else
#define MG_LOG_FILE              "/tmp/" MG_EXT_NAME ".log"
#endif
#define MG_PRODUCT               "g"
#endif

#define MG_MAXCON                32

#define MG_TX_DATA               0
#define MG_TX_AKEY               1
#define MG_TX_AREC               2
#define MG_TX_EOD                3
#define MG_TX_OREF               5
#define MG_TX_AREC_FORMATTED     9

#define MG_RECV_HEAD             8

#define MG_CHUNK_SIZE_BASE       62

#define MG_BUFSIZE               32768
#define MG_BUFMAX                32767

#define MG_ES_DELIM              0
#define MG_ES_BLOCK              1

typedef struct tagMGBUF {
   unsigned long     size;
   unsigned long     data_size;
   unsigned long     increment_size;
   unsigned char *   p_buffer;
} MGBUF, *LPMGBUF;

typedef struct tagMGSTR {
   unsigned int      size;
   unsigned char *   ps;
} MGSTR, *LPMGSTR;

typedef struct tagMGSRV {
   short       mem_error;
   short       storage_mode;
   short       mode;
   int         error_mode;
   int         header_len;
   int         error_no;
   char        error_code[128];
   char        error_mess[256];
   char        info[256];
   char        server[64];
   char        uci[128];
   char        shdir[256];
   char        ip_address[64];
   int         port;
   int         timeout;
   int         no_retry;
   int         nagle_algorithm;
   char        username[64];
   char        password[256];
   char        dbtype_name[32];
   char        product[4];
   MGBUF *     p_env;
   MGBUF *     p_params;
   DBXLOG *    p_log;
   PDBXCON     pcon[MG_MAXCON];
} MGSRV, *LPMGSRV;


#if defined(_WIN32)
extern CRITICAL_SECTION  dbx_global_mutex;
#else
extern pthread_mutex_t   dbx_global_mutex;
#endif

extern MG_MALLOC     dbx_ext_malloc;
extern MG_REALLOC    dbx_ext_realloc;
extern MG_FREE       dbx_ext_free;

DBX_EXTFUN(int)         dbx_init                      ();
DBX_EXTFUN(int)         dbx_version                   (int index, char *output, int output_len);
DBX_EXTFUN(int)         dbx_open                      (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_close                     (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_set                       (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_set_x                     (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_set_ex                    (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_get                       (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_get_x                     (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_get_ex                    (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_next                      (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_next_x                    (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_next_data                 (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_next_data_x               (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_next_ex                   (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_previous                  (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_previous_x                (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_previous_data             (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_previous_data_x           (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_previous_ex               (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_delete                    (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_delete_x                  (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_delete_ex                 (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_defined                   (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_defined_x                 (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_defined_ex                (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_increment                 (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_increment_x               (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_increment_ex              (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_merge                     (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_merge_x                   (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_merge_ex                  (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_lock                      (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_lock_x                    (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_lock_ex                   (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_unlock                    (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_unlock_x                  (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_unlock_ex                 (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_tstart                    (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_tstart_ex                 (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_tlevel                    (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_tlevel_ex                 (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_tcommit                   (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_tcommit_ex                (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_trollback                 (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_trollback_ex              (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_function                  (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_function_x                (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_function_ex               (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_classmethod               (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_classmethod_x             (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_method                    (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_method_x                  (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_getproperty               (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_getproperty_x             (DBXMETH *pmeth);
DBX_EXTFUN(int)         dbx_setproperty               (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_closeinstance             (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_getnamespace              (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_setnamespace              (unsigned char *input, unsigned char *output);
DBX_EXTFUN(int)         dbx_sleep                     (int period_ms);
DBX_EXTFUN(int)         dbx_benchmark                 (unsigned char *inputstr, unsigned char *outputstr);

int                     isc_load_library              (DBXCON *pcon);
int                     isc_authenticate              (DBXCON *pcon);
int                     isc_open                      (DBXMETH *pmeth);
int                     isc_parse_zv                  (char *zv, DBXZV * p_isc_sv);
int                     isc_change_namespace          (DBXCON *pcon, char *nspace);
int                     isc_pop_value                 (DBXCON *pcon, DBXVAL *value, int required_type);
int                     isc_error_message             (DBXMETH *pmeth, int error_code);

int                     ydb_load_library              (DBXCON *pcon);
int                     ydb_open                      (DBXMETH *pmeth);
int                     ydb_parse_zv                  (char *zv, DBXZV * p_ydb_sv);
int                     ydb_get_intsvar               (DBXCON *pcon, char *svarname);
int                     ydb_error_message             (DBXMETH *pmeth, int error_code);
int                     ydb_function                  (DBXMETH *pmeth, DBXFUN *pfun);
int                     ydb_function_ex               (DBXMETH *pmeth, DBXFUN *pfun);
int                     ydb_transaction_cb            (void *pargs);
#if defined(_WIN32)
LPTHREAD_START_ROUTINE  ydb_transaction_thread        (LPVOID pargs);
#else
void *                  ydb_transaction_thread        (void *pargs);
#endif
int                     ydb_transaction               (DBXMETH *pmeth);
int                     ydb_transaction_task          (DBXMETH *pmeth, int context);

int                     gtm_load_library              (DBXCON *pcon);
int                     gtm_open                      (DBXMETH *pmeth);
int                     gtm_parse_zv                  (char *zv, DBXZV * p_gtm_sv);
int                     gtm_error_message             (DBXMETH *pmeth, int error_code);

DBXMETH *               mg_unpack_header              (unsigned char *input, unsigned char *output);
int                     mg_unpack_arguments           (DBXMETH *pmeth);
int                     mg_global_reference           (DBXMETH *pmeth);
int                     mg_function_reference         (DBXMETH *pmeth, DBXFUN *pfun);
int                     mg_class_reference            (DBXMETH *pmeth, short context);

int                     mg_add_block_head             (DBXSTR *block, unsigned long buffer_size, unsigned long index);
int                     mg_add_block_head_size        (DBXSTR *block, unsigned long data_len, int cmnd);
int                     mg_add_block_data             (DBXSTR *block, unsigned char *data, unsigned long data_len, int dsort, int dtype);
int                     mg_add_block_size             (DBXSTR *block, unsigned long offset, unsigned long data_len, int dsort, int dtype);
unsigned long           mg_get_block_size             (DBXSTR *block, unsigned long offset, int *dsort, int *dtype);
int                     mg_set_size                   (unsigned char *str, unsigned long data_len);
unsigned long           mg_get_size                   (unsigned char *str);

int                     mg_buf_init                   (MGBUF *p_buf, int size, int increment_size);
int                     mg_buf_resize                 (MGBUF *p_buf, unsigned long size);
int                     mg_buf_free                   (MGBUF *p_buf);
int                     mg_buf_cpy                    (MGBUF *p_buf, char * buffer, unsigned long size);
int                     mg_buf_cat                    (MGBUF *p_buf, char * buffer, unsigned long size);

void *                  mg_realloc                    (void *p, int curr_size, int new_size, short id);
void *                  mg_malloc                     (int size, short id);
int                     mg_free                       (void *p, short id);

int                     mg_ucase                      (char *string);
int                     mg_lcase                      (char *string);
int                     mg_create_string              (DBXMETH *pmeth, void *data, short type);
int                     mg_log_init                   (DBXLOG *p_log);
int                     mg_log_event                  (DBXLOG *p_log, char *message, char *title, int level);
int                     mg_log_buffer                 (DBXLOG *p_log, char *buffer, int buffer_len, char *title, int level);
int                     mg_pause                      (int msecs);
DBXPLIB                 mg_dso_load                   (char *library);
DBXPROC                 mg_dso_sym                    (DBXPLIB p_library, char *symbol);
int                     mg_dso_unload                 (DBXPLIB p_library);
DBXTHID                 mg_current_thread_id          (void);
unsigned long           mg_current_process_id         (void);
int                     mg_error_message              (DBXMETH *pmeth, int error_code);
int                     mg_set_error_message          (DBXMETH *pmeth);
int                     mg_set_error_message_ex       (unsigned char *output, char *error_message);
int                     mg_cleanup                    (DBXMETH *pmeth);

int                     mg_mutex_create               (DBXMUTEX *p_mutex);
int                     mg_mutex_lock                 (DBXMUTEX *p_mutex, int timeout);
int                     mg_mutex_unlock               (DBXMUTEX *p_mutex);
int                     mg_mutex_destroy              (DBXMUTEX *p_mutex);
int                     mg_init_critical_section      (void *p_crit);
int                     mg_delete_critical_section    (void *p_crit);
int                     mg_enter_critical_section     (void *p_crit);
int                     mg_leave_critical_section     (void *p_crit);

int                     mg_sleep                      (unsigned long msecs);

int                     netx_load_winsock             (DBXCON *pcon, int context);
int                     netx_tcp_connect              (DBXCON *pcon, int context);
int                     netx_tcp_handshake            (DBXCON *pcon, int context);
int                     netx_tcp_command              (DBXMETH *pmeth, int context);
int                     netx_tcp_connect_ex           (DBXCON *pcon, xLPSOCKADDR p_srv_addr, socklen_netx srv_addr_len, int timeout);
int                     netx_tcp_disconnect           (DBXCON *pcon, int context);
int                     netx_tcp_write                (DBXCON *pcon, unsigned char *data, int size);
int                     netx_tcp_read                 (DBXCON *pcon, unsigned char *data, int size, int timeout, int context);
int                     netx_get_last_error           (int context);
int                     netx_get_error_message        (int error_code, char *message, int size, int context);
int                     netx_get_std_error_message    (int error_code, char *message, int size, int context);


int                     mg_db_command                 (DBXMETH *pmeth, int context);
int                     mg_db_connect                 (MGSRV *p_srv, int *chndle, short context);
int                     mg_db_disconnect              (MGSRV *p_srv, int chndle, short context);
int                     mg_db_send                    (MGSRV *p_srv, int chndle, MGBUF *p_buf, int mode);
int                     mg_db_receive                 (MGSRV *p_srv, int chndle, MGBUF *p_buf, int size, int mode);
int                     mg_db_connect_init            (MGSRV *p_srv, int chndle);
int                     mg_db_ayt                     (MGSRV *p_srv, int chndle);
int                     mg_db_get_last_error          (int context);

int                     mg_request_header             (MGSRV *p_srv, MGBUF *p_buf, char *command, char *product);
int                     mg_request_add                (MGSRV *p_srv, int chndle, MGBUF *p_buf, unsigned char *element, int size, short byref, short type);

int                     mg_encode_size64              (int n10);
int                     mg_decode_size64              (int nxx);
int                     mg_encode_size                (unsigned char *esize, int size, short base);
int                     mg_decode_size                (unsigned char *esize, int len, short base);
int                     mg_encode_item_header         (unsigned char * head, int size, short byref, short type);
int                     mg_decode_item_header         (unsigned char * head, int * size, short * byref, short * type);
int                     mg_get_error                  (MGSRV *p_srv, char *buffer);

int                     mg_extract_substrings         (MGSTR * records, char* buffer, int tsize, char delim, int offset, int no_tail, short type);
int                     mg_compare_keys               (MGSTR * key, MGSTR * rkey, int max);
int                     mg_replace_substrings         (char * tbuffer, char *fbuffer, char * replace, char * with);

int                     mg_bind_server_api            (MGSRV *p_srv, short context);
int                     mg_release_server_api         (MGSRV *p_srv, short context);
int                     mg_invoke_server_api          (MGSRV *p_srv, int chndle, MGBUF *p_buf, int size, int mode);

#ifdef __cplusplus
}
#endif

#endif


