#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif /* _XOPEN_SOURCE */

#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include "xdo.h"
#include "xdo_select.h"

static int compile_re(const char *pattern, regex_t *re);
static int check_window_match(xdo_t *xdo, Window wid, xdo_select_t *selection);
static int _select_by_class(const xdo_t *xdo, const xdo_select_t *selection,
                                   Window window);
static int _select_by_classname(const xdo_t *xdo, const xdo_select_t *selection,
                                       Window window);
static int _select_by_title(const xdo_t *xdo, const xdo_select_t *selection,
                                   Window window);
static int _select_by_role(const xdo_t *xdo, const xdo_select_t *selection,
                                  Window window);
static int _select_by_pid(const xdo_t *xdo, const xdo_select_t *selection,
                                 Window window);
static int _select_by_prop_exists(const xdo_t *xdo,
                                  const xdo_select_t *selection,
                                  Window window);
static int _select_by_visible(const xdo_t *xdo, Window wid);
static void find_matching_windows(xdo_t *xdo, Window window,
                                  xdo_select_t *selection,
                                  Window **windowlist_ret,
                                  unsigned int *nwindows_ret,
                                  unsigned int *windowlist_size,
                                  int current_depth);

static int _select_by_title(const xdo_t *xdo, const xdo_select_t *selection,
                                   Window window) {
  unsigned char *name;
  int name_len;
  int name_type;

  xdo_get_window_name(xdo, window, &name, &name_len, &name_type);

  char *title = "";
  int result;

  if (name_len)
    title = (char *)name;

  if (selection->rxmask & SEARCH_TITLE)
    result = regexec(&selection->wintitlerx, title, 0, NULL, 0);
  else
    result = strcmp(title, selection->wintitle);

  free(name);
  return result == 0;
}

static int _select_by_class(const xdo_t *xdo, const xdo_select_t *selection,
                                   Window window) {
  XWindowAttributes attr;
  XClassHint classhint;
  XGetWindowAttributes(xdo->xdpy, window, &attr);
  char *res_class = "";
  char *res_name = NULL;
  int have_hint = XGetClassHint(xdo->xdpy, window, &classhint);

  if (have_hint) {
    res_class = classhint.res_class;
    res_name = classhint.res_name;
  }

  int result;

  if (selection->rxmask & SEARCH_CLASS)
    result = regexec(&selection->winclassrx, res_class, 0, NULL, 0);
  else
    result = strcmp(res_class, selection->winclass);

  if (have_hint) {
    free(classhint.res_name);
    free(classhint.res_class);
  }

  return result == 0;
}

static int _select_by_classname(const xdo_t *xdo,
                                       const xdo_select_t *selection,
                                       Window window) {
  XWindowAttributes attr;
  XClassHint classhint;
  XGetWindowAttributes(xdo->xdpy, window, &attr);
  char *res_class = NULL;
  char *res_name = "";
  int have_hint = XGetClassHint(xdo->xdpy, window, &classhint);

  if (have_hint) {
    res_class = classhint.res_class;
    res_name = classhint.res_name;
  }

  int result;

  if (selection->rxmask & SEARCH_CLASSNAME)
    result = regexec(&selection->winclassnamerx, res_name, 0, NULL, 0);
  else
    result = strcmp(res_name, selection->winclassname);

  if (have_hint) {
    free(classhint.res_name);
    free(classhint.res_class);
  }

  return result == 0;
}

static int _select_by_role(const xdo_t *xdo, const xdo_select_t *selection,
                                  Window window) {
  int status;
  int count = 0;
  char **list = NULL;
  XTextProperty tp;
  char *role;

  status = XGetTextProperty(xdo->xdpy, window, &tp,
                            XInternAtom(xdo->xdpy, "WM_WINDOW_ROLE", False));

  if (status && tp.nitems) {
    Xutf8TextPropertyToTextList(xdo->xdpy, &tp, &list, &count);
    role = list[0];
  }
  else
    role = "";

  int result;

  if (selection->rxmask & SEARCH_ROLE)
    result = regexec(&selection->winrolerx, role, 0, NULL, 0);
  else
    result = strcmp(role, selection->winrole);

  XFreeStringList(list);
  free(tp.value);

  return result == 0;
}

