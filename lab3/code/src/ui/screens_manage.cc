// Copyright 2026 R
// 管理页面：我的活动、活动编辑（界面 7）、名单统计与检票安排（界面 8）、
// 审核台（界面 9）、平台统计与操作日志（界面 10）。
#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "common/file_util.h"
#include "common/time_util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace ticketing {
namespace ui {
namespace {

const char* const kCategories[] = {"演出", "讲座", "工作坊", "社团", "其他"};

void CopyText(char* dst, size_t size, const std::string& src) {
  snprintf(dst, size, "%s", src.c_str());
}

// 解析表单中的时间，失败时把字段名写入 *error。
bool ReadTime(const char* text, const char* field, Time* out,
              std::string* error) {
  if (ParseTime(text, out)) return true;
  if (error->empty()) {
    *error = std::string(field) + "格式应为 2026-11-05 19:00";
  }
  return false;
}

// 带标签的单行输入框，标签在上方。
void LabeledInput(const char* label, const char* id, char* buffer,
                  size_t size, float width) {
  ImGui::BeginGroup();
  ImGui::PushFont(nullptr, 13.0f);
  Muted(label);
  ImGui::PopFont();
  ImGui::SetNextItemWidth(width);
  ImGui::InputText(id, buffer, size);
  ImGui::EndGroup();
}

// 同一行放两个带标签的输入框，两者顶部对齐。
void LabeledPair(const char* label_a, const char* id_a, char* buffer_a,
                 size_t size_a, const char* label_b, const char* id_b,
                 char* buffer_b, size_t size_b, float width) {
  const float y = ImGui::GetCursorPosY();
  LabeledInput(label_a, id_a, buffer_a, size_a, width);
  ImGui::SameLine();
  ImGui::SetCursorPosY(y);
  LabeledInput(label_b, id_b, buffer_b, size_b, width);
}

void RedText(const std::string& text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kRed));
  ImGui::TextWrapped("%s", text.c_str());
  ImGui::PopStyleColor();
}

}  // namespace

void App::LoadEditForm(Id activity_id) {
  edit_ = EditForm();
  edit_.activity_id = activity_id;
  ActivityDraft draft;
  if (activity_id != 0) {
    draft = sys_->activities.LoadDraft(activity_id);
  } else {
    // 新活动的默认时间：一周后 19:00 开始。
    const Time day = sys_->clock.Now() - sys_->clock.Now() % kMinutesPerDay;
    const Time start = day + 7 * kMinutesPerDay + 19 * kMinutesPerHour;
    SessionDraft session;
    session.reg_open_at = day + kMinutesPerDay + 9 * kMinutesPerHour;
    session.reg_close_at = start - 7 * kMinutesPerHour;
    session.checkin_start_at = start - 30;
    session.start_at = start;
    session.end_at = start + 2 * kMinutesPerHour;
    session.ticket_types.push_back({0, "普通观众票", 100});
    draft.category = kCategories[0];
    draft.sessions.push_back(session);
  }
  CopyText(edit_.title, sizeof(edit_.title), draft.title);
  CopyText(edit_.category, sizeof(edit_.category), draft.category);
  CopyText(edit_.description, sizeof(edit_.description), draft.description);
  for (const SessionDraft& sd : draft.sessions) {
    SessionForm form;
    form.id = sd.id;
    CopyText(form.location, sizeof(form.location), sd.location);
    CopyText(form.start, sizeof(form.start), FormatTime(sd.start_at));
    CopyText(form.end, sizeof(form.end), FormatTime(sd.end_at));
    CopyText(form.reg_open, sizeof(form.reg_open), FormatTime(sd.reg_open_at));
    CopyText(form.reg_close, sizeof(form.reg_close),
             FormatTime(sd.reg_close_at));
    CopyText(form.checkin, sizeof(form.checkin),
             FormatTime(sd.checkin_start_at));
    for (const TicketTypeDraft& td : sd.ticket_types) {
      TypeForm type;
      type.id = td.id;
      CopyText(type.name, sizeof(type.name), td.name);
      type.capacity = td.capacity;
      form.types.push_back(type);
    }
    edit_.sessions.push_back(form);
  }
}

bool App::BuildDraft(ActivityDraft* draft) {
  std::string error;
  draft->title = edit_.title;
  draft->category = edit_.category;
  draft->description = edit_.description;
  for (const SessionForm& form : edit_.sessions) {
    SessionDraft sd;
    sd.id = form.id;
    sd.location = form.location;
    ReadTime(form.reg_open, "报名开放时间", &sd.reg_open_at, &error);
    ReadTime(form.reg_close, "报名截止时间", &sd.reg_close_at, &error);
    ReadTime(form.start, "活动开始时间", &sd.start_at, &error);
    ReadTime(form.end, "活动结束时间", &sd.end_at, &error);
    ReadTime(form.checkin, "检票开始时间", &sd.checkin_start_at, &error);
    for (const TypeForm& type : form.types) {
      sd.ticket_types.push_back({type.id, type.name, type.capacity});
    }
    draft->sessions.push_back(sd);
  }
  if (!error.empty()) {
    ShowResult(Status::Error(error));
    return false;
  }
  return true;
}

void App::LoadChangeForm(Id session_id) {
  change_ = ChangeForm();
  const EventSession* session = sys_->store.FindSession(session_id);
  if (session == nullptr) return;
  change_.session_id = session_id;
  CopyText(change_.location, sizeof(change_.location), session->location);
  CopyText(change_.start, sizeof(change_.start),
           FormatTime(session->start_at));
  CopyText(change_.end, sizeof(change_.end), FormatTime(session->end_at));
  CopyText(change_.reg_close, sizeof(change_.reg_close),
           FormatTime(session->reg_close_at));
  CopyText(change_.checkin, sizeof(change_.checkin),
           FormatTime(session->checkin_start_at));
  for (const TicketType* type : sys_->store.TicketTypesOf(session_id)) {
    if (!type->cancelled) change_.capacities.push_back({type->id,
                                                        type->capacity});
  }
}

