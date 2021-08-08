#include "xdo_cmd.h"
#include <string.h>

#define CRITERIA_CASE(criteria, flag) \
case opt_match##criteria: \
  ret = xdo_select_set_criteria(selection, flag, 1, optarg); \
  break; \
case opt_exact##criteria: \
  ret = xdo_select_set_criteria(selection, flag, 0, optarg); \
  break; \

int cmd_select(context_t *context) {
  char *cmd = *context->argv;
  int c;
  int option_index;
  int ret = XDO_SUCCESS;

  xdo_select_t *selection = xdo_select_new(context->xdo);

  typedef enum {
    opt_all,
    opt_clients,
    opt_exact_class,
    opt_exact_classname,
    opt_exact_role,
    opt_exact_title,
    opt_desktop,
    opt_limit,
    opt_match_class,
    opt_match_classname,
    opt_match_role,
    opt_match_title,
    opt_max_depth,
    opt_pid,
    opt_screen,
    opt_help,
  } optlist_t;
  struct option longopts[] = {
    { "all", no_argument, NULL, opt_all },
    { "class", required_argument, NULL, opt_match_class },
    { "classname", required_argument, NULL, opt_match_classname },
    { "clients", no_argument, NULL, opt_clients },
    { "desktop", required_argument, NULL, opt_desktop },
    { "exact-class", required_argument, NULL, opt_exact_class },
    { "exact-classname", required_argument, NULL, opt_exact_classname },
    { "exact-role", required_argument, NULL, opt_exact_role },
    { "exact-title", required_argument, NULL, opt_exact_title },
    { "limit", required_argument, NULL, opt_limit },
    { "max-depth", required_argument, NULL, opt_max_depth },
    { "pid", required_argument, NULL, opt_pid },
    { "role", required_argument, NULL, opt_match_role },
    { "screen", required_argument, NULL, opt_screen },
    { "title", required_argument, NULL, opt_match_title },
    { "help", no_argument, NULL, opt_help },
    { 0, 0, 0, 0 },
  };
  static const char *usage =
    "Usage: %s [options]\n"
    "--exact-<criteria> <string>  must be exactly <string>\n"
    "--<criteria> <pattern>       match against regexp <pattern>\n"
    "--pid <id>                   check for _NET_WM_PID being <id>\n"
    "\n"
    "--desktop <N>                specific desktop to search\n"
    "--screen <N>                 specific screen to search\n"
    "--max-depth <depth>          max window child depth\n"
    "\n"
    "--all                        include hidden windows\n"
    "-c/--clients                 use managed clients\n"
    "                             (wm must support _NET_CLIENT_LIST)\n"
    "--limit <count>              max # of windows to return\n"
    "-h/--help                    display this help and exit\n"
    "\n"
    "Criteria can be any of the following:\n"
    "  class, classname, role, title\n"
    ;

  while ((c = getopt_long_only(context->argc, context->argv, "+ch",
                               longopts, &option_index)) != -1) {
    switch (c) {
      CRITERIA_CASE(_class, SEARCH_CLASS)
      CRITERIA_CASE(_classname, SEARCH_CLASSNAME)
      CRITERIA_CASE(_role, SEARCH_ROLE)
      CRITERIA_CASE(_title, SEARCH_TITLE)
      case opt_all:
        xdo_select_set_require_visible(selection, 0);
        break;
      case 'c':
      case opt_clients:
        xdo_select_set_use_client_list(selection, 1);
        break;
      case opt_desktop:
        ret = xdo_select_set_desktop(selection, atoi(optarg));
        break;
      case opt_limit:
        ret = xdo_select_set_limit(selection, atoi(optarg));
        break;
      case opt_max_depth:
        ret = xdo_select_set_max_depth(selection, atoi(optarg));
        break;
      case opt_pid:
        ret = xdo_select_set_criteria(selection, SEARCH_PID, 0, optarg);
        break;
      case opt_screen:
        ret = xdo_select_set_screen(selection, atoi(optarg));
        break;
      case opt_help:
        printf(usage, cmd);
        consume_args(context, context->argc);
        return EXIT_SUCCESS;
      default:
        fprintf(stderr, usage, cmd);
        return EXIT_FAILURE;
    }

    if (ret == XDO_ERROR)
      return EXIT_FAILURE;
  }

  consume_args(context, optind);

  int nwindows;
  Window *list;
  xdo_select_windows(selection, &list, &nwindows);

  if (context->argc == 0) {
    int i;
    /* Only print if the last command. */
    for (i = 0; i < nwindows; i++)
      window_print(list[i]);
  }

  /* Free old list as it's malloc'd by xdo_search_windows. */
  free(context->windows);
  context->windows = list;
  context->nwindows = nwindows;

  xdo_select_free(selection);

  return nwindows ? EXIT_SUCCESS : EXIT_FAILURE;
}

#undef CRITERIA_CASE
