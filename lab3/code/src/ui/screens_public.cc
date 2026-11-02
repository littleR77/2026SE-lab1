// Copyright 2026 R
// 公开页面：登录 / 注册（界面 1）、活动广场（界面 2）、
// 活动详情与报名（界面 3）。
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "common/time_util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace ticketing {
namespace ui {
namespace {

// 卡片顶部色带按分类区分。
ImU32 CategoryColor(const std::string& category) {
  if (category == "演出") return IM_COL32(120, 196, 255, 255);
  if (category == "讲座") return IM_COL32(186, 160, 255, 255);
  if (category == "工作坊") return IM_COL32(255, 184, 110, 255);
  if (category == "社团") return IM_COL32(120, 214, 170, 255);
  return IM_COL32(170, 190, 215, 255);
}

// 面向公众的活动状态：在“已发布”内部再细分报名情况。
struct PublicStatus {
  const char* text;
  Tone tone;
};

PublicStatus StatusFor(const System& sys, Store* store,
                       const Activity& activity, int remaining) {
  switch (activity.status) {
    case ActivityStatus::kDraft: return {"草稿", Tone::kPurple};
    case ActivityStatus::kPending: return {"待审核", Tone::kBlue};
    case ActivityStatus::kEnded: return {"已结束", Tone::kGray};
    case ActivityStatus::kCancelled: return {"已取消", Tone::kGray};
    case ActivityStatus::kPublished: break;
  }
  const Time now = sys.clock.Now();
  bool open = false, upcoming = false, running = false;
  for (const EventSession* session : store->SessionsOf(activity.id)) {
    if (session->cancelled || session->finished) continue;
    if (now >= session->reg_open_at && now < session->reg_close_at) open = true;
    if (now < session->reg_open_at) upcoming = true;
    if (now >= session->start_at && now < session->end_at) running = true;
  }
  if (running) return {"进行中", Tone::kBlue};
  if (open) {
    return remaining > 0 ? PublicStatus{"报名中", Tone::kGreen}
                         : PublicStatus{"已满 · 可候补", Tone::kOrange};
  }
  if (upcoming) return {"即将开放", Tone::kBlue};
  return {"报名截止", Tone::kGray};
}

// 细长的进度条。
void ThinBar(float fraction, float width, ImU32 color) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float h = Px(6);
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + h),
                      IM_COL32(255, 255, 255, 230), h);
  const float filled = width * std::min(std::max(fraction, 0.0f), 1.0f);
  if (filled > 0) {
    list->AddRectFilled(pos, ImVec2(pos.x + filled, pos.y + h), color, h);
  }
  ImGui::Dummy(ImVec2(width, h));
}

// 画五颗星，返回被点击的星级（0 表示没有点击）。
int Stars(const char* id, int value, bool clickable) {
  if (!clickable) {
    // 只读时用文字绘制，和同一行的数字按基线对齐。
    for (int i = 1; i <= 5; ++i) {
      if (i > 1) ImGui::SameLine(0, Px(4));
      ImGui::TextColored(i <= value ? ImVec4(0.96f, 0.68f, 0.05f, 1)
                                    : ImVec4(0.78f, 0.82f, 0.9f, 1),
                         "★");
    }
    return 0;
  }
  int clicked = 0;
  ImGui::PushID(id);
  for (int i = 1; i <= 5; ++i) {
    if (i > 1) ImGui::SameLine(0, Px(2));
    ImGui::PushID(i);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, i <= value
                                             ? ImVec4(0.96f, 0.68f, 0.05f, 1)
                                             : ImVec4(0.78f, 0.82f, 0.9f, 1));
    if (!clickable) ImGui::BeginDisabled();
    if (ImGui::Button("★", ImVec2(Px(26), 0))) clicked = i;
    if (!clickable) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);
    ImGui::PopID();
  }
  ImGui::PopID();
  return clicked;
}

