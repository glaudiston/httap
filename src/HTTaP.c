/* HTTaP.c
   (C) 2011,2017 Yann Guidon whygee@f-cpu.org
   The source code of this project is released under the AGPLv3 or later,
   see https://www.gnu.org/licenses/agpl-3.0.en.html

version 20170327: split from serv_11_CORS.c, a lot of renaming and translations
version 20170331: timeout and connection persistence work,
                  HTTaP and file serving appeared

This file is included by HTTaP.h, where you can configure the parameters.
 */

#define HTTAP_REQUESTS 1 /* Don't accept more than one connection.
  Changing this would create a mess... */

enum FSM_list_of_states {
  STATE0_initialise,
  STATE1_wait_connection,
  STATE2_wait_incoming_data
} server_state=STATE0_initialise;

/* persistent variables */
int sock_server,
    sock_client,
    keepalive,
    keepalive_counter,
    mode_polled,
    fcntl_flags;
const int one = 1, zero=0; /* required by setsockopt() */
struct sockaddr_storage sockaddr_client;
socklen_t size_client = sizeof(sockaddr_client);

#ifdef HTTAP_SERVE_FILES
struct stat stat_buf;
char *root_page;
uid_t prog_uid;
#endif

char header_buffer[HEADER_BUFFER_LEN],
            buffer[HTTAP_BUFFER_LEN];

/* A wrapper to prevent empty environment variables
   from being registered as valid */
void *my_getenv(char *s) {
  s=getenv(s);
  if (s && s[0])
    return s;
  return NULL;
}

/* Local error messages, with variations */
void HTTaP_erreur(char *msg) {
  fflush(NULL);
  perror(msg);
  exit(EXIT_FAILURE);
}

void HTTaP_err(char *msg) {
  fflush(NULL);
  printf(msg);
  exit(EXIT_FAILURE);
}

void HTTaP_polled() {
  mode_polled=MSG_DONTWAIT;
  fcntl(sock_server, F_SETFL, fcntl_flags | O_NONBLOCK);
}

void HTTaP_blocking() {
  mode_polled=0;
  fcntl(sock_server, F_SETFL, fcntl_flags);
}


