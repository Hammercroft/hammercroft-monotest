#include "../engine.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstring>
#include <iostream>
#include <unistd.h> // for usleep fallback if needed

// PLATFORM-SIDE IMPLEMENTATION, X11, SOURCE

namespace mtengine {

// --- X11 Global State for Singleton ---
static Display *display = nullptr;
static Window window = 0;
static int screen = 0;
static GC window_gc = 0;

static Pixmap canvas = 0;
static GC canvas_gc = 0;
static Pixmap back_buffer = 0;

// XImage wrapper for background data (does not own data)
static XImage *bg_ximage = nullptr;

static int window_width = 0;
static int window_height = 0;
static int canvas_width = 0;
static int canvas_height = 0;
static float canvas_scale = 1.0f;

// --- Engine Implementation ---

Engine::Engine() {}

Engine::~Engine() {
  std::cout << "Engine::~Engine() cleaning up X11...\n";
  if (display) {
    if (bg_ximage) {
      bg_ximage->data = nullptr; // Important: prevent XDestroyImage from
                                 // freeing our asset data!
      XDestroyImage(bg_ximage);
      bg_ximage = nullptr;
    }
    if (back_buffer)
      XFreePixmap(display, back_buffer);
    if (canvas)
      XFreePixmap(display, canvas);
    if (canvas_gc)
      XFreeGC(display, canvas_gc);
    if (window_gc)
      XFreeGC(display, window_gc);
    if (window)
      XDestroyWindow(display, window);
    XCloseDisplay(display);
    display = nullptr;
  }
}

bool Engine::init(int width, int height, float scale, size_t sprite_mem_size,
                  size_t bkg_mem_size) {
  std::cout << "Engine::init() called for X11\n";

  canvas_width = width;
  canvas_height = height;
  canvas_scale = scale;

  // Enforce multiple of 32 for width (optimization/requirement for PBM/XImage
  // alignment)
  if (width % 32 != 0) {
    std::cerr << "Error: Canvas width must be a multiple of 32.\n";
    return false;
  }

  init_asset_management(sprite_mem_size, bkg_mem_size);

  display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    std::cerr << "Failed to open X display\n";
    return false;
  }

  screen = DefaultScreen(display);

  window_width = (int)(canvas_width * canvas_scale);
  window_height = (int)(canvas_height * canvas_scale);

  // Create Window
  window = XCreateSimpleWindow(
      display, RootWindow(display, screen), 0, 0, window_width, window_height,
      1, BlackPixel(display, screen), WhitePixel(display, screen));

  XStoreName(display, window, "MONOTEST X11");
  // Allow window to be closed by window manager
  Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

  XSelectInput(display, window,
               ExposureMask | KeyPressMask | StructureNotifyMask);
  XMapWindow(display, window);

  window_gc = XCreateGC(display, window, 0, nullptr);

  // Create Back Buffer (matches window size)
  back_buffer = XCreatePixmap(display, window, window_width, window_height,
                              DefaultDepth(display, screen));

  // Create Canvas (matches game resolution)
  canvas = XCreatePixmap(display, window, canvas_width, canvas_height,
                         DefaultDepth(display, screen));
  canvas_gc = XCreateGC(display, canvas, 0, nullptr);

  // Create Reusable XImage for Background (1-bit depth)
  bg_ximage = XCreateImage(display, DefaultVisual(display, screen), 1, XYBitmap,
                           0, nullptr, canvas_width, canvas_height, 32, 0);

  if (bg_ximage) {
    bg_ximage->bitmap_bit_order = MSBFirst;
    bg_ximage->byte_order = MSBFirst;
  } else {
    std::cerr << "Failed to create bg_ximage\n";
    return false;
  }

  std::cout << "X11 Init Complete: " << window_width << "x" << window_height
            << "\n";
  return true;
}

void Engine::poll_events() {
  if (!display)
    return;

  while (XPending(display) > 0) {
    XEvent event;
    XNextEvent(display, &event);

    if (event.type == ClientMessage) {
      Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
      if ((Atom)event.xclient.data.l[0] == wmDeleteMessage) {
        main_game_loop_running = false;
      }
    } else if (event.type == KeyPress) {
      KeySym key = XLookupKeysym(&event.xkey, 0);
      if (key == XK_Escape) {
        main_game_loop_running = false;
      } else if (key == XK_F6) {
        pixel_perfect_mode = !pixel_perfect_mode;
        std::cout << "Pixel Perfect: " << pixel_perfect_mode << "\n";
      } else if (key == XK_F7) {
        invert_colors = !invert_colors;
        std::cout << "Invert Colors: " << invert_colors << "\n";
      } else if (key == XK_F9) {
        dead_space_is_white = !dead_space_is_white;
        std::cout << "Dead Space White: " << dead_space_is_white << "\n";
      }
    }

    // Handle Window Resize
    if (event.type == ConfigureNotify) {
      XConfigureEvent xce = event.xconfigure;
      if (xce.width != window_width || xce.height != window_height) {
        window_width = xce.width;
        window_height = xce.height;
        if (back_buffer)
          XFreePixmap(display, back_buffer);
        back_buffer =
            XCreatePixmap(display, window, window_width, window_height,
                          DefaultDepth(display, screen));
      }
    }
  }
}

