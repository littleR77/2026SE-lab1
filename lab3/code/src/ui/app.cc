// Copyright 2026 R
#include "ui/app.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "common/time_util.h"
#include "ui/theme.h"

namespace ticketing {
namespace ui {
namespace {

constexpr char kDemoPassword[] = "123456";

// 导航栏中的一个页面入口。
struct NavItem {
  const char* label;
  Screen screen;
};

bool NavButton(const char* label, bool active) {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.9f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
                                             ToneColor(Tone::kBlue)));
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.27f, 0.38f, 1));
  }
  const bool clicked = ImGui::Button(label);
  if (active) {
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(min.x + Px(12), max.y - Px(3)), ImVec2(max.x - Px(12), max.y),
        ToneColor(Tone::kBlue), Px(2));
  }
  ImGui::PopStyleColor(5);
  return clicked;
}

// 导航栏左侧的云朵标志。
void Logo() {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float size = Px(34);
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->AddRectFilledMultiColor(pos, ImVec2(pos.x + size, pos.y + size),
                                IM_COL32(84, 190, 255, 255),
                                IM_COL32(84, 190, 255, 255),
                                IM_COL32(16, 132, 236, 255),
                                IM_COL32(16, 132, 236, 255));
  list->AddRect(pos, ImVec2(pos.x + size, pos.y + size),
                IM_COL32(255, 255, 255, 200), Px(10), 0, Px(2));
  const ImU32 white = IM_COL32(255, 255, 255, 255);
  list->AddCircleFilled(ImVec2(pos.x + size * 0.38f, pos.y + size * 0.55f),
                        size * 0.17f, white);
  list->AddCircleFilled(ImVec2(pos.x + size * 0.55f, pos.y + size * 0.45f),
                        size * 0.2f, white);
  list->AddCircleFilled(ImVec2(pos.x + size * 0.7f, pos.y + size * 0.58f),
                        size * 0.14f, white);
  list->AddRectFilled(ImVec2(pos.x + size * 0.28f, pos.y + size * 0.55f),
                      ImVec2(pos.x + size * 0.78f, pos.y + size * 0.72f),
                      white, size * 0.08f);
  ImGui::Dummy(ImVec2(size, size));
}

}  // namespace

App::App(System* system) : sys_(system) {
  snprintf(account_, sizeof(account_), "%s", "alice");
  snprintf(password_, sizeof(password_), "%s", kDemoPassword);
}

void App::Go(Screen screen) {
  screen_ = screen;
  if (screen == Screen::kCheckin) checkin_history_.clear();
}

void App::OpenDetail(Id activity_id) {
  detail_id_ = activity_id;
  chosen_session_ = 0;
  chosen_type_ = 0;
  reply_to_ = 0;
  Go(Screen::kDetail);
}

void App::ShowResult(const Status& status) {
  toast_ = status.message();
  toast_ok_ = status.ok();
  toast_until_ = ImGui::GetTime() + 3.5;
}

void App::Logout() {
  actor_ = Actor();
  login_error_.clear();
  Go(Screen::kLogin);
}

bool App::DebugOpen(const std::string& account, Screen screen,
                    Id activity_id) {
  if (!account.empty()) {
    if (!sys_->accounts.Login(account, kDemoPassword, &actor_).ok()) {
      return false;
    }
  }
  Go(screen);
  if (screen == Screen::kDetail) OpenDetail(activity_id);
  if (screen == Screen::kEdit) LoadEditForm(activity_id);
  if (screen == Screen::kManage) manage_id_ = activity_id;
  return true;
}

std::string App::SessionText(const EventSession& session) const {
  return FormatShort(session.start_at).substr(0, 5) + " " +
         WeekdayName(session.start_at) + " " +
         FormatShort(session.start_at).substr(6) + "–" +
         FormatShort(session.end_at).substr(6) + " · " + session.location;
}

