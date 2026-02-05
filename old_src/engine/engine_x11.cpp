#ifdef PLATFORM_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <vector>

#include "engine.h"
#include "sprite.h"

#include "bkgimage.h"

#include "../vendor/miniaudio.h"
#include <map>
#include <string>

// The X11 implementation of Engine will be our reference implementation for
// MONOTEST

class EngineX11 : public Engine {
private:
  Display *display;
  Window window;
  Pixmap canvas;
  Pixmap back_buffer; // New back buffer
  GC window_gc;
  GC canvas_gc;
  int screen;
  bool running;
  int window_width;
  int window_height;
  int canvas_width;
  int canvas_height;
  int scale;

  // Background
  BkgImage *active_background;
  BkgImage *default_background;
  XImage *bg_ximage; // Reusable wrapper for background data
  bool is_even_phase;
  Pixmap even_mask;
  Pixmap odd_mask;

  // Pixel buffer for rendering
  char *image_data;
  XImage *ximage;

  // Audio State
  ma_engine engine;
  bool audio_initialized;

  struct CachedSound {
    void *pData;
    ma_uint64 frameCount;
  };

  std::map<std::string, CachedSound> sound_cache;

  struct ActiveVoice {
    ma_sound sound;
    ma_audio_buffer buffer;
    bool active;
  };