// 圆形头像，显示名字的第一个字。
void Avatar(const std::string& name, ImU32 color) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float r = Px(16);
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->AddCircleFilled(ImVec2(pos.x + r, pos.y + r), r, color);
  // 取第一个 UTF-8 字符。
  size_t len = 1;
  const unsigned char lead = name.empty() ? 0 : name[0];
  if (lead >= 0xF0) {
    len = 4;
  } else if (lead >= 0xE0) {
    len = 3;
  } else if (lead >= 0xC0) {
    len = 2;
  }
  const std::string first = name.substr(0, len);
  const ImVec2 text = ImGui::CalcTextSize(first.c_str());
  list->AddText(ImVec2(pos.x + r - text.x / 2, pos.y + r - text.y / 2),
                IM_COL32_WHITE, first.c_str());
  ImGui::Dummy(ImVec2(r * 2, r * 2));
}

}  // namespace

void App::DrawLogin(ImVec2 size) {
  const float card_width = Px(400);
  const float x = (size.x - card_width) * 0.5f;
  ImGui::SetCursorPos(ImVec2(x + Px(94), Px(70)));
  ImGui::BeginGroup();
  {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float s = Px(46);
    list->AddRectFilled(pos, ImVec2(pos.x + s, pos.y + s),
                        ToneColor(Tone::kBlue), Px(14));
    list->AddCircleFilled(ImVec2(pos.x + s * 0.4f, pos.y + s * 0.55f),
                          s * 0.18f, IM_COL32_WHITE);
    list->AddCircleFilled(ImVec2(pos.x + s * 0.58f, pos.y + s * 0.45f),
                          s * 0.21f, IM_COL32_WHITE);
    list->AddRectFilled(ImVec2(pos.x + s * 0.28f, pos.y + s * 0.55f),
                        ImVec2(pos.x + s * 0.78f, pos.y + s * 0.73f),
                        IM_COL32_WHITE, s * 0.08f);
    ImGui::Dummy(ImVec2(s, s));
  }
  ImGui::SameLine(0, Px(12));
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading("活动现场", 30);
  ImGui::PopStyleColor();
  ImGui::EndGroup();
  ImGui::SetCursorPos(ImVec2(x + Px(98), Px(128)));
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  ImGui::TextUnformatted("讲座 · 工作坊 · 演出 · 社团活动");
  ImGui::PopStyleColor();

  ImGui::SetCursorPos(ImVec2(x, Px(210)));
  BeginCard("login", ImVec2(card_width, 0), 0.62f);
  ImGui::Dummy(ImVec2(0, Px(6)));
  const float inner = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX((card_width - Px(144)) * 0.5f);
  Heading("欢迎回来", 34);
  ImGui::Dummy(ImVec2(0, Px(4)));
  const char* tabs[] = {"登录", "注册"};
  if (Segmented("tabs", tabs, 2, &login_tab_, inner)) login_error_.clear();
  ImGui::Dummy(ImVec2(0, Px(4)));
  ImGui::SetNextItemWidth(inner);
  ImGui::InputTextWithHint("##account", "请输入账号", account_,
                           sizeof(account_));
  if (login_tab_ == 1) {
    ImGui::SetNextItemWidth(inner);
    ImGui::InputTextWithHint("##nick", "请输入昵称", nickname_,
                             sizeof(nickname_));
  }
  ImGui::SetNextItemWidth(inner);
  const bool enter = ImGui::InputTextWithHint(
      "##password", "请输入密码", password_, sizeof(password_),
      ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::Dummy(ImVec2(0, Px(4)));
  const bool submit =
      FilledButton(login_tab_ == 0 ? "登录" : "注册并登录", Tone::kBlue,
                   ImVec2(inner, Px(46))) ||
      enter;
  if (submit) {
    Status status = Status::Ok();
    if (login_tab_ == 1) {
      status = sys_->accounts.Register(account_, nickname_, password_,
                                       Role::kParticipant, nullptr);
    }
    if (status.ok()) status = sys_->accounts.Login(account_, password_,
                                                   &actor_);
    if (status.ok()) {
      login_error_.clear();
      if (actor_.role == Role::kChecker) {
        Go(Screen::kCheckin);
      } else if (actor_.role == Role::kAdmin) {
        Go(Screen::kReview);
      } else if (actor_.role == Role::kOrganizer) {
        Go(Screen::kMyActivities);
      } else {
        Go(Screen::kPlaza);
      }
      ShowResult(Status::Ok("欢迎，" + sys_->accounts.NameOf(actor_.user_id)));
    } else {
      login_error_ = status.message();
    }
  }
  if (!login_error_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kRed));
    ImGui::TextWrapped("%s", login_error_.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::PushFont(nullptr, 13.0f);
  Muted("自行注册的账号为参与者；组织者、检票员和管理员账号由平台分配。");
  ImGui::PopFont();
  EndCard();
  const ImVec2 card_min = ImGui::GetItemRectMin();
  const ImVec2 card_max = ImGui::GetItemRectMax();
  DrawMascot(ImVec2(card_max.x, card_min.y), Px(120));

  ImGui::SetCursorPos(ImVec2(x, card_max.y - ImGui::GetWindowPos().y +
                                     Px(20)));
  BeginCard("guest", ImVec2(card_width, 0), 0.78f);
  ImGui::BeginGroup();
  Heading("先随便看看", 18);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("游客可浏览活动、发表评论和回复");
  ImGui::PopFont();
  ImGui::EndGroup();
  ImGui::SameLine(card_width - Px(150));
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(6));
  if (GhostButton("游客浏览 ›", ImVec2(Px(110), Px(38)))) {
    actor_ = Actor();
    Go(Screen::kPlaza);
  }
  EndCard();

  ImGui::SetCursorPos(ImVec2(x - Px(40), size.y - Px(70)));
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 235));
  ImGui::TextUnformatted(
      "演示账号（密码均为 123456）：alice / bob 参与者 · org 组织者 · "
      "checker 检票员 · admin 管理员");
  ImGui::PopStyleColor();
}