static int _select_by_pid(const xdo_t *xdo, const xdo_select_t *selection,
                                 Window window) {
  int window_pid = xdo_get_pid_window(xdo, window);

  if (selection->pid == window_pid) {
    return True;
  } else {
    return False;
  }
}

static int _select_by_has_prop(const xdo_t *xdo, const xdo_select_t *selection,
                               Window window) {
  if (xdo_get_has_property(xdo, window, selection->hasprop)) {
    return True;
  } else {
    return False;
  }
}

static int compile_re(const char *pattern, regex_t *re) {
  int ret;
  if (pattern == NULL) {
    regcomp(re, "^$", REG_EXTENDED | REG_ICASE);
    return True;
  }

  ret = regcomp(re, pattern, REG_EXTENDED | REG_ICASE);
  if (ret != 0)
    return False;

  return True;
}

static int _xdo_is_window_visible(const xdo_t *xdo, Window wid) {
  XWindowAttributes wattr;
  XGetWindowAttributes(xdo->xdpy, wid, &wattr);
  if (wattr.map_state != IsViewable)
    return False;

  return True;
}

static int check_window_match(xdo_t *xdo, Window wid,
                              xdo_select_t *selection) {
  int class_want = selection->searchmask & SEARCH_CLASS;
  int classname_want = selection->searchmask & SEARCH_CLASSNAME;
  int desktop_want = selection->searchmask & SEARCH_DESKTOP;
  int name_want = selection->searchmask & SEARCH_NAME;
  int pid_want = selection->searchmask & SEARCH_PID;
  int role_want = selection->searchmask & SEARCH_ROLE;
  int title_want = selection->searchmask & SEARCH_TITLE;
  int visible_want = selection->searchmask & SEARCH_ONLYVISIBLE;
  int prop_want = selection->searchmask & SEARCH_HAS_PROPERTY;

  int ok = False;

  do {
    if (desktop_want) {
      long desktop = -1;

      /* We're modifying xdo here, but since we restore it, we're still
       * obeying the "const" contract. */
      int old_quiet = xdo->quiet;
      xdo_t *xdo2 = (xdo_t *)xdo;
      xdo2->quiet = 1;
      int ret = xdo_get_desktop_for_window(xdo2, wid, &desktop);
      xdo2->quiet = old_quiet;

      /* Desktop matched if we support desktop queries *and* the desktop is
       * equal */
      if (ret != XDO_SUCCESS || desktop != selection->desktop)
        break;
    }

    if (visible_want && !_xdo_is_window_visible(xdo, wid))
      break;

    if (class_want && !_select_by_class(xdo, selection, wid))
      break;

    if (title_want && !_select_by_title(xdo, selection, wid))
      break;

    if (classname_want && !_select_by_classname(xdo, selection, wid))
      break;

    if (pid_want && !_select_by_pid(xdo, selection, wid))
      break;

    if (role_want && !_select_by_role(xdo, selection, wid))
      break;

    if (prop_want && !_select_by_has_prop(xdo, selection, wid))
      break;

    ok = True;
  } while (0);

  return ok;
}

