#ifndef TASKSCONTROLLER_H
#define TASKSCONTROLLER_H

#include <string_view>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>

/* Task example

  1. P DD/MM hh:mm:ss

     description:

     P - time point

     DD - day: 14, range[0,31]
     MM - month: 11, range[0,12]
     hh - hours: 15, range[0,24] - 24:00 equivalent to 00:00
     mm - minutes: 44, range[0,60] - 60 equivalent to 00
     ss - seconds: 32, range[0,60] - 60 equivalent to 00

     reuslt:P 14/11 15:44:32

     example:

     0. non-working:P 00/00 00:00:00

     1. every year:P 00/11 00:00:00,
                    P 05/11 15:35:01

     2. every month:P 05/00 00:00:00,
                    P 05/00 15:35:01

     3. every day:P 00/00 15:00:00, but does not work at 00:00:00(P 00/00 00:00:00), so only from 01:00:00 to 24:00:00, 24:00 equivalent to 00:00
                  P 00/00 15:35:01

     4. every hours:P 00/00 00:35:00,
                    P 00/00 00:35:15

     5. every seconds:P 00/00 00:00:35,

  2. SP DD/MM hh:mm:ss - single point, same as time point, only fires once

  3. W D hh:mm:ss

     description:

     W - weekday point

     D - weekday, 2, range[1,7]
     hh - hours: 15, range[0,23]
     mm - minutes: 44, range[0,59]
     ss - seconds: 32, range[0,59]

     reuslt:W 2 15:44:32

     example:

     0. non-working:W 0 00:00:00

     1. W 1 00:00:00

     2. W 7 23:59:59

     3. W 2 15:44:15

  4. SW D hh:mm:ss - single weekday point, same as weekday point, only fires once

  5. I DDDDD hh:mm:ss

     description:

     I - interval

     DDDDD - days, 00005, range[0,65535]
     hh - hours: 15, range[0,23]
     mm - minutes: 44, range[0,59]
     ss - seconds: 32, range[0,59]

     reuslt:I 00005 15:44:32

     example:

     0. non-working:I 00000 00:00:00

     1. every days(1):I 00001 00:00:00

     2. every days(2) + hours(15):I 00002 15:00:00

     3. every days(1) + houurs(14) + minutes(23):I 00001 14:23:00

     4. every days(3) + houurs(15) + minutes(25) + seconds(5):I 00003 15:23:05

     5. hours(15):I 00000 15:00:00

     6. houurs(14) + minutes(23):I 00000 14:23:00

     n. ....

  6. SI DDDDD hh:mm:ss - single interval, same as interval, only fires once

*/

class Task final
{
    using Now = std::chrono::system_clock::time_point;
public:

    enum Type : unsigned char
    {
         None = 0,
         Point,
         SinglePoint,
         Interval,
         SingleInterval
    };

    explicit Task();
    explicit Task(std::string_view value);

    bool isValid() const;
    bool taskCalculate(const Now & now, bool recalc) const;
    Type taskType() const;
    bool isSingle() const;

    bool pointDayTaskInit(const unsigned char seconds,
                          const unsigned char minutes = 0,
                          const unsigned char hours = 0,
                          const unsigned char day = 0,
                          const unsigned char month = 0);

    bool singlePointDayTaskInit(const unsigned char seconds,
                                const unsigned char minutes = 0,
                                const unsigned char hours = 0,
                                const unsigned char day = 0,
                                const unsigned char month = 0);

    bool pointWeekTaskInit(const unsigned char seconds,
                           const unsigned char minutes = 0,
                           const unsigned char hours = 0,
                           const unsigned char weekday = 0);

    bool singlePointWeekTaskInit(const unsigned char seconds,
                                 const unsigned char minutes = 0,
                                 const unsigned char hours = 0,
                                 const unsigned char weekday = 0);

    bool intervalTaskInit(const unsigned char seconds,
                          const unsigned char minutes = 0,
                          const unsigned char hours = 0,
                          const unsigned short days = 0);

    bool singleIntervalTaskInit(const unsigned char seconds,
                                const unsigned char minutes = 0,
                                const unsigned char hours = 0,
                                const unsigned short days = 0);

    bool parseFromString(std::string_view value);

private:
    Type type = None;
    std::function<bool(const Now &, bool)> calculate = nullptr;
};

class TasksController final //Time change detection is not support
{
    std::atomic_bool isrun = false;
    std::atomic_ushort _accuracy = 10;
    std::mutex mutex;
    std::map<std::string, std::pair<Task,std::vector<std::function<void()>>>> tasks;

public:

    explicit TasksController();
    explicit TasksController(unsigned short accuracy);

    bool clearTasks();
    int countTasks();

    unsigned short accuracy() const;
    bool setAccuracy(unsigned short ms = 10);

    bool contains(const std::string & name);

    bool addTask(const std::string & name, std::string_view value);
    bool addTask(const std::string & name, std::string_view value, const std::function<void()> & callback);
    bool addTask(const std::string & name, std::string_view value, const std::vector<std::function<void()>> & callbacks);

    bool addTask(const std::string & name, const Task & task);
    bool addTask(const std::string & name, const Task & task, const std::function<void()> & callback);
    bool addTask(const std::string & name, const Task & task, const std::vector<std::function<void()>> & callbacks);

    bool addCallback(const std::string & name, const std::function<void()> & callback);
    bool addCallbacks(const std::string & name, const std::vector<std::function<void()>> & callbacks);
    void clearCallbacks(const std::string & name);

    bool isRun() const;
    void run();
    void stop();
};

#endif // TASKSCONTROLLER_H