void App::DrawPlaza(ImVec2 size) {
  // 筛选条：条件改变时立即刷新列表，输入内容保持不变（REQ-026）。
  BeginCard("filters", ImVec2(size.x, 0), 0.62f);
  std::vector<std::string> categories = sys_->queries.Categories();
  categories.insert(categories.begin(), "全部");
  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    if (i > 0) ImGui::SameLine(0, Px(6));
    if (category_ == i) {
      if (FilledButton(categories[i].c_str(), Tone::kBlue)) category_ = i;
    } else if (GhostButton(categories[i].c_str())) {
      category_ = i;
    }
  }
  ImGui::SameLine(0, Px(24));
  ImGui::SetNextItemWidth(Px(120));
  ImGui::InputTextWithHint("##from", "开始 2026-11-01", date_from_,
                           sizeof(date_from_));
  ImGui::SameLine(0, Px(4));
  ImGui::TextUnformatted("至");
  ImGui::SameLine(0, Px(4));
  ImGui::SetNextItemWidth(Px(120));
  ImGui::InputTextWithHint("##to", "结束 2026-11-30", date_to_,
                           sizeof(date_to_));
  ImGui::SameLine(0, Px(10));
  const char* statuses[] = {"全部状态", "已发布", "已结束", "已取消"};
  ImGui::SetNextItemWidth(Px(110));
  ImGui::Combo("##status", &status_filter_, statuses, 4);
  ImGui::SameLine(0, Px(10));
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - Px(70));
  ImGui::InputTextWithHint("##kw", "搜索活动名称、介绍或地点", keyword_,
                           sizeof(keyword_));
  ImGui::SameLine(0, Px(8));
  if (GhostButton("清空")) {
    category_ = 0;
    status_filter_ = 0;
    date_from_[0] = date_to_[0] = keyword_[0] = '\0';
  }
  EndCard();

  ActivityFilter filter;
  if (category_ > 0 && category_ < static_cast<int>(categories.size())) {
    filter.category = categories[category_];
  }
  const int status_values[] = {-1, 2, 3, 4};
  filter.status = status_values[status_filter_];
  filter.keyword = keyword_;
  std::string date_error;
  if (date_from_[0] != '\0' && !ParseTime(date_from_, &filter.from)) {
    date_error = "开始日期格式应为 2026-11-01";
  }
  if (date_to_[0] != '\0' && !ParseTime(date_to_, &filter.to)) {
    date_error = "结束日期格式应为 2026-11-30";
  }
  std::vector<const Activity*> found;
  if (date_error.empty()) {
    const Status status = sys_->queries.Search(actor_, filter, &found);
    if (!status.ok()) date_error = status.message();
  }

  ImGui::Dummy(ImVec2(0, Px(4)));
  const float body_top = ImGui::GetCursorPosY();
  const float body_height = size.y - (body_top - Px(16)) + Px(8);
  const float left_width = Px(300);

  // 左栏：一周内即将开始的场次。
  BeginCard("upcoming", ImVec2(left_width, body_height), 0.55f);
  Heading("本周值得期待", 20);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("按开始时间排序的已发布场次");
  ImGui::PopFont();
  std::vector<const EventSession*> soon;
  for (const EventSession& session : sys_->store.data().sessions) {
    const Activity* activity = sys_->store.FindActivity(session.activity_id);
    if (activity->status == ActivityStatus::kPublished && !session.cancelled &&
        session.start_at >= sys_->clock.Now() &&
        session.start_at < sys_->clock.Now() + 7 * kMinutesPerDay) {
      soon.push_back(&session);
    }
  }
  std::sort(soon.begin(), soon.end(),
            [](const EventSession* a, const EventSession* b) {
              return a->start_at < b->start_at;
            });
  for (size_t i = 0; i < soon.size() && i < 3; ++i) {
    const Activity* activity = sys_->store.FindActivity(soon[i]->activity_id);
    ImGui::PushID(static_cast<int>(soon[i]->id));
    BeginCard("soon", ImVec2(0, 0), 0.92f);
    Tag(activity->category.c_str(), Tone::kBlue);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - Px(70));
    ImGui::PushFont(nullptr, 13.0f);
    Muted((FormatShort(soon[i]->start_at).substr(0, 5) + " " +
           WeekdayName(soon[i]->start_at)).c_str());
    ImGui::PopFont();
    Heading(activity->title.c_str(), 16);
    ImGui::PushFont(nullptr, 13.0f);
    Muted((FormatShort(soon[i]->start_at).substr(6) + " · " +
           soon[i]->location).c_str());
    ImGui::PopFont();
    const bool clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);
    EndCard();
    if (clicked) OpenDetail(activity->id);
    ImGui::PopID();
  }
  PerchTip("tip", "切换分类或日期时只刷新右侧列表，保留输入内容和浏览位置。",
           left_width - Px(70), body_height - Px(116));
  EndCard();

  // 右侧：活动卡片网格。
  ImGui::SameLine(0, Px(20));
  ImGui::BeginGroup();
  const std::string count = "找到 " + std::to_string(found.size()) + " 个活动";
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading(count.c_str(), 20);
  ImGui::PopStyleColor();
  if (!date_error.empty()) {
    ImGui::SameLine(0, Px(16));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 236, 236, 255));
    ImGui::TextUnformatted(date_error.c_str());
    ImGui::PopStyleColor();
  }
  const float grid_width = size.x - left_width - Px(20);
  ImGui::BeginChild("grid", ImVec2(grid_width, body_height - Px(40)));
  const float gap = Px(18);
  const float card_width = (grid_width - gap * 2 - Px(12)) / 3;
  if (found.empty()) {
    Muted("没有符合条件的活动，试试清空条件。");
  }
  for (size_t i = 0; i < found.size(); ++i) {
    if (i % 3 != 0) ImGui::SameLine(0, gap);
    DrawActivityCard(*found[i], card_width);
  }
  ImGui::EndChild();
  ImGui::EndGroup();
}