  std::vector<ActiveVoice *> active_voices;
  std::string exe_dir;

public:
  EngineX11()
      : display(nullptr), running(false), active_background(nullptr),
        default_background(nullptr), bg_ximage(nullptr), is_even_phase(true),
        even_mask(0), odd_mask(0), image_data(nullptr), ximage(nullptr),
        audio_initialized(false) {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
      exe_dir = dirname(result);
    } else {
      exe_dir = ".";
    }
  }

  bool init(int width, int height, int scale_factor) override {
    if (width % 32 != 0) {
      std::cerr << "Error: Canvas width must be a multiple of 32." << std::endl;
      return false;
    }

    canvas_width = width;
    canvas_height = height;
    scale = scale_factor;

    // Create Default Background (White)
    // 32-pixel aligned width
    int32_t width_in_words = width / 32;
    size_t total_bytes = width_in_words * 4 * height;
    default_background = (BkgImage *)malloc(sizeof(BkgImage) + total_bytes);
    if (!default_background)
      return false;

    default_background->width = width;
    default_background->height = height;
    default_background->width_in_words = width_in_words;
    default_background->_padding = 0;
    memset(default_background->pixels, 0x00, total_bytes);

    active_background = default_background;

    display = XOpenDisplay(nullptr);
    if (display == nullptr) {
      std::cerr << "Failed to open X display" << std::endl;
      return false;
    }

    screen = DefaultScreen(display);

    // Initial window size
    window_width = canvas_width * scale;
    window_height = canvas_height * scale;

    // Create window
    window = XCreateSimpleWindow(
        display, RootWindow(display, screen), 0, 0, window_width, window_height,
        1, BlackPixel(display, screen), WhitePixel(display, screen));

    XStoreName(display, window, "MONOTEST");
    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    window_gc = XCreateGC(display, window, 0, nullptr);

    // Initial Back Buffer
    back_buffer = XCreatePixmap(display, window, window_width, window_height,
                                DefaultDepth(display, screen));

    // Create low-res canvas (Pixmap)
    canvas = XCreatePixmap(display, window, canvas_width, canvas_height,
                           DefaultDepth(display, screen));
    canvas_gc = XCreateGC(display, canvas, 0, nullptr);

    // Create reusable XImage for background
    // We create it once since dimensions are enforced.
    // depth=1, format=XYBitmap, offset=0, data=NULL (set at draw time)
    // width, height, bitmap_pad=32, bytes_per_line=0 (auto)
    bg_ximage =
        XCreateImage(display, DefaultVisual(display, screen), 1, XYBitmap, 0,
                     nullptr, canvas_width, canvas_height, 32, 0);
    if (bg_ximage) {
      bg_ximage->bitmap_bit_order = MSBFirst; // PBM is MSB first
      bg_ximage->byte_order = MSBFirst;       // Matches file read
      bg_ximage->bitmap_bit_order = MSBFirst; // PBM is MSB first
      bg_ximage->byte_order = MSBFirst;       // Matches file read
    }

    // Create Interlace Masks (Stencils)
    // 1-bit depth maps where 1=Draw, 0=Mask
    even_mask = XCreatePixmap(display, window, canvas_width, canvas_height, 1);
    odd_mask = XCreatePixmap(display, window, canvas_width, canvas_height, 1);

    GC mask_gc = XCreateGC(display, even_mask, 0, nullptr);

    // EVEN MASK: Rows 0, 2, 4... are 1 (Draw), others 0
    XSetForeground(display, mask_gc, 0);
    XFillRectangle(display, even_mask, mask_gc, 0, 0, canvas_width,
                   canvas_height);
    XSetForeground(display, mask_gc, 1);
    for (int y = 0; y < canvas_height; y += 2) {
      XDrawLine(display, even_mask, mask_gc, 0, y, canvas_width, y);
    }

    // ODD MASK: Rows 1, 3, 5... are 1 (Draw), others 0
    XSetForeground(display, mask_gc, 0);
    XFillRectangle(display, odd_mask, mask_gc, 0, 0, canvas_width,
                   canvas_height);
    XSetForeground(display, mask_gc, 1);
    for (int y = 1; y < canvas_height; y += 2) {
      XDrawLine(display, odd_mask, mask_gc, 0, y, canvas_width, y);
    }

    XFreeGC(display, mask_gc);
    ma_engine_config audio_config = ma_engine_config_init();
    audio_config.channels = 2;
    audio_config.sampleRate = 22050; // Retro sample rate

    if (ma_engine_init(&audio_config, &engine) == MA_SUCCESS) {
      audio_initialized = true;
      std::cout << "Audio initialized successfully." << std::endl;
    } else {
      std::cerr << "Failed to initialize audio engine." << std::endl;
    }

    running = true;
    return true;
  }

  bool process_events() override {
    // Clean up finished voices
    if (audio_initialized) {
      for (auto it = active_voices.begin(); it != active_voices.end();) {
        ActiveVoice *voice = *it;
        if (ma_sound_at_end(&voice->sound)) {
          ma_sound_uninit(&voice->sound);
          ma_audio_buffer_uninit(&voice->buffer);
          delete voice;
          it = active_voices.erase(it);
        } else {
          ++it;
        }
      }
    }

    XEvent event;
    while (XPending(display) > 0) {
      XNextEvent(display, &event);
      switch (event.type) {
      case ConfigureNotify: {
        XConfigureEvent xce = event.xconfigure;
        if (xce.width != window_width || xce.height != window_height) {
          window_width = xce.width;
          window_height = xce.height;
          // Resize back buffer
          if (back_buffer)
            XFreePixmap(display, back_buffer);
          back_buffer =
              XCreatePixmap(display, window, window_width, window_height,
                            DefaultDepth(display, screen));
        }
        break;
      }
      case KeyPress: {
        KeySym key = XLookupKeysym(&event.xkey, 0);
        if (key == XK_F6) {
          toggle_pixel_perfect();
        } else if (key == XK_F7) {
          toggle_invert_colors();
        } else if (key == XK_F8) {
          toggle_interlace();
        } else if (key == XK_F9) {
          toggle_dead_space_color();
        }
        break;
      }
      case ClientMessage:
        // Handle close
        break;
      }
    }
    return running;
  }

  void draw_start() override {
    // Toggle phase every frame if in interlaced mode
    if (interlaced_mode) {
      is_even_phase = !is_even_phase;
    } else {
      is_even_phase = true; // Reset to full draw or consistent state
    }

    if (!active_background) {
      XSetForeground(display, canvas_gc, WhitePixel(display, screen));
      XFillRectangle(display, canvas, canvas_gc, 0, 0, canvas_width,
                     canvas_height);
      return;
    }

    if (bg_ximage) {
      // Point data to the active background pixels
      bg_ximage->data = (char *)active_background->pixels;

      // Set colors for 1->Black, 0->White translation (XYBitmap)
      XSetForeground(display, canvas_gc, BlackPixel(display, screen));
      XSetBackground(display, canvas_gc, WhitePixel(display, screen));

      if (interlaced_mode) {
        // Optimization via Clip Mask
        XSetClipMask(display, canvas_gc, is_even_phase ? even_mask : odd_mask);
        XSetClipOrigin(display, canvas_gc, 0, 0);

        // Bulk Transfer using XPutImage (Masked by Server)
        XPutImage(display, canvas, canvas_gc, bg_ximage, 0, 0, 0, 0,
                  canvas_width, canvas_height);

        // Clear clip mask for subsequent drawing operations
        XSetClipMask(display, canvas_gc, None);
      } else {
        // Bulk Transfer using XPutImage (Optimization)
        XPutImage(display, canvas, canvas_gc, bg_ximage, 0, 0, 0, 0,
                  canvas_width, canvas_height);
      }

      // Unset data to prevent double free or dangling pointers
      bg_ximage->data = nullptr;
    } else {
      // Fallback
      XSetForeground(display, canvas_gc, WhitePixel(display, screen));
      XFillRectangle(display, canvas, canvas_gc, 0, 0, canvas_width,
                     canvas_height);
    }
  }

  void draw_lists() override {
    // Draw for all foreground drawables
    for (int i = 0; i < foreground_drawables_count; i++) {
      ForegroundDrawable &fd = foreground_drawables[i];
      if (!fd.sprite || !fd.mask)
        continue;

      // Handle Hidden Flag
      if (fd.flags & DRAW_FLAG_HIDDEN)
        continue;

      int width = fd.sprite->width;
      int height = fd.sprite->height;
      int stride_bytes = fd.sprite->width_in_words * 4;

      uint8_t *sprite_bytes = (uint8_t *)fd.sprite->pixels;
      uint8_t *mask_bytes = (uint8_t *)fd.mask->pixels;

      // Invert Mode: Skip erasure, just invert destination where Ink is present
      bool invert = (fd.flags & DRAW_FLAG_INVERT);

      // Determine loop parameters based on interlacing
      int y_step = interlaced_mode ? 2 : 1;
      int start_y = 0;

      if (interlaced_mode) {
        // Calculate the first local 'y' that corresponds to a screen 'y'
        // matching the current phase.
        // Screen Y = fd.y + y
        // We want (fd.y + y) % 2 == (is_even_phase ? 0 : 1)
        int desired_parity = is_even_phase ? 0 : 1;
        int current_parity = (fd.y) & 1; // parity of fd.y + 0

        if (current_parity != desired_parity) {
          start_y = 1;
        }
      }

      // Pass 1: Draw Erasure (White) - ONLY if not inverting
      if (!invert) {
        XSetForeground(display, canvas_gc, WhitePixel(display, screen));
        for (int y = start_y; y < height; y += y_step) {
          for (int byte_col = 0; byte_col < stride_bytes; byte_col++) {
            int idx = y * stride_bytes + byte_col;
            uint8_t s_byte = sprite_bytes[idx];
            uint8_t m_byte = mask_bytes[idx];

            if (m_byte == 0)
              continue;

            for (int bit = 0; bit < 8; bit++) {
              int px_offset = byte_col * 8 + bit;
              if (px_offset >= width)
                break;

              int shift = 7 - bit;
              bool is_opaque = (m_byte >> shift) & 1;
              bool is_ink = (s_byte >> shift) & 1;

              if (is_opaque && !is_ink) {
                XDrawPoint(display, canvas, canvas_gc, fd.x + px_offset,
                           fd.y + y);
              }
            }
          }
        }
      }

      // Pass 2: Draw Ink (Black or Invert)
      if (invert) {
        XSetFunction(display, canvas_gc, GXinvert);
      } else {
        XSetForeground(display, canvas_gc, BlackPixel(display, screen));
      }

      for (int y = start_y; y < height; y += y_step) {

        for (int byte_col = 0; byte_col < stride_bytes; byte_col++) {
          int idx = y * stride_bytes + byte_col;
          uint8_t s_byte = sprite_bytes[idx];
          uint8_t m_byte = mask_bytes[idx];

          if (m_byte == 0)
            continue;

          for (int bit = 0; bit < 8; bit++) {
            int px_offset = byte_col * 8 + bit;
            if (px_offset >= width)
              break;

            int shift = 7 - bit;
            bool is_opaque = (m_byte >> shift) & 1;
            bool is_ink = (s_byte >> shift) & 1;

            if (is_opaque && is_ink) {
              XDrawPoint(display, canvas, canvas_gc, fd.x + px_offset,
                         fd.y + y);
            }
          }
        }
      }

      if (invert) {
        XSetFunction(display, canvas_gc, GXcopy); // Restore normal mode
      }
    }
  }

  void draw_end() override {
    // Determine colors
    // Paper Color for Dead Space
    unsigned long dead_space_color = dead_space_white
                                         ? WhitePixel(display, screen)
                                         : BlackPixel(display, screen);

    // Ink/Paper for Canvas Content (Inversion Logic)
    unsigned long paper_color = invert_colors ? BlackPixel(display, screen)
                                              : WhitePixel(display, screen);
    unsigned long ink_color = invert_colors ? WhitePixel(display, screen)
                                            : BlackPixel(display, screen);

    // 1. Draw to Back Buffer (Clear with Dead Space Color)
    XSetForeground(display, window_gc, dead_space_color);
    XFillRectangle(display, back_buffer, window_gc, 0, 0, window_width,
                   window_height);

    XImage *canvas_img = XGetImage(display, canvas, 0, 0, canvas_width,
                                   canvas_height, AllPlanes, ZPixmap);
    if (!canvas_img)
      return;

    if (pixel_perfect_mode) {
      int scale_x = window_width / canvas_width;
      int scale_y = window_height / canvas_height;
      int current_scale = (scale_x < scale_y) ? scale_x : scale_y;
      if (current_scale < 1)
        current_scale = 1;

      int offset_x = (window_width - (canvas_width * current_scale)) / 2;
      int offset_y = (window_height - (canvas_height * current_scale)) / 2;

      // Fill the canvas area with Paper Color (to ensure opacity over dead
      // space)
      XSetForeground(display, window_gc, paper_color);
      XFillRectangle(display, back_buffer, window_gc, offset_x, offset_y,
                     canvas_width * current_scale,
                     canvas_height * current_scale);

      // Draw Ink
      XSetForeground(display, window_gc, ink_color);

      for (int y = 0; y < canvas_height; y++) {
        for (int x = 0; x < canvas_width; x++) {
          unsigned long pixel = XGetPixel(canvas_img, x, y);
          // If the pixel on canvas is Black (Ink), we draw it as 'ink_color' on
          // screen
          if (pixel == BlackPixel(display, screen)) {
            XFillRectangle(
                display, back_buffer, window_gc, offset_x + x * current_scale,
                offset_y + y * current_scale, current_scale, current_scale);
          }
        }
      }
    } else {
      // Stretched Mode - No dead space visible (fills window)
      // But we still need to fill with paper color first
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
          if (pixel == BlackPixel(display, screen)) {
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

    // 2. Flip: Copy Back Buffer to Window
    XCopyArea(display, back_buffer, window, window_gc, 0, 0, window_width,
              window_height, 0, 0);

    XFlush(display);
  }

  void set_active_background(BkgImage *bkg) override {
    if (bkg) {
      if (bkg->width != canvas_width || bkg->height != canvas_height) {
        std::cerr << "Error: Active background size mismatch! Expected "
                  << canvas_width << "x" << canvas_height << ", got "
                  << bkg->width << "x" << bkg->height << std::endl;
        return;
      }
      active_background = bkg;
    } else {
      active_background = default_background;
    }
  }

  BkgImage *get_active_background() override { return active_background; }

  // ... (Audio stubs unchanged) ...
  bool is_running() override { return running; }
  unsigned long get_time_ms() override {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
  }
  void sleep_ms(int ms) override { usleep(ms * 1000); }
  void play_sound(const char *filename) override {
    if (!audio_initialized)
      return;

    // Ensure loaded
    load_sound(filename);

    if (sound_cache.find(filename) == sound_cache.end())
      return; // Load failed

    CachedSound &cached = sound_cache[filename];

    ActiveVoice *voice = new ActiveVoice();
    voice->active = true;

    // Re-configure to not copy data
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        ma_format_f32, 2, cached.frameCount, cached.pData, NULL);
    ma_result result = ma_audio_buffer_init(&config, &voice->buffer);

    if (result != MA_SUCCESS) {
      std::cerr << "Failed to create audio buffer for " << filename
                << std::endl;
      delete voice;
      return;
    }

    result = ma_sound_init_from_data_source(&engine, &voice->buffer, 0, NULL,
                                            &voice->sound);
    if (result != MA_SUCCESS) {
      std::cerr << "Failed to init sound for " << filename << std::endl;
      ma_audio_buffer_uninit(&voice->buffer);
      delete voice;
      return;
    }

    ma_sound_start(&voice->sound);
    active_voices.push_back(voice);
  }

  void load_sound(const char *filename) override {
    if (!audio_initialized)
      return;
    if (sound_cache.find(filename) != sound_cache.end())
      return;

    std::string path = exe_dir + "/" + filename;

    ma_decoder_config config = ma_decoder_config_init(
        ma_format_f32, 2, ma_engine_get_sample_rate(&engine));

    ma_uint64 frameCount;
    void *pData;

    ma_result result =
        ma_decode_file(path.c_str(), &config, &frameCount, &pData);
    if (result != MA_SUCCESS) {
      std::cout << "Failed to load sound: " << path << " (Error " << result
                << ")" << std::endl;
      return;
    } else {
      std::cout << "Loaded sound: " << path << std::endl;
    }

    CachedSound sound;
    sound.pData = pData;
    sound.frameCount = frameCount;
    sound_cache[filename] = sound;
  }

  void clear_sounds() override {
    // Stop all voices
    for (auto *voice : active_voices) {
      ma_sound_stop(&voice->sound);
      ma_sound_uninit(&voice->sound);
      ma_audio_buffer_uninit(&voice->buffer);
      delete voice;
    }
    active_voices.clear();

    // Free cached data
    for (auto &pair : sound_cache) {
      ma_free(pair.second.pData, NULL);
    }
    sound_cache.clear();
  }
  int get_width() const override { return canvas_width; }
  int get_height() const override { return canvas_height; }

  ~EngineX11() {
    clear_sounds();
    if (audio_initialized) {
      ma_engine_uninit(&engine);
    }
    if (display) {
      if (default_background)
        free(default_background);
      if (even_mask)
        XFreePixmap(display, even_mask);
      if (odd_mask)
        XFreePixmap(display, odd_mask);
      if (bg_ximage) {
        bg_ximage->data = nullptr; // Ensure we don't free external data
        XDestroyImage(bg_ximage);
      }
      XFreePixmap(display, back_buffer); // Clean up
      XFreePixmap(display, canvas);
      XFreeGC(display, canvas_gc);
      XFreeGC(display, window_gc);
      XDestroyWindow(display, window);
      XCloseDisplay(display);
    }
  }
};

Engine *create_engine() { return new EngineX11(); }

#endif // PLATFORM_X11