void HTTaP_init() {
  char *env_port, *b;
  int flags, sockV4, sockV6;
  struct addrinfo hints, *list_adresses, *p_adress, *aiv4, *aiv6, *chosen_ai;

#ifdef HTTAP_SERVE_FILES
  char *env_user, *env_group;
  struct passwd *pswd=NULL;
  struct group *grp=NULL;
  char *static_path;
#endif

/* Configuration */

  /* Port number */
  env_port = my_getenv(HTTAP_TCPPORT_NAME);
  if (!env_port)
    env_port=HTTAP_TCPPORT;

  /* How long can a connection remain open ? */
  keepalive = HTTAP_KEEPALIVE;
  b = my_getenv(HTTAP_KEEPALIVE_NAME);
  if (b) {
    keepalive = atol(b);
    if ((keepalive < 3) || (keepalive > 200))
      HTTaP_err("$" HTTAP_KEEPALIVE_NAME " must be between 3 and 200\n");
  }
#ifdef HTTAP_VERBOSE
  printf("Port: %s, Keepalive: %ds\n", env_port, keepalive);
#endif

#ifdef HTTAP_SERVE_FILES

  /* Path to the directory that contains the static files */
  static_path = my_getenv(HTTAP_STATICPATH_NAME);
  if (!static_path)
    static_path = "."; /* The current working directory */

      /* Nom du fichier pour l'URI racine (habituellement index.html) */
  root_page = my_getenv(HTTAP_ROOTPAGE_NAME);
  if (!root_page)
    root_page = HTTAP_ROOTPAGE;

#ifdef HTTAP_VERBOSE
  printf("Path: %s  Root page: %s\n", static_path, root_page);
#endif

  /* Check if we change the user and group */
  env_user = my_getenv(HTTAP_USER_NAME);
  if (env_user) {
    errno = 0;
    pswd = getpwnam(env_user);
    if (!pswd)
      HTTaP_erreur("$" HTTAP_USER_NAME " is invalid ");

#ifdef HTTAP_VERBOSE
    printf("User: %s", env_user);
#endif
    prog_uid = pswd->pw_uid;

    env_group = my_getenv(HTTAP_GROUP_NAME);
    if (env_group) {
      errno = 0;
      grp = getgrnam(env_group);
      if (!grp)
        HTTaP_erreur("$" HTTAP_GROUP_NAME "is invalid ");

#ifdef HTTAP_VERBOSE
      printf(", Group: %s", env_group);
#endif
    }
  }
  else
    prog_uid = getuid();
#ifdef HTTAP_VERBOSE
  printf("\n");
#endif

  /* Check the path to access the static files */
  if (stat(static_path, &stat_buf)
  || !S_ISDIR(stat_buf.st_mode))
    HTTaP_erreur("$" HTTAP_STATICPATH_NAME " : invalid path");
  if (stat_buf.st_uid != prog_uid)
    HTTaP_err("The program's user is not owner of $" HTTAP_STATICPATH_NAME "\n");
  if (chdir(static_path))
    HTTaP_erreur("Failed to change directory to $" HTTAP_STATICPATH_NAME );
  if (chroot(static_path))
    perror("Warning: chroot() failure (are you root ?) ");

  /* Check the default root file */
  if (lstat(root_page, &stat_buf)
  || !S_ISREG(stat_buf.st_mode))
    HTTaP_erreur("$" HTTAP_ROOTPAGE_NAME " : invalid file");
  if (stat_buf.st_uid != prog_uid)
    HTTaP_err("The user doesn't own $" HTTAP_ROOTPAGE_NAME "\n");

  /* change the group THEN the user, after chroot() */
  if (pswd){ /* $HTTAP_USER exists */
    if (grp) {
      if (setgid(grp->gr_gid))
        HTTaP_erreur ("Failed to change running group ");
    }
    if (setuid(pswd->pw_uid))
      HTTaP_erreur ("Failed to change running user ");
  }
#endif

/*
   Open the socket
*/

  /* Describes the type of socket we want to open */
  memset(&hints, 0, sizeof(hints)); /* cleanup */
  hints.ai_family   = AF_UNSPEC;   /* Either IPv4 or 6, or both */
  hints.ai_socktype = SOCK_STREAM; /* TCP */
  hints.ai_flags    = AI_PASSIVE;  /* server mode */
  if ((flags=getaddrinfo(NULL, env_port, &hints, &list_adresses)) < 0) {
    printf("getaddrinfo: %s\n", gai_strerror(flags));
    exit(EXIT_FAILURE);
  }

  sockV4=-1; sockV6=-1;
  p_adress = list_adresses;
  /* scan the linked list of addresses and tries to open sockets */
  while (p_adress) {
    if (p_adress->ai_family == AF_INET6) { /* IPV6 */
      if (sockV6 < 0) {
        if ((sockV6 = socket(p_adress->ai_family,
            p_adress->ai_socktype, p_adress->ai_protocol)) >= 0) {
          aiv6 = p_adress;
#ifdef HTTAP_VERBOSE
          printf("IPv6 found, ");
#endif
        }
      }
    }
    else {
      if (p_adress->ai_family == AF_INET) { /* IPv4 */
        if (sockV4 < 0) {
          if ((sockV4 = socket(p_adress->ai_family,
              p_adress->ai_socktype, p_adress->ai_protocol)) >= 0) {
            aiv4 = p_adress;
#ifdef HTTAP_VERBOSE
            printf("IPv4 found, ");
#endif
          }
        }
      }  /* else : unknown protocol, try the next entry */
    }
    p_adress=p_adress->ai_next; /* proceed to the next entry */
  }

  /* select the socket */
  if (sockV6 >= 0) {
    chosen_ai = aiv6;
    sock_server = sockV6;
    /* Try to enable the IPv6->IPv4 translation */
    if (setsockopt(sockV6, IPPROTO_IPV6,
          IPV6_V6ONLY, &zero, sizeof(zero)) < 0) {
      perror("notice : setsockopt(IPV6_V6ONLY)");

      /* If the translation is not possible, try IPv4 */
      if (sockV4 >= 0) {
#ifdef HTTAP_VERBOSE
        printf("close(V6)");
#endif
        close(sockV6);
      }
    }
    else {    /* translation available : close IPv4 */
      if (sockV4 >= 0) {
#ifdef HTTAP_VERBOSE
        printf("v4 over v6 => close(v4)");
#endif
        close(sockV4);
        sockV4=-1;
      }
    }
  }

  /* no IPv4 or no IPv6 translation */
  if (sockV4 >= 0) {
    chosen_ai = aiv4;
    sock_server = sockV4;
  }
  else
    if (sockV6 < 0)
      HTTaP_err("No suitable network socket found");

#ifdef HTTAP_VERBOSE
  printf("\n");
#endif

#ifdef SO_REUSEADDR
  if (setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
    perror("setsockopt(SO_REUSEADDR)");
#endif

#ifdef SO_REUSEPORT
  if (setsockopt(sock_server, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0)
    perror("setsockopt(SO_REUSEPORT)");
#endif

  /* Connect the socket */
  if (bind(sock_server, chosen_ai->ai_addr, chosen_ai->ai_addrlen) < 0)
    HTTaP_erreur("Failed to bind() the server");

  /* Listen on the socket */
  if (listen(sock_server, HTTAP_REQUESTS) < 0)
    HTTaP_erreur("Failed to listen()");

  /* Unblock the socket */
  fcntl_flags = fcntl(sock_server, F_GETFL);
  HTTaP_polled();

  /* free the memory used by the linked list */
  freeaddrinfo(list_adresses);

  server_state=STATE1_wait_connection;
}

void HTTaP_server(int second) {
  int rfd, len, i, j, k, offset,
         recv_len, keep_server_open;
  char c, d, *b, *e, *fname;

  switch(server_state) {
/********************************************************/
    case STATE0_initialise :
      /* this code is so large, it is now
         moved to the separate function above,
         to keep the stack lean for most of the calls */
      HTTaP_init();

/********************************************************/
    case STATE1_wait_connection:
wait_connection:
      /* Is somebody wanting to connect ? */
      if ((sock_client = accept(sock_server,
         (struct sockaddr *) &sockaddr_client, &size_client)) < 0) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
          return;
        else
          HTTaP_erreur("accept(): Failure to wait for a network connection");
      }

      /* Disable Nagle */
      if (setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
        HTTaP_erreur("setsockopt(TCP_NODELAY) / Can't disable Nagle ");

#ifdef SHOW_CLIENT
      getnameinfo((struct sockaddr *) &sockaddr_client, size_client,
         buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST);
      b=buffer;
      if (memcmp("::ffff:",buffer,7)==0)
        b+=7; // skip the IPv6 prefix
      printf("Connection from %s\n",b);
#endif
      keepalive_counter = 3; // A HTTP command
       // must arrive immediately after connexion is open
      server_state=STATE2_wait_incoming_data;

/********************************************************/
    case STATE2_wait_incoming_data :
wait_data:
      /* finally, something to actually do */
      if ( (recv_len=recv(sock_client, buffer, HTTAP_BUFFER_LEN, mode_polled))
              < 0) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
// Check timeout
          if (second) {
            keepalive_counter--;
#ifdef HTTAP_VERBOSE
            printf("   %d  \x0d", keepalive_counter);
#endif
            if (!keepalive_counter)
              goto close_sock_client;
          }
          return;
        }
        else {
          perror("Failed to read the client's request");
          goto close_sock_client;
        }
      }

      b=err400; /* error by default */
      keep_server_open=0;

      if (recv_len>=HTTAP_BUFFER_LEN)
        goto process_reply; // receive buffer overflow -> bye


      /*
          Examine the client's request
       */

      if ((recv_len >= 5) && (strncmp("GET /", buffer, 5) == 0)) {
#ifdef HTTAP_VERBOSE
        printf("GET !\n");
#endif

      /*
          HTTaP
       */


        if (buffer[5]=='?') {
          // Default root
          if ((recv_len == 6)
           ||((recv_len >  6) && (buffer[6]==' '))) {
            len=strlen(HTTaP_json_root);
            snprintf(buffer, HTTAP_BUFFER_LEN,
                "%sContent-Length: %d\x0d\x0a\x0d\x0a%s",
                HTTaP_header, len, HTTaP_json_root);
            b=buffer;
            keep_server_open=1;
          } // root
          else {
            /* insert your own HTTaP code here */


            /* don't forget to set these variables if the request ends well
            b=buffer;
            keep_server_open=1;
             */
          }

        } // End of HTTaP domain


      /*
          The static file server
       */

#ifdef HTTAP_SERVE_FILES
        else {
          fname=buffer+5; // skip the "GET /" prefix
          if ((recv_len == 5)
           ||((recv_len >  5) && (*fname==' '))) {
#ifdef HTTAP_VERBOSE
            printf("got root\n");
#endif
            strncpy(buffer+5, root_page, MAX_URI_LEN);
          }

          len=0;
          d=0;
          // scan the URI
Scan_URI_Loop:
          c=fname[len];
          switch(c){
            case ' ':
            case 0:
              goto endscanURI;

            case '.':
              if (d=='.') goto process_reply;
            case '/':
            case '-':  // only allowed characters
            case '_':
              break;

            default:
              if (c< '0') goto process_reply;
              if (c<='9') break;
              if (c< 'A') goto process_reply;
              if (c<='Z') break;
              if((c< 'a')
              || (c> 'z')) goto process_reply;
            // fold!
          }
          d=c;
          if (++len > MAX_URI_LEN)
            goto process_reply; /* URI size overflow: skip everything */
          if (len < recv_len)
            goto Scan_URI_Loop; /* one more loop to process the next character */
endscanURI:
          fname[len]=0; /* coming from "case ' ':", terminate the string */

#ifdef HTTAP_VERBOSE
          printf("reading %s\n", fname);
#endif

          if (lstat(fname, &stat_buf) == -1) {
#ifdef HTTAP_VERBOSE
            perror("stat");
#endif
            switch (errno) {
              case EACCES:
#ifdef HTTAP_VERBOSE
                printf("403: ");
#endif
                b=err403;
                break;

              case ENOTDIR:
              case ENOENT:
#ifdef HTTAP_VERBOSE
                printf("404: ");
#endif
                b=err404;
                break;

              default:
#ifdef HTTAP_VERBOSE
                printf("500: ");
#endif
                b=err500;
            }
            goto process_reply;
          }

          if (!S_ISREG(stat_buf.st_mode)
          ||  stat_buf.st_uid != prog_uid) {
#ifdef HTTAP_VERBOSE
            printf("403: ");
#endif
            b=err403;
            goto process_reply;
          }

          /* locate the MIME TYPE file */
          buffer[1]='.';
          buffer[2]='.'; // prepend "..." to the path
          buffer[3]='.';
          i=0;
          rfd=open(buffer+1, O_RDONLY);
          if (rfd != -1) {
            i=read(rfd, &header_buffer, HEADER_BUFFER_LEN-1);
            close(rfd);
            if (i>= HEADER_BUFFER_LEN-1) {
              printf("\nWARNING ! MIME type: file too long!\n");
              i=0;
            }
            if (i<0)
              i=0;
          }
          header_buffer[i]=0; // zero termination
          e="text/plain";
          if (i)
            e=header_buffer;

#ifdef HTTAP_VERBOSE
          printf("MIME type: %s\n", header_buffer);
#endif

          /* open the actual data file now */
          rfd=open(buffer+5, O_RDONLY);
          if (rfd == -1) {
            perror("can't open data file ! ");
            b=err500;
            goto process_reply;
          }

          /* Assemble the header */
          offset=snprintf(buffer, HTTAP_BUFFER_LEN,
             "HTTP/1.1 200 OK\x0d\x0a"
             "Content-Type: %s\x0d\x0a"
             "Content-Length: %d\x0d\x0a\x0d\x0a",
               e, (int)stat_buf.st_size);
          if (offset >= HTTAP_BUFFER_LEN) {
            b=err500; // Check overflow, unlikely but you never know
            goto process_reply;
          }


          do {
            /* Read and appends the file's contents after the header */
            j=HTTAP_BUFFER_LEN-offset;
            i=read(rfd, buffer+offset, j);
            if (i == -1) {
              perror("can't read the data file ! ");
              b=err500;
              goto process_reply;
            }
            k=i+offset;
            offset=0;

            if (k) { // don't call in the unlikely event that nothing has been read
              /* send the header and the available data */
              if (send(sock_client, buffer, k, 0) != k) {
                perror("Failed to send the reply");
                goto close_sock_client;
              }
            }
          } while (i==j);

          goto proceed;
        }  // end of HTTAP_SERVE_FILES
#endif
      } // end of GET method

process_reply:
      /* Send as simple reply */
      len=strlen(b);
      if (send(sock_client, b, len, 0) != len) {
        perror("Failed to send the reply");
        goto close_sock_client;
      }

      if (keep_server_open){
proceed:
        keepalive_counter = keepalive;
        server_state=STATE2_wait_incoming_data;
        goto wait_data;
      }

close_sock_client:
      close(sock_client);
#ifdef HTTAP_VERBOSE
      printf("Connection closed.\n");
#endif

      server_state=STATE1_wait_connection;
      goto wait_connection;

/********************************************************/
    default: HTTaP_err("Internal error: Forbidden state!");
  }
}
