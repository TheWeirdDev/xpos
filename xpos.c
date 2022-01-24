#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

struct MwmHints {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
};
enum {
  MWM_HINTS_FUNCTIONS = (1L << 0),
  MWM_HINTS_DECORATIONS = (1L << 1),

  MWM_FUNC_ALL = (1L << 0),
  MWM_FUNC_RESIZE = (1L << 1),
  MWM_FUNC_MOVE = (1L << 2),
  MWM_FUNC_MINIMIZE = (1L << 3),
  MWM_FUNC_MAXIMIZE = (1L << 4),
  MWM_FUNC_CLOSE = (1L << 5)
};

// unsigned long _RGB(int r,int g, int b)
// {
//     return b + (g<<8) + (r<<16);
// }

#define _NET_WM_STATE_REMOVE 0 // remove/unset property
#define _NET_WM_STATE_ADD 1    // add/set property
#define _NET_WM_STATE_TOGGLE 2 // toggle property

Bool MakeAlwaysOnTop(Display *display, Window root, Window mywin) {
  Atom wmStateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", 1);
  if (wmStateAbove == None) {
    printf("ERROR: cannot find atom for _NET_WM_STATE_ABOVE !\n");
    return False;
  }

  Atom wmNetWmState = XInternAtom(display, "_NET_WM_STATE", 1);
  if (wmNetWmState == None) {
    printf("ERROR: cannot find atom for _NET_WM_STATE !\n");
    return False;
  }

  // set window always on top hint
  if (wmStateAbove != None) {
    XClientMessageEvent xclient;
    memset(&xclient, 0, sizeof(xclient));
    //
    // window  = the respective client window
    // message_type = _NET_WM_STATE
    // format = 32
    // data.l[0] = the action, as listed below
    // data.l[1] = first property to alter
    // data.l[2] = second property to alter
    // data.l[3] = source indication (0-unk,1-normal app,2-pager)
    // other data.l[] elements = 0
    //
    xclient.type = ClientMessage;
    xclient.window = mywin;              // GDK_WINDOW_XID(window);
    xclient.message_type = wmNetWmState; // gdk_x11_get_xatom_by_name_for_display(
                                         // display, "_NET_WM_STATE" );
    xclient.format = 32;
    xclient.data.l[0] =
        _NET_WM_STATE_ADD; // add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    xclient.data.l[1] =
        wmStateAbove; // gdk_x11_atom_to_xatom_for_display (display, state1);
    xclient.data.l[2] =
        0; // gdk_x11_atom_to_xatom_for_display (display, state2);
    xclient.data.l[3] = 0;
    xclient.data.l[4] = 0;
    // gdk_wmspec_change_state( FALSE, window,
    //   gdk_atom_intern_static_string ("_NET_WM_STATE_BELOW"),
    //   GDK_NONE );
    XSendEvent(display,
               // mywin - wrong, not app window, send to root window!
               root, // <-- DefaultRootWindow( display )
               False, SubstructureRedirectMask | SubstructureNotifyMask,
               (XEvent *)&xclient);

    XFlush(display);

    return True;
  }

  return False;
}