static void find_matching_windows(xdo_t *xdo, Window window,
                                  xdo_select_t *selection,
                                  Window **windowlist_ret,
                                  unsigned int *nwindows_ret,
                                  unsigned int *windowlist_size,
                                  int current_depth) {
  /* Query for children of 'wid'. For each child, check match.
   * We want to do a breadth-first search.
   *
   * If match, add to list.
   * If over limit, break.
   * Recurse.
   */

  Window dummy;
  Window *children;
  unsigned int i, nchildren;

  /* Break early, if we have enough windows already. */
  if (selection->limit > 0 && *nwindows_ret >= selection->limit) {
    return;
  }

  /* Break if too deep */
  if (selection->max_depth != 0 && current_depth > selection->max_depth) {
    return;
  }

  /* Break if XQueryTree fails.
   * TODO(sissel): report an error? */
  Status success = XQueryTree(xdo->xdpy, window, &dummy, &dummy, &children, &nchildren);

  if (!success) {
    if (children != NULL)
      free(children);
    return;
  }

  /* Breadth first, check all children for matches */
  for (i = 0; i < nchildren; i++) {
    Window child = children[i];
    if (!check_window_match(xdo, child, selection))
      continue;

    (*windowlist_ret)[*nwindows_ret] = child;
    (*nwindows_ret)++;

    if (selection->limit > 0 && *nwindows_ret >= selection->limit) {
      /* Limit hit, break early. */
      break;
    }

    if (*windowlist_size == *nwindows_ret) {
      *windowlist_size *= 2;
      *windowlist_ret = realloc(*windowlist_ret,
                                *windowlist_size * sizeof(Window));
    }
  }

  /* Now check children-children */
  if (selection->max_depth == 0 || (current_depth + 1) <= selection->max_depth) {
    for (i = 0; i < nchildren; i++) {
      find_matching_windows(xdo, children[i], selection, windowlist_ret,
                            nwindows_ret, windowlist_size,
                            current_depth + 1);
    }
  }

  if (children != NULL)
    free(children);
}

xdo_select_t *xdo_select_new(xdo_t *xdo)
{
  xdo_select_t *selection = calloc(1, sizeof(*selection));

  selection->xdo = xdo;
  selection->searchmask |= SEARCH_ONLYVISIBLE;
  return selection;
}

#define FREE_CRITERIA(flag, textprop, rxprop) \
{ \
  if (selection->rxmask & flag) \
    regfree(&selection->rxprop); \
  else if (selection->searchmask & flag) \
    free(selection->textprop); \
}

void xdo_select_free(xdo_select_t *selection)
{
  FREE_CRITERIA(SEARCH_CLASS, winclass, winclassrx)
  FREE_CRITERIA(SEARCH_CLASSNAME, winclassname, winclassnamerx)
  FREE_CRITERIA(SEARCH_TITLE, wintitle, wintitlerx)
  FREE_CRITERIA(SEARCH_ROLE, winrole, winrolerx)
  free(selection);
}

#undef FREE_CRITERIA

#define SET_CRITERIA(textprop, rxprop) \
{ \
  if (is_pattern == 0) \
    selection->textprop = strdup(text); \
  else \
    result = compile_re(text, &selection->rxprop); \
  \
}

int xdo_select_set_criteria(xdo_select_t *selection, unsigned int criteria,
                            int is_pattern, const char *text)
{
  if (selection->searchmask & criteria || selection->failed)
    return XDO_ERROR;

  int result = True;

  if (criteria == SEARCH_CLASS)
    SET_CRITERIA(winclass, winclassrx)
  else if (criteria == SEARCH_CLASSNAME)
    SET_CRITERIA(winclassname, winclassnamerx)
  else if (criteria == SEARCH_ROLE)
    SET_CRITERIA(winrole, winrolerx)
  else if (criteria == SEARCH_TITLE)
    SET_CRITERIA(wintitle, wintitlerx)
  else if (criteria == SEARCH_PID)
    selection->pid = atoi(text);

  if (result) {
    if (is_pattern)
      selection->rxmask |= criteria;

    selection->searchmask |= criteria;
  }
  else
    selection->failed = 1;

  return selection->failed;
}

#undef SET_CRITERIA

int xdo_select_set_desktop(xdo_select_t *selection, int desktop)
{
  if (desktop >= 0) {
    selection->desktop = (long)desktop;
    selection->searchmask |= SEARCH_DESKTOP;
  }
  else
    selection->failed = 1;

  return selection->failed;
}

int xdo_select_set_limit(xdo_select_t *selection, int limit)
{
  if (limit >= 0)
    selection->limit = limit;
  else
    selection->failed = 1;

  return selection->failed;
}