void App::DrawMyActivities(ImVec2 size) {
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading("我的活动", 28);
  ImGui::PopStyleColor();
  ImGui::SameLine(size.x - Px(130));
  if (FilledButton("+ 新建活动", Tone::kBlue, ImVec2(Px(120), Px(40)))) {
    LoadEditForm(0);
    Go(Screen::kEdit);
  }
  BeginCard("list", ImVec2(size.x, size.y - Px(54)), 0.62f);
  const std::vector<const Activity*> list =
      sys_->activities.ListManaged(actor_);
  if (list.empty()) Muted("还没有活动，点击右上角新建。");
  for (const Activity* activity : list) {
    ImGui::PushID(static_cast<int>(activity->id));
    BeginCard("row", ImVec2(0, 0), 0.9f);
    Heading(activity->title.c_str(), 18);
    ImGui::SameLine();
    Tag(ActivityStatusName(activity->status), ToneOf(activity->status));
    ImGui::SameLine();
    Tag(activity->category.c_str(), Tone::kBlue);
    std::string sessions;
    for (const EventSession* s : sys_->store.SessionsOf(activity->id)) {
      if (!sessions.empty()) sessions += "；";
      sessions += SessionText(*s);
      if (s->cancelled) sessions += "（已取消）";
    }
    Muted(sessions.c_str());
    if (activity->status == ActivityStatus::kDraft &&
        !activity->review_reason.empty()) {
      RedText("审核未通过：" + activity->review_reason);
    }
    if (activity->status == ActivityStatus::kCancelled) {
      Muted(("取消原因：" + activity->cancel_reason).c_str());
    }
    switch (activity->status) {
      case ActivityStatus::kDraft:
        if (GhostButton("编辑")) {
          LoadEditForm(activity->id);
          Go(Screen::kEdit);
        }
        ImGui::SameLine();
        if (FilledButton("提交审核", Tone::kBlue)) {
          ShowResult(sys_->activities.Submit(actor_, activity->id));
        }
        break;
      case ActivityStatus::kPending:
        Muted("等待管理员审核，审核通过后才会公开并接受报名。");
        break;
      case ActivityStatus::kPublished:
        if (GhostButton("名单统计")) {
          manage_id_ = activity->id;
          Go(Screen::kManage);
        }
        ImGui::SameLine();
        if (GhostButton("申请修改 / 取消")) {
          LoadEditForm(activity->id);
          change_ = ChangeForm();
          Go(Screen::kEdit);
        }
        ImGui::SameLine();
        if (GhostButton("查看详情")) OpenDetail(activity->id);
        break;
      case ActivityStatus::kEnded:
      case ActivityStatus::kCancelled:
        if (GhostButton("名单统计")) {
          manage_id_ = activity->id;
          Go(Screen::kManage);
        }
        break;
    }
    EndCard();
    ImGui::PopID();
  }
  EndCard();
}

