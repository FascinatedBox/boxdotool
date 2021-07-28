#ifndef _XDO_SELECT_H_
#define _XDO_SELECT_H_

#include <regex.h>
#include "xdo.h"

typedef struct xdo_select {
  xdo_t *xdo;
  char *winclass;
  char *winclassname;
  char *winrole;
  char *wintitle;

  regex_t winclassrx;     /* pattern to test against a window class */
  regex_t winclassnamerx; /* pattern to test against a window classname */
  regex_t winrolerx;      /* pattern to test against a window role */
  regex_t wintitlerx;     /* pattern to test against a window title */

  int pid;            /* window pid (From window atom _NET_WM_PID) */
  long max_depth;     /* depth of search. 1 means only toplevel windows */
  int screen;         /* screen to search if searchmask has SEARCH_SCREEN. */

  /* Set to 1 if any criteria assignment has failed. */
  int failed;

  /* Bitmask of criteria being searched for that is a pattern. */
  unsigned int rxmask;

  /* Bitmask of criteria being searched for. */
  unsigned int searchmask;

  /* Desktop to search if searchmask has SEARCH_DESKTOP. */
  long desktop;

  /* How many results to return? If 0, return all. */
  unsigned int limit;
} xdo_select_t;

#endif