int xdo_select_set_max_depth(xdo_select_t *selection, int max_depth)
{
  if (max_depth >= 0)
    selection->max_depth = (long)max_depth;
  else
    selection->failed = 1;

  return selection->failed;
}

int xdo_select_set_require_visible(xdo_select_t *selection, int visibility)
{
  if (visibility == 1)
    selection->searchmask |= SEARCH_ONLYVISIBLE;
  else if (visibility == 0)
    selection->searchmask &= ~SEARCH_ONLYVISIBLE;
  else
    selection->failed = 1;

  return selection->failed;
}

int xdo_select_set_screen(xdo_select_t *selection, int screen)
{
  if (screen >= 0) {
    selection->screen = screen;
    selection->searchmask |= SEARCH_SCREEN;
  }
  else
    selection->failed = 1;

  return selection->failed;
}

void xdo_select_set_use_client_list(xdo_select_t *selection, int use)
{
  if (use)
    selection->searchmask |= SEARCH_CLIENT_LIST;
  else
    selection->searchmask &= ~SEARCH_CLIENT_LIST;
}

void xdo_select_set_has_property(xdo_select_t *selection, const char *property)
{
  Atom prop = XInternAtom(selection->xdo->xdpy, property, False);

  selection->searchmask |= SEARCH_HAS_PROPERTY;
  selection->hasprop = prop;
}

int xdo_select_windows(xdo_select_t *selection, Window **windowlist_ret,
                       unsigned int *nwindows_ret) {
  if (selection->failed)
    return XDO_ERROR;

  xdo_t *xdo = selection->xdo;
  unsigned int windowlist_size = 100;
  *nwindows_ret = 0;
  *windowlist_ret = calloc(sizeof(Window), windowlist_size);

  if (selection->searchmask & SEARCH_SCREEN) {
      Window root = RootWindow(xdo->xdpy, selection->screen);
      if (check_window_match(xdo, root, selection)) {
        (*windowlist_ret)[*nwindows_ret] = root;
        (*nwindows_ret)++;
        /* Don't have to check for size bounds here because
         * we start with array size 100 */
      }

      /* Start with depth=1 since we already covered the root windows */
      find_matching_windows(xdo, root, selection, windowlist_ret, nwindows_ret,
                            &windowlist_size, 1);
  }
  else if ((selection->searchmask & SEARCH_CLIENT_LIST) == 0) {
    int i;
    const int screencount = ScreenCount(xdo->xdpy);
    for (i = 0; i < screencount; i++) {
      Window root = RootWindow(xdo->xdpy, i);
      if (check_window_match(xdo, root, selection)) {
        (*windowlist_ret)[*nwindows_ret] = root;
        (*nwindows_ret)++;
        /* Don't have to check for size bounds here because
         * we start with array size 100 */
      }

      /* Start with depth=1 since we already covered the root windows */
      find_matching_windows(xdo, root, selection, windowlist_ret,
                            nwindows_ret, &windowlist_size, 1);
    }
  }
  else {
    /* Search using the client list. */
    Atom request = XInternAtom(xdo->xdpy, "_NET_CLIENT_LIST", False);
    Window root = XDefaultRootWindow(xdo->xdpy);
    Atom type;
    int i, size;
    long nitems;

    unsigned char *data = xdo_get_window_property_by_atom(xdo, root, request,
        &nitems, &type, &size);
    Window *windows = (Window *)data;

    for (i = 0;i < nitems;i++) {
      Window w = windows[i];

      if (check_window_match(xdo, w, selection)) {
        (*windowlist_ret)[*nwindows_ret] = w;
        (*nwindows_ret)++;
      }

      if (selection->limit > 0 && *nwindows_ret >= selection->limit) {
        /* Limit hit, break early. */
        break;
      }

      /* Recursing seems unnecessary here since these are not root windows. */
    }

    free(windows);
  }

  return XDO_SUCCESS;
}
