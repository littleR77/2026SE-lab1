// Copyright 2026 R
// 个人页面：我的票券（界面 4）、消息与收藏（界面 5）、检票（界面 6）。
#include <algorithm>
#include <string>
#include <vector>

#include "common/time_util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace ticketing {
namespace ui {
namespace {

Tone MessageTone(const std::string& type) {
  if (type == "候补提醒") return Tone::kOrange;
  if (type == "递补成功") return Tone::kGreen;
  return Tone::kGray;
}

}  // namespace

void App::DrawTickets(ImVec2 size) {
  if (!IsParticipant()) {
    Muted("请以参与者身份登录。");
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading("我的票券", 28);
  ImGui::PopStyleColor();
  ImGui::SameLine(Px(180));
  const char* tabs[] = {"有效", "全部"};
  Segmented("tabs", tabs, 2, &ticket_tab_, Px(220));
  ImGui::SameLine(size.x - Px(170));
  if (GhostButton("清理失效记录", ImVec2(Px(150), 0))) {
    const int count = sys_->registrations.HideAllInvalid(actor_);
    ShowResult(Status::Ok("已清理 " + std::to_string(count) +
                          " 条记录，后台历史和统计不受影响"));
  }

  const Time now = sys_->clock.Now();
  std::vector<const Registration*> list;
  for (const Registration* reg : sys_->registrations.ListMine(actor_)) {
    const EventSession* session = sys_->store.FindSession(reg->session_id);
    const bool valid = reg->status == RegStatus::kWaiting ||
                       reg->status == RegStatus::kConfirmed ||
                       (reg->status == RegStatus::kCheckedIn &&
                        !session->finished);
    if (ticket_tab_ == 1 || valid) list.push_back(reg);
  }

  ImGui::BeginChild("tickets", ImVec2(size.x, size.y - Px(56)));
  if (list.empty()) Muted("暂无记录，去活动广场看看吧。");
  const float card_width = (size.x - Px(30)) / 2;
  for (size_t i = 0; i < list.size(); ++i) {
    const Registration& reg = *list[i];
    const EventSession* session = sys_->store.FindSession(reg.session_id);
    const Activity* activity = sys_->store.FindActivity(session->activity_id);
    const TicketType* type = sys_->store.FindTicketType(reg.ticket_type_id);
    const Ticket* ticket = sys_->store.FindTicketOf(reg.id);
    const bool waiting = reg.status == RegStatus::kWaiting;
    const bool dead = !IsActiveRegistration(reg.status) || session->finished;
    if (i % 2 == 1) ImGui::SameLine(0, Px(18));
    ImGui::PushID(static_cast<int>(reg.id));
    if (dead) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
    BeginCard("ticket", ImVec2(card_width, Px(214)),
              waiting ? 0.7f : 0.78f);
    Heading(activity->title.c_str(), 19);
    ImGui::SameLine(card_width - Px(130));
    Tag(waiting ? "候补凭证" : RegStatusName(reg.status),
        ToneOf(reg.status));
    Muted((SessionText(*session) + " · " + type->name).c_str());
    ImGui::Separator();
    if (waiting) {
      Muted("候补排位");
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kOrange));
      Heading(("第 " +
               std::to_string(sys_->registrations.WaitingPosition(reg.id)) +
               " 位").c_str(), 28);
      ImGui::PopStyleColor();
      ImGui::SameLine(card_width - Px(220));
      ImGui::BeginGroup();
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kOrange));
      Heading("不可入场", 15);
      ImGui::PopStyleColor();
      Muted("递补后自动换发正式票");
      ImGui::EndGroup();
    } else if (ticket != nullptr) {
      Muted("票码");
      Heading(ticket->code.c_str(), 26);
      ImGui::SameLine(card_width - Px(170));
      ImGui::BeginGroup();
      const Checkin* checkin = sys_->store.FindCheckin(ticket->code);
      if (checkin != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kBlue));
        Heading("已签到", 15);
        ImGui::PopStyleColor();
        Muted(FormatShort(checkin->checked_at).c_str());
      } else if (ticket->valid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kGreen));
        Heading("可入场", 15);
        ImGui::PopStyleColor();
        Muted((FormatShort(session->checkin_start_at).substr(6) + " 开始检票")
                  .c_str());
      } else {
        Heading("已失效", 15);
      }
      ImGui::EndGroup();
    }
    // 退票须早于检票开始（REQ-027）；失效记录可从个人列表清理（REQ-020）。
    const bool can_cancel = (reg.status == RegStatus::kConfirmed || waiting) &&
                            now < session->checkin_start_at;
    ImGui::SetCursorPosY(Px(160));
    if (reg.status == RegStatus::kConfirmed || waiting) {
      Muted(can_cancel ? "检票开始前可退票" : "检票已开始，不能退票");
      ImGui::SameLine(card_width - Px(150));
      if (FilledButton(waiting ? "退出候补" : "退票", Tone::kRed,
                       ImVec2(Px(110), 0), can_cancel)) {
        confirm_cancel_ = reg.id;
        ImGui::OpenPopup("确认退票");
      }
    } else if (sys_->registrations.CanHide(reg)) {
      Muted("该记录已失效，可从列表中清理");
      ImGui::SameLine(card_width - Px(130));
      if (GhostButton("清理", ImVec2(Px(90), 0))) {
        ShowResult(sys_->registrations.Hide(actor_, reg.id));
      }
    }

    if (ImGui::BeginPopupModal("确认退票", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      Heading(waiting ? "确定要退出候补吗？" : "确定要退票吗？", 22);
      ImGui::Text("《%s》%s 场", activity->title.c_str(),
                  FormatShort(session->start_at).c_str());
      if (!waiting && ticket != nullptr) {
        ImGui::Text("退票后票码 %s 立即失效，", ticket->code.c_str());
        ImGui::TextUnformatted("名额将按顺序释放给候补的同学。");
      } else {
        ImGui::TextUnformatted("退出后候补凭证失效，排位不再保留。");
      }
      ImGui::Dummy(ImVec2(0, Px(6)));
      if (GhostButton("再想想", ImVec2(Px(150), Px(40)))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (FilledButton(waiting ? "确定退出" : "确定退票", Tone::kRed,
                       ImVec2(Px(150), Px(40)))) {
        ShowResult(sys_->registrations.Cancel(actor_, confirm_cancel_));
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    EndCard();
    if (dead) ImGui::PopStyleVar();
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void App::DrawMessages(ImVec2 size) {
  if (!IsParticipant()) {
    Muted("请以参与者身份登录。");
    return;
  }
  const float left = size.x * 0.56f;
  BeginCard("messages", ImVec2(left, size.y), 0.66f);
  Heading("消息", 22);
  const int unread = sys_->notifier.UnreadCount(actor_);
  if (unread > 0) {
    ImGui::SameLine();
    Tag((std::to_string(unread) + " 条未读").c_str(), Tone::kRed);
    ImGui::SameLine(left - Px(160));
    if (GhostButton("全部标为已读")) sys_->notifier.MarkAllRead(actor_);
  }
  const std::vector<const Message*> messages =
      sys_->notifier.ListMine(actor_);
  // 成功进入候补后给出显眼提示（REQ-033）。
  for (const Message* message : messages) {
    if (!message->read && message->type == "候补提醒") {
      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 236, 205, 255));
      ImGui::BeginChild("banner", ImVec2(0, 0),
                        ImGuiChildFlags_AutoResizeY |
                            ImGuiChildFlags_AlwaysUseWindowPadding);
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kOrange));
      Heading("新消息：你已成功进入候补队列", 16);
      ImGui::PopStyleColor();
      Muted("候补不代表已获得入场名额，递补成功后会再通知你。");
      ImGui::EndChild();
      ImGui::PopStyleColor();
      break;
    }
  }
  ImGui::BeginChild("list", ImVec2(0, 0));
  if (messages.empty()) Muted("暂无消息。");
  for (const Message* message : messages) {
    ImGui::PushID(static_cast<int>(message->id));
    BeginCard("msg", ImVec2(0, 0), message->read ? 0.7f : 0.95f);
    Tag(message->type.c_str(), MessageTone(message->type));
    ImGui::SameLine();
    Muted(FormatShort(message->created_at).c_str());
    if (!message->read) {
      ImGui::SameLine();
      Tag("未读", Tone::kRed);
    }
    ImGui::TextWrapped("%s", message->content.c_str());
    EndCard();
    ImGui::PopID();
  }
  ImGui::EndChild();
  EndCard();

  // 收藏：按收藏时间倒序，活动结束或取消后仍保留（REQ-039、REQ-041）。
  ImGui::SameLine(0, Px(20));
  BeginCard("favorites", ImVec2(size.x - left - Px(20), size.y), 0.66f);
  Heading("我的收藏", 22);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("按收藏时间倒序 · 已结束或已取消的活动也会保留");
  ImGui::PopFont();
  const std::vector<const Favorite*> favorites =
      sys_->favorites.ListMine(actor_);
  if (favorites.empty()) Muted("还没有收藏活动。");
  for (const Favorite* favorite : favorites) {
    const Activity* activity = sys_->store.FindActivity(favorite->activity_id);
    if (activity == nullptr) continue;
    ImGui::PushID(static_cast<int>(activity->id));
    const bool closed = activity->status == ActivityStatus::kEnded ||
                        activity->status == ActivityStatus::kCancelled;
    if (closed) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
    BeginCard("fav", ImVec2(0, 0), 0.9f);
    Heading(activity->title.c_str(), 16);
    ImGui::SameLine();
    Tag(ActivityStatusName(activity->status), ToneOf(activity->status));
    ImGui::PushFont(nullptr, 13.0f);
    const std::vector<EventSession*> sessions =
        sys_->store.SessionsOf(activity->id);
    Muted(("收藏于 " + FormatShort(favorite->created_at) +
           (sessions.empty() ? std::string()
                             : " · 活动 " +
                                   FormatShort(sessions.front()->start_at)))
              .c_str());
    ImGui::PopFont();
    if (GhostButton("查看详情")) OpenDetail(activity->id);
    ImGui::SameLine();
    if (GhostButton("取消收藏")) {
      ShowResult(sys_->favorites.Remove(actor_, activity->id));
    }
    EndCard();
    if (closed) ImGui::PopStyleVar();
    ImGui::PopID();
  }
  EndCard();
}

