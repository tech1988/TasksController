#include "TasksController.h"

#include <thread>
#include <charconv>

#ifdef WIN32
#include <Windows.h>
#endif

using Now = std::chrono::system_clock::time_point;
using namespace std::chrono;

static hours getTimeZone()
{
#ifdef WIN32
    TIME_ZONE_INFORMATION timeZoneInfo;
    if(GetTimeZoneInformation(&timeZoneInfo) == TIME_ZONE_ID_INVALID) return hours(0);
    return hours(-(timeZoneInfo.Bias/60));
#else
       return hours(0);
#endif
}

static inline system_clock::time_point GetFromNow()
{
    return system_clock::now() + getTimeZone();
}

static inline system_clock::time_point GetFromDate(const unsigned char day, const unsigned char month, const unsigned int year)
{
    return system_clock::time_point{sys_days{year_month_day{::year(year), ::month(month), ::day(day)}}.time_since_epoch()};
}

static inline system_clock::time_point GetFromWeekDate(const unsigned char index, const unsigned char weekday, const month month, const year year)
{
    return {sys_days{year_month_weekday{year, month, weekday_indexed(::weekday(weekday), index)}}};
}

static inline system_clock::time_point GetOnlyDateFromPoint(const Now & now)
{
    return system_clock::time_point{sys_days{year_month_day{floor<days>(now)}}.time_since_epoch()};
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------Day------------------------------

static void dayPattern(std::function<bool(const Now &, bool)> & calculate, const unsigned char day, const system_clock::time_point & now, const seconds & sum)
{
    int year;
    unsigned char month;

    {
      year_month_day ymd(floor<days>(now));
      year = static_cast<int>(ymd.year());
      month = static_cast<unsigned>(ymd.month());
    }

    while(!month_day(::month(month), ::day(day)).ok()) month++;
    system_clock::time_point finish = GetFromDate(day, month, year) + sum;

    if(now > finish)
    {
       month++;
       if(month <= 12)
       {
          while(!month_day(::month(month), ::day(day)).ok()) month++;
          finish = GetFromDate(day, month, year) + sum;
       }
       else finish = GetFromDate(day, 1, year + 1) + sum;
    }

    calculate = [day, sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {          
          int year;
          unsigned char month;

          {
            year_month_day ymd(floor<days>(now));
            year = static_cast<int>(ymd.year());
            month = static_cast<unsigned>(ymd.month()) + 1;
          }

          if(month <= 12)
          {
             while(!month_day(::month(month), ::day(day)).ok()) month++;
             finish = GetFromDate(day, month, year) + sum;
          }
          else finish = GetFromDate(day, 1, year + 1) + sum;

          return !recalc;
       }
       return false;
    };
}

static void dayMonthPattern(std::function<bool(const Now &, bool)> & calculate, const unsigned char day, const unsigned char month, const system_clock::time_point & now, const seconds & sum)
{
    int year = static_cast<int>(year_month_day(floor<days>(now)).year());
    while(!year_month_day{::year(year), ::month(month), ::day(day)}.ok()) year++;
    system_clock::time_point finish = GetFromDate(day, month, year) + sum;

    if(now > finish)
    {
       year++;
       while(!year_month_day{::year(year), ::month(month), ::day(day)}.ok()) year++;
       finish = GetFromDate(day, month, year) + sum;
    }

    calculate = [day, month, sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          int year = static_cast<int>(year_month_day(floor<days>(now)).year()) + 1;
          while(!year_month_day{::year(year), ::month(month), ::day(day)}.ok()) year++;
          finish = GetFromDate(day, month, year) + sum;

          return !recalc;
       }
       return false;
    };
}

//------------------Only Weekday--------------------------

static void weekdayPattern(std::function<bool(const Now &, bool)> & calculate, const unsigned char weekday, const system_clock::time_point & now, const seconds & sum)
{
    const unsigned char c_weekday = (weekday == 7) ? 0 : weekday;
    year_month_weekday cw{floor<days>(now)};
    system_clock::time_point finish = GetFromWeekDate(cw.weekday_indexed().index(), c_weekday, cw.month(), cw.year()) + sum;

    if(now > finish) finish = GetFromWeekDate(cw.weekday_indexed().index() + 1, c_weekday, cw.month(), cw.year()) + sum;

    calculate = [c_weekday, sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          year_month_weekday cw{floor<days>(now)};
          finish = GetFromWeekDate(cw.weekday_indexed().index(), c_weekday, cw.month(), cw.year()) + sum;
          if(now > finish) finish = GetFromWeekDate(cw.weekday_indexed().index() + 1, c_weekday, cw.month(), cw.year()) + sum;

          return !recalc;
       }
       return false;
    };
}