void App::DrawActivityCard(const Activity& activity, float width) {
  const int remaining = Remaining(activity.id);
  const int capacity = Capacity(activity.id);
  const PublicStatus status =
      StatusFor(*sys_, &sys_->store, activity, remaining);
  const bool closed = activity.status == ActivityStatus::kEnded ||
                      activity.status == ActivityStatus::kCancelled;
  ImGui::PushID(static_cast<int>(activity.id));
  if (closed) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.62f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  BeginCard("card", ImVec2(width, Px(236)), closed ? 0.5f : 0.75f);
  ImGui::PopStyleVar();
  // 顶部色带。
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImDrawList* list = ImGui::GetWindowDrawList();
  const float band = Px(70);
  const ImU32 color = closed ? IM_COL32(176, 186, 200, 255)
                             : CategoryColor(activity.category);
  list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + band), color,
                      Px(20), ImDrawFlags_RoundCornersTop);
  list->AddText(nullptr, Px(28), ImVec2(pos.x + Px(18), pos.y + Px(20)),
                IM_COL32(255, 255, 255, 230), activity.category.c_str());
  ImGui::SetCursorPos(ImVec2(width - Px(110), Px(14)));
  ImGui::PushFont(nullptr, 0.0f);
  Tag(status.text, status.tone);
  ImGui::PopFont();
  ImGui::SetCursorPos(ImVec2(Px(18), band + Px(12)));
  ImGui::BeginGroup();
  ImGui::PushTextWrapPos(width - Px(18));
  Heading(activity.title.c_str(), 17);
  ImGui::PopTextWrapPos();
  const std::vector<EventSession*> sessions =
      sys_->store.SessionsOf(activity.id);
  std::string when;
  if (!sessions.empty()) {
    when = FormatShort(sessions.front()->start_at) + " · " +
           sessions.front()->location;
    if (sessions.size() > 1) {
      when += "（共 " + std::to_string(sessions.size()) + " 场）";
    }
  }
  ImGui::PushFont(nullptr, 13.0f);
  Muted(when.c_str());
  char seats[64];
  snprintf(seats, sizeof(seats), "剩余名额 %d / %d", remaining, capacity);
  Muted(seats);
  ImGui::PopFont();
  ThinBar(capacity == 0 ? 0.0f
                        : 1.0f - static_cast<float>(remaining) / capacity,
          width - Px(36),
          remaining == 0 ? ToneColor(Tone::kOrange) : ToneColor(Tone::kBlue));
  ImGui::Dummy(ImVec2(0, Px(2)));
  Tag(activity.category.c_str(), Tone::kBlue);
  ImGui::SameLine(width - Px(60));
  FavoriteStar(activity.id);
  ImGui::EndGroup();
  const bool clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) &&
                       !ImGui::IsAnyItemHovered();
  EndCard();
  if (closed) ImGui::PopStyleVar();
  ImGui::PopID();
  if (clicked) OpenDetail(activity.id);
}

