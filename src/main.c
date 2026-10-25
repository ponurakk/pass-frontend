#include "crypto.h"
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <gpgme.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 100
#define PADDING 20
#define INPUT_SIZE 512

int main() {
  gpgme_check_version(NULL);

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

  XQueryPointer(display, root, &root_return, &child_return, &mouse_x, &mouse_y,
                &win_x, &win_y, &mask);

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

  Window win =
      XCreateWindow(display, root, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                    CopyFromParent, InputOutput, CopyFromParent,
                    CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

  XStoreName(display, win, "pass-frontend");
  XClassHint class_hint = {.res_name = "pass-frontend",
                           .res_class = "PassFrontend"};
  XSetClassHint(display, win, &class_hint);

  XMapRaised(display, win);
  XSetInputFocus(display, win, RevertToPointerRoot, CurrentTime);
  XFlush(display);

  GC gc = XCreateGC(display, win, 0, NULL);

  const unsigned long bg = 0x1A1A1A;
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

  XftDraw *draw = XftDrawCreate(display, win, DefaultVisual(display, screen),
                                DefaultColormap(display, screen));

  XftColor color;
  XftColorAllocName(display, DefaultVisual(display, screen),
                    DefaultColormap(display, screen), "#ffffff", &color);

  char input[INPUT_SIZE] = "";
  size_t length = 0;

  XGrabPointer(display, win, False, ButtonPressMask, GrabModeAsync,
               GrabModeAsync, None, None, CurrentTime);

  XEvent event;
  while (1) {
    XNextEvent(display, &event);

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

      XftDrawStringUtf8(draw, &color, font_small, PADDING, 30,
                        (const FcChar8 *)input, (int)length);
    }

    if (event.type == KeyPress) {
      KeySym key;
      char buffer[32];

      int n = XLookupString(&event.xkey, buffer, sizeof(buffer), &key, NULL);

      if (key == XK_Escape) {
        break;
      }

      if (key == XK_Return) {
        char *password;
        if (decrypt_file("./tmp.gpg", &password) != 0) {
          break;
        }

        printf("Password: %s\n", password);

        XClearArea(display, win, 0, 0, 0, 0, True);
        break;
      }

      if (key == XK_BackSpace) {
        if (length > 0)
          input[--length] = '\0';
      } else if (n > 0 && length + (size_t)n < INPUT_SIZE) {
        memcpy(input + length, buffer, (size_t)n);
        length += (size_t)n;
        input[length] = '\0';
      }

      XClearArea(display, win, 0, 0, 0, 0, True);
    }
  }

  XftColorFree(display, DefaultVisual(display, screen),
               DefaultColormap(display, screen), &color);
  XftDrawDestroy(draw);
  XftFontClose(display, font_large);

  XFreeGC(display, gc);
  XDestroyWindow(display, win);
  XCloseDisplay(display);

  // char *password;
  // if (decrypt_file("./tmp.gpg", &password) != 0) {
  //   fprintf(stderr, "Failed to decrypt file\n");
  //   return 1;
  // }

  // printf("%s\n", password);

  return 0;
}
