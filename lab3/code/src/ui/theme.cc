// Copyright 2026 R
#include "ui/theme.h"

#include <algorithm>

namespace ticketing {
namespace ui {
namespace {

float g_scale = 1.0f;
ImFont* g_bold = nullptr;
ImTextureID g_mascot = 0;
int g_mascot_width = 0;
int g_mascot_height = 0;

constexpr ImU32 kInk = IM_COL32(18, 33, 58, 255);
constexpr ImU32 kInkMuted = IM_COL32(104, 120, 146, 255);

ImVec4 Rgba(int r, int g, int b, float a = 1.0f) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

// 用几个半透明圆拼出一朵云。
void Cloud(ImDrawList* list, ImVec2 center, float size, float alpha) {
  const ImU32 color = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255));
  const float offsets[][3] = {{-0.9f, 0.15f, 0.55f}, {-0.35f, -0.2f, 0.75f},
                              {0.35f, -0.1f, 0.65f}, {0.95f, 0.2f, 0.5f},
                              {0.0f, 0.3f, 0.6f}};
  for (const auto& o : offsets) {
    list->AddCircleFilled(
        ImVec2(center.x + o[0] * size, center.y + o[1] * size), o[2] * size,
        color, 48);
  }
}

// 把颜色调亮或调暗，factor > 1 变亮。
ImVec4 Shade(ImU32 color, float factor) {
  ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
  if (factor > 1.0f) {
    const float t = factor - 1.0f;
    return ImVec4(c.x + (1 - c.x) * t, c.y + (1 - c.y) * t,
                  c.z + (1 - c.z) * t, c.w);
  }
  return ImVec4(c.x * factor, c.y * factor, c.z * factor, c.w);
}

}  // namespace

float Px(float value) { return value * g_scale; }

void LoadFonts() {
  ImGuiIO& io = ImGui::GetIO();
  ImFont* regular = io.Fonts->AddFontFromFileTTF(
      "C:\\Windows\\Fonts\\msyh.ttc", 16.0f);
  if (regular == nullptr) {
    regular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf",
                                           16.0f);
  }
  if (regular == nullptr) io.Fonts->AddFontDefault();
  g_bold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyhbd.ttc",
                                        16.0f);
  if (g_bold == nullptr) g_bold = regular;
}