int App::Remaining(Id activity_id) const {
  int total = 0;
  for (const EventSession* session : sys_->store.SessionsOf(activity_id)) {
    if (session->cancelled || session->finished) continue;
    for (const TicketType* type : sys_->store.TicketTypesOf(session->id)) {
      if (!type->cancelled) total += sys_->registrations.Remaining(type->id);
    }
  }
  return total;
}

int App::Capacity(Id activity_id) const {
  int total = 0;
  for (const EventSession* session : sys_->store.SessionsOf(activity_id)) {
    if (session->cancelled) continue;
    for (const TicketType* type : sys_->store.TicketTypesOf(session->id)) {
      if (!type->cancelled) total += type->capacity;
    }
  }
  return total;
}

void App::FavoriteStar(Id activity_id) {
  if (!IsParticipant()) return;
  const bool on = sys_->favorites.IsFavorite(actor_, activity_id);
  ImGui::PushID(static_cast<int>(activity_id));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(0.95f, 0.66f, 0.05f, 1)
                                          : ImVec4(0.7f, 0.76f, 0.85f, 1));
  if (ImGui::Button(on ? "★" : "☆")) {
    ShowResult(on ? sys_->favorites.Remove(actor_, activity_id)
                  : sys_->favorites.Add(actor_, activity_id));
  }
  ImGui::PopStyleColor(3);
  ImGui::PopID();
}

void App::DrawNav(float width) {
  BeginCard("nav", ImVec2(width, Px(64)), 0.82f,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::SetCursorPosY(Px(15));
  Logo();
  ImGui::SameLine(0, Px(10));
  ImGui::SetCursorPosY(Px(16));
  Heading("活动现场", 20);
  ImGui::SameLine(0, Px(28));

  // 每种角色可以进入的页面。
  std::vector<NavItem> items;
  auto add = [&items](const char* label, Screen screen) {
    items.push_back({label, screen});
  };
  switch (actor_.role) {
    case Role::kGuest:
      add("活动广场", Screen::kPlaza);
      break;
    case Role::kParticipant:
      add("活动广场", Screen::kPlaza);
      add("我的票券", Screen::kTickets);
      add("消息与收藏", Screen::kMessages);
      break;
    case Role::kOrganizer:
      add("活动广场", Screen::kPlaza);
      add("我的活动", Screen::kMyActivities);
      add("名单统计", Screen::kManage);
      break;
    case Role::kChecker:
      add("检票", Screen::kCheckin);
      add("活动广场", Screen::kPlaza);
      break;
    case Role::kAdmin:
      add("审核台", Screen::kReview);
      add("平台统计与日志", Screen::kPlatform);
      add("活动广场", Screen::kPlaza);
      break;
  }
  for (const NavItem& item : items) {
    ImGui::SetCursorPosY(Px(13));
    bool active = screen_ == item.screen;
    if (item.screen == Screen::kPlaza && screen_ == Screen::kDetail) {
      active = true;
    }
    if (item.screen == Screen::kMyActivities && screen_ == Screen::kEdit) {
      active = true;
    }
    std::string label = item.label;
    if (item.screen == Screen::kMessages) {
      const int unread = sys_->notifier.UnreadCount(actor_);
      if (unread > 0) label += " (" + std::to_string(unread) + ")";
    }
    if (item.screen == Screen::kReview) {
      const size_t pending = sys_->reviews.PendingActivities(actor_).size() +
                             sys_->reviews.PendingChanges(actor_).size();
      if (pending > 0) label += " (" + std::to_string(pending) + ")";
    }
    if (NavButton(label.c_str(), active)) Go(item.screen);
    ImGui::SameLine(0, Px(4));
  }

  // 右侧：模拟时钟（演示截止、检票和缺席规则）以及当前用户。
  const std::string now = "模拟时间 " +
                          FormatTime(sys_->clock.Now()).substr(5, 5) + " " +
                          WeekdayName(sys_->clock.Now()) + " " +
                          FormatTime(sys_->clock.Now()).substr(11);
  const float right_width = Px(560);
  ImGui::SetCursorPos(ImVec2(width - right_width, Px(18)));
  ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kBlue));
  ImGui::TextUnformatted(now.c_str());
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::SetCursorPosY(Px(13));
  if (GhostButton("+1 小时")) {
    const SchedulerReport r = sys_->AdvanceClock(kMinutesPerHour);
    ShowResult(Status::Ok("时间推进 1 小时；候补失效 " +
                          std::to_string(r.expired) + " 条，缺席 " +
                          std::to_string(r.absent) + " 条，活动结束 " +
                          std::to_string(r.ended) + " 个"));
  }
  ImGui::SameLine(0, Px(6));
  if (GhostButton("+1 天")) {
    const SchedulerReport r = sys_->AdvanceClock(kMinutesPerDay);
    ShowResult(Status::Ok("时间推进 1 天；候补失效 " +
                          std::to_string(r.expired) + " 条，缺席 " +
                          std::to_string(r.absent) + " 条，活动结束 " +
                          std::to_string(r.ended) + " 个"));
  }
  ImGui::SameLine(0, Px(16));
  ImGui::SetCursorPosY(Px(19));
  const std::string who =
      actor_.user_id == 0
          ? std::string("游客")
          : sys_->accounts.NameOf(actor_.user_id) + " · " +
                RoleName(actor_.role);
  ImGui::TextUnformatted(who.c_str());
  ImGui::SameLine(0, Px(10));
  ImGui::SetCursorPosY(Px(13));
  if (GhostButton(actor_.user_id == 0 ? "登录" : "退出")) Logout();
  EndCard();
}

