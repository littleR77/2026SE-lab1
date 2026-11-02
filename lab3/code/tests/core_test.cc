// Copyright 2026 R
// 业务规则测试：按实验一需求的验收方法逐条检查核心模块。
// 不依赖测试框架，失败时打印文件行号，最后返回失败数。
#include <cstdio>
#include <string>
#include <vector>

#include "common/file_util.h"
#include "common/time_util.h"
#include "service/system.h"

namespace ticketing {
namespace {

int g_failures = 0;
int g_checks = 0;

#define EXPECT(cond)                                                  \
  do {                                                               \
    ++g_checks;                                                      \
    if (!(cond)) {                                                   \
      ++g_failures;                                                  \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                \
  } while (0)

// 每个用例使用一套全新的系统和几个账号。
struct Fixture {
  Fixture() : sys(MakeTime(2026, 11, 1, 10, 0)) {
    Id id = 0;
    sys.accounts.Register("admin", "管理员", "123456", Role::kAdmin, &id);
    admin = {id, Role::kAdmin};
    sys.accounts.Register("org", "组织者", "123456", Role::kOrganizer, &id);
    org = {id, Role::kOrganizer};
    sys.accounts.Register("org2", "其他组织者", "123456", Role::kOrganizer,
                          &id);
    org2 = {id, Role::kOrganizer};
    sys.accounts.Register("checker", "检票员", "123456", Role::kChecker, &id);
    checker = {id, Role::kChecker};
    for (const char* name : {"alice", "bob", "carol"}) {
      sys.accounts.Register(name, name, "123456", Role::kParticipant, &id);
      people.push_back({id, Role::kParticipant});
    }
  }

  // 一个场次 11/05 19:00，报名 11/01 09:00～11/05 12:00，检票 18:30 开始。
  static ActivityDraft Draft(int capacity) {
    ActivityDraft draft;
    draft.title = "测试活动";
    draft.category = "演出";
    SessionDraft session;
    session.location = "礼堂";
    session.reg_open_at = MakeTime(2026, 11, 1, 9, 0);
    session.reg_close_at = MakeTime(2026, 11, 5, 12, 0);
    session.checkin_start_at = MakeTime(2026, 11, 5, 18, 30);
    session.start_at = MakeTime(2026, 11, 5, 19, 0);
    session.end_at = MakeTime(2026, 11, 5, 21, 0);
    session.ticket_types.push_back({0, "普通票", capacity});
    draft.sessions.push_back(session);
    return draft;
  }

  Id Publish(int capacity) {
    Id id = 0;
    sys.activities.CreateDraft(org, Draft(capacity), &id);
    sys.activities.Submit(org, id);
    sys.reviews.Approve(admin, id, sys.store.FindActivity(id)->version);
    activity = id;
    session = sys.store.SessionsOf(id).front()->id;
    type = sys.store.TicketTypesOf(session).front()->id;
    return id;
  }

  Registration* Reg(Id id) { return sys.store.FindRegistration(id); }
  std::string Code(Id reg_id) { return sys.store.FindTicketOf(reg_id)->code; }

  System sys;
  Actor admin, org, org2, checker;
  std::vector<Actor> people;
  Id activity = 0, session = 0, type = 0;
};

void TestTime() {
  Time t = 0;
  EXPECT(ParseTime("2026-11-05 19:00", &t));
  EXPECT(FormatTime(t) == "2026-11-05 19:00");
  EXPECT(FormatShort(t) == "11/05 19:00");
  EXPECT(std::string(WeekdayName(t)) == "周四");
  EXPECT(!ParseTime("2026-02-30 10:00", &t));
  EXPECT(!ParseTime("2026-11-05 24:00", &t));
  EXPECT(!ParseTime("11-05", &t));
  EXPECT(ParseTime("2026-11-05", &t) && FormatTime(t) == "2026-11-05 00:00");
}

void TestDraftValidation() {
  ActivityDraft draft = Fixture::Draft(10);
  EXPECT(ActivityService::Validate(draft).ok());
  draft.sessions[0].checkin_start_at = draft.sessions[0].start_at + 1;
  EXPECT(!ActivityService::Validate(draft).ok());  // 检票开始晚于活动开始
  draft = Fixture::Draft(0);
  EXPECT(!ActivityService::Validate(draft).ok());  // 名额非正整数
  draft = Fixture::Draft(10);
  draft.sessions[0].ticket_types.push_back({0, "普通票", 5});
  EXPECT(!ActivityService::Validate(draft).ok());  // 同场次票种重名
}

void TestReviewFlow() {
  Fixture f;
  Id id = 0;
  EXPECT(f.sys.activities.CreateDraft(f.org, Fixture::Draft(5), &id).ok());
  EXPECT(!f.sys.activities.CreateDraft(f.people[0], Fixture::Draft(5), &id)
             .ok());
  std::vector<const Activity*> found;
  f.sys.queries.Search(Actor(), ActivityFilter(), &found);
  EXPECT(found.empty());  // 游客看不到草稿
  EXPECT(f.sys.activities.Submit(f.org, id).ok());
  EXPECT(!f.sys.activities.Submit(f.org, id).ok());  // 不能重复提交
  EXPECT(!f.sys.reviews.Reject(f.admin, id, "  ").ok());  // 驳回须有原因
  EXPECT(f.sys.reviews.Reject(f.admin, id, "时间冲突").ok());
  EXPECT(f.sys.store.FindActivity(id)->status == ActivityStatus::kDraft);
  EXPECT(f.sys.activities.Submit(f.org, id).ok());
  EXPECT(!f.sys.reviews.Approve(f.org, id, 1).ok());  // 组织者不能审核
  EXPECT(f.sys.reviews.Approve(f.admin, id, 1).ok());
  EXPECT(!f.sys.reviews.Approve(f.admin, id, 1).ok());  // 不能重复审核
  f.sys.queries.Search(Actor(), ActivityFilter(), &found);
  EXPECT(found.size() == 1);
}

void TestRegisterAndWaitlist() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2, again;
  EXPECT(f.sys.registrations.Register(f.people[0], f.session, f.type, &r1)
             .ok());
  EXPECT(!r1.waitlisted);
  EXPECT(f.sys.registrations.Register(f.people[1], f.session, f.type, &r2)
             .ok());
  EXPECT(r2.waitlisted && r2.position == 1);
  // 重复提交返回已有报名，不新增记录和票券。
  const size_t tickets = f.sys.store.data().tickets.size();
  EXPECT(f.sys.registrations.Register(f.people[0], f.session, f.type, &again)
            .ok());
  EXPECT(again.existing && again.registration_id == r1.registration_id);
  EXPECT(f.sys.store.data().tickets.size() == tickets);
  // 入队成功后本人收到一次提醒，其他人没有。
  EXPECT(f.sys.notifier.ListMine(f.people[1]).size() == 1);
  EXPECT(f.sys.notifier.ListMine(f.people[0]).empty());
  EXPECT(f.sys.registrations.Occupied(f.type) == 1);
  // 报名截止后拒绝新报名。
  f.sys.AdvanceClock(5 * kMinutesPerDay);
  RegisterResult late;
  EXPECT(!f.sys.registrations.Register(f.people[2], f.session, f.type, &late)
             .ok());
}

void TestCancelPromotesWaiting() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2, r3;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  f.sys.registrations.Register(f.people[2], f.session, f.type, &r3);
  const std::string voucher = f.Code(r2.registration_id);
  EXPECT(!f.sys.registrations.Cancel(f.people[1], r1.registration_id).ok());
  EXPECT(f.sys.registrations.Cancel(f.people[0], r1.registration_id).ok());
  EXPECT(f.Reg(r1.registration_id)->status == RegStatus::kCancelled);
  EXPECT(!f.sys.store.FindTicketOf(r1.registration_id)->valid);
  // 最早的候补递补，并换发正式票；候补凭证失效。
  EXPECT(f.Reg(r2.registration_id)->status == RegStatus::kConfirmed);
  EXPECT(!f.sys.store.FindTicketByCode(voucher)->valid);
  EXPECT(f.sys.store.FindTicketOf(r2.registration_id)->kind ==
        TicketKind::kFormal);
  EXPECT(f.sys.registrations.WaitingPosition(r3.registration_id) == 1);
  // 重复取消不重复释放名额。
  EXPECT(f.sys.registrations.Cancel(f.people[0], r1.registration_id).ok());
  EXPECT(f.Reg(r3.registration_id)->status == RegStatus::kWaiting);
  // 检票开始后不能退票。
  f.sys.clock.Set(MakeTime(2026, 11, 5, 18, 30));
  EXPECT(!f.sys.registrations.Cancel(f.people[1], r2.registration_id).ok());
}

void TestCheckin() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  const std::string code = f.Code(r1.registration_id);
  f.sys.clock.Set(MakeTime(2026, 11, 5, 18, 40));
  EXPECT(f.sys.checkins.Verify(f.checker, f.session, code).outcome ==
        CheckinOutcome::kRejected);  // 未授权
  EXPECT(f.sys.activities.AssignChecker(f.org, f.session, f.checker.user_id)
            .ok());
  EXPECT(f.sys.checkins.Verify(f.checker, f.session, "TK-0000-0000").outcome ==
        CheckinOutcome::kRejected);
  EXPECT(f.sys.checkins
            .Verify(f.checker, f.session, f.Code(r2.registration_id))
            .outcome == CheckinOutcome::kRejected);  // 候补凭证
  EXPECT(f.sys.checkins.Verify(f.checker, f.session, code).outcome ==
        CheckinOutcome::kPassed);
  const CheckinResult repeat = f.sys.checkins.Verify(f.checker, f.session,
                                                     code);
  EXPECT(repeat.outcome == CheckinOutcome::kRepeated);
  EXPECT(repeat.checked_at == MakeTime(2026, 11, 5, 18, 40));
  EXPECT(f.sys.store.data().checkins.size() == 1);
  EXPECT(f.sys.checkins.CheckedInCount(f.session) == 1);
}