void ApplyTheme(float scale) {
  g_scale = scale;
  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsLight(&style);
  style.WindowPadding = ImVec2(20, 18);
  style.FramePadding = ImVec2(12, 7);
  style.ItemSpacing = ImVec2(10, 9);
  style.ItemInnerSpacing = ImVec2(8, 6);
  style.WindowRounding = 0;
  style.ChildRounding = 20;
  style.FrameRounding = 12;
  style.PopupRounding = 16;
  style.GrabRounding = 10;
  style.ScrollbarRounding = 10;
  style.ScrollbarSize = 10;
  style.WindowBorderSize = 0;
  style.ChildBorderSize = 1.5f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.ScaleAllSizes(scale);
  style.FontSizeBase = 16.0f;
  style.FontScaleDpi = scale;

  ImVec4* c = style.Colors;
  c[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(kInk);
  c[ImGuiCol_TextDisabled] = Rgba(150, 160, 178);
  c[ImGuiCol_WindowBg] = Rgba(255, 255, 255, 0.0f);
  c[ImGuiCol_ChildBg] = Rgba(255, 255, 255, 0.0f);
  c[ImGuiCol_PopupBg] = Rgba(250, 252, 255, 0.98f);
  c[ImGuiCol_Border] = Rgba(196, 222, 248);
  c[ImGuiCol_FrameBg] = Rgba(255, 255, 255, 0.92f);
  c[ImGuiCol_FrameBgHovered] = Rgba(255, 255, 255);
  c[ImGuiCol_FrameBgActive] = Rgba(240, 248, 255);
  c[ImGuiCol_Button] = Rgba(16, 140, 240);
  c[ImGuiCol_ButtonHovered] = Rgba(52, 162, 248);
  c[ImGuiCol_ButtonActive] = Rgba(10, 116, 210);
  c[ImGuiCol_Header] = Rgba(214, 235, 255);
  c[ImGuiCol_HeaderHovered] = Rgba(226, 241, 255);
  c[ImGuiCol_HeaderActive] = Rgba(196, 226, 255);
  c[ImGuiCol_CheckMark] = Rgba(16, 140, 240);
  c[ImGuiCol_SliderGrab] = Rgba(16, 140, 240);
  c[ImGuiCol_ScrollbarBg] = Rgba(255, 255, 255, 0.0f);
  c[ImGuiCol_ScrollbarGrab] = Rgba(150, 190, 230, 0.7f);
  c[ImGuiCol_TableHeaderBg] = Rgba(232, 243, 255);
  c[ImGuiCol_TableRowBgAlt] = Rgba(255, 255, 255, 0.35f);
  c[ImGuiCol_TableBorderLight] = Rgba(214, 230, 248);
  c[ImGuiCol_TableBorderStrong] = Rgba(196, 222, 248);
  c[ImGuiCol_Separator] = Rgba(196, 222, 248);
  c[ImGuiCol_ModalWindowDimBg] = Rgba(13, 40, 80, 0.35f);
  c[ImGuiCol_PlotHistogram] = Rgba(16, 140, 240);
}

ImU32 ToneColor(Tone tone) {
  switch (tone) {
    case Tone::kBlue: return IM_COL32(16, 140, 240, 255);
    case Tone::kGreen: return IM_COL32(22, 163, 106, 255);
    case Tone::kOrange: return IM_COL32(240, 138, 0, 255);
    case Tone::kGray: return IM_COL32(130, 142, 160, 255);
    case Tone::kRed: return IM_COL32(225, 72, 80, 255);
    case Tone::kPurple: return IM_COL32(128, 92, 220, 255);
  }
  return kInk;
}

Tone ToneOf(RegStatus status) {
  switch (status) {
    case RegStatus::kConfirmed: return Tone::kGreen;
    case RegStatus::kCheckedIn: return Tone::kBlue;
    case RegStatus::kWaiting: return Tone::kOrange;
    case RegStatus::kAbsent: return Tone::kRed;
    case RegStatus::kCancelled:
    case RegStatus::kExpired: return Tone::kGray;
  }
  return Tone::kGray;
}

Tone ToneOf(ActivityStatus status) {
  switch (status) {
    case ActivityStatus::kPublished: return Tone::kGreen;
    case ActivityStatus::kPending: return Tone::kBlue;
    case ActivityStatus::kDraft: return Tone::kPurple;
    case ActivityStatus::kEnded:
    case ActivityStatus::kCancelled: return Tone::kGray;
  }
  return Tone::kGray;
}

void DrawSky(ImDrawList* list, ImVec2 min, ImVec2 max) {
  const ImU32 top = IM_COL32(58, 152, 240, 255);
  const ImU32 bottom = IM_COL32(146, 204, 255, 255);
  list->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
  const float w = max.x - min.x;
  const float h = max.y - min.y;
  // 云朵避开左上角的页面标题，以免白字看不清。
  Cloud(list, ImVec2(min.x + w * 0.03f, min.y + h * 0.42f), Px(64), 0.45f);
  Cloud(list, ImVec2(min.x + w * 0.62f, min.y + h * 0.14f), Px(48), 0.35f);
  Cloud(list, ImVec2(min.x + w * 0.93f, min.y + h * 0.88f), Px(84), 0.6f);
  Cloud(list, ImVec2(min.x + w * 0.42f, min.y + h * 0.97f), Px(60), 0.5f);
  Cloud(list, ImVec2(min.x + w * 0.04f, min.y + h * 0.80f), Px(50), 0.4f);
}

void BeginCard(const char* id, ImVec2 size, float alpha,
               ImGuiWindowFlags window_flags) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, alpha));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.92f));
  ImGuiChildFlags flags =
      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
  if (size.y == 0) flags |= ImGuiChildFlags_AutoResizeY;
  ImGui::BeginChild(id, size, flags, window_flags);
  ImGui::PopStyleColor(2);
}

void EndCard() { ImGui::EndChild(); }