//------------------Only Month----------------------------

static void monthPattern(std::function<bool(const Now &, bool)> & calculate, const unsigned char month, const system_clock::time_point & now, const seconds & sum)
{
    year_month_day ymd(floor<days>(now));
    system_clock::time_point finish = GetFromDate(1, month, static_cast<int>(ymd.year())) + sum;

    if(now > finish) finish = GetFromDate(1, month, static_cast<int>(ymd.year()) + 1) + sum;

    calculate = [month, sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          year_month_day ymd(floor<days>(now));
          finish = GetFromDate(1, month, static_cast<int>(ymd.year()) + 1) + sum;

          return !recalc;
       }
       return false;
    };
}

//------------------Only Time-----------------------------

static void hoursPattern(std::function<bool(const Now &, bool)> & calculate, const system_clock::time_point & now, const seconds & sum)
{
    system_clock::time_point finish = GetOnlyDateFromPoint(now) + sum;

    if(now > finish) finish += days(1);

    calculate = [sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          finish = GetOnlyDateFromPoint(now) + days(1) + sum;

          return !recalc;
       }
       return false;
    };
}

static void minutesPattern(std::function<bool(const Now &, bool)> & calculate, const system_clock::time_point & now, const seconds & sum)
{
    auto onlyDate = GetOnlyDateFromPoint(now);
    hh_mm_ss time(now - onlyDate);
    system_clock::time_point finish = onlyDate + time.hours() + sum;

    if(now > finish) finish += ::hours(1);

    calculate = [sum, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          system_clock::time_point date = GetOnlyDateFromPoint(now);
          hh_mm_ss time(now - date);
          finish = date + time.hours() + ::hours(1) + sum;

          return !recalc;
       }
       return false;
    };
}

