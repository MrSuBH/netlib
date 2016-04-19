/*
 * Svc.cpp
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <limits.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <string.h>
#include "Svc.h"
#include "Block_Buffer.h"
#include "Lib_Log.h"
#include "Server.h"
#include "Connector.h"
#include "Svc_Tcp.h"
#include "Svc_WebSocket.h"

Svc_Handler::Svc_Handler():
	parent_(0),
	max_list_size_(MAX_LIST_SIZE),
	max_pack_size_(MAX_PACK_SIZE)
{
}

Svc_Handler::~Svc_Handler(){
	parent_ = NULL;
}

void Svc_Handler::reset(void){
	Data_Block_List::BList blist;
	int cid = parent_->get_cid();

	blist.clear();
	recv_block_list_.swap(blist);
	for (Data_Block_List::BList::iterator it = blist.begin(); it != blist.end(); ++it) {
		parent_->push_block(cid, *it);
	}

	blist.clear();
	send_block_list_.swap(blist);
	for (Data_Block_List::BList::iterator it = blist.begin(); it != blist.end(); ++it) {
		parent_->push_block(cid, *it);
	}

	max_list_size_ = MAX_LIST_SIZE;
	max_pack_size_ = MAX_PACK_SIZE;
	parent_ = 0;
}

void Svc_Handler::set_max_list_size(size_t max_size){
	max_list_size_ = max_size;
}

void Svc_Handler::set_max_pack_size(size_t max_size){
	max_pack_size_ = max_size;
}

void Svc_Handler::set_parent(Svc *parent){
	parent_ = parent;
}

int Svc_Handler::push_recv_block(Block_Buffer *buf){
	if (recv_block_list_.size() >= max_list_size_) {
		LIB_LOG_INFO("recv_block_list_ has full.");
		return -1;
	}
	recv_block_list_.push_back(buf);
	return 0;

}

int Svc_Handler::push_send_block(Block_Buffer *buf){
	if (send_block_list_.size() >= max_list_size_) {
		LIB_LOG_INFO("role_id:%ld send_block_list_ has full send_block_list_.size() = %d, max_list_size_ = %d", parent_->get_role_id(), send_block_list_.size(), max_list_size_);
		parent_->handle_close();
		return -1;
	}
	send_block_list_.push_back(buf);
	return 0;
}

Svc::Svc(void):
	server_(0),
	connector_(0),
	cid_(0),
	is_closed_(false),
	is_reg_recv_(false),
	is_reg_send_(false),
	peer_port_(0),
	role_id_(0),
	network_procotol_type_(NETWORK_PROTOCOL_TCP),
	handler_(0)
{ }

Svc::~Svc(void) { }

void Svc::set_server(Server *server) {
	server_ = server;
}

void Svc::set_svc_handler(Svc_Handler *svc_handler){
	handler_ = svc_handler;
}

void Svc::set_connector(Connector *connector) {
	connector_ = connector;
}

Block_Buffer *Svc::pop_block(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::push_block(int cid, Block_Buffer *block) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::register_recv_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::unregister_recv_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::register_send_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::unregister_send_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::recv_handler(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::close_handler(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::handle_input(void) {
	return recv_data();
}

int Svc::handle_close(void) {
	if (is_closed_) {
		return 0;
	} else {
		is_closed_ = true;
		return close_handler(cid_);
	}
}

int Svc::close_fd(void) {
	if (is_closed_) {
		LIB_LOG_DEBUG("close fd = %d", this->get_fd());
		::close(this->get_fd());
	}
	return 0;
}

int Svc::get_peer_addr(std::string &ip, int &port) {
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (::getpeername(this->get_fd(), (struct sockaddr *)&sa, &len) < 0) {
		LIB_LOG_TRACE("getpeername wrong, fd:%d", this->get_fd());
		return -1;
	}

	std::stringstream stream;
	stream << inet_ntoa(sa.sin_addr);

	ip = stream.str();
	port = ntohs(sa.sin_port);

	return 0;
}

int Svc::get_local_addr(std::string &ip, int &port) {
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (::getsockname(this->get_fd(), (struct sockaddr *)&sa, &len) < 0) {
		LIB_LOG_TRACE("getsockname wrong, fd:%d", this->get_fd());
		return -1;
	}

	std::stringstream stream;
	stream << inet_ntoa(sa.sin_addr);

	ip = stream.str();
	port = ntohs(sa.sin_port);

	return 0;
}

int Svc::recv_data(void) {
	return handler_->handle_recv();
}

int Svc::send_data(void) {
	return handler_->handle_send();
}

int Svc::pack_data(Block_Vector &block_vec){
	return handler_->handle_pack(block_vec);
}

void Svc::create_handler(NetWork_Protocol network_protocol_type) {
	network_procotol_type_ = network_protocol_type;
	switch(network_protocol_type){
		case NETWORK_PROTOCOL_TCP:
			handler_ = Svc_Tcp::create_object();
			break;
		case NETWORK_PROTOCOL_UDP:
			break;
		case NETWORK_PROTOCOL_WEBSOCKET:
			handler_ = Svc_Websocket::create_object();
			break;
		default:
			break;
	}
	handler_->set_parent(this);
}

void Svc::reclaim_handler() {
	switch(network_procotol_type_){
		case NETWORK_PROTOCOL_TCP:
			Svc_Tcp::reclaim_object(static_cast<Svc_Tcp *>(handler_));
			break;
		case NETWORK_PROTOCOL_UDP:
			break;
		case NETWORK_PROTOCOL_WEBSOCKET:
			Svc_Websocket::reclaim_object(static_cast<Svc_Websocket *>(handler_));
			break;
		default:
			break;
	}
	handler_ = 0;
	network_procotol_type_ = NETWORK_PROTOCOL_TCP;
}