void App::DrawCheckin(ImVec2 size) {
  if (actor_.role != Role::kChecker) {
    Muted("请以检票员身份登录。");
    return;
  }
  const float width = Px(560);
  const float x = (size.x - width) * 0.5f + Px(24);
  ImGui::SetCursorPosX(x);
  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading("检票", 30);
  ImGui::PopStyleColor();

  const std::vector<const EventSession*> sessions =
      sys_->checkins.AssignedSessions(actor_);
  BeginCard("form", ImVec2(width, 0), 0.62f);
  Muted("当前场次（仅显示已授权场次）");
  if (sessions.empty()) {
    Muted("你还没有被分配任何场次。发布活动不会自动授予检票权限，"
          "请联系组织者分配。");
    EndCard();
    ImGui::EndGroup();
    return;
  }
  bool found = false;
  for (const EventSession* s : sessions) {
    if (s->id == checkin_session_) found = true;
  }
  if (!found) {
    // 默认选正在检票或即将开始的场次，都结束时选第一个。
    checkin_session_ = sessions.front()->id;
    for (const EventSession* s : sessions) {
      if (s->end_at > sys_->clock.Now()) {
        checkin_session_ = s->id;
        break;
      }
    }
  }
  const EventSession* session = sys_->store.FindSession(checkin_session_);
  const Activity* activity = sys_->store.FindActivity(session->activity_id);
  const std::string preview =
      activity->title + " · " + FormatShort(session->start_at);
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##session", preview.c_str())) {
    for (const EventSession* s : sessions) {
      const Activity* a = sys_->store.FindActivity(s->activity_id);
      const std::string label = a->title + " · " + FormatShort(s->start_at);
      ImGui::PushID(static_cast<int>(s->id));
      if (ImGui::Selectable(label.c_str(), s->id == checkin_session_)) {
        checkin_session_ = s->id;
        checkin_history_.clear();
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  const Time now = sys_->clock.Now();
  const char* window_state = now < session->checkin_start_at ? "尚未开始检票"
                             : now >= session->end_at        ? "场次已结束"
                                                             : "当前可检票";
  ImGui::PushFont(nullptr, 13.0f);
  Muted(("检票时间 " + FormatShort(session->checkin_start_at) + " – " +
         FormatShort(session->end_at).substr(6) + " · " + window_state)
            .c_str());
  ImGui::PopFont();
  Muted("票码");
  ImGui::PushFont(nullptr, 24.0f);
  ImGui::SetNextItemWidth(-1);
  const bool enter = ImGui::InputTextWithHint(
      "##code", "TK-XXXX-XXXX", ticket_code_, sizeof(ticket_code_),
      ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopFont();
  if (FilledButton("核 验", Tone::kBlue, ImVec2(-1, Px(50))) || enter) {
    checkin_history_.insert(
        checkin_history_.begin(),
        sys_->checkins.Verify(actor_, checkin_session_, ticket_code_));
    if (checkin_history_.size() > 4) checkin_history_.resize(4);
    ticket_code_[0] = '\0';
  }
  EndCard();

  // 核验结果：绿色通过、橙色重复、红色拒绝。
  for (size_t i = 0; i < checkin_history_.size(); ++i) {
    const CheckinResult& r = checkin_history_[i];
    const Tone tone = r.outcome == CheckinOutcome::kPassed     ? Tone::kGreen
                      : r.outcome == CheckinOutcome::kRepeated ? Tone::kOrange
                                                               : Tone::kRed;
    ImVec4 bg = ImGui::ColorConvertU32ToFloat4(ToneColor(tone));
    bg.w = i == 0 ? 0.2f : 0.1f;
    ImGui::PushID(static_cast<int>(i));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, ToneColor(tone));
    ImGui::BeginChild("result", ImVec2(width, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(tone));
    Heading(r.outcome == CheckinOutcome::kPassed     ? "核验通过"
            : r.outcome == CheckinOutcome::kRepeated ? "该票已签到"
                                                     : "无法入场",
            i == 0 ? 20.0f : 15.0f);
    ImGui::PopStyleColor();
    std::string detail = r.message;
    if (!r.holder.empty()) detail = r.holder + " · " + r.ticket_type + " · " +
                                    detail;
    Muted(detail.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
  }

  ImGui::Dummy(ImVec2(0, Px(30)));
  BeginCard("count", ImVec2(width, 0), 0.85f);
  const int checked = sys_->checkins.CheckedInCount(session->id);
  const int expected = sys_->checkins.ExpectedCount(session->id);
  Heading("本场已签到", 18);
  ImGui::SameLine(width - Px(190));
  Heading(std::to_string(checked).c_str(), 30);
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(10));
  Muted(("/ " + std::to_string(expected) + " 应到").c_str());
  ImGui::ProgressBar(expected == 0 ? 0.0f
                                   : static_cast<float>(checked) / expected,
                     ImVec2(-1, Px(8)), "");
  EndCard();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  DrawMascot(ImVec2(max.x, min.y), Px(100));
  ImGui::EndGroup();
}

}  // namespace ui
}  // namespace ticketing