int main(int argc, char **argv) {
  Display *display;
  Window root_window;

  /* Initialize (FIXME: no error checking). */
  display = XOpenDisplay(0);
  root_window = XRootWindow(display, 0);

  int s = DefaultScreen(display);

  // create window
  Window w =
      XCreateSimpleWindow(display, root_window, 10, 10, 120, 50, 1,
                          BlackPixel(display, s), WhitePixel(display, s));
  GC gc = DefaultGC(display, s);

  // set title bar name of window
  XStoreName(display, w, "XPos");

  XSelectInput(display, w, ExposureMask | KeyPressMask);
  XMapWindow(display, w);
  Atom mwmHintsProperty = XInternAtom(display, "_MOTIF_WM_HINTS", 0);
  struct MwmHints hints;
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = 0;
  XChangeProperty(display, w, mwmHintsProperty, mwmHintsProperty, 32,
                  PropModeReplace, (unsigned char *)&hints, 5);

  double alpha = 0.7;
  unsigned long opacity = (unsigned long)(0xFFFFFFFFul * alpha);
  Atom XA_NET_WM_WINDOW_OPACITY =
      XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
  XChangeProperty(display, w, XA_NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&opacity, 1L);
  /* check XInput */
  int xi_opcode, event, error;
  if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event,
                       &error)) {
    fprintf(stderr, "Error: XInput extension is not supported!\n");
    return 1;
  }

  /* Check XInput 2.0 */
  int major = 2;
  int minor = 0;
  int retval = XIQueryVersion(display, &major, &minor);
  if (retval != Success) {
    fprintf(stderr, "Error: XInput 2.0 is not supported (ancient X11?)\n");
    return 1;
  }

  /*
   * Set mask to receive XI_RawMotion events. Because it's raw,
   * XWarpPointer() events are not included, you can use XI_Motion
   * instead.
   */
  unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8] = {0}; /* must be zeroed! */
  XISetMask(mask_bytes, XI_RawMotion);

  /* Set mask to receive events from all master devices */
  XIEventMask evmasks[1];
  /* You can use XIAllDevices for XWarpPointer() */
  evmasks[0].deviceid = XIAllMasterDevices;
  evmasks[0].mask_len = sizeof(mask_bytes);
  evmasks[0].mask = mask_bytes;
  XISelectEvents(display, root_window, evmasks, 1);

  char msg[20] = {0};
  char msg2[20] = {0};
  XEvent xevent;

  Window root_return, child_return;
  int root_x_return, root_y_return;
  int win_x_return, win_y_return;
  unsigned int mask_return;

  int retval2 = XQueryPointer(display, root_window, &root_return, &child_return,
                              &root_x_return, &root_y_return, &win_x_return,
                              &win_y_return, &mask_return);
  if (!retval2) {
    fprintf(stderr, "XQueryPointer failed\n");
    return 1;
  }

  /* We used root window as its reference, so both should be the same */
  assert(root_x_return == win_x_return);
  assert(root_y_return == win_y_return);
  sprintf(msg, "x: %d", root_x_return);
  sprintf(msg2, "y: %d", root_y_return);
  XMoveWindow(display, w, root_x_return + 5, root_y_return + 5);
  MakeAlwaysOnTop(display, root_window, w);

  while (1) {
    XNextEvent(display, &xevent);
    if (xevent.type == Expose) {
      XClearWindow(display, w);

      XFontStruct *font;
      char *name = "-*-dejavu sans-bold-r-*-*-*-150-100-100-*-*-iso8859-1";
      font = XLoadQueryFont(display, name);
      XSetFont(display, gc, font->fid);
      XDrawString(display, w, gc, 20, 20, msg, strlen(msg));
      XDrawString(display, w, gc, 20, 40, msg2, strlen(msg2));
    }

    if (xevent.xcookie.type != GenericEvent ||
        xevent.xcookie.extension != xi_opcode) {
      /* not an XInput event */
      continue;
    }
    XGetEventData(display, &xevent.xcookie);
    if (xevent.xcookie.evtype != XI_RawMotion) {
      /*
       * Not an XI_RawMotion event (you may want to detect
       * XI_Motion as well, see comments above).
       */
      XFreeEventData(display, &xevent.xcookie);
      continue;
    }
    XFreeEventData(display, &xevent.xcookie);

    Window root_return, child_return;
    int root_x_return, root_y_return;
    int win_x_return, win_y_return;
    unsigned int mask_return;
    /*
     * We need:
     *     child_return - the active window under the cursor
     *     win_{x,y}_return - pointer coordinate with respect to root window
     */
    int retval = XQueryPointer(display, root_window, &root_return,
                               &child_return, &root_x_return, &root_y_return,
                               &win_x_return, &win_y_return, &mask_return);
    if (!retval) {
      /* pointer is not in the same screen, ignore */
      continue;
    }

    /* We used root window as its reference, so both should be the same */
    assert(root_x_return == win_x_return);
    assert(root_y_return == win_y_return);

    if (child_return) {
      int local_x, local_y;
      XTranslateCoordinates(display, root_window, child_return, root_x_return,
                            root_y_return, &local_x, &local_y, &child_return);
      XMoveWindow(display, w, root_x_return + 5, root_y_return + 5);
      sprintf(msg, "x: %d", root_x_return);
      sprintf(msg2, "y: %d", root_y_return);
      XEvent exppp;

      memset(&exppp, 0, sizeof(exppp));
      exppp.type = Expose;
      exppp.xexpose.window = w;
      XSendEvent(display, w, False, ExposureMask, &exppp);
      XFlush(display);
    }
  }

  XCloseDisplay(display);

  return 0;
}