static bool onlyTimePattern(std::function<bool(const Now &, bool)> & calculate,
                            const unsigned char seconds,
                            const unsigned char minutes,
                            const unsigned char hours,
                            const system_clock::time_point & now,
                            bool isZeroHour,
                            bool isZeroMinute,
                            bool isZeroSecond)
{
    if(hours > 0 || isZeroHour)
    {
       hoursPattern(calculate, now, ::seconds(seconds) + ::minutes(minutes) + ::hours(hours));
       return true;
    }

    if(minutes > 0 || isZeroMinute)
    {
       minutesPattern(calculate, now, ::seconds(seconds) + ::minutes(minutes));
       return true;
    }

    if(seconds > 0 || isZeroSecond)
    {
       ::seconds s_seconds(seconds);
       auto onlyDate = GetOnlyDateFromPoint(now);
       hh_mm_ss time(now - onlyDate);
       system_clock::time_point finish = onlyDate + time.hours() + time.minutes() + s_seconds;

       if(now > finish) finish += ::minutes(1);

       calculate = [s_seconds, finish](const Now & now, bool recalc) mutable
       {
          if(now > finish || recalc)
          {
             system_clock::time_point date = GetOnlyDateFromPoint(now);
             hh_mm_ss time(now - date);
             finish = date + time.hours() + time.minutes() + ::minutes(1) + s_seconds;

             return !recalc;
          }
          return false;
       };

       return true;
    }

    return false;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Task::Task(){}

Task::Task(std::string_view value){ parseFromString(value); }

bool Task::isValid() const
{
    return (calculate) ? true : false;
}

bool Task::taskCalculate(const Now &now, bool recalc) const
{
    if(calculate) return calculate(now, recalc);
    return false;
}

Task::Type Task::taskType() const
{
    return type;
}

bool Task::isSingle() const
{
    return (type == SinglePoint || type == SingleInterval);
}

bool Task::pointDayTaskInit(const unsigned char seconds,
                            const unsigned char minutes,
                            const unsigned char hours,
                            const unsigned char day,
                            const unsigned char month)
{
    calculate = nullptr;
    type = None;

    //==============================================

    if(seconds > 60 || minutes > 60 || hours > 24 || day > 31 || month > 12) return false;
    if(seconds == 0 && minutes == 0 && hours == 0 && day == 0 && month == 0) return false;

    unsigned char h = hours, m = minutes, s = seconds;
    bool isZeroHour = false, isZeroMinute = false, isZeroSecond = false;

    if(hours == 24)
    {
       isZeroHour = true;
       h = 0;
    }

    if(minutes == 60)
    {
       isZeroMinute = true;
       m = 0;
    }

    if(seconds == 60)
    {
       isZeroSecond = true;
       s = 0;
    }

    //==============================================

    type = Point;

    //----------------

    system_clock::time_point now = GetFromNow();

    //----------------

    if(day > 0 && month == 0)
    {
       dayPattern(calculate, day, now, ::seconds(s) + ::minutes(m) + ::hours(h));
       return true;
    }

    if(day == 0 && month > 0)
    {
       monthPattern(calculate, month, now, ::seconds(s) + ::minutes(m) + ::hours(h));
       return true;
    }

    if(day > 0 && month > 0)
    {
       if(!month_day{::month(month), ::day(day)}.ok()) return false;
       dayMonthPattern(calculate, day, month, now, ::seconds(s) + ::minutes(m) + ::hours(h));
       return true;
    }

    if(onlyTimePattern(calculate, s, m, h, now, isZeroHour, isZeroMinute, isZeroSecond)) return true;

    type = None;

    return false;
}

bool Task::singlePointDayTaskInit(const unsigned char seconds,
                                  const unsigned char minutes,
                                  const unsigned char hours,
                                  const unsigned char day,
                                  const unsigned char month)
{
    if(pointDayTaskInit(seconds, minutes, hours, day, month))
    {
       type = SinglePoint;
       return true;
    }

    return false;
}


bool Task::pointWeekTaskInit(const unsigned char seconds,
                             const unsigned char minutes,
                             const unsigned char hours,
                             const unsigned char weekday)
{
    calculate = nullptr;
    type = None;

    //==============================================

    if(seconds >= 60 || minutes >= 60 || hours >= 24 || weekday > 7) return false;
    if(seconds == 0 && minutes == 0 && hours == 0 && weekday == 0) return false;

    //==============================================

    type = Point;

    //----------------

    system_clock::time_point now = GetFromNow();

    //----------------

    if(weekday > 0)
    {
       weekdayPattern(calculate, weekday, now, ::seconds(seconds) + ::minutes(minutes) + ::hours(hours));
       return true;
    }

    if(onlyTimePattern(calculate, seconds, minutes, hours, now, false, false, false)) return true;

    type = None;

    return false;
}

bool Task::singlePointWeekTaskInit(const unsigned char seconds,
                                   const unsigned char minutes,
                                   const unsigned char hours,
                                   const unsigned char weekday)
{
    if(pointWeekTaskInit(seconds, minutes, hours, weekday))
    {
       type = SinglePoint;
       return true;
    }

    return false;
}

bool Task::intervalTaskInit(const unsigned char seconds,
                            const unsigned char minutes,
                            const unsigned char hours,
                            const unsigned short days)
{
    calculate = nullptr;
    type = None;

    //==============================================

    if(seconds >= 60 || minutes >= 60 || hours >= 24) return false;
    if(seconds == 0 && minutes == 0 && hours == 0 && days == 0) return false;

    //==============================================

    type = Point;

    const ::seconds interval = ::seconds(seconds) + ::minutes(minutes) + ::hours(hours) + ::days(days);
    system_clock::time_point finish = GetFromNow() + interval;

    calculate = [interval, finish](const Now & now, bool recalc) mutable
    {
       if(now > finish || recalc)
       {
          finish = now + interval;

          return !recalc;
       }
       else return false;
    };

    return true;
}

bool Task::singleIntervalTaskInit(const unsigned char seconds,
                                  const unsigned char minutes,
                                  const unsigned char hours,
                                  const unsigned short days)
{
    if(intervalTaskInit(seconds, minutes, hours, days))
    {
       type = SingleInterval;
       return true;
    }

    return false;
}

static bool parsePoint(int offset, std::string_view value, const std::function<bool(unsigned char,unsigned char,unsigned char,unsigned char,unsigned char)> & func)
{
    if(value[4 + offset] != '/' || value[7 + offset] != ' ' || value[10 + offset] != ':' || value[13 + offset] != ':') return false;

    std::string_view sd = value.substr(2 + offset,2);
    if(!std::isdigit(sd[0]) || !std::isdigit(sd[1])) return false;

    unsigned char day;
    std::from_chars(sd.begin(), sd.end(), day);

    std::string_view sm = value.substr(5 + offset,2);
    if(!std::isdigit(sm[0]) || !std::isdigit(sm[1])) return false;

    unsigned char month;
    std::from_chars(sm.begin(), sm.end(), month);

    std::string_view th = value.substr(8 + offset,2);
    if(!std::isdigit(th[0]) || !std::isdigit(th[1])) return false;

    unsigned char hours;
    std::from_chars(th.begin(), th.end(), hours);

    std::string_view tm = value.substr(11 + offset,2);
    if(!std::isdigit(tm[0]) || !std::isdigit(tm[1])) return false;

    unsigned char minutes;
    std::from_chars(tm.begin(), tm.end(), minutes);

    std::string_view ts = value.substr(14 + offset,2);
    if(!std::isdigit(ts[0]) || !std::isdigit(ts[1])) return false;

    unsigned char seconds;
    std::from_chars(ts.begin(), ts.end(), seconds);

    if(func) return func(seconds, minutes, hours, day, month);

    return false;
}

static bool parseInterval(int offset, std::string_view value, const std::function<bool(unsigned char,unsigned char,unsigned char,unsigned short)> & func)
{
    if(value[7 + offset] != ' ' || value[10 + offset] != ':' || value[13 + offset] != ':') return false;

    std::string_view sd = value.substr(2 + offset,5);
    if(!std::isdigit(sd[0]) || !std::isdigit(sd[1]) || !std::isdigit(sd[2]) || !std::isdigit(sd[3]) || !std::isdigit(sd[4])) return false;

    unsigned short days;
    std::from_chars(sd.begin(), sd.end(), days);

    std::string_view th = value.substr(8 + offset,2);
    if(!std::isdigit(th[0]) || !std::isdigit(th[1])) return false;

    unsigned char hours;
    std::from_chars(th.begin(), th.end(), hours);

    std::string_view tm = value.substr(11 + offset,2);
    if(!std::isdigit(tm[0]) || !std::isdigit(tm[1])) return false;

    unsigned char minutes;
    std::from_chars(tm.begin(), tm.end(), minutes);

    std::string_view ts = value.substr(14 + offset,2);
    if(!std::isdigit(ts[0]) || !std::isdigit(ts[1])) return false;

    unsigned char seconds;
    std::from_chars(ts.begin(), ts.end(), seconds);

    if(func) return func(seconds, minutes, hours, days);

    return false;
}

static bool parseWeek(int offset, std::string_view value, const std::function<bool(unsigned char,unsigned char,unsigned char,unsigned char)> & func)
{
    if(value[3 + offset] != ' ' || value[6 + offset] != ':' || value[9 + offset] != ':') return false;

    std::string_view swd = value.substr(2 + offset,1);

    if(!std::isdigit(swd[0])) return false;

    unsigned char weekday;
    std::from_chars(swd.begin(), swd.end(), weekday);

    std::string_view th = value.substr(4 + offset,2);
    if(!std::isdigit(th[0]) || !std::isdigit(th[1])) return false;

    unsigned char hours;
    std::from_chars(th.begin(), th.end(), hours);

    std::string_view tm = value.substr(7 + offset,2);
    if(!std::isdigit(tm[0]) || !std::isdigit(tm[1])) return false;

    unsigned char minutes;
    std::from_chars(tm.begin(), tm.end(), minutes);

    std::string_view ts = value.substr(10 + offset,2);
    if(!std::isdigit(ts[0]) || !std::isdigit(ts[1])) return false;

    unsigned char seconds;
    std::from_chars(ts.begin(), ts.end(), seconds);

    if(func) return func(seconds, minutes, hours, weekday);

    return false;
}

bool Task::parseFromString(std::string_view value)
{
    if(value.starts_with("P "))
    {
       if(value.size() != 16) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned char day, unsigned char month){
                       return pointDayTaskInit(seconds,minutes,hours,day,month); };

       return parsePoint(0, value, l);
    }
    else if(value.starts_with("SP "))
    {
       if(value.size() != 17) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned char day, unsigned char month){
                       return singlePointDayTaskInit(seconds,minutes,hours,day,month); };

       return parsePoint(1, value, l);
    }
    else if(value.starts_with("W "))
    {
       if(value.size() != 12) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned char weekday){
                       return pointWeekTaskInit(seconds,minutes,hours,weekday); };

       return parseWeek(0, value, l);
    }
    else if(value.starts_with("SW "))
    {
       if(value.size() != 13) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned char weekday){
                       return singlePointWeekTaskInit(seconds,minutes,hours,weekday); };

       return parseWeek(1, value, l);
    }
    else if(value.starts_with("I "))
    {
       if(value.size() != 16) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned short days){
                       return intervalTaskInit(seconds,minutes,hours,days); };

       return parseInterval(0,value,l);
    }
    else if(value.starts_with("SI "))
    {
       if(value.size() != 17) return false;

       auto l = [this](unsigned char seconds, unsigned char minutes, unsigned char hours, unsigned short days){
                       return singleIntervalTaskInit(seconds,minutes,hours,days); };

       return parseInterval(1,value,l);
    }

    return false;
}