void TestSchedulerMarksAbsent() {
  Fixture f;
  f.Publish(2);
  RegisterResult r1, r2, r3;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  f.sys.registrations.Register(f.people[2], f.session, f.type, &r3);
  f.sys.activities.AssignChecker(f.org, f.session, f.checker.user_id);
  f.sys.clock.Set(MakeTime(2026, 11, 5, 18, 40));
  SchedulerReport report = f.sys.scheduler.RunDue();
  EXPECT(report.expired == 1);  // 报名截止后候补失效
  EXPECT(f.Reg(r3.registration_id)->status == RegStatus::kExpired);
  f.sys.checkins.Verify(f.checker, f.session, f.Code(r1.registration_id));
  f.sys.clock.Set(MakeTime(2026, 11, 5, 21, 0));
  report = f.sys.scheduler.RunDue();
  EXPECT(report.absent == 1 && report.ended == 1);
  EXPECT(f.Reg(r1.registration_id)->status == RegStatus::kCheckedIn);
  EXPECT(f.Reg(r2.registration_id)->status == RegStatus::kAbsent);
  EXPECT(!f.sys.store.FindTicketOf(r2.registration_id)->valid);
  EXPECT(f.sys.store.FindActivity(f.activity)->status ==
        ActivityStatus::kEnded);
  // 再次运行不会重复处理。
  report = f.sys.scheduler.RunDue();
  EXPECT(report.absent == 0 && report.ended == 0);
}

