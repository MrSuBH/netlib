/*
 * Public_Define.h
 *
 *  Created on: 2016年8月6日
 *      Author: zhangyalei
 */

#ifndef PUBLIC_DEFINE_H_
#define PUBLIC_DEFINE_H_

#include <vector>

enum NetWork_Protocol {
	NETWORK_PROTOCOL_TCP = 0,
	NETWORK_PROTOCOL_UDP = 1,
	NETWORK_PROTOCOL_WEBSOCKET = 2
};

enum {
	LIB_LOG_TRACE = 0,		//打印程序运行堆栈，跟踪记录数据信息，与DEBUG相比更细致化的记录信息
	LIB_LOG_DEBUG = 1,		//细粒度信息事件对调试应用程序是非常有帮助的
	LIB_LOG_INFO = 2,			//消息在粗粒度级别上突出强调应用程序的运行过程
	LIB_LOG_WARN = 3,			//会出现潜在错误的情形
	LIB_LOG_ERROR = 4,		//虽然发生错误事件，但仍然不影响系统的继续运行
	LIB_LOG_FATAL = 5			//严重的错误事件，将会导致应用程序的退出
};

enum {
	LOG_TRACE = 0,				//打印程序运行堆栈，跟踪记录数据信息，与DEBUG相比更细致化的记录信息
	LOG_DEBUG = 1,				//细粒度信息事件对调试应用程序是非常有帮助的
	LOG_INFO = 2,					//消息在粗粒度级别上突出强调应用程序的运行过程
	LOG_WARN = 3,					//会出现潜在错误的情形
	LOG_ERROR = 4,				//虽然发生错误事件，但仍然不影响系统的继续运行
	LOG_FATAL = 5,				//严重的错误事件，将会导致应用程序的退出
};

enum Color {
	BLACK = 30,
	RED = 31,
	GREEN = 32,
	BROWN = 33,
	BLUE = 34,
	MAGENTA = 35,
	CYAN = 36,
	GREY = 37,
	LRED = 41,
	LGREEN = 42,
	YELLOW = 43,
	LBLUE = 44,
	LMAGENTA = 45,
	LCYAN = 46,
	WHITE = 47
};

struct Msg_Process_Time {
	int msg_id;
	int times;
	Time_Value tv;
	Msg_Process_Time(void) : msg_id(0), times(0) {}
	void add_time(Time_Value &process_tv) {;
		tv += process_tv;
		times++;
	}
};

class Block_Buffer;
struct Block_Group_Info {
	size_t free_list_size_;
	size_t used_list_size_;
	size_t sum_bytes_;

	void serialize(Block_Buffer &buf);
	void deserialize(Block_Buffer &buf);
	void reset(void);
};

struct Server_Info {
	size_t svc_pool_free_list_size_;
	size_t svc_pool_used_list_size_;
	size_t svc_list_size_;
	std::vector<Block_Group_Info> block_group_info_;

	void serialize(Block_Buffer &buf);
	void deserialize(Block_Buffer &buf);
	void reset(void);
};

#endif /* PUBLIC_DEFINE_H_ */
