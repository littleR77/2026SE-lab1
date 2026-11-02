// Copyright 2026 R
#include "service/system.h"

#include <string>
#include <vector>

#include "common/file_util.h"

namespace ticketing {
namespace {

constexpr char kDemoPassword[] = "123456";

SessionDraft MakeSession(const std::string& location, Time start, Time end,
                         Time reg_open, Time reg_close, Time checkin_start,
                         const std::vector<TicketTypeDraft>& types) {
  SessionDraft session;
  session.location = location;
  session.start_at = start;
  session.end_at = end;
  session.reg_open_at = reg_open;
  session.reg_close_at = reg_close;
  session.checkin_start_at = checkin_start;
  session.ticket_types = types;
  return session;
}

}  // namespace

System::System(Time now)
    : clock(now),
      audit(&store, &clock),
      notifier(&store, &clock),
      accounts(&store, &audit),
      registrations(&store, &clock, &audit, &notifier),
      activities(&store, &clock, &audit, &registrations),
      reviews(&store, &clock, &audit, &registrations),
      queries(&store),
      checkins(&store, &clock, &audit),
      comments(&store, &clock, &audit),
      favorites(&store, &clock, &audit),
      stats(&store),
      scheduler(&store, &clock, &audit, &registrations) {
  // 各模块通过指针共享同一份数据、时钟和日志，构造顺序即依赖顺序。
}

Status System::Open(const std::string& path) {
  path_ = path;
  if (FileExists(path)) {
    Status loaded = store.Load(path);
    if (!loaded.ok()) return loaded;
    if (store.data().clock_now != 0) clock.Set(store.data().clock_now);
  } else {
    Seed();
  }
  scheduler.RunDue();
  return Persist();
}

Status System::Persist() {
  if (path_.empty()) return Status::Ok();
  store.data().clock_now = clock.Now();
  return store.SaveIfDirty(path_);
}

SchedulerReport System::AdvanceClock(Time minutes) {
  clock.Advance(minutes);
  store.data().clock_now = clock.Now();
  store.MarkDirty();
  return scheduler.RunDue();
}

void System::Seed() {
  // 演示数据全部通过业务接口写入，因此操作日志、消息和票券都是真实生成的。
  const Time demo_now = clock.Now();
  clock.Set(MakeTime(2026, 10, 10, 9, 0));

  Id admin_id = 0, org_id = 0, org2_id = 0, checker_id = 0, checker2_id = 0;
  Id alice = 0, bob = 0, carol = 0, dave = 0;
  accounts.Register("admin", "平台管理员", kDemoPassword, Role::kAdmin,
                    &admin_id);
  accounts.Register("org", "文艺部", kDemoPassword, Role::kOrganizer, &org_id);
  accounts.Register("org2", "陶艺社", kDemoPassword, Role::kOrganizer,
                    &org2_id);
  accounts.Register("checker", "阿杰", kDemoPassword, Role::kChecker,
                    &checker_id);
  accounts.Register("checker2", "小林", kDemoPassword, Role::kChecker,
                    &checker2_id);
  accounts.Register("alice", "小雨", kDemoPassword, Role::kParticipant, &alice);
  accounts.Register("bob", "小鹿", kDemoPassword, Role::kParticipant, &bob);
  accounts.Register("carol", "林同学", kDemoPassword, Role::kParticipant,
                    &carol);
  accounts.Register("dave", "阿明", kDemoPassword, Role::kParticipant, &dave);
  const Actor admin{admin_id, Role::kAdmin};
  const Actor org{org_id, Role::kOrganizer};
  const Actor org2{org2_id, Role::kOrganizer};
  const Actor checker{checker_id, Role::kChecker};
  const Actor a{alice, Role::kParticipant};
  const Actor b{bob, Role::kParticipant};
  const Actor c{carol, Role::kParticipant};
  const Actor d{dave, Role::kParticipant};
  const Time open = MakeTime(2026, 10, 10, 10, 0);

  // 发布一个活动：创建草稿、提交、审核通过。
  auto publish = [&](const Actor& owner, ActivityDraft draft) {
    Id id = 0;
    activities.CreateDraft(owner, draft, &id);
    activities.Submit(owner, id);
    reviews.Approve(admin, id, store.FindActivity(id)->version);
    return id;
  };
  auto first_session = [&](Id activity_id) {
    return store.SessionsOf(activity_id).front()->id;
  };
  auto type_of = [&](Id session_id, size_t index) {
    return store.TicketTypesOf(session_id)[index]->id;
  };
  RegisterResult result;

  // 1. 已结束的活动：有签到、缺席、评分和评论。
  ActivityDraft piano;
  piano.title = "钢琴社秋季音乐分享";
  piano.category = "演出";
  piano.description = "钢琴社成员轮流演奏秋季曲目，演出后有交流环节。";
  piano.sessions.push_back(MakeSession(
      "琴房", MakeTime(2026, 10, 25, 19, 0), MakeTime(2026, 10, 25, 21, 0),
      open, MakeTime(2026, 10, 25, 12, 0), MakeTime(2026, 10, 25, 18, 30),
      {{0, "普通观众票", 40}}));
  const Id piano_id = publish(org, piano);
  const Id piano_session = first_session(piano_id);

  // 2. 草坪音乐会：两个场次，前排区名额少，用来演示候补和扩容。
  ActivityDraft lawn;
  lawn.title = "草坪音乐会 · 秋日限定";
  lawn.category = "演出";
  lawn.description =
      "带上野餐垫，在秋天的晚风里听一场户外音乐会。乐队、民谣与合唱轮番登场，"
      "现场提供热饮。请凭正式票码入场，候补凭证不能入场。";
  lawn.sessions.push_back(MakeSession(
      "东区大草坪", MakeTime(2026, 11, 5, 19, 0), MakeTime(2026, 11, 5, 21, 0),
      open, MakeTime(2026, 11, 5, 12, 0), MakeTime(2026, 11, 5, 18, 30),
      {{0, "普通观众票", 180}, {0, "草坪前排区", 2}}));
  lawn.sessions.push_back(MakeSession(
      "东区大草坪", MakeTime(2026, 11, 6, 19, 0), MakeTime(2026, 11, 6, 21, 0),
      open, MakeTime(2026, 11, 6, 12, 0), MakeTime(2026, 11, 6, 18, 30),
      {{0, "普通观众票", 180}}));
  const Id lawn_id = publish(org, lawn);
  const Id lawn_session = first_session(lawn_id);

  // 3. 陶艺工作坊：名额已满，有两人候补。
  ActivityDraft pottery;
  pottery.title = "陶艺手作工作坊";
  pottery.category = "工作坊";
  pottery.description = "从揉泥到拉坯，老师手把手带你做一只自己的杯子。";
  pottery.sessions.push_back(MakeSession(
      "艺术楼 201", MakeTime(2026, 11, 8, 10, 0), MakeTime(2026, 11, 8, 12, 0),
      open, MakeTime(2026, 11, 7, 20, 0), MakeTime(2026, 11, 8, 9, 30),
      {{0, "体验名额", 2}}));
  const Id pottery_id = publish(org2, pottery);
  const Id pottery_session = first_session(pottery_id);

  // 4. 今天中午的音乐会：把时间推进两小时即可演示检票。
  ActivityDraft strings;
  strings.title = "弦乐四重奏午间音乐会";
  strings.category = "演出";
  strings.description = "午休时间的一小时弦乐，曲目包括海顿和德沃夏克。";
  strings.sessions.push_back(MakeSession(
      "音乐厅", MakeTime(2026, 11, 1, 12, 0), MakeTime(2026, 11, 1, 13, 0),
      open, MakeTime(2026, 11, 1, 11, 0), MakeTime(2026, 11, 1, 11, 30),
      {{0, "普通观众票", 120}}));
  const Id strings_id = publish(org, strings);
  const Id strings_session = first_session(strings_id);

  // 5. 社团开放日。
  ActivityDraft stars;
  stars.title = "星空摄影社开放日";
  stars.category = "社团";
  stars.description = "参观天文台，学习用手机拍星轨。";
  stars.sessions.push_back(MakeSession(
      "天文台", MakeTime(2026, 11, 7, 20, 0), MakeTime(2026, 11, 7, 22, 0),
      open, MakeTime(2026, 11, 7, 18, 0), MakeTime(2026, 11, 7, 19, 30),
      {{0, "参观名额", 50}}));
  publish(org2, stars);

  // 6. 将被取消的活动。
  ActivityDraft brass;
  brass.title = "管乐团户外快闪";
  brass.category = "演出";
  brass.description = "管乐团在中心广场的快闪演出。";
  brass.sessions.push_back(MakeSession(
      "中心广场", MakeTime(2026, 10, 30, 16, 0), MakeTime(2026, 10, 30, 17, 0),
      open, MakeTime(2026, 10, 30, 12, 0), MakeTime(2026, 10, 30, 15, 30),
      {{0, "观众名额", 1}}));
  const Id brass_id = publish(org, brass);
  const Id brass_session = first_session(brass_id);

  activities.AssignChecker(org, piano_session, checker_id);
  activities.AssignChecker(org, strings_session, checker_id);
  activities.AssignChecker(org, lawn_session, checker_id);
  activities.AssignChecker(org2, pottery_session, checker2_id);

  clock.Set(MakeTime(2026, 10, 12, 10, 0));
  const Id piano_type = type_of(piano_session, 0);
  registrations.Register(a, piano_session, piano_type, &result);
  registrations.Register(b, piano_session, piano_type, &result);
  registrations.Register(c, piano_session, piano_type, &result);
  registrations.Register(d, piano_session, piano_type, &result);

  // 钢琴社活动当晚检票，阿明缺席，场次结束后活动自动结束。
  clock.Set(MakeTime(2026, 10, 25, 18, 40));
  for (const Actor& who : {a, b, c}) {
    for (const Registration* reg : registrations.ListMine(who)) {
      if (reg->session_id == piano_session) {
        checkins.Verify(checker, piano_session,
                        store.FindTicketOf(reg->id)->code);
      }
    }
  }
  clock.Set(MakeTime(2026, 10, 25, 21, 0));
  scheduler.RunDue();

  clock.Set(MakeTime(2026, 10, 26, 9, 30));
  comments.Rate(a, piano_id, 5);
  comments.Rate(b, piano_id, 4);
  comments.Rate(c, piano_id, 5);
  Id piano_comment = 0;
  comments.Post(c, piano_id, 0, "", "第二首《秋日私语》太好听了，期待下一次！",
                &piano_comment);
  comments.Post(org, piano_id, piano_comment, "", "谢谢支持，冬季专场见～",
                nullptr);

  // 草坪音乐会、陶艺工作坊和管乐团的报名。
  clock.Set(MakeTime(2026, 10, 28, 20, 0));
  registrations.Register(a, lawn_session, type_of(lawn_session, 0), &result);
  registrations.Register(b, lawn_session, type_of(lawn_session, 1), &result);
  registrations.Register(c, lawn_session, type_of(lawn_session, 1), &result);
  registrations.Register(d, lawn_session, type_of(lawn_session, 1), &result);
  const Id pottery_type = type_of(pottery_session, 0);
  registrations.Register(c, pottery_session, pottery_type, &result);
  registrations.Register(d, pottery_session, pottery_type, &result);
  registrations.Register(b, pottery_session, pottery_type, &result);
  registrations.Register(a, pottery_session, pottery_type, &result);
  const Id brass_type = type_of(brass_session, 0);
  registrations.Register(a, brass_session, brass_type, &result);
  registrations.Register(b, brass_session, brass_type, &result);
  registrations.Register(c, strings_session, type_of(strings_session, 0),
                         &result);
  registrations.Register(a, strings_session, type_of(strings_session, 0),
                         &result);

  Id lawn_comment = 0;
  comments.Post(c, lawn_id, 0, "",
                "去年的草坪音乐会超棒！建议早点到占前排位置，记得带外套。",
                &lawn_comment);
  comments.Post(org, lawn_id, lawn_comment, "",
                "今年现场会提供免费热饮，入口在东区南门。", nullptr);
  comments.Like(a, lawn_comment);
  comments.Like(b, lawn_comment);
  comments.Post(Actor(), lawn_id, 0, "路过的云朵", "校外人员可以来听吗？",
                nullptr);

  clock.Set(MakeTime(2026, 10, 29, 9, 0));
  favorites.Add(a, brass_id);
  favorites.Add(a, strings_id);
  favorites.Add(a, lawn_id);
  CancelImpact impact;
  activities.CancelActivity(org, brass_id, "天气原因，演出取消", &impact);

  // 被驳回后回到草稿的活动。
  clock.Set(MakeTime(2026, 10, 30, 15, 0));
  ActivityDraft singer;
  singer.title = "校园歌手大赛决赛";
  singer.category = "演出";
  singer.description = "十强选手同台竞演，现场观众投票。";
  singer.sessions.push_back(MakeSession(
      "大学生活动中心", MakeTime(2026, 11, 12, 18, 30),
      MakeTime(2026, 11, 12, 21, 0), MakeTime(2026, 11, 2, 9, 0),
      MakeTime(2026, 11, 12, 12, 0), MakeTime(2026, 11, 12, 18, 0),
      {{0, "普通观众票", 300}}));
  Id singer_id = 0;
  activities.CreateDraft(org, singer, &singer_id);
  activities.Submit(org, singer_id);
  reviews.Reject(admin, singer_id,
                 "11/12 场次与学院活动时间冲突，请调整场次时间后重新提交");

  // 待审核的讲座和待审核的扩容申请。
  clock.Set(MakeTime(2026, 10, 31, 10, 0));
  ActivityDraft lecture;
  lecture.title = "AI 与未来教育";
  lecture.category = "讲座";
  lecture.description = "邀请教育学院老师谈大模型进入课堂后的变化。";
  lecture.sessions.push_back(MakeSession(
      "图书馆报告厅", MakeTime(2026, 11, 6, 14, 0),
      MakeTime(2026, 11, 6, 16, 0), MakeTime(2026, 11, 2, 9, 0),
      MakeTime(2026, 11, 6, 12, 0), MakeTime(2026, 11, 6, 13, 30),
      {{0, "听众", 200}}));
  Id lecture_id = 0;
  activities.CreateDraft(org, lecture, &lecture_id);
  activities.Submit(org, lecture_id);

  const EventSession* session = store.FindSession(lawn_session);
  ChangeRequest change;
  change.activity_id = lawn_id;
  change.session_id = lawn_session;
  change.new_location = session->location;
  change.new_start_at = session->start_at;
  change.new_end_at = session->end_at;
  change.new_reg_close_at = session->reg_close_at;
  change.new_checkin_start_at = session->checkin_start_at;
  change.capacities.push_back({type_of(lawn_session, 0), 180});
  change.capacities.push_back({type_of(lawn_session, 1), 3});
  activities.RequestChange(org, change, nullptr);

  clock.Set(demo_now);
  scheduler.RunDue();
  store.MarkDirty();
}

}  // namespace ticketing
