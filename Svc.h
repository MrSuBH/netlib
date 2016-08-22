/*
 * Svc.h
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#ifndef SVC_H_
#define SVC_H_

#include "Event_Handler.h"
#include "Block_List.h"
#include "Public_Define.h"

class Server;
class Connector;
class Svc;

class Svc_Handler {
public:
	typedef Block_List<Thread_Mutex> Data_Block_List;
	typedef std::vector<Block_Buffer *> Block_Vector;

	Svc_Handler(void);
	virtual ~Svc_Handler(void);

	void reset(void);

	void set_parent(Svc *parent) { parent_ = parent; }

	int push_recv_block(Block_Buffer *buffer);
	int push_send_block(Block_Buffer *buffer);

	virtual int handle_send(void) = 0;
	virtual int handle_pack(Block_Vector &block_vec) = 0;

protected:
	Svc *parent_;
	Data_Block_List recv_block_list_;
	Data_Block_List send_block_list_;

	size_t max_list_size_;
	size_t max_pack_size_;
};

class Svc: public Event_Handler {
public:
	typedef std::vector<Block_Buffer *> Block_Vector;
	
	Svc(void);
	virtual ~Svc(void);

	void reset(void);

	virtual Block_Buffer *pop_block(int cid);
	virtual int push_block(int cid, Block_Buffer *buffer);
	virtual int post_block(Block_Buffer* buffer);

	virtual int register_recv_handler(void);
	virtual int unregister_recv_handler(void);

	virtual int register_send_handler(void);
	virtual int unregister_send_handler(void);

	virtual int close_handler(int cid);

	int create_handler(NetWork_Protocol protocol_type);

	//接收数据
	virtual int handle_input(void);
	//发送数据
	virtual int handle_send(void);

	virtual int handle_close(void);
	int close_fd(void);

	int push_recv_block(Block_Buffer *buffer);
	int push_send_block(Block_Buffer *buffer);

	void set_cid(int cid);
	int get_cid(void);

	bool get_reg_recv(void);
	void set_reg_recv(bool val);

	bool get_reg_send(void);
	void set_reg_send(bool val);

	bool is_closed(void);
	void set_closed(bool v);

	void set_peer_addr(void);
	int get_peer_addr(std::string &ip, int &port);
	int get_local_addr(std::string &ip, int &port);

	void set_server(Server *server);
	void set_connector(Connector *connector);
	
	std::string get_peer_ip();
	int get_peer_port();

protected:
	Server *server_;
	Connector *connector_;

private:
	int cid_;
	bool is_closed_;
	bool is_reg_recv_, is_reg_send_;

	std::string peer_ip_;
	int peer_port_;

	Svc_Handler *handler_;
};

inline void Svc::reset(void) {
	cid_ = 0;
	is_closed_ = false;
	is_reg_recv_ = false;
	is_reg_send_ = false;

	peer_ip_.clear();
	peer_port_ = 0;

	if (handler_) {
		handler_->reset();
		delete handler_;
		handler_ = 0;
	}
	server_ = 0;
	connector_ = 0;
	Event_Handler::reset();
}

inline int Svc::push_recv_block(Block_Buffer *buffer) {
	if (is_closed_)
		return -1;
	return handler_->push_recv_block(buffer);
}

inline int Svc::push_send_block(Block_Buffer *buffer) {
	if (is_closed_)
		return -1;
	return handler_->push_send_block(buffer);
}

inline void Svc::set_cid(int cid) {
	cid_ = cid;
}

inline int Svc::get_cid(void) {
	return cid_;
}

inline bool Svc::get_reg_recv(void) {
	return is_reg_recv_;
}

inline void Svc::set_reg_recv(bool val) {
	is_reg_recv_ = val;
}

inline bool Svc::get_reg_send(void) {
	return is_reg_send_;
}

inline void Svc::set_reg_send(bool val) {
	is_reg_send_ = val;
}

inline bool Svc::is_closed(void) {
	return is_closed_;
}

inline void Svc::set_closed(bool v) {
	is_closed_ = v;
}

inline void Svc::set_peer_addr(void) {
	get_peer_addr(peer_ip_, peer_port_);
}

inline std::string Svc::get_peer_ip(void) {
	return peer_ip_;
}
	
inline int Svc::get_peer_port(void) {
	return peer_port_;
}

#endif /* SVC_H_ */