void Engine::draw_prepare() {
  if (!display)
    return;

  if (active_background) {
    // Draw 1-bit background
    // X11 XYBitmap: Set Foreground is '1' in bitmap, Background is '0'

    // PBM implies 1=Black, 0=White usually?
    // Old code:
    // XSetForeground(..., BlackPixel(..)); // for bit 1
    // XSetBackground(..., WhitePixel(..)); // for bit 0

    XSetForeground(display, canvas_gc, BlackPixel(display, screen));
    XSetBackground(display, canvas_gc, WhitePixel(display, screen));

    bg_ximage->data = (char *)active_background->pixels;

    XPutImage(display, canvas, canvas_gc, bg_ximage, 0, 0, 0, 0, canvas_width,
              canvas_height);

    bg_ximage->data = nullptr; // Safety

  } else {
    // Clear to white if no background
    XSetForeground(display, canvas_gc, WhitePixel(display, screen));
    XFillRectangle(display, canvas, canvas_gc, 0, 0, canvas_width,
                   canvas_height);
  }
}

void Engine::draw_lists() {
  // "No sprites yet" per instruction.
}

void Engine::draw_present() {
  if (!display)
    return;

  XImage *canvas_img = XGetImage(display, canvas, 0, 0, canvas_width,
                                 canvas_height, AllPlanes, ZPixmap);
  if (!canvas_img)
    return;

  // Determine colors
  unsigned long dead_space_color = dead_space_is_white
                                       ? WhitePixel(display, screen)
                                       : BlackPixel(display, screen);

  unsigned long paper_color =
      invert_colors ? BlackPixel(display, screen) : WhitePixel(display, screen);
  unsigned long ink_color =
      invert_colors ? WhitePixel(display, screen) : BlackPixel(display, screen);

  // 1. Fill Back Buffer with Dead Space Color
  XSetForeground(display, window_gc, dead_space_color);
  XFillRectangle(display, back_buffer, window_gc, 0, 0, window_width,
                 window_height);

  unsigned long black_pixel = BlackPixel(display, screen);

  if (pixel_perfect_mode) {
    // Integer Scaling logic
    int scale_x = window_width / canvas_width;
    int scale_y = window_height / canvas_height;
    int current_scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (current_scale < 1)
      current_scale = 1;

    int offset_x = (window_width - (canvas_width * current_scale)) / 2;
    int offset_y = (window_height - (canvas_height * current_scale)) / 2;

    // Fill Canvas Area with Paper Color
    XSetForeground(display, window_gc, paper_color);
    XFillRectangle(display, back_buffer, window_gc, offset_x, offset_y,
                   canvas_width * current_scale, canvas_height * current_scale);

    // Draw Ink
    XSetForeground(display, window_gc, ink_color);

    for (int y = 0; y < canvas_height; y++) {
      for (int x = 0; x < canvas_width; x++) {
        unsigned long pixel = XGetPixel(canvas_img, x, y);
        // Check for Ink (Black on Canvas)
        if (pixel == black_pixel) {
          XFillRectangle(
              display, back_buffer, window_gc, offset_x + x * current_scale,
              offset_y + y * current_scale, current_scale, current_scale);
        }
      }
    }
  } else {
    // Stretched Mode
    XSetForeground(display, window_gc, paper_color);
    XFillRectangle(display, back_buffer, window_gc, 0, 0, window_width,
                   window_height);

    XSetForeground(display, window_gc, ink_color);

    float scale_x_f = (float)window_width / canvas_width;
    float scale_y_f = (float)window_height / canvas_height;

    for (int y = 0; y < canvas_height; y++) {
      int dest_y = (int)(y * scale_y_f);
      int dest_h = (int)((y + 1) * scale_y_f) - dest_y;
      if (dest_h < 1)
        dest_h = 1;

      for (int x = 0; x < canvas_width; x++) {
        unsigned long pixel = XGetPixel(canvas_img, x, y);
        if (pixel == black_pixel) {
          int dest_x = (int)(x * scale_x_f);
          int dest_w = (int)((x + 1) * scale_x_f) - dest_x;
          if (dest_w < 1)
            dest_w = 1;

          XFillRectangle(display, back_buffer, window_gc, dest_x, dest_y,
                         dest_w, dest_h);
        }
      }
    }
  }

  XDestroyImage(canvas_img);

  // Flip
  XCopyArea(display, back_buffer, window, window_gc, 0, 0, window_width,
            window_height, 0, 0);

  XFlush(display);
}

} // namespace mtengine