void App::DrawDetail(ImVec2 size) {
  const Activity* activity = sys_->store.FindActivity(detail_id_);
  if (activity == nullptr || !CanView(actor_, *activity)) {
    Muted("活动不存在或无权查看。");
    if (GhostButton("返回活动广场")) Go(Screen::kPlaza);
    return;
  }
  const float right_width = Px(440);
  const float left_width = size.x - right_width - Px(20);
  BeginCard("info", ImVec2(left_width, size.y), 0.6f);
  ImGui::PushFont(nullptr, 13.0f);
  if (GhostButton("‹ 返回")) Go(Screen::kPlaza);
  ImGui::SameLine();
  Muted(("活动广场 › " + activity->category).c_str());
  ImGui::PopFont();
  Heading(activity->title.c_str(), 26);
  ImGui::SameLine(0, Px(12));
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(8));
  const PublicStatus status =
      StatusFor(*sys_, &sys_->store, *activity, Remaining(activity->id));
  Tag(status.text, status.tone);
  const std::vector<EventSession*> sessions =
      sys_->store.SessionsOf(activity->id);
  ImGui::TextUnformatted(
      ("主办：" + sys_->accounts.NameOf(activity->organizer_id)).c_str());
  ImGui::SameLine();
  Tag("活动官方", Tone::kPurple);
  ImGui::SameLine(0, Px(16));
  if (!sessions.empty()) {
    ImGui::TextUnformatted((sessions.front()->location + " · 共 " +
                            std::to_string(sessions.size()) + " 场")
                               .c_str());
  }
  Muted(activity->description.c_str());
  if (activity->status == ActivityStatus::kCancelled) {
    ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kRed));
    ImGui::TextWrapped("活动已取消：%s", activity->cancel_reason.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::Dummy(ImVec2(0, Px(4)));

  // 评分区：平均分保留一位小数，活动结束后才能评分（REQ-032、REQ-034）。
  const float half = (ImGui::GetContentRegionAvail().x - Px(12)) / 2;
  const ImGuiWindowFlags fixed =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  BeginCard("rating", ImVec2(half, Px(118)), 0.9f, fixed);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("活动评分");
  ImGui::PopFont();
  double average = 0;
  int count = 0;
  if (sys_->comments.Average(activity->id, &average, &count)) {
    char text[16];
    snprintf(text, sizeof(text), "%.1f", average);
    Heading(text, 30);
    ImGui::SameLine(0, Px(12));
    Stars("avg", static_cast<int>(average + 0.5), false);
    ImGui::PushFont(nullptr, 13.0f);
    Muted((std::to_string(count) + " 人评价 · 平均分保留一位小数").c_str());
    ImGui::PopFont();
  } else {
    Heading("暂无评分", 22);
  }
  EndCard();
  ImGui::SameLine(0, Px(12));
  BeginCard("mine", ImVec2(half, Px(118)), 0.9f, fixed);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("我的评分");
  ImGui::PopFont();
  const bool can_rate =
      IsParticipant() && activity->status == ActivityStatus::kEnded;
  const int star = Stars("my", sys_->comments.MyRating(actor_, activity->id),
                         can_rate);
  if (star > 0) ShowResult(sys_->comments.Rate(actor_, activity->id, star));
  ImGui::PushFont(nullptr, 13.0f);
  Muted(can_rate ? "点击星星即可评分，可以修改"
                 : "活动结束后，参与者可评分（1～5 星）");
  ImGui::PopFont();
  EndCard();

  ImGui::Dummy(ImVec2(0, Px(4)));
  DrawComments(*activity);
  EndCard();

  ImGui::SameLine(0, Px(20));
  DrawRegisterPanel(*activity, right_width, size.y);
}

