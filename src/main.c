#include "crypto.h"
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <fontconfig/fontconfig.h>
#include <gpgme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 160
#define PADDING 20
#define INPUT_SIZE 512

typedef struct {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
} MotifWmHints;

#define MWM_HINTS_DECORATIONS (1L << 1)

#define arr_append_impl(items, count, capacity, v)                                                 \
  do {                                                                                             \
    if ((count) >= (capacity)) {                                                                   \
      (capacity) = (capacity) == 0 ? 256 : (capacity) * 2;                                         \
      (items) = realloc((items), (capacity) * sizeof(*(items)));                                   \
    }                                                                                              \
    (items)[(count)++] = (v);                                                                      \
  } while (0)

#define arr_appendp(arr, v) arr_append_impl((arr)->items, (arr)->count, (arr)->capacity, (v))

#define arr_append(arr, v) arr_append_impl((arr).items, (arr).count, (arr).capacity, (v))

typedef struct {
  char **items;
  size_t count;
  size_t capacity;
} Passwords;

int get_passwords(char *path, Passwords *passwords) {
  DIR *dir = opendir(path);

  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory\n");
    return 1;
  }

  struct dirent *de;
  int result = 0;

  while ((de = readdir(dir)) != NULL) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 ||
        strcmp(de->d_name, ".stfolder") == 0) {
      continue;
    }

    char *new_path = NULL;
    if (asprintf(&new_path, "%s/%s", path, de->d_name) == -1) {
      result = 1;
      break;
    };

    if (de->d_type == DT_DIR) {
      if (get_passwords(new_path, passwords) != 0) {
        result = 1;
      };

      free(new_path);
    } else if (de->d_type == DT_REG) {
      size_t len = strlen(de->d_name);

      if (len >= 4 && strcmp(de->d_name + len - 4, ".gpg") == 0) {
        arr_appendp(passwords, new_path);
        continue;
      }

      free(new_path);
    } else {
      free(new_path);
    }

    if (result != 0)
      break;
  }

  closedir(dir);

  return result;
}

