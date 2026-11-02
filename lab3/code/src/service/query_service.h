// Copyright 2026 R
// M4 活动查询：分类、日期、关键词、状态组合查询（REQ-006）。
#ifndef SERVICE_QUERY_SERVICE_H_
#define SERVICE_QUERY_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "data/store.h"
#include "domain/model.h"

namespace ticketing {

struct ActivityFilter {
  std::string category;  // 空表示全部分类
  Time from = 0;         // 0 表示不限；按场次开始时间判断
  Time to = 0;           // 0 表示不限；含当天整天
  std::string keyword;   // 在名称、介绍、地点中查找
  int status = -1;       // -1 表示全部，否则为 ActivityStatus 的取值
};

class QueryService {
 public:
  explicit QueryService(Store* store) : store_(store) {}

  // 返回 actor 有权查看且满足全部条件的活动。
  // 排序：未结束、未取消的在前，再按最早场次开始时间升序。
  // 异常：from 晚于 to 时返回错误，*out 保持为空。
  Status Search(const Actor& actor, const ActivityFilter& filter,
                std::vector<const Activity*>* out) const;

  // 活动的全部分类（去重，按出现顺序）。
  std::vector<std::string> Categories() const;
  // 活动最早场次的开始时间；没有场次时为 0。
  Time FirstStart(Id activity_id) const;

 private:
  Store* store_;
};

}  // namespace ticketing

#endif  // SERVICE_QUERY_SERVICE_H_