void TestCancelActivityClearsWaitlist() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  CancelImpact impact;
  EXPECT(!f.sys.activities.CancelActivity(f.org2, f.activity, "原因", &impact)
             .ok());  // 不能取消他人活动
  EXPECT(!f.sys.activities.CancelActivity(f.org, f.activity, "", &impact).ok());
  EXPECT(f.sys.activities.CancelActivity(f.org, f.activity, "天气原因", &impact)
            .ok());
  EXPECT(impact.cancelled == 1 && impact.waitlist_removed == 1);
  // 报名转为已取消、票券失效；候补记录被删除，没有触发递补。
  EXPECT(f.Reg(r1.registration_id)->status == RegStatus::kCancelled);
  EXPECT(!f.sys.store.FindTicketOf(r1.registration_id)->valid);
  EXPECT(f.Reg(r2.registration_id) == nullptr);
  EXPECT(f.sys.store.FindTicketOf(r2.registration_id) == nullptr);
  EXPECT(!f.sys.activities.CancelActivity(f.org, f.activity, "再次", &impact)
             .ok());
}

void TestChangeRequest() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  const EventSession* s = f.sys.store.FindSession(f.session);
  ChangeRequest change;
  change.activity_id = f.activity;
  change.session_id = f.session;
  change.new_location = "体育馆";
  change.new_start_at = s->start_at;
  change.new_end_at = s->end_at;
  change.new_reg_close_at = s->reg_close_at;
  change.new_checkin_start_at = s->checkin_start_at;
  change.capacities.push_back({f.type, 0});
  Id change_id = 0;
  EXPECT(!f.sys.activities.RequestChange(f.org, change, &change_id).ok());
  change.capacities[0].capacity = 2;
  EXPECT(f.sys.activities.RequestChange(f.org, change, &change_id).ok());
  EXPECT(!f.sys.activities.RequestChange(f.org, change, nullptr).ok());
  // 审核前仍使用原配置。
  EXPECT(f.sys.store.FindSession(f.session)->location == "礼堂");
  EXPECT(f.sys.reviews.EstimatePromotion(
            *f.sys.store.FindChangeRequest(change_id)) == 1);
  int promoted = 0;
  EXPECT(f.sys.reviews.ApproveChange(f.admin, change_id, &promoted).ok());
  EXPECT(promoted == 1);
  EXPECT(f.sys.store.FindSession(f.session)->location == "体育馆");
  EXPECT(f.Reg(r2.registration_id)->status == RegStatus::kConfirmed);
  EXPECT(f.sys.store.FindActivity(f.activity)->version == 2);
  EXPECT(!f.sys.reviews.ApproveChange(f.admin, change_id, &promoted).ok());
}

