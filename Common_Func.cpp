/*
 *  Created on: Dec 16, 2015
 *      Author: zhangyalei
 */

#include "Common_Func.h"
#include "Date_Time.h"
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <sstream>
//#include <openssl/md5.h>


long elf_hash(const char *str, unsigned int len) {
	long int hash = 0, x = 0;

	for(size_t i = 0; i < len; ++i) {
		hash = (hash << 4) + str[i];
		if((x = hash & 0xF0000000L) != 0) {
			hash ^= (x >> 24);
		}
		hash &= ~x;
	}

	return hash;
}

void make_session(std::string& account, std::string& session){
	long timesamp = Time_Value::gettimeofday().sec() + Time_Value::gettimeofday().usec();
	long hash = elf_hash(account.c_str(), account.size());
	int rand = random() % hash;

	std::stringstream stremsession;
	stremsession << timesamp;
	stremsession << hash;
	stremsession << rand;

	session =  stremsession.str();
}

int validate_md5(const char *key, const char *account, const char *time, const char *session) {
    //static const char *key = "123!@#qwe";

    char mine_src[256 + 1], mine_md5[256 + 1];
    memset(mine_src, 0x00, 256 + 1);
    memset(mine_md5, 0x00, 256 + 1);

    snprintf(mine_src, sizeof(mine_src), "%s%s%s", account, time, key);
    //const unsigned char *tmp_md5 = MD5((const unsigned char *) mine_src, strlen(mine_src), 0);

    for (uint i = 0; i < 16; i++) {
        //sprintf(&mine_md5[i * 2], "%.2x", tmp_md5[i]);
    }

    return strncmp(session, mine_md5, strlen(session));
}

void set_date_to_day(Date_Time &date_time, int time) {
	date_time.year(time / 10000);
	time = time % 10000;
	date_time.month(time / 100);
	time = time % 100;
	date_time.day(time);
	date_time.hour(0);
	date_time.minute(0);
	date_time.second(0);
}

void set_date_time(Date_Time &date_time, int time) {
	int int_tmp1, int_tmp2, int_tmp3;
	int_tmp1 = time;
	int_tmp3 = int_tmp1 % 100;
	int_tmp1 = int_tmp1 / 100;
	int_tmp2 = int_tmp1 % 100;
	int_tmp1 = int_tmp1 / 100;
	date_time.hour(int_tmp1);
	date_time.minute(int_tmp2);
	date_time.second(int_tmp3);
}

void set_date_to_hour(Date_Time &date_time, int time) {
	date_time.year(time / 1000000);
	time = time % 1000000;
	date_time.month(time / 10000);
	time = time % 10000;
	date_time.day(time / 100);
	date_time.hour(time % 100);
	date_time.minute(0);
	date_time.second(0);
}

int get_time_zero(void) {
	Date_Time date_time;
	date_time.hour(0);
	date_time.minute(0);
	date_time.second(0);

	return date_time.time_sec() + 86400;
}

int get_time_zero(const int sec) {
	int zeor_time = sec - (sec + 28800) % 86400;
	return zeor_time;
}

int get_today_zero(void) {
	Date_Time date_time;
	date_time.hour(0);
	date_time.minute(0);
	date_time.second(0);

	return date_time.time_sec();
}

int set_time_to_zero(const Time_Value &time_src, Time_Value &time_des) {
	Date_Time date_tmp(time_src);
	date_tmp.hour(0);
	date_tmp.minute(0);
	date_tmp.second(0);
	time_des.sec(date_tmp.time_sec());
	return 0;
}

int get_sunday_time_zero(void) {
	Time_Value time = get_week_time(7, 23, 59, 59);
	return time.sec();
}

Time_Value get_week_time(int week, int hour, int minute, int second) {
	Time_Value time(Time_Value::gettimeofday());
	Date_Time date_now(time);
	int day_gap = 0;
	int weekday = date_now.weekday();
	if (weekday > 0) {
		day_gap = week - weekday;
	}
	int hour_gap = hour - date_now.hour();
	int minute_gap = minute - date_now.minute();
	int second_gap = second - date_now.second();

	int sec = time.sec();
	int rel_sec = 0;
	rel_sec += day_gap*Time_Value::ONE_DAY_IN_SECS;
	rel_sec += hour_gap*Time_Value::ONE_HOUR_IN_SECS;
	rel_sec += minute_gap*Time_Value::ONE_MINUTE_IN_SECS;
	rel_sec += second_gap;
	if (rel_sec <= 0) {
		rel_sec += Time_Value::ONE_DAY_IN_SECS * 7;
	}

	time.set(sec + rel_sec, 0);
	return time;
}