void App::DrawEdit(ImVec2 size) {
  const Activity* activity = sys_->store.FindActivity(edit_.activity_id);
  const bool is_draft =
      activity == nullptr || activity->status == ActivityStatus::kDraft;
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading(activity == nullptr ? "新建活动" : "编辑活动", 28);
  ImGui::PopStyleColor();
  if (activity != nullptr) {
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(8));
    Tag(ActivityStatusName(activity->status), ToneOf(activity->status));
  }
  ImGui::SameLine(size.x - Px(100));
  if (GhostButton("‹ 返回", ImVec2(Px(90), 0))) Go(Screen::kMyActivities);

  if (activity != nullptr && !activity->review_reason.empty() && is_draft) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(253, 232, 236, 255));
    ImGui::BeginChild("rejected", ImVec2(size.x, 0),
                      ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    RedText("审核未通过  原因：" + activity->review_reason);
    ImGui::EndChild();
    ImGui::PopStyleColor();
  }
  if (!is_draft) {
    // 已发布活动：左侧申请修改，右侧取消场次、票种或整个活动。
    const float width = ImGui::GetContentRegionAvail().x;
    const float left = width * 0.58f;
    DrawChangeRequest(*activity, left);
    ImGui::SameLine(0, Px(20));
    DrawCancelPanel(*activity, width - left - Px(20));
    return;
  }

  const float right = Px(380);
  const float left = size.x - right - Px(20);
  const float height = size.y - ImGui::GetCursorPosY() + Px(60);
  ImGui::BeginChild("form", ImVec2(left, height));
  BeginCard("basic", ImVec2(left - Px(12), 0), 0.66f);
  Heading("① 基本信息", 18);
  LabeledInput("活动名称", "##title", edit_.title, sizeof(edit_.title),
               left - Px(60));
  ImGui::PushFont(nullptr, 13.0f);
  Muted("分类");
  ImGui::PopFont();
  for (const char* category : kCategories) {
    if (ImGui::RadioButton(category, std::string(edit_.category) == category)) {
      CopyText(edit_.category, sizeof(edit_.category), category);
    }
    ImGui::SameLine();
  }
  ImGui::NewLine();
  ImGui::PushFont(nullptr, 13.0f);
  Muted("活动介绍");
  ImGui::PopFont();
  ImGui::InputTextMultiline("##desc", edit_.description,
                            sizeof(edit_.description),
                            ImVec2(left - Px(60), Px(70)));
  EndCard();

  for (size_t i = 0; i < edit_.sessions.size(); ++i) {
    SessionForm& form = edit_.sessions[i];
    ImGui::PushID(static_cast<int>(i));
    BeginCard("session", ImVec2(left - Px(12), 0), 0.66f);
    Heading(("② 场次 " + std::to_string(i + 1)).c_str(), 18);
    if (edit_.sessions.size() > 1) {
      ImGui::SameLine(left - Px(140));
      if (GhostButton("删除场次")) {
        edit_.sessions.erase(edit_.sessions.begin() + i);
        EndCard();
        ImGui::PopID();
        break;
      }
    }
    const float field = (left - Px(80)) / 2;
    LabeledInput("地点", "##loc", form.location, sizeof(form.location),
                 left - Px(60));
    LabeledPair("报名开放", "##open", form.reg_open, sizeof(form.reg_open),
                "报名截止", "##close", form.reg_close, sizeof(form.reg_close),
                field);
    LabeledPair("活动开始", "##start", form.start, sizeof(form.start),
                "活动结束", "##end", form.end, sizeof(form.end), field);
    LabeledInput("检票开始（即退票截止）", "##checkin", form.checkin,
                 sizeof(form.checkin), field);
    if (ImGui::BeginTable("types", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("票种名称");
      ImGui::TableSetupColumn("名额", ImGuiTableColumnFlags_WidthFixed,
                              Px(140));
      ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed,
                              Px(70));
      ImGui::TableHeadersRow();
      for (size_t j = 0; j < form.types.size(); ++j) {
        TypeForm& type = form.types[j];
        ImGui::PushID(static_cast<int>(j));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", type.name, sizeof(type.name));
        ImGui::TableNextColumn();
        const bool bad = type.capacity <= 0;
        if (bad) {
          ImGui::PushStyleColor(ImGuiCol_Border, ToneColor(Tone::kRed));
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##cap", &type.capacity);
        if (bad) {
          ImGui::PopStyleColor();
          ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kRed));
          ImGui::TextUnformatted("名额须为正整数");
          ImGui::PopStyleColor();
        }
        ImGui::TableNextColumn();
        bool removed = false;
        if (form.types.size() > 1 && GhostButton("删除")) {
          form.types.erase(form.types.begin() + j);
          removed = true;
        }
        ImGui::PopID();
        if (removed) break;
      }
      ImGui::EndTable();
    }
    if (GhostButton("+ 添加票种")) {
      TypeForm type;
      type.capacity = 50;
      form.types.push_back(type);
    }
    EndCard();
    ImGui::PopID();
  }
  if (GhostButton("+ 添加场次")) {
    SessionForm copy = edit_.sessions.back();
    copy.id = 0;
    for (TypeForm& type : copy.types) type.id = 0;
    edit_.sessions.push_back(copy);
  }
  ImGui::EndChild();

  ImGui::SameLine(0, Px(20));
  // 右侧留出空隙，让吉祥物探出卡片右上角时不被窗口边缘裁掉。
  BeginCard("rules", ImVec2(right - Px(30), 0), 0.82f);
  Heading("③ 规则校验", 18);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("· 报名开放 < 报名截止 ≤ 活动开始 < 活动结束");
  Muted("· 检票开始不能晚于活动开始，退票须在检票开始前");
  Muted("· 每个场次至少一个票种，名称不重复，名额为正整数");
  Muted("· 本期只提供免费票，不涉及支付和退款");
  Muted("· 时间格式：2026-11-05 19:00");
  ImGui::PopFont();
  ActivityDraft draft;
  std::string check;
  {
    // 实时校验但不弹提示，只在下方显示第一条问题。
    const std::string saved_toast = toast_;
    const double saved_until = toast_until_;
    if (BuildDraft(&draft)) {
      const Status valid = ActivityService::Validate(draft);
      check = valid.ok() ? "" : valid.message();
    } else {
      check = toast_;
    }
    toast_ = saved_toast;
    toast_until_ = saved_until;
  }
  ImGui::Dummy(ImVec2(0, Px(4)));
  if (check.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kGreen));
    ImGui::TextUnformatted("√ 校验通过，可以保存");
    ImGui::PopStyleColor();
  } else {
    RedText(check);
  }
  ImGui::Dummy(ImVec2(0, Px(6)));
  const float button = ImGui::GetContentRegionAvail().x;
  if (GhostButton("保存草稿", ImVec2(button, Px(42)))) {
    draft = ActivityDraft();
    if (BuildDraft(&draft)) {
      Status status = Status::Ok();
      if (edit_.activity_id == 0) {
        Id id = 0;
        status = sys_->activities.CreateDraft(actor_, draft, &id);
        if (status.ok()) LoadEditForm(id);
      } else {
        status = sys_->activities.UpdateDraft(actor_, edit_.activity_id,
                                              draft);
        if (status.ok()) LoadEditForm(edit_.activity_id);
      }
      ShowResult(status);
    }
  }
  if (FilledButton("保存并提交审核", Tone::kBlue, ImVec2(button, Px(42)),
                   check.empty())) {
    draft = ActivityDraft();
    if (BuildDraft(&draft)) {
      Id id = edit_.activity_id;
      Status status = id == 0 ? sys_->activities.CreateDraft(actor_, draft, &id)
                              : sys_->activities.UpdateDraft(actor_, id, draft);
      if (status.ok()) status = sys_->activities.Submit(actor_, id);
      ShowResult(status);
      if (status.ok()) Go(Screen::kMyActivities);
    }
  }
  if (activity != nullptr) {
    if (GhostButton("取消活动", ImVec2(button, Px(36)))) {
      cancel_ = CancelDialog();
      cancel_.kind = 0;
      cancel_.target = activity->id;
      cancel_.title = activity->title;
      open_cancel_ = true;
    }
  }
  EndCard();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  DrawMascot(ImVec2(max.x, min.y), Px(100));
}

