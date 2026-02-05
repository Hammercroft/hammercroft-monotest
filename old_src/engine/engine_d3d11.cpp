#ifdef PLATFORM_D3D11

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

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
class EngineD3D11;

// Global pointer for window procedure
static EngineD3D11 *g_engine = nullptr;

// Simple fullscreen quad vertex shader
static const char *g_vertex_shader_src = R"(
struct VS_INPUT {
  float2 pos : POSITION;
  float2 uv : TEXCOORD;
};

struct PS_INPUT {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD;
};

PS_INPUT main(VS_INPUT input) {
  PS_INPUT output;
  output.pos = float4(input.pos, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}
)";

// Pixel shader with color inversion support
static const char *g_pixel_shader_src = R"(
struct PS_INPUT {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

cbuffer Constants : register(b0) {
  float invert_colors;
  float3 padding;
};

float4 main(PS_INPUT input) : SV_TARGET {
  float value = tex.Sample(samp, input.uv).r;
  if (invert_colors > 0.5) {
    value = 1.0 - value;
  }
  return float4(value, value, value, 1.0);
}
)";

struct SimpleVertex {
  float x, y;
  float u, v;
};

class EngineD3D11 : public Engine {
private:
  HWND hwnd;
  bool running;
  int window_width;
  int window_height;
  int buffer_width;
  int buffer_height;
  int canvas_width;
  int canvas_height;
  int scale;

  // D3D11 resources
  ID3D11Device *d3d_device;
  ID3D11DeviceContext *d3d_context;
  IDXGISwapChain *swap_chain;
  ID3D11RenderTargetView *render_target_view;

  // Canvas texture (dynamic, CPU-writable)
  ID3D11Texture2D *canvas_texture;
  ID3D11ShaderResourceView *canvas_srv;

  // Shaders for presenting canvas
  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11InputLayout *input_layout;
  ID3D11Buffer *vertex_buffer;
  ID3D11Buffer *constant_buffer;
  ID3D11SamplerState *sampler_state;

  // Canvas bitmap data (1-bit format, matching GDI)
  uint8_t *canvas_bits;
  int canvas_stride;

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
  EngineD3D11()
      : hwnd(nullptr), running(false), d3d_device(nullptr),
        d3d_context(nullptr), swap_chain(nullptr), render_target_view(nullptr),
        canvas_texture(nullptr), canvas_srv(nullptr), vertex_shader(nullptr),
        pixel_shader(nullptr), input_layout(nullptr), vertex_buffer(nullptr),
        constant_buffer(nullptr), sampler_state(nullptr), canvas_bits(nullptr),
        active_background(nullptr), default_background(nullptr),
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
    canvas_stride = (canvas_width + 31) / 32 * 4;

    // Allocate canvas bitmap (1-bit format)
    canvas_bits = (uint8_t *)malloc(canvas_stride * canvas_height);
    if (!canvas_bits)
      return false;
    memset(canvas_bits, 0x00, canvas_stride * canvas_height);

    // Create default background
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

    // Create window
    window_width = canvas_width * scale;
    window_height = canvas_height * scale;

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "MONOTEST_D3D11";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    // Adjust window size to account for title bar and borders
    RECT wr = {0, 0, window_width, window_height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int adjusted_width = wr.right - wr.left;
    int adjusted_height = wr.bottom - wr.top;

    hwnd = CreateWindowExA(0, "MONOTEST_D3D11", "MONOTEST", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, adjusted_width,
                           adjusted_height, NULL, NULL, wc.hInstance, NULL);

    if (!hwnd)
      return false;

    g_engine = this;

    // Initialize D3D11
    if (!init_d3d11())
      return false;

    // Initialize audio
    ma_engine_config audio_config = ma_engine_config_init();
    audio_config.channels = 2;
    audio_config.sampleRate = 22050;

    if (ma_engine_init(&audio_config, &audio_engine) == MA_SUCCESS) {
      audio_initialized = true;
      std::cout << "Audio initialized (D3D11)." << std::endl;
    } else {
      std::cerr << "Failed to initialize audio engine." << std::endl;
    }

    ShowWindow(hwnd, SW_SHOW);
    running = true;

    std::cout << "D3D11 Engine initialized successfully." << std::endl;
    return true;
  }

  bool init_d3d11() {
    // Create swap chain
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = window_width;
    scd.BufferDesc.Height = window_height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL feature_level;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels, 3,
        D3D11_SDK_VERSION, &scd, &swap_chain, &d3d_device, &feature_level,
        &d3d_context);

