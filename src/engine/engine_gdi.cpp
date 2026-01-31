#ifdef PLATFORM_GDI

#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

#include "../vendor/miniaudio.h"
#include "bkgimage.h"
#include "engine.h"
#include "sprite.h"

// Forward declaration
class EngineGDI;

// Global pointer for window procedure
static EngineGDI *g_engine = nullptr;

class EngineGDI : public Engine {
private:
  HWND hwnd;
  HDC window_dc;
  HDC canvas_dc;
  HDC back_buffer_dc;
  HBITMAP canvas_bitmap;
  HBITMAP back_buffer_bitmap;
  HBITMAP old_canvas_bitmap;
  HBITMAP old_back_buffer_bitmap;

  bool running;
  int window_width;
  int window_height;
  int canvas_width;
  int canvas_height;
  int scale;

  // Canvas bitmap data
  BITMAPINFO canvas_bmi;
  void *canvas_bits;

  // Back buffer bitmap data
  BITMAPINFO back_buffer_bmi;
  void *back_buffer_bits;

  // Background
  BkgImage *active_background;
  BkgImage *default_background;
  bool is_even_phase;

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
  EngineGDI()
      : hwnd(nullptr), window_dc(nullptr), canvas_dc(nullptr),
        back_buffer_dc(nullptr), canvas_bitmap(nullptr),
        back_buffer_bitmap(nullptr), old_canvas_bitmap(nullptr),
        old_back_buffer_bitmap(nullptr), running(false), canvas_bits(nullptr),
        back_buffer_bits(nullptr), active_background(nullptr),
        default_background(nullptr), is_even_phase(true),
        audio_initialized(false) {
    // Get executable directory
    char exe_path[MAX_PATH];
    DWORD count = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (count != 0 && count < MAX_PATH) {
      char *last_slash = strrchr(exe_path, '\\');
      if (last_slash) {
        *last_slash = '\0';
        exe_dir = exe_path;
      } else {
        exe_dir = ".";
      }
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
    window_width = canvas_width * scale;
    window_height = canvas_height * scale;

    // Create Default Background (White)
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

    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "MONOTEST";

    if (!RegisterClassExA(&wc)) {
      std::cerr << "Failed to register window class" << std::endl;
      return false;
    }

    // Create window
    hwnd = CreateWindowExA(0, "MONOTEST", "MONOTEST",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                           CW_USEDEFAULT, window_width, window_height, NULL,
                           NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd) {
      std::cerr << "Failed to create window" << std::endl;
      return false;
    }

    // Get window DC
    window_dc = GetDC(hwnd);

    // Create canvas DC and bitmap (1-bit monochrome)
    canvas_dc = CreateCompatibleDC(window_dc);

    // Setup canvas bitmap info for 1-bit DIB
    memset(&canvas_bmi, 0, sizeof(BITMAPINFO));
    canvas_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    canvas_bmi.bmiHeader.biWidth = canvas_width;
    canvas_bmi.bmiHeader.biHeight = -canvas_height; // Top-down
    canvas_bmi.bmiHeader.biPlanes = 1;
    canvas_bmi.bmiHeader.biBitCount = 1;
    canvas_bmi.bmiHeader.biCompression = BI_RGB;

    // Set palette: 0=White, 1=Black
    canvas_bmi.bmiColors[0].rgbRed = 255;
    canvas_bmi.bmiColors[0].rgbGreen = 255;
    canvas_bmi.bmiColors[0].rgbBlue = 255;
    canvas_bmi.bmiColors[1].rgbRed = 0;
    canvas_bmi.bmiColors[1].rgbGreen = 0;
    canvas_bmi.bmiColors[1].rgbBlue = 0;

    canvas_bitmap = CreateDIBSection(canvas_dc, &canvas_bmi, DIB_RGB_COLORS,
                                     &canvas_bits, NULL, 0);
    if (!canvas_bitmap) {
      std::cerr << "Failed to create canvas bitmap" << std::endl;
      return false;
    }
    old_canvas_bitmap = (HBITMAP)SelectObject(canvas_dc, canvas_bitmap);

    // Create back buffer DC and bitmap (24-bit for color support)
    back_buffer_dc = CreateCompatibleDC(window_dc);

    memset(&back_buffer_bmi, 0, sizeof(BITMAPINFO));
    back_buffer_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    back_buffer_bmi.bmiHeader.biWidth = window_width;
    back_buffer_bmi.bmiHeader.biHeight = -window_height; // Top-down
    back_buffer_bmi.bmiHeader.biPlanes = 1;
    back_buffer_bmi.bmiHeader.biBitCount = 24;
    back_buffer_bmi.bmiHeader.biCompression = BI_RGB;

    back_buffer_bitmap =
        CreateDIBSection(back_buffer_dc, &back_buffer_bmi, DIB_RGB_COLORS,
                         &back_buffer_bits, NULL, 0);
    if (!back_buffer_bitmap) {
      std::cerr << "Failed to create back buffer bitmap" << std::endl;
      return false;
    }
    old_back_buffer_bitmap =
        (HBITMAP)SelectObject(back_buffer_dc, back_buffer_bitmap);

    // Initialize audio
    ma_engine_config audio_config = ma_engine_config_init();
    audio_config.channels = 2;
    audio_config.sampleRate = 22050;

    if (ma_engine_init(&audio_config, &audio_engine) == MA_SUCCESS) {
      audio_initialized = true;
      std::cout << "Audio initialized (GDI)." << std::endl;
    } else {
      std::cerr << "Failed to initialize audio engine." << std::endl;
    }

    running = true;
    g_engine = this;
    return true;
  }

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
      if (g_engine) {
        g_engine->running = false;
      }
      return 0;
    case WM_SIZE: {
      if (g_engine) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        g_engine->window_width = rect.right - rect.left;
        g_engine->window_height = rect.bottom - rect.top;

        // Recreate back buffer for new size
        if (g_engine->back_buffer_bitmap) {
          SelectObject(g_engine->back_buffer_dc,
                       g_engine->old_back_buffer_bitmap);
          DeleteObject(g_engine->back_buffer_bitmap);
        }

        g_engine->back_buffer_bmi.bmiHeader.biWidth = g_engine->window_width;
        g_engine->back_buffer_bmi.bmiHeader.biHeight = -g_engine->window_height;

        g_engine->back_buffer_bitmap = CreateDIBSection(
            g_engine->back_buffer_dc, &g_engine->back_buffer_bmi,
            DIB_RGB_COLORS, &g_engine->back_buffer_bits, NULL, 0);
        g_engine->old_back_buffer_bitmap = (HBITMAP)SelectObject(
            g_engine->back_buffer_dc, g_engine->back_buffer_bitmap);
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (g_engine) {
        switch (wParam) {
        case VK_F6:
          g_engine->toggle_pixel_perfect();
          break;
        case VK_F7:
          g_engine->toggle_invert_colors();
          break;
        case VK_F8:
          g_engine->toggle_interlace();
          break;
        case VK_F9:
          g_engine->toggle_dead_space_color();
          break;
        }
      }
      return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
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

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        running = false;
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    return running;
  }

  void draw_start() override {
    // Toggle phase every frame if in interlaced mode
    if (interlaced_mode) {
      is_even_phase = !is_even_phase;
    } else {
      is_even_phase = true;
    }

    if (!active_background || !canvas_bits) {
      // Clear to white
      memset(canvas_bits, 0x00, (canvas_width / 8) * canvas_height);
      return;
    }

    // Copy background to canvas
    uint8_t *src = (uint8_t *)active_background->pixels;
    uint8_t *dst = (uint8_t *)canvas_bits;
    int stride = (canvas_width + 31) / 32 * 4; // Aligned to 32-bit boundary

    if (interlaced_mode) {
      // Only draw even or odd lines
      int start_y = is_even_phase ? 0 : 1;
      for (int y = start_y; y < canvas_height; y += 2) {
        memcpy(dst + y * stride, src + y * stride, stride);
      }
    } else {
      // Copy entire background
      memcpy(dst, src, stride * canvas_height);
    }
  }

  void draw_lists() override {
    if (!canvas_bits)
      return;

    uint8_t *canvas_bytes = (uint8_t *)canvas_bits;
    int canvas_stride = (canvas_width + 31) / 32 * 4;

    // Draw foreground drawables
    for (int i = 0; i < foreground_drawables_count; i++) {
      ForegroundDrawable &fd = foreground_drawables[i];
      if (!fd.sprite || !fd.mask)
        continue;

      if (fd.flags & DRAW_FLAG_HIDDEN)
        continue;

      int width = fd.sprite->width;
      int height = fd.sprite->height;
      int sprite_stride = fd.sprite->width_in_words * 4;

      uint8_t *sprite_bytes = (uint8_t *)fd.sprite->pixels;
      uint8_t *mask_bytes = (uint8_t *)fd.mask->pixels;

      bool invert = (fd.flags & DRAW_FLAG_INVERT);

      // Determine loop parameters based on interlacing
      int y_step = interlaced_mode ? 2 : 1;
      int start_y = 0;

      if (interlaced_mode) {
        int desired_parity = is_even_phase ? 0 : 1;
        int current_parity = (fd.y) & 1;
        if (current_parity != desired_parity) {
          start_y = 1;
        }
      }

      // Draw sprite
      for (int y = start_y; y < height; y += y_step) {
        int screen_y = fd.y + y;
        if (screen_y < 0 || screen_y >= canvas_height)
          continue;

        for (int byte_col = 0; byte_col < sprite_stride; byte_col++) {
          int idx = y * sprite_stride + byte_col;
          uint8_t s_byte = sprite_bytes[idx];
          uint8_t m_byte = mask_bytes[idx];

          if (m_byte == 0)
            continue;

          for (int bit = 0; bit < 8; bit++) {
            int px_offset = byte_col * 8 + bit;
            if (px_offset >= width)
              break;

            int screen_x = fd.x + px_offset;
            if (screen_x < 0 || screen_x >= canvas_width)
              continue;

            int shift = 7 - bit;
            bool is_opaque = (m_byte >> shift) & 1;
            bool is_ink = (s_byte >> shift) & 1;

            if (!is_opaque)
              continue;

            // Calculate canvas position
            int canvas_byte = screen_y * canvas_stride + screen_x / 8;
            int canvas_bit = 7 - (screen_x % 8);

            if (invert) {
              // XOR the pixel
              if (is_ink) {
                canvas_bytes[canvas_byte] ^= (1 << canvas_bit);
              }
            } else {
              // Set or clear the pixel
              if (is_ink) {
                canvas_bytes[canvas_byte] |= (1 << canvas_bit);
              } else {
                canvas_bytes[canvas_byte] &= ~(1 << canvas_bit);
              }
            }
          }
        }
      }
    }
  }

  void draw_end() override {
    if (!back_buffer_dc || !canvas_dc)
      return;

    int scaled_width, scaled_height;
    int offset_x = 0;
    int offset_y = 0;

    if (pixel_perfect_mode) {
      int scale_x = window_width / canvas_width;
      int scale_y = window_height / canvas_height;
      int current_scale = (scale_x < scale_y) ? scale_x : scale_y;
      if (current_scale < 1)
        current_scale = 1;
      scaled_width = canvas_width * current_scale;
      scaled_height = canvas_height * current_scale;
      offset_x = (window_width - scaled_width) / 2;
      offset_y = (window_height - scaled_height) / 2;
    } else {
      scaled_width = window_width;
      scaled_height = window_height;
    }

    // Clear back buffer with dead space color
    RECT rc = {0, 0, window_width, window_height};
    HBRUSH brush = dead_space_white ? (HBRUSH)GetStockObject(WHITE_BRUSH)
                                    : (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(back_buffer_dc, &rc, brush);

    // Use StretchBlt for hardware-accelerated scaling
    // NOTSRCCOPY inverts colors, SRCCOPY doesn't
    DWORD rop = invert_colors ? NOTSRCCOPY : SRCCOPY;
    SetStretchBltMode(back_buffer_dc, COLORONCOLOR); // Nearest neighbor
    StretchBlt(back_buffer_dc, offset_x, offset_y, scaled_width, scaled_height,
               canvas_dc, 0, 0, canvas_width, canvas_height, rop);

    // Blit back buffer to window
    BitBlt(window_dc, 0, 0, window_width, window_height, back_buffer_dc, 0, 0,
           SRCCOPY);
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

  bool is_running() override { return running; }

  unsigned long get_time_ms() override { return GetTickCount(); }

  void sleep_ms(int ms) override { Sleep(ms); }

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
    ma_result result = ma_audio_buffer_init(&config, &voice->buffer);

    if (result != MA_SUCCESS) {
      std::cerr << "Failed to create audio buffer for " << filename
                << std::endl;
      delete voice;
      return;
    }

    result = ma_sound_init_from_data_source(&audio_engine, &voice->buffer, 0,
                                            NULL, &voice->sound);
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

    std::string path = exe_dir + "\\" + filename;

    ma_decoder_config config = ma_decoder_config_init(
        ma_format_f32, 2, ma_engine_get_sample_rate(&audio_engine));

    ma_uint64 frameCount;
    void *pData;

    ma_result result =
        ma_decode_file(path.c_str(), &config, &frameCount, &pData);
    if (result != MA_SUCCESS) {
      std::cout << "Failed to load sound: " << path << " (Error " << result
                << ")" << std::endl;
      return;
    } else {
      std::cout << "Loaded: " << path << std::endl;
    }

    CachedSound sound;
    sound.pData = pData;
    sound.frameCount = frameCount;
    sound_cache[filename] = sound;
  }

  void clear_sounds() override {
    for (auto *voice : active_voices) {
      ma_sound_stop(&voice->sound);
      ma_sound_uninit(&voice->sound);
      ma_audio_buffer_uninit(&voice->buffer);
      delete voice;
    }
    active_voices.clear();

    for (auto &pair : sound_cache) {
      ma_free(pair.second.pData, NULL);
    }
    sound_cache.clear();
  }

  int get_width() const override { return canvas_width; }
  int get_height() const override { return canvas_height; }

  ~EngineGDI() {
    clear_sounds();
    if (audio_initialized) {
      ma_engine_uninit(&audio_engine);
    }

    if (default_background)
      free(default_background);

    if (canvas_dc) {
      if (old_canvas_bitmap)
        SelectObject(canvas_dc, old_canvas_bitmap);
      if (canvas_bitmap)
        DeleteObject(canvas_bitmap);
      DeleteDC(canvas_dc);
    }

    if (back_buffer_dc) {
      if (old_back_buffer_bitmap)
        SelectObject(back_buffer_dc, old_back_buffer_bitmap);
      if (back_buffer_bitmap)
        DeleteObject(back_buffer_bitmap);
      DeleteDC(back_buffer_dc);
    }

    if (window_dc && hwnd)
      ReleaseDC(hwnd, window_dc);

    if (hwnd)
      DestroyWindow(hwnd);

    g_engine = nullptr;
  }
};

Engine *create_engine() { return new EngineGDI(); }

#endif // PLATFORM_GDI
