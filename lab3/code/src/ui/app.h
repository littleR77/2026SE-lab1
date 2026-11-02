// Copyright 2026 R
// 表现层：按实验二的 10 个界面组织，每帧根据当前角色和页面绘制界面，
// 所有业务操作都调用 System 中的业务模块，界面本身不改数据。
#ifndef UI_APP_H_
#define UI_APP_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "domain/model.h"
#include "imgui/imgui.h"
#include "service/system.h"

namespace ticketing {
namespace ui {

enum class Screen {
  kLogin,         // 界面 1 登录 / 注册
  kPlaza,         // 界面 2 活动广场
  kDetail,        // 界面 3 活动详情与报名
  kTickets,       // 界面 4 我的票券
  kMessages,      // 界面 5 消息与收藏
  kCheckin,       // 界面 6 检票
  kMyActivities,  // 组织者的活动列表
  kEdit,          // 界面 7 活动编辑
  kManage,        // 界面 8 名单统计与检票安排
  kReview,        // 界面 9 审核台
  kPlatform,      // 界面 10 平台统计与操作日志
};

class App {
 public:
  explicit App(System* system);

  // 绘制一帧。size 为窗口客户区大小。
  void Frame(ImVec2 size);
  // 以指定账号登录并跳到某个页面（命令行截图用）。
  bool DebugOpen(const std::string& account, Screen screen, Id activity_id);

 private:
  // 编辑表单中的票种和场次。时间以文本输入，保存时再解析。
  struct TypeForm {
    Id id = 0;
    char name[48] = "";
    int capacity = 0;
  };
  struct SessionForm {
    Id id = 0;
    char location[96] = "";
    char start[20] = "";
    char end[20] = "";
    char reg_open[20] = "";
    char reg_close[20] = "";
    char checkin[20] = "";
    std::vector<TypeForm> types;
  };
  struct EditForm {
    Id activity_id = 0;  // 0 表示新建
    char title[96] = "";
    char category[32] = "";
    char description[600] = "";
    std::vector<SessionForm> sessions;
  };
  // 修改申请表单。
  struct ChangeForm {
    Id session_id = 0;
    char location[96] = "";
    char start[20] = "";
    char end[20] = "";
    char reg_close[20] = "";
    char checkin[20] = "";
    std::vector<CapacityChange> capacities;
  };
  // 取消活动、场次或票种时弹出的确认框。
  struct CancelDialog {
    int kind = 0;  // 0 活动，1 场次，2 票种
    Id target = 0;
    std::string title;
    char reason[200] = "";
  };

  // 页面框架。
  void Go(Screen screen);
  void OpenDetail(Id activity_id);
  void ShowResult(const Status& status);
  void Logout();
  void DrawNav(float width);
  void DrawToast(ImVec2 size);
  bool IsParticipant() const { return actor_.role == Role::kParticipant; }

  // screens_public.cc
  void DrawLogin(ImVec2 size);
  void DrawPlaza(ImVec2 size);
  void DrawActivityCard(const Activity& activity, float width);
  void DrawDetail(ImVec2 size);
  void DrawRegisterPanel(const Activity& activity, float width, float height);
  void DrawComments(const Activity& activity);

  // screens_personal.cc
  void DrawTickets(ImVec2 size);
  void DrawMessages(ImVec2 size);
  void DrawCheckin(ImVec2 size);

  // screens_manage.cc
  void DrawMyActivities(ImVec2 size);
  void DrawEdit(ImVec2 size);
  void DrawChangeRequest(const Activity& activity, float width);
  void DrawCancelPanel(const Activity& activity, float width);
  void DrawManage(ImVec2 size);
  void DrawReview(ImVec2 size);
  void DrawPlatform(ImVec2 size);
  void DrawCancelDialog();
  void LoadEditForm(Id activity_id);
  bool BuildDraft(ActivityDraft* draft);
  void LoadChangeForm(Id session_id);

  // 公用的小部件和文字。
  std::string SessionText(const EventSession& session) const;
  int Remaining(Id activity_id) const;
  int Capacity(Id activity_id) const;
  void FavoriteStar(Id activity_id);

  System* sys_;
  Actor actor_;
  Screen screen_ = Screen::kLogin;

  // 提示条。
  std::string toast_;
  bool toast_ok_ = true;
  double toast_until_ = 0;

  // 登录页。
  int login_tab_ = 0;
  char account_[32] = "";
  char password_[32] = "";
  char nickname_[32] = "";
  std::string login_error_;

  // 活动广场。
  int category_ = 0;
  char date_from_[16] = "";
  char date_to_[16] = "";
  char keyword_[64] = "";
  int status_filter_ = 0;

  // 活动详情。
  Id detail_id_ = 0;
  Id chosen_session_ = 0;
  Id chosen_type_ = 0;
  char comment_[400] = "";
  char guest_name_[32] = "";
  Id reply_to_ = 0;
  char reply_[400] = "";

  // 我的票券。
  int ticket_tab_ = 0;
  Id confirm_cancel_ = 0;

  // 检票。
  Id checkin_session_ = 0;
  char ticket_code_[32] = "";
  std::vector<CheckinResult> checkin_history_;

  // 组织者。
  EditForm edit_;
  ChangeForm change_;
  CancelDialog cancel_;
  bool open_cancel_ = false;
  Id manage_id_ = 0;
  int roster_filter_ = 0;
  char roster_search_[32] = "";

  // 管理员。
  int review_kind_ = 0;  // 0 申办，1 修改申请
  Id review_id_ = 0;
  char review_reason_[200] = "";
  int log_action_ = 0;
  char log_keyword_[48] = "";
};

}  // namespace ui
}  // namespace ticketing

#endif  // UI_APP_H_
