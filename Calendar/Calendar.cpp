#include "Calendar.h"
#include <cstring>
#include <cstdlib>

#define maxLineSize	80

using std::stoi;

bool
Calendar::roll(date& dt)
{
	while (!isBusDay(dt)) {
		++dt;
	}
	return SUCCESS;
}

bool
Calendar::isBusDay(const date& dt)
{
	return dt.isWeekDay() && holidays.find(dt) == holidays.end();
}

bool
Calendar::addBusDays(date& dt, int count)
{
	while (count != 0) {
		++dt;
		if (isBusDay(dt)) 
			count--;
	}
	return SUCCESS;
}

bool
Calendar::addMonths(date& dt, int count)
{
	dt.addMonths(count);
	while (!isBusDay(dt) && !dt.EOM()) 
		++dt;
	while (!isBusDay(dt)) 
		--dt;
	return SUCCESS;
}

bool
Calendar::addYears(date& dt, int count)
{
	dt.addYears(count);
	while (!isBusDay(dt) && !dt.EOM()) 
		++dt;
	while (!isBusDay(dt)) 
		--dt;
	return SUCCESS;
}

bool
Calendar::addHoliday(date& dt)
{
	holidays.insert(dt);
	return SUCCESS;
}

bool
Calendar::removeHoliday(date& dt)
{
	if (holidays.find(dt) == holidays.end()) 
		return FAILURE;
	else {
		holidays.erase(dt);
		return SUCCESS;
	}
}

MMCalendar::MMCalendar(string filename, string mkt) :market(mkt) {
	ifstream fin(filename);
	string s;
	while (fin >> s) {
		int ymd = std::stoi(s.substr(0, 8));
		int year = ymd / 10000;
		int month = (ymd - year * 10000) / 100;
		int day = ymd - year * 10000 - month * 100;
		holidays.insert(date(year, month, day));
	}
}

bool
MMCalendar::roll(date& dt)
{
	while (!this->isBusDay(dt) && !dt.EOM()) 
		++dt;
	while (!this->isBusDay(dt)) 
		--dt;
	return SUCCESS;
}

date&
MMCalendar::nextIMMDay(date& dt) {
	if (dt.month() % 3 == 0) {
		//get first day of the month
		date new_dt = date(dt.year(), dt.month(), 1);
		// get the date of third Wednesday
		new_dt = date(dt.year(), dt.month(), 15 + (10 - new_dt.dayOfWeek()) % 7);
		if (new_dt > dt) {
			Calendar::roll(new_dt);
			return new_dt;
		}
	}
	int new_month = dt.month() - dt.month() % 3 + 3;
	date new_date;
	if (new_month > 12)
		new_date = date(dt.year() + 1, new_month - 12, 1);
	else
		new_date = date(dt.year(), new_month, 1);
	return nextIMMDay(new_date);
}