void App::DrawToast(ImVec2 size) {
  if (toast_.empty() || ImGui::GetTime() > toast_until_) return;
  ImDrawList* list = ImGui::GetForegroundDrawList();
  const ImVec2 text = ImGui::CalcTextSize(toast_.c_str());
  const ImVec2 pad(Px(20), Px(12));
  const ImVec2 min((size.x - text.x) * 0.5f - pad.x,
                   size.y - Px(80) - pad.y);
  const ImVec2 max(min.x + text.x + pad.x * 2, min.y + text.y + pad.y * 2);
  list->AddRectFilled(min, max,
                      toast_ok_ ? IM_COL32(22, 163, 106, 235)
                                : IM_COL32(225, 72, 80, 235),
                      Px(14));
  list->AddText(ImVec2(min.x + pad.x, min.y + pad.y), IM_COL32_WHITE,
                toast_.c_str());
}

void App::Frame(ImVec2 size) {
  DrawSky(ImGui::GetBackgroundDrawList(), ImVec2(0, 0), size);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(24), Px(16)));
  ImGui::Begin("main", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();
  const float width = size.x - Px(48);
  if (screen_ != Screen::kLogin) {
    DrawNav(width);
    ImGui::Dummy(ImVec2(0, Px(4)));
  }
  const ImVec2 body(width, size.y - ImGui::GetCursorPosY() - Px(16));
  switch (screen_) {
    case Screen::kLogin: DrawLogin(size); break;
    case Screen::kPlaza: DrawPlaza(body); break;
    case Screen::kDetail: DrawDetail(body); break;
    case Screen::kTickets: DrawTickets(body); break;
    case Screen::kMessages: DrawMessages(body); break;
    case Screen::kCheckin: DrawCheckin(body); break;
    case Screen::kMyActivities: DrawMyActivities(body); break;
    case Screen::kEdit: DrawEdit(body); break;
    case Screen::kManage: DrawManage(body); break;
    case Screen::kReview: DrawReview(body); break;
    case Screen::kPlatform: DrawPlatform(body); break;
  }
  DrawCancelDialog();
  ImGui::End();
  DrawToast(size);
  const Status saved = sys_->Persist();
  if (!saved.ok()) ShowResult(saved);
}

}  // namespace ui
}  // namespace ticketing