void TestCommentsAndRatings() {
  Fixture f;
  f.Publish(5);
  Id guest_comment = 0, official = 0, mine = 0;
  EXPECT(!f.sys.comments.Post(Actor(), f.activity, 0, "", "你好", &mine).ok());
  EXPECT(f.sys.comments.Post(Actor(), f.activity, 0, "路人", "你好",
                            &guest_comment).ok());
  EXPECT(f.sys.store.FindComment(guest_comment)->badge == Badge::kGuest);
  EXPECT(f.sys.comments.Post(f.org, f.activity, guest_comment, "", "欢迎",
                            &official).ok());
  EXPECT(f.sys.store.FindComment(official)->badge == Badge::kOfficial);
  EXPECT(!f.sys.comments.Post(f.people[0], f.activity, 0, "", "   ", &mine)
             .ok());
  EXPECT(f.sys.comments.Post(f.people[0], f.activity, 0, "", "期待", &mine)
            .ok());
  EXPECT(!f.sys.comments.Like(f.people[0], mine).ok());  // 不能赞自己
  EXPECT(f.sys.comments.Like(f.people[1], mine).ok());
  EXPECT(f.sys.comments.Like(f.people[1], mine).ok());
  EXPECT(f.sys.comments.LikeCount(mine) == 1);  // 重复点赞只计一次
  EXPECT(f.sys.comments.Replies(guest_comment).size() == 1);
  // 活动结束前不能评分。
  EXPECT(!f.sys.comments.Rate(f.people[0], f.activity, 5).ok());
  f.sys.clock.Set(MakeTime(2026, 11, 5, 21, 0));
  f.sys.scheduler.RunDue();
  EXPECT(!f.sys.comments.Rate(f.people[0], f.activity, 6).ok());
  EXPECT(f.sys.comments.Rate(f.people[0], f.activity, 5).ok());
  EXPECT(f.sys.comments.Rate(f.people[1], f.activity, 4).ok());
  EXPECT(f.sys.comments.Rate(f.people[2], f.activity, 4).ok());
  EXPECT(f.sys.comments.Rate(f.people[2], f.activity, 4).ok());
  double average = 0;
  int count = 0;
  EXPECT(f.sys.comments.Average(f.activity, &average, &count));
  EXPECT(count == 3 && average > 4.33 && average < 4.34);
}

void TestFavoritesAndStats() {
  Fixture f;
  const Id first = f.Publish(2);
  const Id second = f.Publish(2);
  EXPECT(f.sys.favorites.Add(f.people[0], first).ok());
  f.sys.clock.Advance(1);
  EXPECT(f.sys.favorites.Add(f.people[0], second).ok());
  EXPECT(f.sys.favorites.Add(f.people[0], second).ok());
  std::vector<const Favorite*> list = f.sys.favorites.ListMine(f.people[0]);
  EXPECT(list.size() == 2 && list[0]->activity_id == second);
  EXPECT(f.sys.favorites.Remove(f.people[0], second).ok());
  EXPECT(f.sys.favorites.ListMine(f.people[0]).size() == 1);

  RegisterResult r;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r);
  f.sys.registrations.Register(f.people[2], f.session, f.type, &r);
  Counts counts;
  EXPECT(f.sys.stats.ActivityCounts(f.org, second, &counts).ok());
  EXPECT(counts.capacity == 2 && counts.occupied() == 2 &&
        counts.waiting == 1);
  EXPECT(counts.checkin_rate() == 0.0);
  EXPECT(!f.sys.stats.ActivityCounts(f.org2, second, &counts).ok());
  EXPECT(!f.sys.stats.PlatformCounts(f.org, &counts).ok());
  std::vector<RosterRow> rows;
  EXPECT(f.sys.stats.Roster(f.admin, second, &rows).ok());
  EXPECT(rows.size() == 3);  // 名单包含候补
}

