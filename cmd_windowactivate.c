#include "xdo_cmd.h"

int cmd_windowactivate(context_t *context) {
  int ret = 0;
  char *cmd = *context->argv;
  const char *window_arg = "%1";
  int opsync = 0;

  int c;
  typedef enum {
    opt_unused, opt_help, opt_sync
  } optlist_t;
  static struct option longopts[] = {
    { "help", no_argument, NULL, opt_help },
    { "sync", no_argument, NULL, opt_sync },
    { 0, 0, 0, 0 },
  };
  static const char *usage =
    "Usage: %s [options] [window=%1]\n"
    "--sync    - only exit once the window is active (is visible + active)\n"
    HELP_SEE_WINDOW_STACK;

  int option_index;
  while ((c = getopt_long_only(context->argc, context->argv, "+h",
                               longopts, &option_index)) != -1) {
    switch (c) {
      case 'h':
      case opt_help:
        printf(usage, cmd);
        consume_args(context, context->argc);
        return EXIT_SUCCESS;
        break;
      case opt_sync:
        opsync = 1;
        break;
      default:
        fprintf(stderr, usage, cmd);
        return EXIT_FAILURE;
    }
  }

  consume_args(context, optind);

  if (!window_get_arg(context, 0, 0, &window_arg)) {
    fprintf(stderr, usage, cmd);
    return EXIT_FAILURE;
  }

  window_each(context, window_arg, {
    ret = xdo_activate_window(context->xdo, window);
    if (ret) {
      fprintf(stderr, "xdo_activate_window on window:%ld reported an error\n",
              window);
      return ret;
    } else {
      if (opsync) {
        xdo_wait_for_window_active(context->xdo, window, 1);
      }
    }
  }); /* window_each(...) */

  return ret;
}
