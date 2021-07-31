#include "xdo_cmd.h"

#define WINDOWMOVE_X_CURRENT (1 << 0)
#define WINDOWMOVE_Y_CURRENT (1 << 1)
#define WINDOWMOVE_RELATIVE (1 << 2)

struct windowmove {
  Window window;
  int gravity;
  int x;
  int y;
  int opsync;
  int flags;
};

#define HELP_GRAVITY \
  "These are the available gravities:\n" \
  "\n" \
  "Northwest\n" \
  "North\n" \
  "Northeast\n" \
  "West\n" \
  "Center\n" \
  "East\n" \
  "Southwest\n" \
  "South\n" \
  "Southeast\n" \
  "Static\n"

int gravity_to_number(const char *gravity)
{
  int result = -1;

  if (strcasecmp(gravity, "Northwest") == 0)
    result = 1;
  else if (strcasecmp("North", gravity) == 0)
    result = 2;
  else if (strcasecmp("Northeast", gravity) == 0)
    result = 3;
  else if (strcasecmp("West", gravity) == 0)
    result = 4;
  else if (strcasecmp("Center", gravity) == 0)
    result = 5;
  else if (strcasecmp("East", gravity) == 0)
    result = 6;
  else if (strcasecmp("Southwest", gravity) == 0)
    result = 7;
  else if (strcasecmp("South", gravity) == 0)
    result = 8;
  else if (strcasecmp("Southeast", gravity) == 0)
    result = 9;
  else if (strcasecmp("Static", gravity) == 0)
    result = 10;

  return result;
}

/* This function exists because at one time I had problems embedding certain
 * blocks of code within macros (window_each). */
static int _windowmove(context_t *context, struct windowmove *windowmove);

