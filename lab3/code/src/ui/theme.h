// Copyright 2026 R
// 界面主题：沿用实验二的天空蓝毛玻璃风格（渐变天空、白云、半透明圆角卡片、
// 蓝色主按钮和猫耳吉祥物），并提供卡片、标签、按钮等绘制工具。
#ifndef UI_THEME_H_
#define UI_THEME_H_

#include "domain/model.h"
#include "imgui/imgui.h"

namespace ticketing {
namespace ui {

// 状态颜色约定：绿=成功，橙=候补，灰=取消/失效，红=缺席/驳回，蓝=进行中。
enum class Tone { kBlue, kGreen, kOrange, kGray, kRed, kPurple };

// 加载微软雅黑常规体和粗体。必须在第一帧之前调用。
void LoadFonts();
// 设置颜色、圆角和间距；scale 为显示器缩放比例。
void ApplyTheme(float scale);
// 把设计稿中的像素值按缩放比例换算。
float Px(float value);

ImU32 ToneColor(Tone tone);
Tone ToneOf(RegStatus status);
Tone ToneOf(ActivityStatus status);

// 天空背景：自上而下的蓝色渐变和几朵云。
void DrawSky(ImDrawList* list, ImVec2 min, ImVec2 max);

// 半透明圆角卡片。height 为 0 时高度随内容变化。
void BeginCard(const char* id, ImVec2 size, float alpha = 0.62f,
               ImGuiWindowFlags window_flags = 0);
void EndCard();

// 粗体标题，size 为未缩放的字号。
void Heading(const char* text, float size);
// 灰色说明文字（自动换行）。
void Muted(const char* text);
// 彩色圆角标签，和前一个元素在同一行时请先调用 SameLine。
void Tag(const char* text, Tone tone);
// 实心按钮和浅色按钮。enabled 为 false 时显示为灰色且不可点击。
bool FilledButton(const char* label, Tone tone, ImVec2 size = ImVec2(0, 0),
                  bool enabled = true);
bool GhostButton(const char* label, ImVec2 size = ImVec2(0, 0),
                 bool enabled = true);
// 分段选择器（如“登录 / 注册”），返回是否切换。
bool Segmented(const char* id, const char* const* labels, int count,
               int* current, float width);
// 统计卡片中的一项：小标题、大数字和说明。
void StatTile(const char* id, const char* label, const char* value,
              const char* note, ImU32 color, float width);

// 吉祥物贴图，由 main 加载后设置。
void SetMascotTexture(ImTextureID texture, int width, int height);
// 让吉祥物“咬住”卡片右上角：corner 为卡片右上角坐标，width 为显示宽度。
// 画在最上层，不受卡片裁剪。
void DrawMascot(ImVec2 corner, float width);
// 吉祥物趴着的小提示卡，放在当前容器内 y 处，宽 width。
void PerchTip(const char* id, const char* text, float width, float y);

}  // namespace ui
}  // namespace ticketing

#endif  // UI_THEME_H_
