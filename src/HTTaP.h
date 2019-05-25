/* HTTaP.h
   (C) 2011,2017 Yann Guidon whygee@f-cpu.org
   The source code of this project is released under the AGPLv3 or later,
   see https://www.gnu.org/licenses/agpl-3.0.en.html

version 20170327: split from serv_11_CORS.c, a lot of renaming
version 20170331: more options and some standard error messages

This file includes everything and lets you configure some parameters.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

/*
 Some default values and the names of the
 environment variables that change them
 */

/* Uncomment to enable debug messages:
#define HTTAP_VERBOSE */

/* Uncomment to display the IP address of the client
#define SHOW_CLIENT */

/* Uncomment to enable the loopback server
#define ENABLE_LOOPBACK */

#define HTTAP_SERVER_ID     "test_HTTaP"

#define HTTAP_VERSION       "20170331"

#define HTTAP_TCPPORT         "60000"
#define HTTAP_TCPPORT_NAME    "HTTAP_TCPPORT"

#define HTTAP_KEEPALIVE  15   /* timeout in seconds */
#define HTTAP_KEEPALIVE_NAME  "HTTAP_KEEPALIVE"


#ifdef HTTAP_SERVE_FILES
  /* Path to the sandbox to chroot(): */
  #define HTTAP_STATICPATH_NAME "HTTAP_STATICPATH"
  /* Default name of the root page */
  #define HTTAP_ROOTPAGE        "index.html"
  #define HTTAP_ROOTPAGE_NAME   "HTTAP_ROOTPAGE"
  /* New user name */
  #define HTTAP_USER_NAME       "HTTAP_USER"
  /* New group name */
  #define HTTAP_GROUP_NAME      "HTTAP_GROUP"
#endif

#ifndef  ACCESS_CONTROL_ALLOW_ORIGIN
  #ifndef HTTAP_SERVE_FILES
    #define ACCESS_CONTROL_ALLOW_ORIGIN "Access-Control-Allow-Origin: *\x0d\x0a"
  #else
    #define ACCESS_CONTROL_ALLOW_ORIGIN
  #endif
#endif

char *HTTaP_header="HTTP/1.1 200 OK\x0d\x0a"
ACCESS_CONTROL_ALLOW_ORIGIN
"Connection: \x0d\x0a"
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Expires: 0\x0d\x0a"
"Content-Type: application/json";

char *HTTaP_json_root="{\n"
" HTTaP_version:" HTTAP_VERSION ",\n"
#ifdef HTTAP_SERVER_ID
" ID:\"" HTTAP_SERVER_ID "\",\n"
#endif
" Services:\""
#ifdef ENABLE_LOOPBACK
"Loopback "
#endif
#ifdef HTTAP_SERVE_FILES
"Files "
#endif
"\"\n"
"}";

char *HelloHTTaP="HTTP/1.1 200 OK\x0d\x0a"
ACCESS_CONTROL_ALLOW_ORIGIN
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Expires: 0\x0d\x0a"
"Content-Type: text/html\x0d\x0a"
"Content-Length: 38\x0d\x0a"
"\x0d\x0a"
"<html><body>Hello HTTaP!</body></html>";

/* A few minimalist error pages: */
char *err400="HTTP/1.1 400 Invalid\x0d\x0a"
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Content-Type: text/html\x0d\x0a"
"Content-Length: 37\x0d\x0a"
"\x0d\x0a"
"<html><body>Bad request</body></html>";

#ifdef HTTAP_SERVE_FILES
char *err403="HTTP/1.1 403 Forbidden\x0d\x0a"
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Content-Type: text/html\x0d\x0a"
"Content-Length: 35\x0d\x0a"
"\x0d\x0a"
"<html><body>Forbidden</body></html>";

char *err404="HTTP/1.1 404 Not Found\x0d\x0a"
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Content-Type: text/html\x0d\x0a"
"Content-Length: 35\x0d\x0a"
"\x0d\x0a"
"<html><body>Not Found</body></html>";

char *err500="HTTP/1.1 500 Internal Server Error\x0d\x0a"
"Cache-Control: no-cache, no-store, must-revalidate\x0d\x0a"
"Content-Type: text/html\x0d\x0a"
"Content-Length: 41\x0d\x0a"
"\x0d\x0a"
"<html><body>Internal error</body></html>";

char *ok200="HTTP/1.1 200 OK\x0d\x0a"
ACCESS_CONTROL_ALLOW_ORIGIN;

#endif

#define HTTAP_BUFFER_LEN (65536) /* size of the receive and emit buffer */
#define HEADER_BUFFER_LEN (1024) /* holds header informations from auxiliary files */
#define MAX_URI_LEN (256) /* let's be reasonable, such a long filename is not common */
#include "HTTaP.c"