//===============================================

TasksController::TasksController(){}

TasksController::TasksController(unsigned short accuracy)
{
    setAccuracy(accuracy);
}

bool TasksController::clearTasks()
{
    if(isrun.load()) return false;
    std::lock_guard<std::mutex>lock(mutex);
    tasks.clear();
    return true;
}

int TasksController::countTasks()
{
    std::lock_guard<std::mutex>lock(mutex);
    return tasks.size();
}

unsigned short TasksController::accuracy() const
{
    return _accuracy.load();
}

bool TasksController::setAccuracy(unsigned short ms)
{
    if(ms < 10) _accuracy = 10;
    else if(ms > 500) _accuracy = 500;
    else _accuracy = ms;

    return true;
}

bool TasksController::contains(const std::string & name)
{
    if(name.empty()) return false;
    std::lock_guard<std::mutex>lock(mutex);
    return tasks.contains(name);
}

bool TasksController::addTask(const std::string & name, std::string_view value)
{
    if(name.empty() || value.empty()) return false;

    Task task(value);
    if(!task.isValid()) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addTask(const std::string & name, std::string_view value, const std::function<void()> & callback)
{
    if(name.empty() || value.empty() || !callback) return false;

    Task task(value);
    if(!task.isValid()) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;
    pair.second.push_back(callback);

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addTask(const std::string & name, std::string_view value, const std::vector<std::function<void()>> & callbacks)
{
    if(name.empty() || value.empty() || callbacks.empty()) return false;

    Task task(value);
    if(!task.isValid()) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;

    for(auto & callback : callbacks)
    {
        if(!callback) return false;
        pair.second.push_back(callback);
    }

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addTask(const std::string & name, const Task & task)
{
    if(name.empty() || !task.isValid()) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addTask(const std::string & name, const Task & task, const std::function<void()> & callback)
{
    if(name.empty() || !task.isValid() || !callback) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;
    pair.second.push_back(callback);

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addTask(const std::string & name, const Task & task, const std::vector<std::function<void()>> & callbacks)
{
    if(name.empty() || !task.isValid() || callbacks.empty()) return false;

    std::pair<Task,std::vector<std::function<void()>>> pair;
    pair.first = task;

    for(auto & callback : callbacks)
    {
        if(!callback) return false;
        pair.second.push_back(callback);
    }

    std::lock_guard<std::mutex>lock(mutex);
    if(tasks.contains(name)) return false;

    tasks.insert({name, pair});

    return true;
}

bool TasksController::addCallback(const std::string & name, const std::function<void()> & callback)
{
    if(name.empty() || !callback) return false;

    std::lock_guard<std::mutex>lock(mutex);
    if(!tasks.contains(name)) return false;

    tasks.at(name).second.push_back(callback);

    return true;
}

bool TasksController::addCallbacks(const std::string & name, const std::vector<std::function<void()>> & callbacks)
{
    if(name.empty() || callbacks.empty()) return false;
    for(auto & callback : callbacks){ if(!callback) return false; }

    std::lock_guard<std::mutex>lock(mutex);
    if(!tasks.contains(name)) return false;

    auto & v = tasks.at(name);
    for(auto & callback : callbacks){ v.second.push_back(callback); }

    return true;
}

void TasksController::clearCallbacks(const std::string & name)
{
    std::lock_guard<std::mutex>lock(mutex);
    if(!tasks.contains(name)) return;
    tasks.at(name).second.clear();
}

bool TasksController::isRun() const
{
    return isrun.load();
}

void TasksController::run()
{
    if(tasks.size() == 0) return;

    isrun = true;

    do
    {
       std::this_thread::sleep_for(milliseconds(_accuracy.load()));

       if(!isrun.load()) return;

       Now now = GetFromNow();

       std::lock_guard<std::mutex>lock(mutex);

       for(auto begin = tasks.begin(); begin != tasks.end();)
       {
           auto & task = begin->second;

           if(task.first.taskCalculate(now, false)) //< --- when changing time, recalc - true
           {
              for(auto & func : task.second)
              {
                  func();
                  if(!isrun.load()) return;
              }

              if(task.first.isSingle())
              {
                 begin = tasks.erase(begin);
                 continue;
              }
           }

           begin++;
       }
    }
    while(isrun.load());
}

void TasksController::stop()
{
    isrun = false;
}
