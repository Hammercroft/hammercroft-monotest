#ifdef PLATFORM_XCB

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>
#include <libgen.h>
#include <limits.h>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>
#include <xcb/xcb.h>

#include "bkgimage.h"
#include "engine.h"
// Miniaudio
#include "../vendor/miniaudio.h"
#include "sprite.h"

// XCB Implementation of Engine
// Mirrors EngineX11 functionality

class EngineXCB : public Engine {
private:
  xcb_connection_t *connection;
  xcb_window_t window;
  xcb_screen_t *screen;
  xcb_gcontext_t window_gc;
  xcb_gcontext_t canvas_gc;    // GC for drawing to canvas
  xcb_atom_t wm_delete_window; // Atom for window close event

  // Rendering
  xcb_pixmap_t canvas;      // Screen depth pixmap (easier for XCB blit)
  xcb_pixmap_t back_buffer; // Scaled back buffer

  // Image data for Background
  // XCB doesn't have "XImage" exactly like X11, we pass raw data to put_image
  // We will assume BG is 1-bit XYBitmap

  // Interlacing Masks
  xcb_pixmap_t even_mask;
  xcb_pixmap_t odd_mask;
  bool is_even_phase;

  bool running;
  int window_width;
  int window_height;
  int canvas_width;
  int canvas_height;
  int scale;

  // Background
  BkgImage *active_background;
  BkgImage *default_background;

  // Audio State
  ma_engine audio_engine;
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
  EngineXCB()
      : connection(nullptr), screen(nullptr), is_even_phase(true),
        running(false), active_background(nullptr), default_background(nullptr),
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
    window_width = width * scale;
    window_height = height * scale;

    // Default Background
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