void Heading(const char* text, float size) {
  ImGui::PushFont(g_bold, size);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

void Muted(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, kInkMuted);
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void Tag(const char* text, Tone tone) {
  if (text == nullptr || text[0] == '\0') return;
  ImGui::PushFont(g_bold, 13.0f);
  const ImVec2 text_size = ImGui::CalcTextSize(text);
  const ImVec2 pad(Px(9), Px(3));
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 box(text_size.x + pad.x * 2, text_size.y + pad.y * 2);
  const ImU32 color = ToneColor(tone);
  ImVec4 bg = ImGui::ColorConvertU32ToFloat4(color);
  bg.w = 0.14f;
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y),
                      ImGui::ColorConvertFloat4ToU32(bg), box.y * 0.5f);
  list->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), color, text);
  ImGui::Dummy(box);
  ImGui::PopFont();
}

bool FilledButton(const char* label, Tone tone, ImVec2 size, bool enabled) {
  const ImU32 color = ToneColor(tone);
  ImGui::PushStyleColor(ImGuiCol_Button, Shade(color, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Shade(color, 1.15f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Shade(color, 0.85f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0));
  ImGui::PushFont(g_bold, 0.0f);
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::Button(label, size);
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopFont();
  ImGui::PopStyleColor(5);
  return clicked;
}

bool GhostButton(const char* label, ImVec2 size, bool enabled) {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.85f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 1));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Shade(IM_COL32_WHITE, 0.94f));
  ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::ColorConvertU32ToFloat4(
                            ToneColor(Tone::kBlue)));
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::Button(label, size);
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopStyleColor(4);
  return clicked;
}

bool Segmented(const char* id, const char* const* labels, int count,
               int* current, float width) {
  bool changed = false;
  ImGui::PushID(id);
  const float item_width = (width - Px(8) * (count - 1)) / count;
  for (int i = 0; i < count; ++i) {
    if (i > 0) ImGui::SameLine(0, Px(8));
    const bool active = *current == i;
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 1));
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
                                               ToneColor(Tone::kBlue)));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.35f));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.36f, 0.46f, 1));
    }
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.9f));
    ImGui::PushFont(g_bold, 0.0f);
    if (ImGui::Button(labels[i], ImVec2(item_width, Px(38))) && !active) {
      *current = i;
      changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
  }
  ImGui::PopID();
  return changed;
}

void StatTile(const char* id, const char* label, const char* value,
              const char* note, ImU32 color, float width) {
  BeginCard(id, ImVec2(width, 0), 0.85f);
  Muted(label);
  ImGui::PushFont(g_bold, 26.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(value);
  ImGui::PopStyleColor();
  ImGui::PopFont();
  ImGui::PushFont(nullptr, 13.0f);
  Muted(note);
  ImGui::PopFont();
  EndCard();
}

void SetMascotTexture(ImTextureID texture, int width, int height) {
  g_mascot = texture;
  g_mascot_width = width;
  g_mascot_height = height;
}

void DrawMascot(ImVec2 corner, float width) {
  if (g_mascot == 0 || g_mascot_width == 0) return;
  // 弹出窗口打开时不画，避免浮在遮罩上面。
  if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId |
                                 ImGuiPopupFlags_AnyPopupLevel)) {
    return;
  }
  const float height = width * g_mascot_height / g_mascot_width;
  // 参考图中卡片右上角落在吉祥物宽 62%、高 55% 的位置。
  const ImVec2 min(corner.x - width * 0.62f, corner.y - height * 0.55f);
  const ImVec2 max(min.x + width, min.y + height);
  ImGui::GetForegroundDrawList()->AddImage(g_mascot, min, max);
}

void PerchTip(const char* id, const char* text, float width, float y) {
  ImGui::SetCursorPosY(y);
  BeginCard(id, ImVec2(width, 0), 0.8f);
  ImGui::Dummy(ImVec2(0, Px(14)));
  ImGui::PushFont(nullptr, 13.0f);
  Muted(text);
  ImGui::PopFont();
  EndCard();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  DrawMascot(ImVec2(max.x, min.y), Px(96));
}

}  // namespace ui
}  // namespace ticketing