void App::DrawComments(const Activity& activity) {
  Heading("讨论区", 20);
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(4));
  Muted(("· " + std::to_string(sys_->comments.CountOf(activity.id)) + " 条")
            .c_str());
  // 任何人都能评论，游客需要填写昵称（REQ-036、REQ-040）。
  if (actor_.user_id == 0) {
    ImGui::SetNextItemWidth(Px(120));
    ImGui::InputTextWithHint("##guest", "你的昵称", guest_name_,
                             sizeof(guest_name_));
    ImGui::SameLine();
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - Px(80));
  ImGui::InputTextWithHint("##comment", "说点什么吧，游客也可以参与讨论～",
                           comment_, sizeof(comment_));
  ImGui::SameLine();
  if (FilledButton("发表", Tone::kBlue, ImVec2(Px(70), 0))) {
    const Status status = sys_->comments.Post(actor_, activity.id, 0,
                                              guest_name_, comment_, nullptr);
    if (status.ok()) comment_[0] = '\0';
    ShowResult(status);
  }

  ImGui::BeginChild("comments", ImVec2(0, 0));
  for (const Comment* comment : sys_->comments.TopLevel(activity.id)) {
    std::vector<const Comment*> thread = {comment};
    const std::vector<const Comment*> replies =
        sys_->comments.Replies(comment->id);
    thread.insert(thread.end(), replies.begin(), replies.end());
    for (const Comment* item : thread) {
      const bool is_reply = item->parent_id != 0;
      ImGui::PushID(static_cast<int>(item->id));
      if (is_reply) ImGui::Indent(Px(44));
      const std::string name = item->user_id == 0
                                   ? item->guest_name
                                   : sys_->accounts.NameOf(item->user_id);
      const ImU32 avatar = item->badge == Badge::kOfficial
                               ? ToneColor(Tone::kPurple)
                           : item->user_id == 0 ? ToneColor(Tone::kGray)
                                                : ToneColor(Tone::kBlue);
      Avatar(name, avatar);
      ImGui::SameLine(0, Px(10));
      ImGui::BeginGroup();
      ImGui::PushFont(nullptr, 15.0f);
      Heading(name.c_str(), 15);
      ImGui::PopFont();
      const Badge badge = item->badge;
      if (badge != Badge::kMember) {
        ImGui::SameLine();
        Tag(BadgeName(badge),
            badge == Badge::kAttended ? Tone::kGreen
            : badge == Badge::kOfficial ? Tone::kPurple
                                        : Tone::kGray);
      }
      ImGui::SameLine();
      ImGui::PushFont(nullptr, 13.0f);
      std::string meta = "· " + FormatShort(item->created_at);
      if (is_reply) {
        const Comment* parent = sys_->store.FindComment(item->parent_id);
        meta += " 回复 " + (parent->user_id == 0
                                ? parent->guest_name
                                : sys_->accounts.NameOf(parent->user_id));
      }
      Muted(meta.c_str());
      ImGui::PopFont();
      ImGui::TextWrapped("%s", item->content.c_str());
      ImGui::PushFont(nullptr, 13.0f);
      const int likes = sys_->comments.LikeCount(item->id);
      const bool liked = sys_->comments.HasLiked(actor_, item->id);
      const std::string like_label =
          std::string(liked ? "已赞 " : "赞 ") + std::to_string(likes);
      if (GhostButton(like_label.c_str(), ImVec2(0, 0),
                      IsParticipant() && item->user_id != actor_.user_id)) {
        ShowResult(sys_->comments.Like(actor_, item->id));
      }
      ImGui::SameLine();
      if (GhostButton("回复")) {
        reply_to_ = reply_to_ == item->id ? 0 : item->id;
        reply_[0] = '\0';
      }
      ImGui::PopFont();
      if (reply_to_ == item->id) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - Px(80));
        ImGui::InputTextWithHint("##reply", ("回复 " + name).c_str(), reply_,
                                 sizeof(reply_));
        ImGui::SameLine();
        if (FilledButton("回复##send", Tone::kBlue, ImVec2(Px(70), 0))) {
          // 回复挂在顶层评论下面，便于阅读。
          const Id parent = item->parent_id == 0 ? item->id : item->parent_id;
          const Status status = sys_->comments.Post(
              actor_, activity.id, parent, guest_name_, reply_, nullptr);
          if (status.ok()) reply_to_ = 0;
          ShowResult(status);
        }
      }
      ImGui::EndGroup();
      if (is_reply) ImGui::Unindent(Px(44));
      ImGui::Dummy(ImVec2(0, Px(4)));
      ImGui::PopID();
    }
    ImGui::Separator();
  }
  ImGui::EndChild();
}

