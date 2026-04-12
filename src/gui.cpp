#include "gui.h"
#include "common.h"
#include "icons.h"
#include "ppu.h"

#include <SDL3/SDL_render.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <format>
#include <span>

#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(lucide_ttf, "fonts/lucide.ttf");
INCBIN(monocraft_ttf, "fonts/Monocraft-ttf/Monocraft.ttf");

void gb_imgui_init(gb_state_t *gb_state) {
  // This is what is drawn to when using fullscreen dockspace, I use this so that I can do things like draw where the
  // current line is at.
  gb_state->imgui.viewport_target = SDL_CreateTexture(gb_state->video.sdl_renderer, SDL_PIXELFORMAT_RGBA32,
                                                      SDL_TEXTUREACCESS_TARGET, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  // Initialize ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                   // changing this requires resetting Style + calling this again)
  style.FontScaleDpi = main_scale; // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true
                                   // automatically overrides this for every window depending on the current monitor)

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLRenderer(gb_state->video.sdl_window, gb_state->video.sdl_renderer);
  ImGui_ImplSDLRenderer3_Init(gb_state->video.sdl_renderer);

  ImFontConfig default_font_config;
  default_font_config.PixelSnapH           = true;
  default_font_config.FontDataOwnedByAtlas = false; // I don't want ImGui to free this static memory.
  io.Fonts->AddFontFromMemoryTTF((void *)monocraft_ttf_data, monocraft_ttf_size, 0.0, &default_font_config);

  float base_font_size = 13.0f; // 13.0f is the size of the default font. Change to the font size you use.
  float icon_font_size = base_font_size * 2.0f / 3.0f; // sizes reduced by a third in order to align correctly

  static const ImWchar icons_ranges[] = {ICON_MIN_LC, ICON_MAX_16_LC, 0};
  ImFontConfig         icons_config;
  icons_config.MergeMode            = true;
  icons_config.PixelSnapH           = true;
  icons_config.GlyphMinAdvanceX     = icon_font_size;
  icons_config.FontDataOwnedByAtlas = false; // I don't want ImGui to free this static memory.
  io.Fonts->AddFontFromMemoryTTF((void *)lucide_ttf_data, lucide_ttf_size, icon_font_size, &icons_config, icons_ranges);
}

void gb_imgui_free(gb_state_t *gb_state) {
  SDL_DestroyTexture(gb_state->imgui.viewport_target);
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

bool gb_gui_handle_sdl_event(struct gb_state *gb_state, SDL_Event *event) {
  (void)gb_state;
  auto io = ImGui::GetIO();
  ImGui_ImplSDL3_ProcessEvent(event);
  // If ImGui wants keyboard events don't try to handle keyboard input myself. I don't do anything with mouse atm, this
  // may change down the road.
  return io.WantCaptureKeyboard;
}

static void gb_imgui_show_mem_val(struct gb_state *gb_state, const char *name, const uint16_t addr) {
  std::string formatted_text = std::format("{0:s} ({1:#04x}) Value is:\n"
                                           "  Hex: {2:#04x}\n"
                                           "  Dec: {2:d}\n"
                                           "  Bin: {2:#010b}",
                                           name, addr, gb_read_mem(gb_state, addr));
  ImGui::TextUnformatted(formatted_text.c_str());
}
static void gb_imgui_show_val(const char *name, const uint8_t val) {
  std::string formatted_text = std::format("{0:s}:\n"
                                           "  Hex: {1:#04x}\n"
                                           "  Dec: {1:d}\n"
                                           "  Bin: {1:#010b}",
                                           name, val);
  ImGui::TextUnformatted(formatted_text.c_str());
}

static void gb_imgui_show_val(const char *name, const uint16_t val) {
  std::string formatted_text = std::format("{0:s}:\n"
                                           "  Hex: {1:#06x}\n"
                                           "  Dec: {1:d}\n"
                                           "  Bin: {1:#018b}",
                                           name, val);
  ImGui::TextUnformatted(formatted_text.c_str());
}

static void gb_imgui_main_menu_bar(gb_state_t *gb_state) {
  gb_imgui_state_t &imgui_state = gb_state->imgui;
  ImGui::PushFont(NULL, 24.0);
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Windows")) {
      ImGui::MenuItem("Fullscreen Debug UI", NULL, &imgui_state.fs_dockspace);
      ImGui::MenuItem("OAM Viewer", NULL, &imgui_state.oam_viewer);
      // TODO: This should probably be broken up into multiple windows.
      ImGui::MenuItem("State Inspector", NULL, &imgui_state.state_inspector);
      ImGui::MenuItem("Settings", NULL, &imgui_state.settings);
      ImGui::EndMenu();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool pressed;
    pressed = ImGui::MenuItem(ICON_LC_PLAY, NULL, false, gb_state->dbg.execution_paused);
    ImGui::SetItemTooltip("Resume Execution");
    if (pressed) gb_state->dbg.cont();

    pressed = ImGui::MenuItem(ICON_LC_PAUSE, NULL, false, !gb_state->dbg.execution_paused);
    ImGui::SetItemTooltip("Immediately Pause Execution");
    if (pressed) gb_state->dbg.pause();

    pressed = ImGui::MenuItem(ICON_LC_MONITOR, NULL, false, gb_state->dbg.execution_paused);
    ImGui::SetItemTooltip("Run Until Next VBlank");
    if (pressed) gb_state->dbg.next_frame();

    pressed = ImGui::MenuItem(ICON_LC_ARROW_DOWN_TO_LINE, NULL, false, gb_state->dbg.execution_paused);
    ImGui::SetItemTooltip("Run Until Next HBlank");
    if (pressed) gb_state->dbg.next_line();

    pressed = ImGui::MenuItem(ICON_LC_STEP_FORWARD, NULL, false, gb_state->dbg.execution_paused);
    ImGui::SetItemTooltip("Step Instruction");
    if (pressed) gb_state->dbg.step_inst();

    ImGui::EndMainMenuBar();
  }
  ImGui::PopFont();
}

static void gb_imgui_oam_viewer(gb_state_t *gb_state) {
  ImGui::Begin("OAM Viewer");

  std::span oam_entries((const oam_entry_t *)gb_state->saved.mem.oam, 40);
  // This should not copy the data itself, this is just supposed to be a view.
  GB_assert(oam_entries.data() == (void *)gb_state->saved.mem.oam);

  bool draw_double_height = (gb_state->saved.regs.io.lcdc & LCDC_OBJ_SIZE) >> 2;
  ImGui::Value("Height", draw_double_height ? 16 : 8);

  int index = 0;
  for (const oam_entry_t &entry : oam_entries) {
    const int i = index++;
    ImGui::TextUnformatted(std::format("Entry {:d}:\n"
                                       "  Sprite Index:   VRAM{:d}:{:d}\n"
                                       "  Position (X,Y): {:d},{:d}\n"
                                       "  Palette:        {:d}\n"
                                       "  X Flipped:      {:s}\n"
                                       "  Y Flipped:      {:s}\n"
                                       "  Priority:       {:s}\n",
                                       i, entry.bank, entry.index, entry.x_pos, entry.y_pos, entry.dmg_palette,
                                       entry.x_flip, entry.y_flip, entry.priority)
                               .c_str());
    // TODO: Account for double height once I get single height working.
    uint8_t tile_idx = entry.index;
    if (draw_double_height) {
      tile_idx &= 0b1111'1110;
    }

  draw_obj:
    SDL_Texture *&tile_tex = gb_state->video.textures[tile_idx];
    if (tile_tex == NULL) {
      tile_tex =
          SDL_CreateTexture(gb_state->video.sdl_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 8, 8);
    }
    SDL_Surface *locked;
    SDL_LockTextureToSurface(tile_tex, NULL, &locked);
    SDL_Palette *palette;
    if (entry.dmg_palette)
      palette = gb_state->video.sdl_obj_palette_1;
    else
      palette = gb_state->video.sdl_obj_palette_0;

    uint32_t flip_mode = 0;

    if (entry.x_flip) flip_mode |= SDL_FLIP_HORIZONTAL;
    if (entry.y_flip) flip_mode |= SDL_FLIP_VERTICAL;
    gb_draw_tile_to_surface(gb_state, locked, palette, 0, 0, 0x8000 + (tile_idx * 16), SDL_FlipMode(flip_mode));
    SDL_UnlockTexture(tile_tex);
    ImVec2 size = {64, 64};
    ImGui::Image((ImTextureID)(intptr_t)tile_tex, size);
    if (draw_double_height && ((tile_idx & 1) == 0)) {
      tile_idx++;
      goto draw_obj;
    }
  }

  ImGui::End();
}

static void gb_imgui_display_viewport_win(gb_state_t *gb_state) {
  ImGuiIO       &io  = ImGui::GetIO();
  SDL_Renderer *&ren = gb_state->video.sdl_renderer;
  uint8_t       &ly  = gb_state->saved.regs.io.ly;
  ImGui::Begin("Display Viewport");

  if (ImGui::IsWindowFocused()) io.WantCaptureKeyboard = false;
  // always keep the correct aspect ratio based off of the window width.
  ImVec2 win_size;
  win_size.x = ImGui::GetWindowWidth();
  win_size.y = ((win_size.x * GB_DISPLAY_HEIGHT) / GB_DISPLAY_WIDTH);
  SDL_SetRenderTarget(ren, gb_state->imgui.viewport_target);
  SDL_RenderTexture(ren, gb_state->video.sdl_composite_target_front, NULL, NULL);
  if (gb_state->imgui.show_scanline) {
    SDL_SetRenderDrawColor(ren, 255, 64, 64, 128);
    SDL_RenderLine(ren, 0, ly, GB_DISPLAY_WIDTH, ly);
  }
  SDL_SetRenderTarget(gb_state->video.sdl_renderer, NULL);

  ImGui::Image((ImTextureID)(intptr_t)gb_state->imgui.viewport_target, win_size);
  ImGui::End();
}

static void gb_imgui_state_inspector_win(gb_state_t *gb_state) {
  ImGuiIO          &io          = ImGui::GetIO();
  gb_imgui_state_t &imgui_state = gb_state->imgui;
  ImGui::Begin("GB State");

  if (ImGui::IsWindowFocused()) io.WantCaptureKeyboard = true;
  if (ImGui::TreeNodeEx("Execution", ImGuiTreeNodeFlags_Framed)) {
    if (ImGui::Button("Reset")) {
      gb_state_reset(gb_state);
    }
    // Framerate
    // TODO: I might want to track average framerate as well as 1% lows to identify stuttering if that becomes an
    // issue.
    float last_frametime = (float)gb_state->dbg.ns_last_frametime / NS_PER_SEC;
    float last_frame_fps = 0.0f;
    if (last_frametime != 0.0f) {
      last_frame_fps = 1 / last_frametime;
    }
    ImGui::Value("Last Frametime", last_frametime, "%.6f");
    ImGui::Value("Last Frame FPS", last_frame_fps, "%.6f");

    ImGui::Checkbox("Paused", &gb_state->dbg.execution_paused);
    ImGui::Checkbox("Halted", &gb_state->saved.halted);

    ImGui::TextUnformatted(
        std::format("Sys Clock (a.k.a. Full Div): {0:#018b} - {0:#06x}", gb_state->saved.regs.io.div).c_str());

    ImGui::TextUnformatted("Addr:");
    ImGui::SameLine();
    ImGui::InputScalar("##addr", ImGuiDataType_U16, &imgui_state.breakpoint_addr, NULL, NULL, "%.4x");
    if (ImGui::Button("Set Breakpoint")) {
      gb_state->breakpoints->push_back({.addr = imgui_state.breakpoint_addr, .enable = true});
    }
    int i = 1;
    for (gb_breakpoint_t &bp : *gb_state->breakpoints) {

      ImGui::PushID(i);
      ImGui::Checkbox("##bp_enabled", &bp.enable);
      ImGui::SameLine();
      const debug_symbol_t *sym = symbol_from_addr(&gb_state->dbg.syms, bp.addr);
      if (sym != NULL) {
        ImGui::Text("Breakpoint %d: [%s+$%X] [$%.4X]", i, sym->name, bp.addr - sym->start_offset, bp.addr);
      } else {
        ImGui::Text("Breakpoint %d: [$%.4X]", i, bp.addr);
      }
      i++;
      ImGui::PopID();
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("PPU", ImGuiTreeNodeFlags_Framed)) {
    ImGui::TextUnformatted(std::format("LCDC: {0:#010b}", gb_state->saved.regs.io.lcdc).c_str());
    ImGui::Value("LCDC[7] - LCD Enabled", (gb_state->saved.regs.io.lcdc & LCDC_ENABLE) != 0);
    ImGui::Value("LCDC[6] - Window Tilemap", (gb_state->saved.regs.io.lcdc & LCDC_WIN_TILEMAP) != 0);
    ImGui::Value("LCDC[5] - Window Enabled", (gb_state->saved.regs.io.lcdc & LCDC_WIN_ENABLE) != 0);
    ImGui::Value("LCDC[4] - Background/Window Tile Data",
                 (gb_state->saved.regs.io.lcdc & LCDC_BG_WIN_TILE_DATA_AREA) != 0);
    ImGui::Value("LCDC[3] - Screen Enabled", (gb_state->saved.regs.io.lcdc & LCDC_BG_TILE_MAP_AREA) != 0);
    ImGui::Value("LCDC[2] - Obj Double Height", (gb_state->saved.regs.io.lcdc & LCDC_OBJ_SIZE) != 0);
    ImGui::Value("LCDC[1] - Obj Enabled", (gb_state->saved.regs.io.lcdc & LCDC_OBJ_ENABLE) != 0);
    ImGui::Value("LCDC[0] - Background/Window Enabled", (gb_state->saved.regs.io.lcdc & LCDC_BG_WIN_ENABLE) != 0);
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("Serial Port Output", ImGuiTreeNodeFlags_Framed)) {
    ImGui::TextUnformatted(gb_state->serial_port_output_string->c_str());
    ImGui::TreePop();
  }

  if (ImGui::TreeNodeEx("Layers", ImGuiTreeNodeFlags_Framed)) {
    ImGui::Checkbox("Clear Before Render", &gb_state->dbg.clear_composite);
    ImGui::Checkbox("Background Hidden", &gb_state->dbg.hide_bg);
    ImGui::Checkbox("Window Hidden", &gb_state->dbg.hide_win);
    ImGui::Checkbox("Objs Hidden", &gb_state->dbg.hide_objs);
    ImGui::TreePop();
  }

  if (ImGui::TreeNodeEx("Inspect Memory", ImGuiTreeNodeFlags_Framed)) {

    ImGui::TextUnformatted("Addr:");
    ImGui::SameLine();
    ImGui::InputScalar("##addr", ImGuiDataType_U16, &imgui_state.mem_inspect_addr, NULL, NULL, "%.4x");

    ImGui::TextUnformatted("Val: ");
    ImGui::SameLine();
    ImGui::InputScalar("##val", ImGuiDataType_U8, &imgui_state.mem_inspect_val, NULL, NULL, "%.2x");

    if (ImGui::Button("Read")) {
      imgui_state.mem_inspect_last_read_addr = imgui_state.mem_inspect_addr;
      imgui_state.mem_inspect_last_read_val  = gb_read_mem(gb_state, imgui_state.mem_inspect_addr);
    }
    ImGui::SameLine();
    if (ImGui::Button("Write")) {
      imgui_state.mem_inspect_last_write_addr = imgui_state.mem_inspect_addr;
      imgui_state.mem_inspect_last_write_val  = imgui_state.mem_inspect_val;
      gb_write_mem(gb_state, imgui_state.mem_inspect_last_write_addr, imgui_state.mem_inspect_last_write_val);
    }
    std::string formatted_read_text =
        std::format("Value read from addr {0:#06x} is:\n"
                    "  Hex: {1:#04x}\n"
                    "  Dec: {1:d}\n"
                    "  Bin: {1:#010b}",
                    imgui_state.mem_inspect_last_read_addr, imgui_state.mem_inspect_last_read_val);
    std::string formatted_write_text =
        std::format("Value written to addr {0:#06x} is:\n"
                    "  Hex: {1:#04x}\n"
                    "  Dec: {1:d}\n"
                    "  Bin: {1:#010b}",
                    imgui_state.mem_inspect_last_write_addr, imgui_state.mem_inspect_last_write_val);
    ImGui::TextUnformatted(formatted_read_text.c_str());
    ImGui::TextUnformatted(formatted_write_text.c_str());
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("Joy-Pad State", ImGuiTreeNodeFlags_Framed)) {
    ImGui::Value("Up", gb_state->joy_pad.dpad_up);
    ImGui::Value("Down", gb_state->joy_pad.dpad_down);
    ImGui::Value("Left", gb_state->joy_pad.dpad_left);
    ImGui::Value("Right", gb_state->joy_pad.dpad_right);
    ImGui::Value("A Button", gb_state->joy_pad.button_a);
    ImGui::Value("B Button", gb_state->joy_pad.button_b);
    ImGui::Value("Start Button", gb_state->joy_pad.button_start);
    ImGui::Value("Select Button", gb_state->joy_pad.button_select);
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("MBC State", ImGuiTreeNodeFlags_Framed)) {
    switch (gb_state->saved.header.mbc_type) {
    case GB_NO_MBC: ImGui::TextUnformatted("No MBC"); break;
    case GB_MBC1:
      ImGui::TextUnformatted("MBC1");
      ImGui::Value("Rom Bank", gb_state->saved.regs.mbc1_regs.rom_bank);
      break;
    default: ImGui::TextUnformatted("Debug viewer is not setup for the current MBC type."); break;
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("Reg Values", ImGuiTreeNodeFlags_Framed)) {
    gb_imgui_show_val("A", gb_state->saved.regs.a);
    gb_imgui_show_val("B", gb_state->saved.regs.b);
    gb_imgui_show_val("C", gb_state->saved.regs.c);
    gb_imgui_show_val("D", gb_state->saved.regs.d);
    gb_imgui_show_val("E", gb_state->saved.regs.e);
    gb_imgui_show_val("F", gb_state->saved.regs.f);
    gb_imgui_show_val("H", gb_state->saved.regs.h);
    gb_imgui_show_val("L", gb_state->saved.regs.l);
    gb_imgui_show_val("PC", gb_state->saved.regs.pc);
    gb_imgui_show_val("SP", gb_state->saved.regs.sp);
    ImGui::Value("IME", gb_state->saved.regs.io.ime);
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("IO Reg Values", ImGuiTreeNodeFlags_Framed)) {
    for (io_reg_addr_t io_reg : io_regs) {
      gb_imgui_show_mem_val(gb_state, gb_io_reg_name(io_reg), io_reg);
    }
    ImGui::TreePop();
  }

  ImGui::End();
}

static void gb_imgui_settings_win(gb_state_t *gb_state) {
  if (ImGui::Begin("Settings")) {
    ImGui::SliderFloat("Internal GB Speed", &gb_state->dbg.speed_factor, 0.0f, 10.0f);
    ImGui::Checkbox("Pause on Error", &gb_state->dbg.pause_on_err);
    ImGui::Checkbox("Print Instructions", &gb_state->dbg.trace_exec);
    ImGui::Checkbox("Show Scanline", &gb_state->imgui.show_scanline);
  }
  ImGui::End();
}

void gb_imgui_render(gb_state_t *gb_state) {
  gb_imgui_state_t &imgui_state = gb_state->imgui;
  ImGuiIO          &io          = ImGui::GetIO();
  (void)io;

  GB_CheckSDLCall(
      SDL_SetRenderLogicalPresentation(gb_state->video.sdl_renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
  // Start ImGui frame
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  gb_imgui_main_menu_bar(gb_state);

  if (imgui_state.fs_dockspace) {
    ImGui::DockSpaceOverViewport();
    gb_imgui_display_viewport_win(gb_state);
  }

  if (imgui_state.oam_viewer) {
    gb_imgui_oam_viewer(gb_state);
  }

  if (imgui_state.state_inspector) {
    gb_imgui_state_inspector_win(gb_state);
  }

  if (imgui_state.settings) {
    gb_imgui_settings_win(gb_state);
  }

  ImGui::Render();

  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), gb_state->video.sdl_renderer);

  GB_CheckSDLCall(SDL_SetRenderLogicalPresentation(gb_state->video.sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                                   SDL_LOGICAL_PRESENTATION_LETTERBOX));
}