int cmd_windowmove(context_t *context) {
  int ret = 0;
  unsigned int width, height;
  int is_width_percent = 0, is_height_percent = 0;
  char *cmd = *context->argv;
  struct windowmove windowmove;

  windowmove.x = 0;
  windowmove.y = 0;
  windowmove.opsync = 0;
  windowmove.window = CURRENTWINDOW;
  windowmove.flags = 0;

  int c;
  typedef enum {
    opt_gravity,
    opt_relative,
    opt_sync,
    opt_help_gravity,
    opt_help,
  } optlist_t;
  static struct option longopts[] = {
    { "gravity", required_argument, NULL, opt_gravity },
    { "sync", no_argument, NULL, opt_sync },
    { "relative", no_argument, NULL, opt_relative },
    { "help-gravity", no_argument, NULL, opt_help_gravity },
    { "help", no_argument, NULL, opt_help },
    { 0, 0, 0, 0 },
  };
  static const char *usage =
    "Usage: %s [options] [window=%1] x y\n"
    "--gravity <g>       specify a gravity for the movement\n"
    "                    see --help-gravity for values\n"
    "--sync              only exit once the window has moved\n"
    "--relative          move relative to window position\n"
    "-h/--help           display this help and exit\n"
    "\n"
    "If you use literal 'x' or 'y' for the x coordinates, then the current\n"
    "coordinate will be used. This is useful for moving the window along\n"
    "only one axis.\n"
    ;

  int option_index;
  while ((c = getopt_long_only(context->argc, context->argv, "+h",
                               longopts, &option_index)) != -1) {
    switch (c) {
      case opt_gravity:
        windowmove.gravity = gravity_to_number(optarg);

        if (windowmove.gravity == -1) {
          fprintf(stderr, "windowmove: Invalid gravity %s.\n", optarg);
          return EXIT_FAILURE;
        }
        break;
      case opt_relative:
        windowmove.flags |= WINDOWMOVE_RELATIVE;
        break;
      case opt_sync:
        windowmove.opsync = 1;
        break;
      case opt_help_gravity:
        puts(HELP_GRAVITY);
        consume_args(context, context->argc);
        return EXIT_SUCCESS;
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

  if (windowmove.flags & WINDOWMOVE_RELATIVE && windowmove.gravity) {
    fputs("windowmove: Cannot mix --relative and --gravity.", stderr);
    return EXIT_FAILURE;
  }

  consume_args(context, optind);

  const char *window_arg = "%1";

  if (!window_get_arg(context, 2, 0, &window_arg)) {
    fprintf(stderr, usage, cmd);
    return EXIT_FAILURE;
  }

  if (context->argv[0][0] == 'x') {
    windowmove.flags |= WINDOWMOVE_X_CURRENT;
  } else {
    /* Use percentage if given a percent. */
    if (strchr(context->argv[0], '%')) {
        is_width_percent = 1;
    } else {
        windowmove.x = (int)strtol(context->argv[0], NULL, 0);
    }
  }

  if (context->argv[1][0] == 'y') {
    windowmove.flags |= WINDOWMOVE_Y_CURRENT;
  } else {
    /* Use percentage if given a percent. */
    if (strchr(context->argv[0], '%')) {
        is_height_percent = 1;
    } else {
        windowmove.y = (int)strtol(context->argv[1], NULL, 0);
    }
  }

  width = (unsigned int)strtoul(context->argv[0], NULL, 0);
  height = (unsigned int)strtoul(context->argv[1], NULL, 0);
  consume_args(context, 2);

  XWindowAttributes wattr;
  unsigned int original_w, original_h;
  unsigned int root_w, root_h; /* for percent */

  window_each(context, window_arg, {
      if (is_width_percent || is_height_percent) {
        Window root = 0;
        XGetWindowAttributes(context->xdo->xdpy, window, &wattr);
        root = wattr.root;
        xdo_get_window_size(context->xdo, root, &root_w, &root_h);

        if (is_width_percent) {
          windowmove.x = (root_w * width / 100);
        }

        if (is_height_percent) {
          windowmove.y = (root_h * height / 100);
        }
      }
      windowmove.window = window;
      _windowmove(context, &windowmove);
    }); /* window_each(...) */
  return ret;
}

static int _windowmove(context_t *context, struct windowmove *windowmove) {
  int orig_win_x = 0;
  int orig_win_y = 0;
  int ret;

  /* Grab the current position of the window if we are moving synchronously
   * or if we are moving along an axis.
   * That is, with --sync or x or y in args were literally 'x' or 'y'
   * or if --relative is given*/
  if (windowmove->opsync || windowmove->flags != 0) {
    xdo_get_window_location(context->xdo, windowmove->window,
                            &orig_win_x, &orig_win_y, NULL);
    /* Break early if we don't need to move the window */
    if (orig_win_x == windowmove->x && orig_win_y == windowmove->y) {
      return 0;
    }
  }

  int target_gravity = windowmove->gravity;
  int target_x = windowmove->x;
  int target_y = windowmove->y;

  if (windowmove->flags & WINDOWMOVE_RELATIVE) {
    target_x = orig_win_x + windowmove->x;
    target_y = orig_win_y + windowmove->y;
  }

  if (windowmove->flags & WINDOWMOVE_X_CURRENT) {
    target_x = orig_win_x;
    xdotool_debug(context, "Using %d for x\n", windowmove->x);
  }

  if (windowmove->flags & WINDOWMOVE_Y_CURRENT) {
    target_y = orig_win_y;
    xdotool_debug(context, "Using %d for y\n", windowmove->y);
  }

  ret = xdo_move_window(context->xdo, windowmove->window, target_gravity,
                        target_x, target_y);
  if (ret) {
    fprintf(stderr,
            "xdo_move_window reported an error while moving window %ld\n",
            windowmove->window);
  } else {
    if (windowmove->opsync) {
      /* This 'sync' request is stateful (we need to know the original window
       * location to make the decision about 'done'
       * Some window managers force alignments or otherwise mangle move
       * requests, so we can't just look for the x,y positions exactly.
       * Just look for any change in the window's position. */
      int win_x, win_y;
      xdo_get_window_location(context->xdo, windowmove->window,
                              &win_x, &win_y, NULL);
      /* Permit imprecision to account for window borders and titlebar */
      while (orig_win_x == win_x && orig_win_y == win_y
             && abs(windowmove->x - win_x) > 10
             && abs(windowmove->y - win_y) > 50) {
        xdo_get_window_location(context->xdo, windowmove->window,
                                &win_x, &win_y, NULL);
        usleep(30000);
      }
    }
  }

  return ret;
}