    // Connect XCB
    int screen_num;
    connection = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(connection)) {
      std::cerr << "Error opening XCB connection" << std::endl;
      return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; ++i) {
      xcb_screen_next(&iter);
    }
    screen = iter.data;

    // Create Window
    window = xcb_generate_id(connection);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0,
                      0, window_width, window_height, 1,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask,
                      values);

    // Hints for Closing Window
    xcb_intern_atom_cookie_t protocol_cookie =
        xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *protocol_reply =
        xcb_intern_atom_reply(connection, protocol_cookie, 0);
    xcb_atom_t wm_protocols = protocol_reply->atom;
    free(protocol_reply);

    xcb_intern_atom_cookie_t delete_cookie =
        xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *delete_reply =
        xcb_intern_atom_reply(connection, delete_cookie, 0);
    wm_delete_window = delete_reply->atom;
    free(delete_reply);

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, wm_protocols,
                        XCB_ATOM_ATOM, 32, 1, &wm_delete_window);

    const char *title = "MONOTEST (XCB)";
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title),
                        title);

    xcb_map_window(connection, window);

    // GCs
    window_gc = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = screen->black_pixel;
    values[1] = 0;
    xcb_create_gc(connection, window_gc, window, mask, values);

    // Create Canvas
    canvas = xcb_generate_id(connection);
    xcb_void_cookie_t cookie =
        xcb_create_pixmap_checked(connection, screen->root_depth, canvas,
                                  window, canvas_width, canvas_height);
    xcb_generic_error_t *error = xcb_request_check(connection, cookie);
    if (error) {
      std::cerr << "Error creating canvas pixmap: " << error->error_code
                << std::endl;
      free(error);
      return false;
    }

    canvas_gc = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    values[0] = screen->black_pixel;
    values[1] = screen->white_pixel;
    values[2] = 0;
    xcb_create_gc(connection, canvas_gc, canvas, mask, values);

    // Back buffer
    back_buffer = xcb_generate_id(connection);
    xcb_create_pixmap(connection, screen->root_depth, back_buffer, window,
                      window_width, window_height);

    // Masks logic (1-bit pixmaps)
    even_mask = xcb_generate_id(connection);
    odd_mask = xcb_generate_id(connection);
    xcb_create_pixmap(connection, 1, even_mask, window, canvas_width,
                      canvas_height);
    xcb_create_pixmap(connection, 1, odd_mask, window, canvas_width,
                      canvas_height);

    // Fill Masks
    xcb_gcontext_t mask_gc = xcb_generate_id(connection);
    mask = XCB_GC_FOREGROUND;
    values[0] = 0;
    xcb_create_gc(connection, mask_gc, even_mask, mask, values);
    xcb_rectangle_t r = {0, 0, (uint16_t)canvas_width, (uint16_t)canvas_height};
    xcb_poly_fill_rectangle(connection, even_mask, mask_gc, 1, &r);
    xcb_poly_fill_rectangle(connection, odd_mask, mask_gc, 1, &r);

    values[0] = 1;
    xcb_change_gc(connection, mask_gc, XCB_GC_FOREGROUND, values);

    // Draw lines
    // Batch lines?
    // EVEN: 0, 2, 4...
    std::vector<xcb_segment_t> even_segs;
    std::vector<xcb_segment_t> odd_segs;
    for (int y = 0; y < canvas_height; y += 2) {
      even_segs.push_back({0, (int16_t)y, (int16_t)canvas_width, (int16_t)y});
    }
    for (int y = 1; y < canvas_height; y += 2) {
      odd_segs.push_back({0, (int16_t)y, (int16_t)canvas_width, (int16_t)y});
    }

    if (!even_segs.empty())
      xcb_poly_segment(connection, even_mask, mask_gc, even_segs.size(),
                       even_segs.data());
    if (!odd_segs.empty())
      xcb_poly_segment(connection, odd_mask, mask_gc, odd_segs.size(),
                       odd_segs.data());

    xcb_free_gc(connection, mask_gc);

    xcb_flush(connection);

    // Audio
    ma_engine_config audio_config = ma_engine_config_init();
    audio_config.channels = 2;
    audio_config.sampleRate = 22050;

    if (ma_engine_init(&audio_config, &audio_engine) == MA_SUCCESS) {
      audio_initialized = true;
      std::cout << "Audio initialized (XCB)." << std::endl;
    } else {
      std::cerr << "Failed to initialize audio engine." << std::endl;
    }

    running = true;
    return true;
  }

  bool process_events() override {
    // Audio cleanup
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

    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(connection))) {
      switch (event->response_type & ~0x80) {
      case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *cfg =
            (xcb_configure_notify_event_t *)event;
        if (cfg->width != window_width || cfg->height != window_height) {
          window_width = cfg->width;
          window_height = cfg->height;
          xcb_free_pixmap(connection, back_buffer);
          back_buffer = xcb_generate_id(connection);
          xcb_create_pixmap(connection, screen->root_depth, back_buffer, window,
                            window_width, window_height);
        }
        break;
      }
      case XCB_CLIENT_MESSAGE: {
        xcb_client_message_event_t *cm = (xcb_client_message_event_t *)event;
        if (cm->data.data32[0] == wm_delete_window) {
          running = false;
        }
        break;
      }
      case XCB_KEY_PRESS: {
        xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
        // Simple hardcoded fallback map for common F-keys on Linux
        // F6=72, F7=73, F8=74, F9=75
        // This is fragile but avoids xcb-keysyms dep for now.
        int code = kp->detail;
        if (code == 72)
          toggle_pixel_perfect(); // F6
        else if (code == 73)
          toggle_invert_colors(); // F7
        else if (code == 74)
          toggle_interlace(); // F8
        else if (code == 75)
          toggle_dead_space_color(); // F9
        break;
      }
      }
      free(event);
    }
    return running;
  }

  void draw_start() override {
    if (interlaced_mode)
      is_even_phase = !is_even_phase;
    else
      is_even_phase = true;

    // Clear Canvas ONLY if no active background (matches X11 behavior)
    // This preserves persistence for interlacing
    if (!active_background) {
      uint32_t values[2];
      values[0] = screen->white_pixel;
      xcb_change_gc(connection, canvas_gc, XCB_GC_FOREGROUND, values);
      xcb_rectangle_t r = {0, 0, (uint16_t)canvas_width,
                           (uint16_t)canvas_height};
      xcb_poly_fill_rectangle(connection, canvas, canvas_gc, 1, &r);
      return;
    }

    if (active_background) {
      // Put XYBitmap
      // Set Fore=Black, Back=White
      uint32_t values[3];
      values[0] = screen->black_pixel;
      values[1] = screen->white_pixel;
      uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;

      // Interlace masking
      if (interlaced_mode) {
        mask |= XCB_GC_CLIP_MASK;
        values[2] = is_even_phase ? even_mask : odd_mask;
      } else {
        mask |= XCB_GC_CLIP_MASK;
        values[2] = XCB_NONE; // Clear Clip
      }

      xcb_change_gc(connection, canvas_gc, mask, values);

      // Set Clip Origin
      if (interlaced_mode) {
        uint32_t clip_values[] = {0, 0};
        xcb_change_gc(connection, canvas_gc,
                      XCB_GC_CLIP_ORIGIN_X | XCB_GC_CLIP_ORIGIN_Y, clip_values);
      }

      xcb_void_cookie_t cookie = xcb_put_image_checked(
          connection, XCB_IMAGE_FORMAT_XY_BITMAP, canvas, canvas_gc,
          active_background->width, active_background->height, 0, 0, 0,
          1, // Depth 1 for bitmap
          (active_background->width * active_background->height) / 8,
          converted_bkg_pixels.data());

      xcb_generic_error_t *error = xcb_request_check(connection, cookie);
      if (error) {
        std::cerr << "XCB Error in put_image (background): "
                  << error->error_code << std::endl;
        free(error);
      }

      // Restore Clip
      if (interlaced_mode) {
        mask = XCB_GC_CLIP_MASK;
        values[0] = XCB_NONE;
        xcb_change_gc(connection, canvas_gc, mask, values);
      }
    }
  }

  void draw_lists() override {
    // Similar to X11, iterate over foreground drawables
    // Using XCB Poly Point or PutImage?
    // PutImage is faster but sprites are small.
    // X11 used XDrawPoint for transparency handling.
    // XCB PolyPoint is efficient if batched.

    uint32_t values[1];

    for (int i = 0; i < foreground_drawables_count; i++) {
      ForegroundDrawable &fd = foreground_drawables[i];
      if (!fd.sprite || !fd.mask)
        continue;
      if (fd.flags & DRAW_FLAG_HIDDEN)
        continue;

      int width = fd.sprite->width;
      int height = fd.sprite->height;
      int stride_bytes = width / 8; // Assuming 8-px aligned

      uint8_t *sprite_bytes = (uint8_t *)fd.sprite->pixels;
      uint8_t *mask_bytes = (uint8_t *)fd.mask->pixels;

      bool invert = (fd.flags & DRAW_FLAG_INVERT);

      int y_step = interlaced_mode ? 2 : 1;
      int start_y = 0;

      if (interlaced_mode) {
        int desired_parity = is_even_phase ? 0 : 1;
        int current_parity = (fd.y) & 1;
        if (current_parity != desired_parity)
          start_y = 1;
      }

      // We will batch points for this sprite
      // Pass 1: Erasure (White) - only if not invert
      if (!invert) {
        std::vector<xcb_point_t> points;
        for (int y = start_y; y < height; y += y_step) {
          for (int byte_col = 0; byte_col < stride_bytes; byte_col++) {
            int idx = y * stride_bytes + byte_col;
            uint8_t s_byte = sprite_bytes[idx];
            uint8_t m_byte = mask_bytes[idx];
            if (m_byte == 0)
              continue;

            for (int bit = 0; bit < 8; bit++) {
              int shift = 7 - bit;
              if ((m_byte >> shift) & 1) {
                if (!((s_byte >> shift) & 1)) {
                  // Opaque and Not Ink -> White
                  points.push_back({(int16_t)(fd.x + byte_col * 8 + bit),
                                    (int16_t)(fd.y + y)});
                }
              }
            }
          }
        }
        if (!points.empty()) {
          values[0] = screen->white_pixel;
          xcb_change_gc(connection, canvas_gc, XCB_GC_FOREGROUND, values);
          xcb_poly_point(connection, XCB_COORD_MODE_ORIGIN, canvas, canvas_gc,
                         points.size(), points.data());
        }
      }

      // Pass 2: Ink (Black or Invert)
      std::vector<xcb_point_t> points;
      if (invert) {
        uint32_t val = XCB_GX_INVERT;
        xcb_change_gc(connection, canvas_gc, XCB_GC_FUNCTION, &val);
      } else {
        values[0] = screen->black_pixel;
        xcb_change_gc(connection, canvas_gc, XCB_GC_FOREGROUND, values);
      }

      for (int y = start_y; y < height; y += y_step) {
        for (int byte_col = 0; byte_col < stride_bytes; byte_col++) {
          int idx = y * stride_bytes + byte_col;
          uint8_t s_byte = sprite_bytes[idx];
          uint8_t m_byte = mask_bytes[idx];
          if (m_byte == 0)
            continue;

          for (int bit = 0; bit < 8; bit++) {
            int shift = 7 - bit;
            if ((m_byte >> shift) & 1) {
              if ((s_byte >> shift) & 1) {
                // Ink
                points.push_back({(int16_t)(fd.x + byte_col * 8 + bit),
                                  (int16_t)(fd.y + y)});
              }
            }
          }
        }
      }

      if (!points.empty()) {
        xcb_poly_point(connection, XCB_COORD_MODE_ORIGIN, canvas, canvas_gc,
                       points.size(), points.data());
      }

      if (invert) {
        uint32_t val = XCB_GX_COPY;
        xcb_change_gc(connection, canvas_gc, XCB_GC_FUNCTION, &val);
      }
    }
  }

  void draw_end() override {
    // Determine colors
    uint32_t white = screen->white_pixel;
    uint32_t black = screen->black_pixel;
    uint32_t dead_space = dead_space_white ? white : black;

    uint32_t paper = invert_colors ? black : white;
    uint32_t ink = invert_colors ? white : black;

    // 1. Clear Back Bufer
    uint32_t values[1];
    values[0] = dead_space;
    xcb_change_gc(connection, window_gc, XCB_GC_FOREGROUND,
                  values); // reusing window_gc for backbuffer generic ops
    xcb_rectangle_t r = {0, 0, (uint16_t)window_width, (uint16_t)window_height};
    xcb_poly_fill_rectangle(connection, back_buffer, window_gc, 1, &r);

    // Get Image from Canvas (Screen Depth)
    xcb_get_image_cookie_t cookie =
        xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, canvas, 0, 0,
                      canvas_width, canvas_height, ~0);
    xcb_get_image_reply_t *reply =
        xcb_get_image_reply(connection, cookie, NULL);

    if (reply) {
      uint8_t *data = xcb_get_image_data(reply);
      // Assuming ZPixmap 32-bit for simple iteration?
      // Depth depends on root depth.
      // XCB doesn't make it easy to get pixel value without knowing format.
      // But we know we wrote Black and White pixels.

      int depth = reply->depth;
      int bpp =
          (depth > 16) ? 32 : 16; // Simplified assumption for common X11 setup
      int stride = canvas_width * (bpp / 8);
      // width * bpp/8)

      // Scaling logic
      int scale_x = window_width / canvas_width;
      int scale_y = window_height / canvas_height;
      int current_scale = (scale_x < scale_y) ? scale_x : scale_y;
      if (current_scale < 1)
        current_scale = 1;

      int offset_x = 0;
      int offset_y = 0;

      if (pixel_perfect_mode) {
        offset_x = (window_width - (canvas_width * current_scale)) / 2;
        offset_y = (window_height - (canvas_height * current_scale)) / 2;
        values[0] = paper;
        xcb_change_gc(connection, window_gc, XCB_GC_FOREGROUND, values);
        xcb_rectangle_t bg_rect = {(int16_t)offset_x, (int16_t)offset_y,
                                   (uint16_t)(canvas_width * current_scale),
                                   (uint16_t)(canvas_height * current_scale)};
        xcb_poly_fill_rectangle(connection, back_buffer, window_gc, 1,
                                &bg_rect);
      } else {
        // Stretch
        current_scale =
            1; // Logic differs for stretch but for simplicity in port let's
               // stick to pixel perfect logic or full fill
        // The X11 stretch logic was complex per-pixel storage.
        // For this XCB port, let's implement the Pixel Perfect one first.
        // And just fill background with Paper.
        values[0] = paper;
        xcb_change_gc(connection, window_gc, XCB_GC_FOREGROUND, values);
        xcb_rectangle_t full_r = {0, 0, (uint16_t)window_width,
                                  (uint16_t)window_height};
        xcb_poly_fill_rectangle(connection, back_buffer, window_gc, 1, &full_r);

        // Recalc for stretch
        // For now, let's just force pixel perfect or simple scale
        // Reuse pixel perfect logic for stability in prototype

        // If we want stretch:
        // float scale_x_f = ...
        // But we are iterating pixels.
      }

      // Draw Ink
      values[0] = ink;
      xcb_change_gc(connection, window_gc, XCB_GC_FOREGROUND, values);

      std::vector<xcb_rectangle_t> rects;
      // Optim: Reuse rects vector capacities?

      for (int y = 0; y < canvas_height; y++) {
        for (int x = 0; x < canvas_width; x++) {
          // Extract pixel
          uint32_t pixel = 0;
          if (bpp == 32) {
            pixel = *(uint32_t *)(data + y * stride + x * 4);
          } else if (bpp == 16) {
            pixel = *(uint16_t *)(data + y * stride + x * 2);
          } else {
            continue;
          }

          // Mask off Alpha/Unused high byte (0xFF000000 vs 0x00000000)
          // Assuming Little Endian layout where high byte is at the top
          if ((pixel & 0x00FFFFFF) == (black & 0x00FFFFFF)) {
            if (pixel_perfect_mode) {
              rects.push_back({(int16_t)(offset_x + x * current_scale),
                               (int16_t)(offset_y + y * current_scale),
                               (uint16_t)current_scale,
                               (uint16_t)current_scale});
            } else {
              float scale_x_f = (float)window_width / canvas_width;
              float scale_y_f = (float)window_height / canvas_height;
              int dest_x = (int)(x * scale_x_f);
              int dest_y = (int)(y * scale_y_f);
              int dest_w = (int)((x + 1) * scale_x_f) - dest_x;
              int dest_h = (int)((y + 1) * scale_y_f) - dest_y;
              if (dest_w < 1)
                dest_w = 1;
              if (dest_h < 1)
                dest_h = 1;
              rects.push_back({(int16_t)dest_x, (int16_t)dest_y,
                               (uint16_t)dest_w, (uint16_t)dest_h});
            }
          }
        }
      }

      // debug_rect_count = rects.size(); // Removed debug variable
      if (!rects.empty()) {
        xcb_poly_fill_rectangle(connection, back_buffer, window_gc,
                                rects.size(), rects.data());
      }
    }

    // Flip
    xcb_copy_area(connection, back_buffer, window, window_gc, 0, 0, 0, 0,
                  window_width, window_height);

    xcb_flush(connection);

    if (reply) { // Free reply after all uses
      free(reply);
    }
  }

  // Audio same as X11
  void play_sound(const char *filename) override {
    if (!audio_initialized)
      return;
    load_sound(filename);
    if (sound_cache.find(filename) == sound_cache.end())
      return;

    CachedSound &cached = sound_cache[filename];
    ActiveVoice *voice = new ActiveVoice();
    voice->active = true;
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        ma_format_f32, 2, cached.frameCount, cached.pData, NULL);
    ma_audio_buffer_init(&config, &voice->buffer);
    ma_sound_init_from_data_source(&audio_engine, &voice->buffer, 0, NULL,
                                   &voice->sound);
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
        ma_format_f32, 2, ma_engine_get_sample_rate(&audio_engine));
    ma_uint64 frameCount;
    void *pData;
    if (ma_decode_file(path.c_str(), &config, &frameCount, &pData) ==
        MA_SUCCESS) {
      CachedSound s = {pData, frameCount};
      sound_cache[filename] = s;
      std::cout << "Loaded: " << filename << std::endl;
    }
  }

  void clear_sounds() override {
    // Cleanup
  }

  int get_width() const override { return canvas_width; }
  int get_height() const override { return canvas_height; }

  void sleep_ms(int ms) override { usleep(ms * 1000); }
  bool is_running() override { return running; }
  unsigned long get_time_ms() override {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
  }

  BkgImage *get_active_background() override { return active_background; }

  std::vector<uint8_t> converted_bkg_pixels;

  uint8_t reverse_byte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
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

    // Convert pixels to match LSBFirst server expectation (PBM is MSBFirst)
    // We assume the server is LSBFirst (typical for PC X servers).
    // Ideally we checked setup->bitmap_format_bit_order.
    // For now, assume conversion is always needed if pbm is MSB.

    // Calculate size
    size_t total_bytes =
        (active_background->width * active_background->height) / 8;
    converted_bkg_pixels.resize(total_bytes);

    uint8_t *src = (uint8_t *)active_background->pixels;
    uint8_t *dst = converted_bkg_pixels.data();
    for (size_t i = 0; i < total_bytes; ++i) {
      dst[i] = reverse_byte(src[i]);
    }
  }

  ~EngineXCB() {
    // Destructor
    if (default_background)
      free(default_background);
    xcb_disconnect(connection);
    if (audio_initialized)
      ma_engine_uninit(&audio_engine);
  }
};

Engine *create_engine() { return new EngineXCB(); }

#endif // PLATFORM_XCB