Time_Value spec_next_day_relative_time(int hour, int minute, int second) {
	Time_Value time(Time_Value::gettimeofday());
	Date_Time date_now(time);
	int hour_gap = hour - date_now.hour();
	int minute_gap = minute - date_now.minute();
	int second_gap = second - date_now.second();

	int sec = 0;
	sec += hour_gap * Time_Value::ONE_HOUR_IN_SECS;
	sec += minute_gap * Time_Value::ONE_MINUTE_IN_SECS;
	sec += second_gap;

	if (sec <= 0) {
		sec += Time_Value::ONE_DAY_IN_SECS;
	}

	time.set(sec, 0);
	return time;
}

Time_Value spec_today_absolute_time(unsigned int hour, unsigned int minute, unsigned int second) {
	Time_Value time(Time_Value::gettimeofday());
	Date_Time date_now(time);
	int hour_gap = hour - date_now.hour();
	int minute_gap = minute - date_now.minute();
	int second_gap = second - date_now.second();

	int sec = 0;
	sec += hour_gap * Time_Value::ONE_HOUR_IN_SECS;
	sec += minute_gap * Time_Value::ONE_MINUTE_IN_SECS;
	sec += second_gap;
	sec += time.sec();

	time.set(sec, 0);

	return time;
}

Time_Value get_day_begin(const Time_Value &now) {
	Date_Time date_now(now);
	date_now.hour(0);
	date_now.minute(0);
	date_now.second(0);
	date_now.microsec(0);
	Time_Value day_begin(date_now.time_sec(), 0);
	return day_begin;
}

void get_date_day_gap(const Time_Value &date1, const Time_Value &date2, int &day) {
	Time_Value local_date1 = get_day_begin(date1);
	Time_Value local_date2 = get_day_begin(date2);
	int64_t sec_gap = local_date2.sec() - local_date1.sec();
	day = sec_gap / Time_Value::ONE_DAY_IN_SECS;
}

void get_next_cycle_time(const Time_Value &begin, const Time_Value &now, const Time_Value &offset,
		const Time_Value &cycle, Time_Value &next_time) {
	Time_Value first_begin_time = begin + offset;
	if (now > first_begin_time) {
		if (cycle <= Time_Value::zero) {
			next_time = Time_Value::max;
			return;
		}
		int cycle_sec = cycle.sec();
		int sec_gap = now.sec() - first_begin_time.sec();
		int cycle_nums = sec_gap / cycle_sec;
		++cycle_nums;
		next_time.set(first_begin_time.sec() + cycle.sec() * cycle_nums, 0);
	} else {
		next_time = first_begin_time;
	}
}

int get_days_delta(Time_Value time1, Time_Value time2) {
	Time_Value time_tmp;
	if (time1 < time2) {
		time_tmp = time1;
		time1 = time2;
		time2 = time_tmp;
	}

	Date_Time date1(time1), date2(time2);
	date1.hour(0);
	date1.minute(0);
	date1.second(0);
	date2.hour(0);
	date2.minute(0);
	date2.second(0);

	return (date1.time_sec() - date2.time_sec()) / Time_Value::ONE_DAY_IN_SECS;
}

double get_big_throw_small(double value) {
	double xiaoshu = static_cast<double>(static_cast<int>(value * 10.0) % 10);
	if (xiaoshu >= 5.0) {
		return value - xiaoshu/10.0 + 1.0;
	} else {
		return value - xiaoshu/10.0;
	}
}

bool in_json_array(const int search, const Json::Value &json_array) {
	for (Json::Value::iterator iter = json_array.begin(); iter != json_array.end(); ++iter) {
		if (search == (*iter).asInt())
			return true;
	}
	return false;
}

bool is_in_vector(int number, std::vector<int> &number_vector) {
	for (std::vector<int>::iterator iter = number_vector.begin();
			iter != number_vector.end(); ++iter) {
		if (number == *iter) {
			return true;
		}
	}
	return false;
}


/*
 * 按权重值取随机值 wight = [权值1,权值2,权值3,权值n] 权值>=0
 * 成功返回wight下标,失败返回-1
 */