void App::DrawChangeRequest(const Activity& activity, float left) {
  BeginCard("change", ImVec2(left, 0), 0.7f);
  Heading("申请修改已发布活动", 20);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("可以修改地点、时间和增加名额（名额只增不减）。所有修改都需管理员审核，"
        "审核期间仍显示原信息。");
  ImGui::PopFont();
  const ChangeRequest* pending = sys_->activities.PendingChangeOf(activity.id);
  if (pending != nullptr) {
    ImGui::Dummy(ImVec2(0, Px(4)));
    Tag("待审核", Tone::kBlue);
    ImGui::SameLine();
    ImGui::Text("修改申请 #%lld，提交于 %s",
                static_cast<long long>(pending->id),  // NOLINT(runtime/int)
                FormatShort(pending->created_at).c_str());
    if (GhostButton("撤回申请")) {
      ShowResult(sys_->activities.WithdrawChange(actor_, pending->id));
    }
    EndCard();
    return;
  }
  // 最近一次被驳回的申请。
  for (const ChangeRequest& c : sys_->store.data().change_requests) {
    if (c.activity_id == activity.id && c.status == ChangeStatus::kRejected) {
      RedText("上次修改申请被驳回：" + c.reason);
    }
  }
  const std::vector<EventSession*> sessions =
      sys_->store.SessionsOf(activity.id);
  const EventSession* current = sys_->store.FindSession(change_.session_id);
  if (current == nullptr || current->activity_id != activity.id) {
    for (const EventSession* s : sessions) {
      if (!s->cancelled && !s->finished) {
        LoadChangeForm(s->id);
        break;
      }
    }
    current = sys_->store.FindSession(change_.session_id);
  }
  if (current == nullptr) {
    Muted("没有可以修改的场次。");
    EndCard();
    return;
  }
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##s", SessionText(*current).c_str())) {
    for (const EventSession* s : sessions) {
      if (s->cancelled || s->finished) continue;
      ImGui::PushID(static_cast<int>(s->id));
      if (ImGui::Selectable(SessionText(*s).c_str(), s->id == current->id)) {
        LoadChangeForm(s->id);
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  const float field = (left - Px(70)) / 2;
  LabeledInput("地点", "##loc", change_.location, sizeof(change_.location),
               left - Px(50));
  LabeledPair("活动开始", "##start", change_.start, sizeof(change_.start),
              "活动结束", "##end", change_.end, sizeof(change_.end), field);
  LabeledPair("报名截止", "##close", change_.reg_close,
              sizeof(change_.reg_close), "检票开始", "##checkin",
              change_.checkin, sizeof(change_.checkin), field);
  bool decreased = false;
  for (CapacityChange& item : change_.capacities) {
    const TicketType* type = sys_->store.FindTicketType(item.ticket_type_id);
    ImGui::PushID(static_cast<int>(item.ticket_type_id));
    ImGui::SetNextItemWidth(Px(160));
    ImGui::InputInt(("「" + type->name + "」名额").c_str(), &item.capacity);
    if (item.capacity < type->capacity) {
      ImGui::SameLine();
      RedText("不能少于原名额 " + std::to_string(type->capacity));
      decreased = true;
    }
    ImGui::PopID();
  }
  if (FilledButton("提交修改申请", Tone::kBlue, ImVec2(Px(200), Px(42)),
                   !decreased)) {
    ChangeRequest request;
    request.activity_id = activity.id;
    request.session_id = change_.session_id;
    request.new_location = change_.location;
    request.capacities = change_.capacities;
    std::string error;
    ReadTime(change_.start, "活动开始时间", &request.new_start_at, &error);
    ReadTime(change_.end, "活动结束时间", &request.new_end_at, &error);
    ReadTime(change_.reg_close, "报名截止时间", &request.new_reg_close_at,
             &error);
    ReadTime(change_.checkin, "检票开始时间", &request.new_checkin_start_at,
             &error);
    ShowResult(error.empty()
                   ? sys_->activities.RequestChange(actor_, request, nullptr)
                   : Status::Error(error));
  }
  EndCard();
}

void App::DrawCancelPanel(const Activity& activity, float width) {
  const std::vector<EventSession*> sessions =
      sys_->store.SessionsOf(activity.id);
  BeginCard("cancel", ImVec2(width, 0), 0.82f);
  Heading("取消", 20);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("取消后范围内的报名转为已取消、票券失效；候补队列直接清空，"
        "不会触发递补。范围外的场次和票种不受影响。");
  ImGui::PopFont();
  for (const EventSession* s : sessions) {
    ImGui::PushID(static_cast<int>(s->id));
    ImGui::Separator();
    ImGui::TextUnformatted(SessionText(*s).c_str());
    if (s->cancelled || s->finished) {
      ImGui::SameLine();
      Tag(s->cancelled ? "已取消" : "已结束", Tone::kGray);
    } else {
      if (GhostButton("取消该场次")) {
        cancel_ = CancelDialog();
        cancel_.kind = 1;
        cancel_.target = s->id;
        cancel_.title = SessionText(*s);
        open_cancel_ = true;
      }
      for (const TicketType* type : sys_->store.TicketTypesOf(s->id)) {
        ImGui::PushID(static_cast<int>(type->id));
        ImGui::SameLine();
        if (type->cancelled) {
          Tag((type->name + " 已取消").c_str(), Tone::kGray);
        } else if (GhostButton(("取消「" + type->name + "」").c_str())) {
          cancel_ = CancelDialog();
          cancel_.kind = 2;
          cancel_.target = type->id;
          cancel_.title = type->name;
          open_cancel_ = true;
        }
        ImGui::PopID();
      }
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  if (FilledButton("取消整个活动", Tone::kRed)) {
    cancel_ = CancelDialog();
    cancel_.kind = 0;
    cancel_.target = activity.id;
    cancel_.title = activity.title;
    open_cancel_ = true;
  }
  EndCard();
}

void App::DrawCancelDialog() {
  if (open_cancel_) {
    ImGui::OpenPopup("取消确认");
    open_cancel_ = false;
  }
  if (!ImGui::BeginPopupModal("取消确认", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  const char* scope[] = {"活动", "场次", "票种"};
  Heading((std::string("取消") + scope[cancel_.kind] + "：" + cancel_.title)
              .c_str(),
          20);
  Muted("范围内报名转为已取消、票券失效，候补队列清空且不递补。");
  ImGui::SetNextItemWidth(Px(420));
  ImGui::InputTextWithHint("##reason", "请填写取消原因（必填）",
                           cancel_.reason, sizeof(cancel_.reason));
  if (GhostButton("返回", ImVec2(Px(150), Px(40)))) ImGui::CloseCurrentPopup();
  ImGui::SameLine();
  if (FilledButton("确认取消", Tone::kRed, ImVec2(Px(150), Px(40)))) {
    CancelImpact impact;
    Status status = Status::Ok();
    if (cancel_.kind == 0) {
      status = sys_->activities.CancelActivity(actor_, cancel_.target,
                                               cancel_.reason, &impact);
    } else if (cancel_.kind == 1) {
      status = sys_->activities.CancelSession(actor_, cancel_.target,
                                              cancel_.reason, &impact);
    } else {
      status = sys_->activities.CancelTicketType(actor_, cancel_.target,
                                                 cancel_.reason, &impact);
    }
    ShowResult(status);
    if (status.ok()) ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void App::DrawManage(ImVec2 size) {
  const std::vector<const Activity*> list =
      sys_->activities.ListManaged(actor_);
  const Activity* activity = nullptr;
  for (const Activity* a : list) {
    if (a->id == manage_id_) activity = a;
  }
  if (activity == nullptr) {
    for (const Activity* a : list) {
      if (a->status != ActivityStatus::kDraft) {
        activity = a;
        break;
      }
    }
  }
  if (activity == nullptr) {
    Muted("还没有可统计的活动。");
    return;
  }
  manage_id_ = activity->id;
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  Heading("名单统计与检票安排", 26);
  ImGui::PopStyleColor();
  ImGui::SameLine(size.x - Px(380));
  ImGui::SetNextItemWidth(Px(370));
  if (ImGui::BeginCombo("##activity", activity->title.c_str())) {
    for (const Activity* a : list) {
      if (a->status == ActivityStatus::kDraft) continue;
      ImGui::PushID(static_cast<int>(a->id));
      if (ImGui::Selectable(a->title.c_str(), a->id == manage_id_)) {
        manage_id_ = a->id;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }

  // 统计卡片：口径与 REQ-018 一致。
  Counts counts;
  sys_->stats.ActivityCounts(actor_, activity->id, &counts);
  const float gap = Px(12);
  const float tile = (size.x - gap * 6) / 7;
  char rate[16];
  snprintf(rate, sizeof(rate), "%.1f%%", counts.checkin_rate() * 100);
  const struct {
    const char* label;
    std::string value;
    const char* note;
    ImU32 color;
  } tiles[] = {
      {"总容量", std::to_string(counts.capacity), "未取消票种之和",
       IM_COL32(18, 33, 58, 255)},
      {"报名成功", std::to_string(counts.confirmed), "尚未签到",
       ToneColor(Tone::kGreen)},
      {"候补中", std::to_string(counts.waiting), "不计入占用",
       ToneColor(Tone::kOrange)},
      {"已签到", std::to_string(counts.checked_in), "首次核销计一次",
       ToneColor(Tone::kBlue)},
      {"已取消", std::to_string(counts.cancelled), "不计入占用",
       ToneColor(Tone::kGray)},
      {"缺席", std::to_string(counts.absent), "场次结束未签到",
       ToneColor(Tone::kRed)},
      {"签到率", rate, "已签到 ÷ 占用", IM_COL32(18, 33, 58, 255)},
  };
  for (int i = 0; i < 7; ++i) {
    if (i > 0) ImGui::SameLine(0, gap);
    ImGui::PushID(i);
    StatTile("tile", tiles[i].label, tiles[i].value.c_str(), tiles[i].note,
             tiles[i].color, tile);
    ImGui::PopID();
  }
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 230));
  ImGui::TextUnformatted("占用 = 报名成功 + 已签到 + 缺席；签到率分母为占用，"
                         "为 0 时显示 0。");
  ImGui::PopStyleColor();

  const float body = size.y - ImGui::GetCursorPosY() + Px(64);
  const float right = Px(360);
  const float left = size.x - right - Px(20);
  BeginCard("roster", ImVec2(left, body), 0.75f);
  Heading("报名名单", 18);
  ImGui::SameLine();
  const char* filters[] = {"全部状态", "候补中", "报名成功", "已签到",
                           "已取消",   "缺席",   "候补失效"};
  ImGui::SetNextItemWidth(Px(120));
  ImGui::Combo("##filter", &roster_filter_, filters, 7);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(150));
  ImGui::InputTextWithHint("##search", "按姓名筛选", roster_search_,
                           sizeof(roster_search_));
  ImGui::SameLine(left - Px(140));
  if (FilledButton("导出 CSV", Tone::kBlue, ImVec2(Px(100), 0))) {
    const std::string path = ExecutableDir() + "\\名单_活动" +
                             std::to_string(activity->id) + ".csv";
    const Status status = sys_->stats.ExportCsv(actor_, activity->id, path);
    if (status.ok()) {
      sys_->audit.Record(actor_, "导出名单", activity->title, activity->id,
                         "成功");
    }
    ShowResult(status);
  }
  std::vector<RosterRow> rows;
  sys_->stats.Roster(actor_, activity->id, &rows);
  if (ImGui::BeginTable("rows", 7,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    for (const char* name : {"编号", "姓名", "场次", "票种", "状态", "报名时间",
                             "签到时间"}) {
      ImGui::TableSetupColumn(name);
    }
    ImGui::TableHeadersRow();
    for (const RosterRow& row : rows) {
      if (roster_filter_ > 0 &&
          static_cast<int>(row.status) != roster_filter_ - 1) {
        continue;
      }
      if (roster_search_[0] != '\0' &&
          row.name.find(roster_search_) == std::string::npos) {
        continue;
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("R-%lld",
                  static_cast<long long>(row.registration_id));  // NOLINT
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.name.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.session.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.ticket_type.c_str());
      ImGui::TableNextColumn();
      Tag(RegStatusName(row.status), ToneOf(row.status));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(FormatShort(row.created_at).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(
          row.checked_at == 0 ? "—" : FormatShort(row.checked_at).c_str());
    }
    ImGui::EndTable();
  }
  EndCard();

  // 检票安排：发布活动不会自动授予检票权限（REQ-014）。
  ImGui::SameLine(0, Px(20));
  BeginCard("checkers", ImVec2(right, body), 0.82f);
  Heading("检票安排", 18);
  const std::vector<const User*> checkers =
      sys_->accounts.ListByRole(Role::kChecker);
  for (const EventSession* s : sys_->store.SessionsOf(activity->id)) {
    ImGui::PushID(static_cast<int>(s->id));
    ImGui::Separator();
    ImGui::TextUnformatted(SessionText(*s).c_str());
    if (s->cancelled || s->finished) {
      Tag(s->cancelled ? "已取消" : "已结束", Tone::kGray);
      ImGui::PopID();
      continue;
    }
    bool any = false;
    for (const User* checker : checkers) {
      ImGui::PushID(static_cast<int>(checker->id));
      bool on = sys_->activities.IsAssigned(s->id, checker->id);
      any = any || on;
      if (ImGui::Checkbox(checker->display_name.c_str(), &on)) {
        ShowResult(on ? sys_->activities.AssignChecker(actor_, s->id,
                                                       checker->id)
                      : sys_->activities.UnassignChecker(actor_, s->id,
                                                         checker->id));
      }
      ImGui::SameLine();
      ImGui::PopID();
    }
    ImGui::NewLine();
    if (!any) {
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kOrange));
      ImGui::TextWrapped("%s",
                         "该场次尚未分配检票员，"
                         "发布活动不会自动授予检票权限。");
      ImGui::PopStyleColor();
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  Heading("最近操作", 16);
  std::vector<const OperationLog*> logs;
  sys_->audit.Query(actor_, LogFilter(), &logs);
  int shown = 0;
  ImGui::PushFont(nullptr, 13.0f);
  for (const OperationLog* log : logs) {
    if (log->activity_id != activity->id || shown >= 6) continue;
    Muted((FormatShort(log->created_at) + "  " + log->actor_name + " " +
           log->action + " · " + log->result).c_str());
    ++shown;
  }
  ImGui::PopFont();
  EndCard();
}

void App::DrawReview(ImVec2 size) {
  if (actor_.role != Role::kAdmin) {
    Muted("只有管理员可以审核。");
    return;
  }
  const std::vector<const Activity*> activities =
      sys_->reviews.PendingActivities(actor_);
  const std::vector<const ChangeRequest*> changes =
      sys_->reviews.PendingChanges(actor_);
  const float left = Px(380);
  BeginCard("queue", ImVec2(left, size.y), 0.66f);
  Heading("待审核申请", 20);
  const std::string tab0 = "申办 " + std::to_string(activities.size());
  const std::string tab1 = "修改 " + std::to_string(changes.size());
  const char* tabs[] = {tab0.c_str(), tab1.c_str()};
  if (Segmented("kind", tabs, 2, &review_kind_,
                ImGui::GetContentRegionAvail().x)) {
    review_id_ = 0;
  }
  if (review_kind_ == 0) {
    if (activities.empty()) Muted("没有待审核的申办。");
    for (const Activity* a : activities) {
      if (review_id_ == 0) review_id_ = a->id;
      ImGui::PushID(static_cast<int>(a->id));
      const std::string label = a->title + "\n" +
                                sys_->accounts.NameOf(a->organizer_id) + " · " +
                                a->category;
      if (ImGui::Selectable(label.c_str(), review_id_ == a->id, 0,
                            ImVec2(0, Px(50)))) {
        review_id_ = a->id;
      }
      ImGui::PopID();
    }
  } else {
    if (changes.empty()) Muted("没有待审核的修改申请。");
    for (const ChangeRequest* c : changes) {
      if (review_id_ == 0) review_id_ = c->id;
      const Activity* a = sys_->store.FindActivity(c->activity_id);
      ImGui::PushID(static_cast<int>(c->id));
      const std::string label = a->title + "\n修改申请 #" +
                                std::to_string(c->id) + " · " +
                                FormatShort(c->created_at);
      if (ImGui::Selectable(label.c_str(), review_id_ == c->id, 0,
                            ImVec2(0, Px(50)))) {
        review_id_ = c->id;
      }
      ImGui::PopID();
    }
  }
  PerchTip("tip", "小提示：驳回时必须填写原因，组织者会在编辑页看到；"
                  "审批时会再次核对活动版本。",
           left - Px(70), size.y - Px(130));
  EndCard();

  ImGui::SameLine(0, Px(20));
  const float right = size.x - left - Px(20);
  BeginCard("detail", ImVec2(right, size.y), 0.82f);
  if (review_kind_ == 0) {
    const Activity* a = sys_->store.FindActivity(review_id_);
    if (a == nullptr || a->status != ActivityStatus::kPending) {
      Muted("请从左侧选择一项申请。");
    } else {
      Heading(("申办：" + a->title).c_str(), 22);
      Muted(("组织者 " + sys_->accounts.NameOf(a->organizer_id) + " · 分类 " +
             a->category + " · 版本 " + std::to_string(a->version))
                .c_str());
      ImGui::TextWrapped("%s", a->description.c_str());
      if (ImGui::BeginTable("sessions", 4, ImGuiTableFlags_Borders |
                                               ImGuiTableFlags_RowBg)) {
        for (const char* name :
             {"场次", "报名时间", "检票开始", "票种与名额"}) {
          ImGui::TableSetupColumn(name);
        }
        ImGui::TableHeadersRow();
        for (const EventSession* s : sys_->store.SessionsOf(a->id)) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextWrapped("%s", SessionText(*s).c_str());
          ImGui::TableNextColumn();
          ImGui::TextWrapped("%s 至 %s", FormatShort(s->reg_open_at).c_str(),
                             FormatShort(s->reg_close_at).c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(FormatShort(s->checkin_start_at).c_str());
          ImGui::TableNextColumn();
          for (const TicketType* t : sys_->store.TicketTypesOf(s->id)) {
            ImGui::Text("%s %d 人", t->name.c_str(), t->capacity);
          }
        }
        ImGui::EndTable();
      }
      ImGui::Dummy(ImVec2(0, Px(6)));
      if (FilledButton("通过并发布", Tone::kGreen, ImVec2(Px(160), Px(42)))) {
        ShowResult(sys_->reviews.Approve(actor_, a->id, a->version));
        review_id_ = 0;
      }
      ImGui::SetNextItemWidth(right - Px(200));
      ImGui::InputTextWithHint("##reason", "驳回原因（驳回时必填）",
                               review_reason_, sizeof(review_reason_));
      ImGui::SameLine();
      if (FilledButton("驳回", Tone::kRed, ImVec2(Px(120), 0))) {
        const Status status =
            sys_->reviews.Reject(actor_, a->id, review_reason_);
        if (status.ok()) {
          review_reason_[0] = '\0';
          review_id_ = 0;
        }
        ShowResult(status);
      }
    }
  } else {
    const ChangeRequest* c = sys_->store.FindChangeRequest(review_id_);
    if (c == nullptr || c->status != ChangeStatus::kPending) {
      Muted("请从左侧选择一项申请。");
    } else {
      const Activity* a = sys_->store.FindActivity(c->activity_id);
      const EventSession* s = sys_->store.FindSession(c->session_id);
      Heading(("修改申请：" + a->title).c_str(), 22);
      Muted(("场次 " + SessionText(*s) + " · 基准版本 " +
             std::to_string(c->base_version) + " · 当前版本 " +
             std::to_string(a->version))
                .c_str());
      // 前后对比，变化的字段用橙色标出。
      if (ImGui::BeginTable("diff", 3, ImGuiTableFlags_Borders |
                                           ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("字段");
        ImGui::TableSetupColumn("修改前");
        ImGui::TableSetupColumn("修改后");
        ImGui::TableHeadersRow();
        std::vector<std::pair<std::string, std::pair<std::string,
                                                     std::string>>> fields = {
            {"地点", {s->location, c->new_location}},
            {"活动开始",
             {FormatTime(s->start_at), FormatTime(c->new_start_at)}},
            {"活动结束", {FormatTime(s->end_at), FormatTime(c->new_end_at)}},
            {"报名截止",
             {FormatTime(s->reg_close_at), FormatTime(c->new_reg_close_at)}},
            {"检票开始",
             {FormatTime(s->checkin_start_at),
              FormatTime(c->new_checkin_start_at)}},
        };
        for (const CapacityChange& item : c->capacities) {
          const TicketType* t = sys_->store.FindTicketType(item.ticket_type_id);
          fields.push_back({"「" + t->name + "」名额",
                            {std::to_string(t->capacity),
                             std::to_string(item.capacity)}});
        }
        for (const auto& field : fields) {
          const bool changed = field.second.first != field.second.second;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(field.first.c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(field.second.first.c_str());
          ImGui::TableNextColumn();
          if (changed) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(Tone::kOrange));
          }
          ImGui::TextUnformatted(field.second.second.c_str());
          if (changed) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
      }
      bool increase_only = true;
      for (const CapacityChange& item : c->capacities) {
        const TicketType* t = sys_->store.FindTicketType(item.ticket_type_id);
        if (item.capacity < t->capacity) increase_only = false;
      }
      const bool checkin_ok = c->new_checkin_start_at <= c->new_start_at;
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(increase_only
                                                         ? Tone::kGreen
                                                         : Tone::kRed));
      ImGui::TextUnformatted(increase_only ? "√ 名额只增未减"
                                           : "× 存在减少名额");
      ImGui::PopStyleColor();
      ImGui::SameLine(0, Px(20));
      ImGui::PushStyleColor(ImGuiCol_Text, ToneColor(checkin_ok
                                                         ? Tone::kGreen
                                                         : Tone::kRed));
      ImGui::TextUnformatted(checkin_ok ? "√ 检票开始不晚于活动开始"
                                        : "× 检票开始晚于活动开始");
      ImGui::PopStyleColor();
      const int promote = sys_->reviews.EstimatePromotion(*c);
      Muted(("通过后将按顺序递补 " + std::to_string(promote) +
             " 名候补；审批时会再次核对活动版本，防止覆盖已变化的数据。")
                .c_str());
      ImGui::Dummy(ImVec2(0, Px(6)));
      if (FilledButton("通过", Tone::kGreen, ImVec2(Px(140), Px(42)))) {
        int promoted = 0;
        ShowResult(sys_->reviews.ApproveChange(actor_, c->id, &promoted));
        review_id_ = 0;
      }
      ImGui::SetNextItemWidth(right - Px(200));
      ImGui::InputTextWithHint("##reason", "驳回原因（驳回时必填）",
                               review_reason_, sizeof(review_reason_));
      ImGui::SameLine();
      if (FilledButton("驳回", Tone::kRed, ImVec2(Px(120), 0))) {
        const Status status =
            sys_->reviews.RejectChange(actor_, c->id, review_reason_);
        if (status.ok()) {
          review_reason_[0] = '\0';
          review_id_ = 0;
        }
        ShowResult(status);
      }
    }
  }
  EndCard();
}

void App::DrawPlatform(ImVec2 size) {
  if (actor_.role != Role::kAdmin) {
    Muted("只有管理员可以查看平台统计。");
    return;
  }
  Counts counts;
  sys_->stats.PlatformCounts(actor_, &counts);
  const float gap = Px(14);
  const float chart = Px(340);
  const float tile = (size.x - chart - gap * 5) / 5;
  char rate[16];
  snprintf(rate, sizeof(rate), "%.1f%%", counts.checkin_rate() * 100);
  StatTile("t1", "活动数", std::to_string(counts.activities).c_str(),
           "不含草稿", IM_COL32(18, 33, 58, 255), tile);
  ImGui::SameLine(0, gap);
  StatTile("t2", "占用名额", std::to_string(counts.occupied()).c_str(),
           ("总容量 " + std::to_string(counts.capacity)).c_str(),
           ToneColor(Tone::kBlue), tile);
  ImGui::SameLine(0, gap);
  StatTile("t3", "候补人数", std::to_string(counts.waiting).c_str(),
           "不计入占用", ToneColor(Tone::kOrange), tile);
  ImGui::SameLine(0, gap);
  StatTile("t4", "签到人数", std::to_string(counts.checked_in).c_str(),
           ("缺席 " + std::to_string(counts.absent)).c_str(),
           ToneColor(Tone::kGreen), tile);
  ImGui::SameLine(0, gap);
  StatTile("t5", "签到率", rate, "已签到 ÷ 占用", IM_COL32(18, 33, 58, 255),
           tile);
  const float tiles_height = ImGui::GetItemRectSize().y;
  ImGui::SameLine(0, gap);

  // 各分类占用名额柱状图。
  BeginCard("chart", ImVec2(chart, tiles_height), 0.85f);
  ImGui::PushFont(nullptr, 13.0f);
  Muted("各分类占用名额");
  ImGui::PopFont();
  std::vector<std::pair<std::string, int>> by_category;
  sys_->stats.OccupiedByCategory(actor_, &by_category);
  int top = 1;
  for (const auto& item : by_category) top = std::max(top, item.second);
  // 柱子上方留出写数值的空间，下方留出写分类名的空间。
  const ImVec2 origin(ImGui::GetCursorScreenPos().x,
                      ImGui::GetCursorScreenPos().y + Px(18));
  const float bar_area = ImGui::GetContentRegionAvail().y - Px(40);
  const float bar_width = Px(40);
  ImDrawList* list = ImGui::GetWindowDrawList();
  for (size_t i = 0; i < by_category.size() && i < 5; ++i) {
    const float x = origin.x + i * (bar_width + Px(22));
    const float h = std::max(Px(4), bar_area * by_category[i].second / top);
    const float bottom = origin.y + bar_area;
    list->AddRectFilledMultiColor(ImVec2(x, bottom - h),
                                  ImVec2(x + bar_width, bottom),
                                  IM_COL32(92, 195, 255, 255),
                                  IM_COL32(92, 195, 255, 255),
                                  ToneColor(Tone::kBlue),
                                  ToneColor(Tone::kBlue));
    list->AddText(ImVec2(x, bottom + Px(2)), IM_COL32(104, 120, 146, 255),
                  by_category[i].first.c_str());
    list->AddText(ImVec2(x + Px(4), bottom - h - Px(18)),
                  IM_COL32(18, 33, 58, 255),
                  std::to_string(by_category[i].second).c_str());
  }
  EndCard();

  // 操作日志：可按动作和关键词查询；个人清理不会删除日志（REQ-019）。
  ImGui::Dummy(ImVec2(0, Px(6)));
  const float logs_height = size.y - ImGui::GetCursorPosY() + Px(64);
  BeginCard("logs", ImVec2(size.x, logs_height), 0.8f);
  Heading("操作日志", 18);
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(3));
  ImGui::PushFont(nullptr, 13.0f);
  Muted("· 关键写操作均记录，个人清理不会删除日志");
  ImGui::PopFont();
  std::vector<std::string> actions = {"全部动作"};
  for (const OperationLog& log : sys_->store.data().logs) {
    if (std::find(actions.begin(), actions.end(), log.action) ==
        actions.end()) {
      actions.push_back(log.action);
    }
  }
  if (log_action_ >= static_cast<int>(actions.size())) log_action_ = 0;
  ImGui::SameLine(size.x - Px(470));
  ImGui::SetNextItemWidth(Px(170));
  if (ImGui::BeginCombo("##action", actions[log_action_].c_str())) {
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
      if (ImGui::Selectable(actions[i].c_str(), log_action_ == i)) {
        log_action_ = i;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(240));
  ImGui::InputTextWithHint("##kw", "活动或对象关键词", log_keyword_,
                           sizeof(log_keyword_));
  LogFilter filter;
  if (log_action_ > 0) filter.action = actions[log_action_];
  filter.keyword = log_keyword_;
  std::vector<const OperationLog*> logs;
  sys_->audit.Query(actor_, filter, &logs);
  if (ImGui::BeginTable("table", 6,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    for (const char* name : {"时间", "操作者", "动作", "对象", "所属活动",
                             "结果"}) {
      ImGui::TableSetupColumn(name);
    }
    ImGui::TableHeadersRow();
    for (const OperationLog* log : logs) {
      const Activity* a = sys_->store.FindActivity(log->activity_id);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(FormatShort(log->created_at).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(log->actor_name.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(log->action.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(log->target.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(a == nullptr ? "—" : a->title.c_str());
      ImGui::TableNextColumn();
      const std::string& r = log->result;
      const Tone tone = r.find("候补") != std::string::npos ||
                                r.find("重复") != std::string::npos
                            ? Tone::kOrange
                        : r.find("驳回") != std::string::npos ? Tone::kRed
                                                              : Tone::kGreen;
      Tag(r.c_str(), tone);
    }
    ImGui::EndTable();
  }
  EndCard();
}

}  // namespace ui
}  // namespace ticketing
