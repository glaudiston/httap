/* test_HTTaP.c
   (C) 2011,2017 Yann Guidon whygee@f-cpu.org
   The source code of this project is released under the AGPLv3 or later,
   see https://www.gnu.org/licenses/agpl-3.0.en.html

version 20170327: split from serv_11_CORS.c, a lot of renaming

This file is an example "main loop" that calls the server
periodically so it updates its internal timeout counters.

Compile:
$ gcc -DHTTAP_VERBOSE -DSHOW_CLIENT -Wall test_HTTaP.c -o test_HTTaP

run:
$ ./test_HTTaP

*/

#include "HTTaP.h"

int main(int argc, char *argv[]) {
  const char spinner[4]="-\\|/";
  int phase=0, second=0;
  while (1) {
    /* some actual work should happen here */
    usleep(100*1000);   /* wait for 100ms */
    printf("%c%c", spinner[(phase++)&3],0xd);
    fflush(NULL);

    /* manage the time */
    if (++second>=10) {
      second=0;
//      putchar('\n');
    }

    /* call the server and update its FSM */
    HTTaP_server(!second);
  }
}