int get_rand_index(const std::vector<int> &wight) {
	unsigned int size = wight.size();
	if (size == 0) return -1;

	int sum = 0;
	for (unsigned int i = 0; i < size; ++i) {
		if (wight[i] < 0) return -1;
		sum += wight[i];
	}

	if (sum == 0) return -1;
	int rand = random() % sum;

	for (unsigned int i = 0; i < size; ++i) {
		rand -= wight[i];
		if (rand < 0) return i;
	}

	return -1;
}

/*
 * 按权重值取随机值 wight = [权值1,权值2,权值3,权值n] 权值>=0
 * 成功返回wight下标,失败返回-1
 */
int get_rand_index(const Json::Value &wight) {
	if (wight == Json::Value::null) return -1;
	unsigned int size = wight.size();
	if (size == 0) return -1;

	int sum = 0;
	for (unsigned int i = 0; i < size; ++i) {
		if ((!wight[i].isInt() && !wight[i].isDouble()) || wight[i].isDouble() < 0) return -1;
		sum += wight[i].asDouble() * 10000;
	}

	if (sum == 0) return -1;
	int rand = random() % sum;

	for (unsigned int i = 0; i < size; ++i) {
		rand -= wight[i].asDouble() * 10000;
		if (rand < 0) return i;
	}

	return -1;
}

int get_loop_index(const Json::Value &wight) {
	if (wight == Json::Value::null) return -1;
	unsigned int size = wight.size();
	if (size == 0) return -1;

	int sum = 0;
	for (unsigned int i = 0; i < size; ++i) {
		if ((!wight[i].isInt() && !wight[i].isDouble()) || wight[i].isDouble() < 0) return -1;
		sum += wight[i].asDouble() * 10000;
	}

	if (sum < 1000000 && random()%1000000 < (1000000 - sum)) {
		return -1;
	}

	if (sum == 0) return -1;
	int rand = random() % sum;

	for (unsigned int i = 0; i < size; ++i) {
		rand -= wight[i].asDouble() * 10000;
		if (rand < 0) return i;
	}

	return -1;
}


struct Time_Value_Greater {
	bool operator() (Time_Value elem1, Time_Value elem2) const {
		return elem1 < elem2;
	}
};

long get_next_tick_time(const Json::Value &date) {
	if (date["date_loop"] != Json::Value::null && date["date_loop"].asInt() >= 0) {
		return date["date_loop"].asInt();
	}

	if (date["date_week"] == Json::Value::null || date["date_time"] == Json::Value::null) return -1;

	if (date["date_week"].size() <= 0 || date["date_time"].size() <= 0) return -2;

	Time_Value tv = Time_Value::gettimeofday();
	Date_Time dt(tv);

	std::vector<Time_Value> time_vec;
	int weekday = 0;
	for (size_t i = 0; i < date["date_week"].size(); ++i) {
		weekday = date["date_week"][i].asInt();
		if (weekday < 0 || weekday > 6) return -3;

		for (size_t j = 0; j < date["date_time"].size(); ++j) {
			Time_Value tmp_tv(tv.sec() + (weekday - dt.weekday()) * 86400);
			Date_Time tmp_dt(tmp_tv);

			tmp_dt.hour(date["date_time"][j][0u].asInt());
			tmp_dt.minute(date["date_time"][j][1u].asInt());
			tmp_dt.second(0);

			Time_Value tmp(tmp_dt.time_sec());
			time_vec.push_back(tmp);
		}
	}

	weekday = date["date_week"][0u].asInt();
	for (size_t j = 0; j < date["date_time"].size(); ++j) {
		Time_Value tmp_tv(tv.sec() + (weekday - dt.weekday()) * 86400);
		Date_Time tmp_dt(tmp_tv);

		tmp_dt.hour(date["date_time"][j][0u].asInt());
		tmp_dt.minute(date["date_time"][j][1u].asInt());
		tmp_dt.second(0);

		Time_Value tmp(tmp_dt.time_sec() + 86400 * 7);
		time_vec.push_back(tmp);
	}

	std::sort(time_vec.begin(), time_vec.end(), Time_Value_Greater());

	for (std::vector<Time_Value>::iterator iter = time_vec.begin(); iter != time_vec.end(); ++iter) {
		if ((*iter - tv).sec() > 0) return (iter->sec() - tv.sec()) * 1000;
	}

	return -1;
}