void App::DrawRegisterPanel(const Activity& activity, float width,
                            float height) {
  BeginCard("register", ImVec2(width, height), 0.82f);
  Heading("选择场次与票种", 20);
  const std::vector<EventSession*> sessions =
      sys_->store.SessionsOf(activity.id);
  // 默认选中第一个可选场次。
  if (sys_->store.FindSession(chosen_session_) == nullptr ||
      sys_->store.FindSession(chosen_session_)->activity_id != activity.id) {
    chosen_session_ = sessions.empty() ? 0 : sessions.front()->id;
    for (const EventSession* s : sessions) {
      if (!s->cancelled && !s->finished) {
        chosen_session_ = s->id;
        break;
      }
    }
    chosen_type_ = 0;
  }
  ImGui::PushFont(nullptr, 13.0f);
  Muted("场次");
  ImGui::PopFont();
  for (const EventSession* session : sessions) {
    ImGui::PushID(static_cast<int>(session->id));
    std::string label = SessionText(*session);
    if (session->cancelled) label += "（已取消）";
    if (session->finished) label += "（已结束）";
    if (ImGui::RadioButton(label.c_str(), chosen_session_ == session->id)) {
      chosen_session_ = session->id;
      chosen_type_ = 0;
    }
    ImGui::PopID();
  }
  const EventSession* session = sys_->store.FindSession(chosen_session_);
  if (session == nullptr) {
    EndCard();
    return;
  }
  ImGui::Dummy(ImVec2(0, Px(2)));
  ImGui::PushFont(nullptr, 13.0f);
  Muted("票种");
  ImGui::PopFont();
  const std::vector<TicketType*> types =
      sys_->store.TicketTypesOf(session->id);
  if (sys_->store.FindTicketType(chosen_type_) == nullptr ||
      sys_->store.FindTicketType(chosen_type_)->session_id != session->id) {
    chosen_type_ = types.empty() ? 0 : types.front()->id;
  }
  for (const TicketType* type : types) {
    ImGui::PushID(static_cast<int>(type->id));
    if (type->cancelled) ImGui::BeginDisabled();
    if (ImGui::RadioButton(type->name.c_str(), chosen_type_ == type->id)) {
      chosen_type_ = type->id;
    }
    if (type->cancelled) ImGui::EndDisabled();
    ImGui::SameLine(width - Px(150));
    const int remaining = sys_->registrations.Remaining(type->id);
    if (type->cancelled) {
      Tag("已取消", Tone::kGray);
    } else if (remaining == 0) {
      Tag("已满 · 可候补", Tone::kOrange);
    } else {
      ImGui::Text("剩 %d / %d", remaining, type->capacity);
    }
    ImGui::PopID();
  }
  ImGui::Dummy(ImVec2(0, Px(6)));

  // 主按钮随状态变化。
  const float button_width = ImGui::GetContentRegionAvail().x;
  const ImVec2 big(button_width, Px(48));
  const Registration* mine = nullptr;
  for (const Registration& reg : sys_->store.data().registrations) {
    if (reg.user_id == actor_.user_id && actor_.user_id != 0 &&
        reg.session_id == session->id && IsActiveRegistration(reg.status)) {
      mine = &reg;
    }
  }
  const Time now = sys_->clock.Now();
  const TicketType* type = sys_->store.FindTicketType(chosen_type_);
  if (actor_.user_id == 0) {
    if (FilledButton("登录后报名", Tone::kBlue, big)) Go(Screen::kLogin);
  } else if (!IsParticipant()) {
    FilledButton("仅参与者可以报名", Tone::kGray, big, false);
  } else if (session->finished) {
    if (GhostButton("场次已结束，查看我的票券", big)) Go(Screen::kTickets);
  } else if (mine != nullptr) {
    const std::string label =
        mine->status == RegStatus::kWaiting
            ? "候补中 · 第 " +
                  std::to_string(sys_->registrations.WaitingPosition(mine->id))
                  + " 位，查看候补凭证"
            : std::string("已报名，查看票券");
    if (GhostButton(label.c_str(), big)) Go(Screen::kTickets);
  } else if (activity.status != ActivityStatus::kPublished ||
             session->cancelled || type == nullptr || type->cancelled) {
    FilledButton("当前不可报名", Tone::kGray, big, false);
  } else if (now < session->reg_open_at) {
    FilledButton(("报名将于 " + FormatShort(session->reg_open_at) + " 开放")
                     .c_str(),
                 Tone::kGray, big, false);
  } else if (now >= session->reg_close_at) {
    FilledButton("报名已截止", Tone::kGray, big, false);
  } else {
    const bool full = sys_->registrations.Remaining(type->id) == 0;
    if (FilledButton(full ? "加入候补（名额已满）" : "立即报名",
                     full ? Tone::kOrange : Tone::kBlue, big)) {
      RegisterResult result;
      ShowResult(sys_->registrations.Register(actor_, session->id, type->id,
                                              &result));
    }
  }
  if (IsParticipant()) {
    const bool fav = sys_->favorites.IsFavorite(actor_, activity.id);
    if (GhostButton(fav ? "★ 已收藏" : "☆ 收藏", ImVec2(button_width, 0))) {
      ShowResult(fav ? sys_->favorites.Remove(actor_, activity.id)
                     : sys_->favorites.Add(actor_, activity.id));
    }
  }

  ImGui::Dummy(ImVec2(0, Px(4)));
  BeginCard("times", ImVec2(button_width, 0), 0.9f);
  const char* labels[] = {"报名开放", "报名截止", "退票截止", "检票开始"};
  const std::string values[] = {FormatTime(session->reg_open_at),
                                FormatTime(session->reg_close_at),
                                "检票开始前",
                                FormatTime(session->checkin_start_at)};
  for (int i = 0; i < 4; ++i) {
    Muted(labels[i]);
    ImGui::SameLine(button_width - Px(150));
    ImGui::TextUnformatted(values[i].c_str());
  }
  EndCard();
  PerchTip("tip", "候补凭证不能入场，有人退票后按顺序自动递补。",
           button_width - Px(30), height - Px(120));
  EndCard();
}

}  // namespace ui
}  // namespace ticketing