int main(void) {
  gpgme_check_version(NULL);
  char *home = getenv("HOME");

  if (home == NULL) {
    perror("HOME not set");
    return 1;
  }

  Passwords passwords = {0};

  char path[256];
  sprintf(path, "%s/.password-store", home);
  if (get_passwords(path, &passwords) != 0) {
    fprintf(stderr, "Failed to get passwords\n");
    return 1;
  }

  Display *display = XOpenDisplay(NULL);

  if (!display) {
    fprintf(stderr, "Cannot open X display\n");
    return 1;
  }

  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  int screen_width = DisplayWidth(display, screen);
  int screen_height = DisplayHeight(display, screen);

  int mouse_x, mouse_y;
  Window root_return, child_return;
  int win_x, win_y;
  uint mask;

  XQueryPointer(display, root, &root_return, &child_return, &mouse_x, &mouse_y, &win_x, &win_y,
                &mask);

  int monitor_x = 0;
  int monitor_y = 0;
  int monitor_width = screen_width;
  int monitor_height = screen_height;

  int nmonitors = 0;
  XRRMonitorInfo *monitors = XRRGetMonitors(display, root, True, &nmonitors);

  for (int i = 0; i < nmonitors; i++) {
    XRRMonitorInfo *m = &monitors[i];
    if ((mouse_x >= m->x && mouse_x < m->x + m->width) &&
        (mouse_y >= m->y && mouse_y < m->y + m->height)) {
      monitor_x = m->x;
      monitor_y = m->y;
      monitor_width = m->width;
      monitor_height = m->height;
      break;
    }
  }

  if (monitors)
    XRRFreeMonitors(monitors);

  int x = monitor_x + (monitor_width - WINDOW_WIDTH) / 2;
  int y = monitor_y + (monitor_height - WINDOW_HEIGHT) / 2;

  XSetWindowAttributes attrs = {0};
  attrs.override_redirect = True;
  attrs.background_pixel = BlackPixel(display, screen);
  attrs.event_mask = ExposureMask | KeyPressMask;

  Window win = XCreateWindow(display, root, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 0, CopyFromParent,
                             InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attrs);

  Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  Atom dialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  Atom motif_hints = XInternAtom(display, "_MOTIF_WM_HINTS", False);

  MotifWmHints hints = {.flags = MWM_HINTS_DECORATIONS, .decorations = 0};

  XChangeProperty(display, win, motif_hints, motif_hints, 32, PropModeReplace,
                  (unsigned char *)&hints, 5);

  XChangeProperty(display, win, type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dialog, 1);

  XStoreName(display, win, "pass-frontend");
  XClassHint class_hint = {.res_name = "pass-frontend", .res_class = "PassFrontend"};
  XSetClassHint(display, win, &class_hint);

  XMapRaised(display, win);
  XFlush(display);

  GC gc = XCreateGC(display, win, 0, NULL);

  const unsigned long bg = 0x010101;
  const unsigned long fg = 0xFFFFFF;
  const unsigned long accent = 0x4E0000;

  XSetWindowBackground(display, win, bg);

  XftFont *font_large = XftFontOpenName(display, screen, "Iosevka:size=16");
  if (!font_large) {
    fprintf(stderr, "Failed to load font\n");
    return 1;
  }

  XftFont *font_small = XftFontOpenName(display, screen, "Iosevka:size=10");
  if (!font_small) {
    fprintf(stderr, "Failed to load font\n");
    return 1;
  }

  XftDraw *draw =
      XftDrawCreate(display, win, DefaultVisual(display, screen), DefaultColormap(display, screen));

  XftColor color;
  XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
                    "#ffffff", &color);

  XftColor color_highlight;
  XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
                    "#98020E", &color_highlight);

  char input[INPUT_SIZE] = "";
  size_t length = 0;

  XGrabPointer(display, win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None,
               CurrentTime);

  Passwords filtered_passwords = {0};

  for (size_t i = 0; i < passwords.count; ++i) {
    int prefix_len = strlen(home) + strlen("/.password-store");
    memmove(passwords.items[i], passwords.items[i] + prefix_len,
            strlen(passwords.items[i] + prefix_len) + 1);
    arr_append(filtered_passwords, passwords.items[i]);
  }

  size_t start = 0;

  XEvent event = {0};

  Atom net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
  event.xclient.type = ClientMessage;
  event.xclient.window = win;
  event.xclient.message_type = net_active;
  event.xclient.format = 32;
  event.xclient.data.l[0] = 1; // Application request
  event.xclient.data.l[1] = CurrentTime;
  event.xclient.data.l[2] = None;

  XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);

  XFlush(display);

  while (1) {
    XNextEvent(display, &event);

    // Close window when clicking outside
    if (event.type == ButtonPress) {
      int px = event.xbutton.x;
      int py = event.xbutton.y;

      if (px < 0 || px >= WINDOW_WIDTH || py < 0 || py >= WINDOW_HEIGHT) {
        break;
      }
    }

    if (event.type == Expose) {
      XSetForeground(display, gc, bg);
      XFillRectangle(display, win, gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

      XSetForeground(display, gc, accent);
      XFillRectangle(display, win, gc, 0, WINDOW_HEIGHT - 3, WINDOW_WIDTH, 3);

      XSetForeground(display, gc, fg);

      for (size_t i = start; i < filtered_passwords.count; ++i) {
        int idx = i - start;
        if (i == start + 5)
          break;

        char *shown = filtered_passwords.items[i];
        int len = strlen(shown);

        if (i == start) {
          XftDrawStringUtf8(draw, &color_highlight, font_small, PADDING,
                            WINDOW_HEIGHT - (idx * 20) - 50, (const FcChar8 *)shown, len);
        } else {
          XftDrawStringUtf8(draw, &color, font_small, PADDING, WINDOW_HEIGHT - (idx * 20) - 50,
                            (const FcChar8 *)shown, len);
        }
      }

      XftDrawStringUtf8(draw, &color, font_small, PADDING, WINDOW_HEIGHT - 20,
                        (const FcChar8 *)input, (int)length);
    }

    if (event.type == KeyPress) {
      KeySym key;
      char buffer[32] = {0}; // typed char

      size_t n = XLookupString(&event.xkey, buffer, sizeof(buffer), &key, NULL);

      if (key == XK_Escape) {
        break;
      }

      if (key == XK_Return) {
        char *password;
        if (decrypt_file(filtered_passwords.items[start], &password, home) != 0) {
          break;
        }

        printf("Password: %s\n", password);

        XClearArea(display, win, 0, 0, 0, 0, True);
        break;
      }

      if (key == XK_Up) {
        start = (start + 1) % filtered_passwords.count;
      } else if (key == XK_Down) {
        start = (start + filtered_passwords.count - 1) % filtered_passwords.count;
      }

      if (key == XK_BackSpace) {
        if (length > 0)
          input[--length] = '\0';
      } else if (n > 0 && length + n < INPUT_SIZE) {
        start = 0;
        memcpy(input + length, buffer, n);
        length += n;
        input[length] = '\0';
      }

      filtered_passwords.count = 0;
      for (size_t i = 0; i < passwords.count; ++i) {
        if (strstr(passwords.items[i], input)) {
          arr_append(filtered_passwords, passwords.items[i]);
        }
      }

      XClearArea(display, win, 0, 0, 0, 0, True);
    }
  }

  XftColorFree(display, DefaultVisual(display, screen), DefaultColormap(display, screen), &color);
  XftColorFree(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
               &color_highlight);
  XftDrawDestroy(draw);
  XftFontClose(display, font_large);

  XFreeGC(display, gc);
  XDestroyWindow(display, win);
  XCloseDisplay(display);

  return 0;
}
