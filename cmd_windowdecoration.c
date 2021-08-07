#include "xdo_cmd.h"

int parse_decorations(context_t *context)
{
  char *decoration_str = context->argv[0];
  char *tok, *save_ptr;
  int decorations = 0;

  while ((tok = strtok_r(decoration_str, ",", &save_ptr))) {
    if (decoration_str != NULL)
      decoration_str = NULL;

    if (strcasecmp(tok, "none") == 0)
      decorations = 0;
    else if (strcasecmp(tok, "resize") == 0)
      decorations |= DECORATION_RESIZE;
    else if (strcasecmp(tok, "minimize") == 0)
      decorations |= DECORATION_RESIZE | DECORATION_MINIMIZE;
    else if (strcasecmp(tok, "maximize") == 0)
      decorations |= DECORATION_RESIZE | DECORATION_MAXIMIZE;
    else if (strcasecmp(tok, "close") == 0)
      decorations |= DECORATION_RESIZE | DECORATION_CLOSE;
    else if (strcasecmp(tok, "all") == 0)
      decorations = DECORATION_RESIZE |
                    DECORATION_MOVE |
                    DECORATION_MAXIMIZE |
                    DECORATION_MINIMIZE |
                    DECORATION_CLOSE;
    else {
      fprintf(stderr, "windowdecoration: Invalid decoration '%s'.\n", tok);
      return -1;
    }
  }

  return decorations;
}

int cmd_windowdecoration(context_t *context) {
  int ret = 0;
  char *cmd = *context->argv;
  const char *window_arg = "%1";
  int c;

  typedef enum {
    opt_help,
  } optlist_t;
  static struct option longopts[] = {
    { "help", no_argument, NULL, opt_help },
    { 0, 0, 0, 0 },
  };
  static const char *usage =
    "Usage: %s [window=%1] decorations...\n"
    "\n"
    "Set a window's decorations to only include 'decorations'.\n"
    "Valid decorations are:\n"
    "   all, none, resize, move, minimize, maximize, close\n"
    "\n"
    "Decorations must be given as a single comma-separated string:\n"
    "   windowdecorate resize,move,close\n"
    "\n"
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
      default:
        fprintf(stderr, usage, cmd);
        return EXIT_FAILURE;
    }
  }

  consume_args(context, optind);

  if (!window_get_arg(context, 1, 0, &window_arg)) {
    fprintf(stderr, usage, cmd);
    return EXIT_FAILURE;
  }

  int decorations = parse_decorations(context);

  if (decorations == -1)
    return EXIT_FAILURE;

  window_each(context, window_arg, {
    ret = xdo_set_window_decorations(context->xdo, window, decorations);
  });

  return ret;
}