    if (FAILED(hr)) {
      std::cerr << "Failed to create D3D11 device" << std::endl;
      return false;
    }

    // Initialize buffer size tracking
    buffer_width = window_width;
    buffer_height = window_height;

    std::cout << "D3D11 device created with feature level: 0x" << std::hex
              << feature_level << std::dec << std::endl;

    // Create render target view
    ID3D11Texture2D *back_buffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer);
    d3d_device->CreateRenderTargetView(back_buffer, nullptr,
                                       &render_target_view);
    back_buffer->Release();

    d3d_context->OMSetRenderTargets(1, &render_target_view, nullptr);

    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)window_width;
    vp.Height = (float)window_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3d_context->RSSetViewports(1, &vp);

    // Create canvas texture (dynamic, CPU-writable)
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = canvas_width;
    tex_desc.Height = canvas_height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DYNAMIC;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = d3d_device->CreateTexture2D(&tex_desc, nullptr, &canvas_texture);
    if (FAILED(hr))
      return false;

    hr = d3d_device->CreateShaderResourceView(canvas_texture, nullptr,
                                              &canvas_srv);
    if (FAILED(hr))
      return false;

    // Compile and create shaders
    if (!init_shaders())
      return false;

    // Create fullscreen quad vertex buffer
    SimpleVertex vertices[] = {
        {-1.0f, 1.0f, 0.0f, 0.0f},  // Top-left
        {1.0f, 1.0f, 1.0f, 0.0f},   // Top-right
        {-1.0f, -1.0f, 0.0f, 1.0f}, // Bottom-left
        {1.0f, -1.0f, 1.0f, 1.0f}   // Bottom-right
    };

    D3D11_BUFFER_DESC vb_desc = {};
    vb_desc.ByteWidth = sizeof(vertices);
    vb_desc.Usage = D3D11_USAGE_DYNAMIC;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA vb_data = {};
    vb_data.pSysMem = vertices;

    hr = d3d_device->CreateBuffer(&vb_desc, &vb_data, &vertex_buffer);
    if (FAILED(hr))
      return false;

    // Create sampler state (nearest neighbor for pixel perfect)
    D3D11_SAMPLER_DESC samp_desc = {};
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = d3d_device->CreateSamplerState(&samp_desc, &sampler_state);
    if (FAILED(hr))
      return false;

    // Create constant buffer for shader constants
    D3D11_BUFFER_DESC cb_desc = {};
    cb_desc.ByteWidth = 16; // float4
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = d3d_device->CreateBuffer(&cb_desc, nullptr, &constant_buffer);
    if (FAILED(hr))
      return false;

    return true;
  }

  bool resize_buffers() {
    if (render_target_view) {
      render_target_view->Release();
      render_target_view = nullptr;
    }

    d3d_context->OMSetRenderTargets(0, 0, 0);

    HRESULT hr = swap_chain->ResizeBuffers(0, window_width, window_height,
                                           DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
      std::cerr << "Failed to resize buffers" << std::endl;
      return false;
    }

    ID3D11Texture2D *back_buffer;
    hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                               (void **)&back_buffer);
    if (FAILED(hr))
      return false;

    hr = d3d_device->CreateRenderTargetView(back_buffer, nullptr,
                                            &render_target_view);
    back_buffer->Release();
    if (FAILED(hr))
      return false;

    d3d_context->OMSetRenderTargets(1, &render_target_view, nullptr);

    // Update buffer size
    buffer_width = window_width;
    buffer_height = window_height;

    // Reset viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)window_width;
    vp.Height = (float)window_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3d_context->RSSetViewports(1, &vp);

    return true;
  }

  bool init_shaders() {
    // Compile vertex shader
    ID3DBlob *vs_blob = nullptr;
    ID3DBlob *error_blob = nullptr;
    HRESULT hr = D3DCompile(g_vertex_shader_src, strlen(g_vertex_shader_src),
                            "VertexShader", nullptr, nullptr, "main", "vs_4_0",
                            0, 0, &vs_blob, &error_blob);
    if (FAILED(hr)) {
      if (error_blob) {
        std::cerr << "Vertex shader compilation error: "
                  << (char *)error_blob->GetBufferPointer() << std::endl;
        error_blob->Release();
      }
      return false;
    }

    hr = d3d_device->CreateVertexShader(vs_blob->GetBufferPointer(),
                                        vs_blob->GetBufferSize(), nullptr,
                                        &vertex_shader);
    if (FAILED(hr)) {
      vs_blob->Release();
      return false;
    }

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = d3d_device->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(),
                                       vs_blob->GetBufferSize(), &input_layout);
    vs_blob->Release();
    if (FAILED(hr))
      return false;

    // Compile pixel shader
    ID3DBlob *ps_blob = nullptr;
    hr = D3DCompile(g_pixel_shader_src, strlen(g_pixel_shader_src),
                    "PixelShader", nullptr, nullptr, "main", "ps_4_0", 0, 0,
                    &ps_blob, &error_blob);
    if (FAILED(hr)) {
      if (error_blob) {
        std::cerr << "Pixel shader compilation error: "
                  << (char *)error_blob->GetBufferPointer() << std::endl;
        error_blob->Release();
      }
      return false;
    }

    hr = d3d_device->CreatePixelShader(ps_blob->GetBufferPointer(),
                                       ps_blob->GetBufferSize(), nullptr,
                                       &pixel_shader);
    ps_blob->Release();
    if (FAILED(hr))
      return false;

    std::cout << "Shaders compiled successfully." << std::endl;
    return true;
  }

  void upload_canvas_to_texture() {
    // Convert 1-bit canvas to 8-bit texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = d3d_context->Map(canvas_texture, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                  &mapped);
    if (FAILED(hr))
      return;

    uint8_t *dst = (uint8_t *)mapped.pData;

    for (int y = 0; y < canvas_height; y++) {
      for (int x = 0; x < canvas_width; x++) {
        int byte_idx = y * canvas_stride + x / 8;
        int bit_idx = 7 - (x % 8);
        bool is_black = (canvas_bits[byte_idx] >> bit_idx) & 1;
        dst[y * mapped.RowPitch + x] = is_black ? 0 : 255;
      }
    }

    d3d_context->Unmap(canvas_texture, 0);
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
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (g_engine) {
        if (wParam == VK_F6) {
          g_engine->toggle_pixel_perfect();
        } else if (wParam == VK_F7) {
          g_engine->toggle_invert_colors();
        } else if (wParam == VK_F9) {
          g_engine->toggle_dead_space_color();
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
    if (!canvas_bits || !active_background) {
      memset(canvas_bits, 0x00, canvas_stride * canvas_height);
      return;
    }

    // Copy background to canvas (CPU-side)
    uint8_t *src = (uint8_t *)active_background->pixels;

    // Copy entire background (Ignores interlacing on D3D11)
    memcpy(canvas_bits, src, canvas_stride * canvas_height);
  }

  void draw_lists() override {
    if (!canvas_bits)
      return;

    // Draw foreground drawables (CPU-side, matching GDI logic)
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

      // Determine loop parameters (Ignores interlacing on D3D11)
      int y_step = 1;
      int start_y = 0;

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
                canvas_bits[canvas_byte] ^= (1 << canvas_bit);
              }
            } else {
              // Set or clear the pixel
              if (is_ink) {
                canvas_bits[canvas_byte] |= (1 << canvas_bit);
              } else {
                canvas_bits[canvas_byte] &= ~(1 << canvas_bit);
              }
            }
          }
        }
      }
    }
  }

  void draw_end() override {
    // Check for resize needed
    if (window_width != buffer_width || window_height != buffer_height) {
      // Don't resize if minimized (0 size)
      if (window_width > 0 && window_height > 0) {
        resize_buffers();
      }
    }

    // Upload canvas to texture
    upload_canvas_to_texture();

    // Clear screen with dead space color
    float clear_color[4] = {dead_space_white ? 1.0f : 0.0f,
                            dead_space_white ? 1.0f : 0.0f,
                            dead_space_white ? 1.0f : 0.0f, 1.0f};
    d3d_context->ClearRenderTargetView(render_target_view, clear_color);

    // Update constant buffer for color inversion
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = d3d_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD,
                                  0, &mapped);
    if (SUCCEEDED(hr)) {
      float *constants = (float *)mapped.pData;
      constants[0] = invert_colors ? 1.0f : 0.0f;
      constants[1] = 0.0f;
      constants[2] = 0.0f;
      constants[3] = 0.0f;
      d3d_context->Unmap(constant_buffer, 0);
    }

    // Calculate quad vertices based on scaling mode
    SimpleVertex vertices[4];

    if (pixel_perfect_mode) {
      // Calculate integer scale
      int scale_x = window_width / canvas_width;
      int scale_y = window_height / canvas_height;
      int current_scale = (scale_x < scale_y) ? scale_x : scale_y;
      if (current_scale < 1)
        current_scale = 1;

      int scaled_width = canvas_width * current_scale;
      int scaled_height = canvas_height * current_scale;

      // Convert to NDC coordinates
      float ndc_width = (float)scaled_width / (float)window_width * 2.0f;
      float ndc_height = (float)scaled_height / (float)window_height * 2.0f;

      float left = -ndc_width / 2.0f;
      float right = ndc_width / 2.0f;
      float top = ndc_height / 2.0f;
      float bottom = -ndc_height / 2.0f;

      vertices[0] = {left, top, 0.0f, 0.0f};     // Top-left
      vertices[1] = {right, top, 1.0f, 0.0f};    // Top-right
      vertices[2] = {left, bottom, 0.0f, 1.0f};  // Bottom-left
      vertices[3] = {right, bottom, 1.0f, 1.0f}; // Bottom-right
    } else {
      // Fullscreen stretched
      vertices[0] = {-1.0f, 1.0f, 0.0f, 0.0f};
      vertices[1] = {1.0f, 1.0f, 1.0f, 0.0f};
      vertices[2] = {-1.0f, -1.0f, 0.0f, 1.0f};
      vertices[3] = {1.0f, -1.0f, 1.0f, 1.0f};
    }

    // Update vertex buffer
    hr =
        d3d_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
      memcpy(mapped.pData, vertices, sizeof(vertices));
      d3d_context->Unmap(vertex_buffer, 0);
    }

    // Set pipeline state
    d3d_context->VSSetShader(vertex_shader, nullptr, 0);
    d3d_context->PSSetShader(pixel_shader, nullptr, 0);
    d3d_context->IASetInputLayout(input_layout);
    d3d_context->PSSetShaderResources(0, 1, &canvas_srv);
    d3d_context->PSSetSamplers(0, 1, &sampler_state);
    d3d_context->PSSetConstantBuffers(0, 1, &constant_buffer);

    // Draw quad
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    d3d_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    d3d_context->Draw(4, 0);

    // Present
    swap_chain->Present(1, 0);
  }

  void set_active_background(BkgImage *bkg) override {
    if (bkg) {
      if (bkg->width != canvas_width || bkg->height != canvas_height) {
        std::cerr << "Error: Active background size mismatch!" << std::endl;
        return;
      }
      active_background = bkg;
    } else {
      active_background = default_background;
    }
  }

  BkgImage *get_active_background() override { return active_background; }

  bool is_running() override { return running; }

  unsigned long get_time_ms() override { return (unsigned long)GetTickCount(); }

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
      delete voice;
      return;
    }

    result = ma_sound_init_from_data_source(&audio_engine, &voice->buffer, 0,
                                            NULL, &voice->sound);
    if (result != MA_SUCCESS) {
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
        ma_format_f32, 2, ma_engine_get_sample_rate(&audio_engine));

    ma_uint64 frameCount;
    void *pData;

    ma_result result =
        ma_decode_file(path.c_str(), &config, &frameCount, &pData);
    if (result != MA_SUCCESS) {
      std::cout << "Failed to load sound: " << path << std::endl;
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

  ~EngineD3D11() {
    clear_sounds();
    if (audio_initialized) {
      ma_engine_uninit(&audio_engine);
    }

    if (default_background)
      free(default_background);

    if (canvas_bits)
      free(canvas_bits);

    // Release D3D11 resources
    if (sampler_state)
      sampler_state->Release();
    if (constant_buffer)
      constant_buffer->Release();
    if (vertex_buffer)
      vertex_buffer->Release();
    if (input_layout)
      input_layout->Release();
    if (pixel_shader)
      pixel_shader->Release();
    if (vertex_shader)
      vertex_shader->Release();
    if (canvas_srv)
      canvas_srv->Release();
    if (canvas_texture)
      canvas_texture->Release();
    if (render_target_view)
      render_target_view->Release();
    if (swap_chain)
      swap_chain->Release();
    if (d3d_context)
      d3d_context->Release();
    if (d3d_device)
      d3d_device->Release();

    if (hwnd)
      DestroyWindow(hwnd);

    g_engine = nullptr;
  }
};

Engine *create_engine() { return new EngineD3D11(); }

#endif // PLATFORM_D3D11