void TestPersistence() {
  Fixture f;
  f.Publish(1);
  RegisterResult r1, r2;
  f.sys.registrations.Register(f.people[0], f.session, f.type, &r1);
  f.sys.registrations.Register(f.people[1], f.session, f.type, &r2);
  const std::string path = ExecutableDir() + "/core_test_data.txt";
  EXPECT(f.sys.store.Save(path).ok());
  System reopened(0);
  EXPECT(reopened.store.Load(path).ok());
  const Data& a = f.sys.store.data();
  const Data& b = reopened.store.data();
  EXPECT(a.registrations.size() == b.registrations.size());
  EXPECT(a.tickets.size() == b.tickets.size());
  EXPECT(a.messages.size() == b.messages.size());
  EXPECT(a.logs.size() == b.logs.size());
  EXPECT(b.messages.front().content == a.messages.front().content);
  EXPECT(reopened.store.FindTicketOf(r1.registration_id)->code ==
        f.Code(r1.registration_id));
  Actor login;
  EXPECT(reopened.accounts.Login("alice", "123456", &login).ok());
  EXPECT(!reopened.accounts.Login("alice", "wrong!", &login).ok());
  EXPECT(RemoveFile(path));
}

void TestSeedData() {
  System sys(MakeTime(2026, 11, 1, 10, 0));
  sys.Seed();
  // 演示数据应覆盖各种状态。
  int published = 0, ended = 0, cancelled = 0, pending = 0, draft = 0;
  for (const Activity& activity : sys.store.data().activities) {
    switch (activity.status) {
      case ActivityStatus::kPublished: ++published; break;
      case ActivityStatus::kEnded: ++ended; break;
      case ActivityStatus::kCancelled: ++cancelled; break;
      case ActivityStatus::kPending: ++pending; break;
      case ActivityStatus::kDraft: ++draft; break;
    }
  }
  EXPECT(published == 4 && ended == 1 && cancelled == 1 && pending == 1 &&
        draft == 1);
  EXPECT(sys.store.data().checkins.size() == 3);
  EXPECT(sys.store.data().change_requests.size() == 1);
  Actor alice;
  EXPECT(sys.accounts.Login("alice", "123456", &alice).ok());
  EXPECT(sys.notifier.UnreadCount(alice) >= 1);
}

}  // namespace
}  // namespace ticketing

int main() {
  // 不缓冲输出，程序中途出错时也能看到已经完成的用例。
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  using ticketing::g_checks;
  using ticketing::g_failures;
  struct Case {
    const char* name;
    void (*run)();
  };
  const Case cases[] = {
      {"时间解析与格式化", ticketing::TestTime},
      {"草稿校验", ticketing::TestDraftValidation},
      {"申办审核流程", ticketing::TestReviewFlow},
      {"报名、重复报名与候补", ticketing::TestRegisterAndWaitlist},
      {"退票与自动递补", ticketing::TestCancelPromotesWaiting},
      {"检票与重复检票", ticketing::TestCheckin},
      {"定时调度：候补失效与缺席", ticketing::TestSchedulerMarksAbsent},
      {"取消活动清空候补", ticketing::TestCancelActivityClearsWaitlist},
      {"修改申请与扩容递补", ticketing::TestChangeRequest},
      {"评论、点赞与评分", ticketing::TestCommentsAndRatings},
      {"收藏与统计", ticketing::TestFavoritesAndStats},
      {"数据保存与读取", ticketing::TestPersistence},
      {"演示数据", ticketing::TestSeedData},
  };
  for (const Case& c : cases) {
    const int before = g_failures;
    c.run();
    std::printf("[%s] %s\n", g_failures == before ? "通过" : "失败", c.name);
  }
  std::printf("共 %d 项检查，失败 %d 项\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
