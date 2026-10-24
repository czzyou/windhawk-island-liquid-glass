#include "nba-parser.h"
#include "tomorrow-brief.h"
#include "important-days.h"
#include <d2d1.h>
#include "calendar-days.h"
int main() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    SYSTEMTIME today{2026,10,0,20};
    auto lunch=today; lunch.wHour=10; lunch.wMinute=49;
    assert(MakeMealReminder(lunch,0,0).key.empty());
    lunch.wMinute=50;
    auto meal=MakeMealReminder(lunch,0,0);
    assert(meal.meal==1 && meal.date==20261020 && meal.body==L"该吃午饭了");
    assert(MakeMealReminder(lunch,20261020,0).key.empty());
    assert(!MakeMealReminder(lunch,20261019,0).key.empty());
    lunch.wHour=11; lunch.wMinute=14;
    assert(!MakeMealReminder(lunch,0,0).key.empty());
    lunch.wMinute=15; assert(MakeMealReminder(lunch,0,0).key.empty());
    auto dinner=today; dinner.wHour=16; dinner.wMinute=50;
    assert(MakeMealReminder(dinner,20261020,0).meal==2);
    assert(MakeMealReminder(dinner,0,20261020).key.empty());
    dinner.wHour=17; dinner.wMinute=15;
    assert(MakeMealReminder(dinner,0,0).key.empty());
    std::vector<ImportantDay> legacy;
    assert(ParseImportantDays(L"[{\"id\":\"abc\",\"title\":\"考试\",\"date\":20261028,\"advance\":1}]",legacy));
    assert(legacy[0].kind==1 && CalendarDayKinds(legacy,20261028)==1);
    ImportantDay birthday{L"def",L"生日",20261028,1,2};
    legacy.push_back(birthday);
    assert(CalendarDayKinds(legacy,20261028)==3 && CalendarDayKinds(legacy,20261029)==0);
    assert(CalendarDayText(legacy,20261028)==L"考试\n生日");
    assert(ParseImportantDays(EncodeImportantDays(legacy),legacy) && legacy[1].kind==2);
    for (int year:{2026,2028}) for (int month=1;month<=12;++month) {
        CalendarGrid grid(year,month);
        assert(grid.rows>=4 && grid.rows<=6);
        for(int day=1;day<=grid.days;++day) {
            const auto cell=grid.Cell(day);
            assert(cell.left>=134 && cell.right<=330 && cell.top>=41 && cell.bottom<=170.01f);
            assert(grid.Hit((cell.left+cell.right)*0.5f,(cell.top+cell.bottom)*0.5f)==day);
        }
        assert(grid.Hit(133,60)==0 && grid.Hit(180,170)==0);
        if(grid.first) assert(grid.Hit(140,42)==0);
    }
    assert(CalendarGrid(2026,5).rows==6 && GetDaysInMonth(2028,2)==29);
    ImportantDay exam{L"abc",L"测试考试",20261028,1};
    assert(ImportantDayValid(exam));
    assert(MakeImportantReminder(exam,today,0).key.empty());
    auto beforeExam=today; beforeExam.wDay=27;
    assert(MakeImportantReminder(exam,beforeExam,0).important);
    assert(MakeImportantReminder(exam,beforeExam,20261027).key.empty());
    beforeExam.wDay=28; assert(MakeImportantReminder(exam,beforeExam,20261027).important);
    beforeExam.wDay=29; assert(MakeImportantReminder(exam,beforeExam,0).key.empty());
    exam.advance=7; beforeExam.wDay=21;
    assert(MakeImportantReminder(exam,beforeExam,0).important);
    std::vector<ImportantDay> saved;
    assert(ParseImportantDays(EncodeImportantDays({exam}),saved) && saved.size()==1 && saved[0].title==exam.title);
    assert(!ParseImportantDays(L"broken",saved) && saved.size()==1);
    assert(!ParseImportantDays(L"[{\"id\":\"a\",\"title\":\"x\",\"date\":20260230,\"advance\":1}]",saved));
    assert(!ParseImportantDays(L"[{\"id\":\"a\",\"title\":\"x\",\"date\":20261028.5,\"advance\":1}]",saved));
    assert(!ParseImportantDays(EncodeImportantDays({exam,exam}),saved));
    assert(BriefDateKey(BriefTomorrow(today)) == 20261021);
    SYSTEMTIME december{2026,12,0,31}, leap{2028,2,0,28};
    assert(BriefDateKey(BriefTomorrow(december)) == 20270101);
    assert(BriefDateKey(BriefTomorrow(leap)) == 20280229);
    const std::string fixture = R"(var fc={"f":[
        {"fi":"10/20"},
        {"fi":"10/21","fa":"00","fc":"11","fd":"-2","fe":"西北风","fg":"4-5级"}]};)";
    auto weather = ParseTomorrowWeather(fixture,today,L"沈阳",false);
    assert(weather.date == 20261021 && weather.weatherCode == 113);
    assert(weather.body.find(L"-2°～11°C") != std::wstring::npos);
    weather.fetchedAt = 100;
    assert(BriefPending(weather,20261021,101,L""));
    assert(!BriefPending(weather,20261021,101,weather.key));
    assert(!BriefPending(weather,20261022,101,L""));
    assert(!BriefPending(weather,20261021,3700,L""));
    assert(!BriefPending(weather,20261021,99,L""));
    assert(ParseTomorrowWeather("bad JSON",today,L"沈阳",false).key.empty());
    auto stale = fixture; stale.replace(stale.find("10/20"),5,"10/19");
    assert(ParseTomorrowWeather(stale,today,L"沈阳",false).key.empty());
    auto inverted = fixture; inverted.replace(inverted.find("\"-2\""),4,"\"20\"");
    assert(ParseTomorrowWeather(inverted,today,L"沈阳",false).key.empty());
    auto fahrenheit = ParseTomorrowWeather(fixture,today,L"沈阳",true);
    assert(fahrenheit.body.find(L"28°～52°F") != std::wstring::npos);
    auto day = GetNbaLocalDay(BriefTomorrow(today));
    NbaSnapshot game; game.gameId=L"a"; game.scheduled=true;
    game.awayAbbr=L"CLE"; game.homeAbbr=L"BOS"; game.startTime=day.start+8*3600;
    auto nba = MakeTomorrowNba({game,game},today,L"CLE,PHI");
    assert(nba.games.size()==1 && nba.games[0].gameId==L"a");
    assert(nba.date == 20261021 && nba.title.find(L"1 场") != std::wstring::npos);
    assert(nba.body.find(L"08:00") != std::wstring::npos);
    assert(MakeTomorrowNba({game},today,L"PHI").key.empty());
    auto second=game; second.gameId=L"b"; second.awayAbbr=L"PHI"; second.startTime+=3600;
    auto third=second; third.gameId=L"c"; third.startTime+=3600;
    auto multiple=MakeTomorrowNba({third,game,second},today,L"CLE,PHI");
    assert(multiple.games.size()==2 && multiple.games[0].gameId==L"a" && multiple.games[1].gameId==L"b");
    game.startTime=day.end; assert(MakeTomorrowNba({game},today,L"CLE").key.empty());
    game.startTime=day.start-1; assert(MakeTomorrowNba({game},today,L"CLE").key.empty());
    game.startTime=day.start; game.scheduled=false;
    assert(MakeTomorrowNba({game},today,L"CLE").key.empty());
    winrt::uninit_apartment();
}