int get_next_tick_time(const Json::Value &week, const Json::Value &time) {

	if (week == Json::Value::null || time == Json::Value::null) return -1;

	if (week.size() <= 0 || time.size() != 2) return -2;

	Time_Value tv = Time_Value::gettimeofday();
	Date_Time dt(tv);

	std::vector<Time_Value> time_vec;
	int weekday = 0;
	for (size_t i = 0; i < week.size(); ++i) {
		weekday = week[i].asInt();
		if (weekday < 0 || weekday > 6) return -3;

		{
			Time_Value tmp_tv(tv.sec() + (weekday - dt.weekday()) * 86400);
			Date_Time tmp_dt(tmp_tv);

			tmp_dt.hour(time[0u].asInt());
			tmp_dt.minute(time[1u].asInt());
			tmp_dt.second(0);

			Time_Value tmp(tmp_dt.time_sec());
			time_vec.push_back(tmp);
		}
	}

	weekday = week[0u].asInt();

	{
		Time_Value tmp_tv(tv.sec() + (weekday - dt.weekday()) * 86400);
		Date_Time tmp_dt(tmp_tv);

		tmp_dt.hour(time[0u].asInt());
		tmp_dt.minute(time[1u].asInt());
		tmp_dt.second(0);

		Time_Value tmp(tmp_dt.time_sec() + 86400 * 7);
		time_vec.push_back(tmp);
	}

	std::sort(time_vec.begin(), time_vec.end(), Time_Value_Greater());

	for (std::vector<Time_Value>::iterator iter = time_vec.begin(); iter != time_vec.end(); ++iter) {
		if ((*iter - tv).sec() > 0) return iter->sec() - tv.sec();
	}

	return -1;
}
/*
 * next_time 离当前最近的绝对时间
 * "time" : [020000,080000,140000,200000]
 * "week" : [ 0, 1, 2, 3, 4, 5, 6 ]
 */
extern int get_next_tick_time(const Json::Value &week, const Json::Value &time, Time_Value &next_time) {
	if (week == Json::Value::null || time == Json::Value::null) return -1;
	if (week.size() <= 0 || time.size() <= 0) return -2;

	std::vector<int> weekday;
	for (size_t i = 0; i < week.size(); ++i) {
		if (week[i].asInt() >= 0 && week[i].asInt() <= 6) weekday.push_back(week[i].asInt());
	}
	if (weekday.size() <= 0) return -2;
	std::sort(weekday.begin(), weekday.end(), std::less<int>());

	std::vector<int> time_vec;
	for (size_t i = 0; i < time.size(); ++i) {
		time_vec.push_back(time[i].asInt());
	}
	if (time_vec.size() <= 0) return -2;
	std::sort(time_vec.begin(), time_vec.end(), std::less<int>());

	Time_Value tv = Time_Value::gettimeofday();
	Date_Time dt(tv);
	if (is_in_vector(dt.weekday(), weekday)) {
		int time_now = dt.hour() * 10000 + dt.minute() * 100 + dt.second();
		for (size_t i = 0; i < time_vec.size(); ++i) {
			if (time_now < time_vec[i]) {
				int sub_hour = time_vec[i] / 10000 - time_now / 10000;
				int sub_minute = time_vec[i] / 100 % 100 - time_now / 100 % 100;
				int sub_second = time_vec[i] % 100 - time_now % 100;
				int sec = (sub_second) + (sub_minute * 60) + (sub_hour * 3600);
				next_time.set(tv.sec() + sec, 0);
				return 0;//同一天
			}
		}
	}

	for (int i = 1; i < 8; ++i) {
		int week_next = dt.weekday() + i;
		if (week_next > 6) week_next -= 7;
		if (is_in_vector(week_next, weekday)) {
			int time_now = dt.hour() * 10000 + dt.minute() * 100 + dt.second();
			int sub_hour = time_vec[0u] / 10000 - time_now / 10000;
			int sub_minute = time_vec[0u] / 100 % 100 - time_now / 100 % 100;
			int sub_second = time_vec[0u] % 100 - time_now % 100;
			int sec = i * 86400 + (sub_second) + (sub_minute * 60) + (sub_hour * 3600);
			next_time.set(tv.sec() + sec, 0);
			return 0;//不同一天
		}
	}

	return -3;
}

void print_time_date(const Time_Value &time, const std::string &time_name) {
	Date_Time timeout_date(time);
	LOG_DEBUG("the %s date is : %d-%d-%d %d:%d:%d", time_name.c_str(), timeout_date.year(), timeout_date.month(), timeout_date.day(),
		timeout_date.hour(), timeout_date.minute(), timeout_date.second());
}
