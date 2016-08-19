/*
 * Svc_Websocket.cpp
 *
 *  Created on: Apr 19,2016
 *      Author: zhangyalei
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "Sha1.h"
#include "Base64.h"
#include "Svc_WebSocket.h"
#include "boost/unordered_map.hpp"

Svc_Websocket::Svc_Websocket():
	Svc_Handler(),
	websocket_connected_(false)
{ }

Svc_Websocket::~Svc_Websocket() {}

void Svc_Websocket::reset(void) {
	websocket_connected_ = false;
	Svc_Handler::reset();
}

int Svc_Websocket::handle_recv(void) {
	if (parent_->is_closed())
		return 0;
	
	int cid = parent_->get_cid();
	Block_Buffer *buf = parent_->pop_block(cid);
	if (! buf) {
		LIB_LOG_ERROR("websocket pop_block fail, cid:%d", cid);
		return -1;
	}
	buf->reset();
	buf->write_int32(cid);

	int n = 0;
	while (1) {
		//每次读2k长度数据
		buf->ensure_writable_bytes(2000);
		n = 0;
		if ((n = ::read(parent_->get_fd(), buf->get_write_ptr(), buf->writable_bytes())) < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EWOULDBLOCK)
				break;

			LIB_LOG_ERROR("websocket read < 0 cid:%d fd=%d,n:%d", cid, parent_->get_fd(), n);
			parent_->push_block(cid, buf);
			parent_->handle_close();
			return 0;
		} else if (n == 0) { /// EOF
			LIB_LOG_ERROR("websocket read eof close cid:%d fd=%d", cid, parent_->get_fd());
			parent_->push_block(cid, buf);
			parent_->handle_close();
			return 0;
		} else {
			buf->set_write_idx(buf->get_write_idx() + n);
		}
	}

	if (push_recv_block(buf) == 0) {
		parent_->recv_handler(cid);
	} else {
		parent_->push_block(cid, buf);
	}

	return 0;
}

int Svc_Websocket::handle_send(void) {
	if (parent_->is_closed())
		return 0;

	int cid = parent_->get_cid();
	Block_Buffer *front_buf = 0;
	while (!send_block_list_.empty()) {
		front_buf = send_block_list_.front();
		
		//构建websocket数据帧头
		Block_Buffer *data_buf = make_websocket_frame(front_buf);
		size_t sum_bytes = data_buf->readable_bytes();
		int ret = ::write(parent_->get_fd(), data_buf->get_read_ptr(), sum_bytes);
		if (ret == -1) {
			LIB_LOG_ERROR("write cid:%d ip:%s port:%d errno:%d", cid, parent_->get_peer_ip().c_str(), parent_->get_peer_port(), errno);
			parent_->push_block(cid, data_buf);
			if (errno == EINTR) { //被打断, 重写
				continue;
			} else if (errno == EWOULDBLOCK) { //EAGAIN,下一次超时再写
				return ret;
			} else { //其他错误，丢掉该客户端全部数据
				parent_->handle_close();
				return ret;
			}
		} else {
			if ((size_t)ret == sum_bytes) { //本次全部写完, 尝试继续写
				parent_->push_block(cid, front_buf);
				parent_->push_block(cid, data_buf);
				send_block_list_.pop_front();
				continue;
			} else { //未写完, 丢掉已发送的数据, 下一次超时再写,front_buf没有发送成功，留到下一次继续发送
				parent_->push_block(cid, data_buf);
				return ret;
			}
		}
	}

	return 0;
}

int Svc_Websocket::handle_pack(Block_Vector &block_vec) {
	size_t rd_idx_orig = 0;
	int32_t cid = 0;
	uint8_t tmp = 0;
	uint8_t fin = 0;
	uint8_t opcode = 0;
	uint8_t mask = 0;
	uint8_t masking_key[4] = {};
	uint16_t payload_length = 0;
	Block_Buffer *front_buf = 0;
	Block_Buffer *free_buf = 0;

	while (! recv_block_list_.empty()) {
		front_buf = recv_block_list_.front();
		if (! front_buf) {
			LIB_LOG_ERROR("front_buf == 0");
			continue;
		}

		rd_idx_orig = front_buf->get_read_idx();
		cid = front_buf->read_int32();
		if(!websocket_connected_){
			return websocket_handshake(front_buf);
		}

		if (front_buf->readable_bytes() <= 0) { /// 数据块异常, 关闭该连接
			LIB_LOG_ERROR("cid:%d fd:%d, data block read bytes<0", cid, parent_->get_fd());
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
			return -1;
		}

		if (front_buf->readable_bytes() < 2 * sizeof(uint8_t)) { /// websocket包头长度不够
			front_buf->set_read_idx(rd_idx_orig);
			if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
				return 0;
			} else {
				parent_->push_block(parent_->get_cid(), free_buf);
				continue;
			}
		}

		tmp = front_buf->read_uint8();
		fin = tmp >> 7;
		opcode = tmp & 0x0f;
		if(opcode == OPCODE_CLOSE) { // websocket被客户端关闭
			LIB_LOG_ERROR("cid:%d fin:%d websocket on close.", cid, fin);
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
			return -1;
		}

		tmp = front_buf->read_uint8();
		mask = tmp >> 7;
		payload_length = tmp & 0x7f;
		if(payload_length == 126){
			if(front_buf->readable_bytes() < 2 * sizeof(uint8_t)){ //获取长度字节不够
				front_buf->set_read_idx(rd_idx_orig);
				if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
					return 0;
				} else {
					parent_->push_block(parent_->get_cid(), free_buf);
					continue;
				}
			}
			payload_length = front_buf->read_uint16();
		} else if(payload_length == 127){
			//payload_length = front_buf->read_uint32(); //包大小不可能达到4字节整数
		}
		if(mask == 1){
			if(front_buf->readable_bytes() < 4 * sizeof(uint8_t)){ //获取掩码字节不够
				front_buf->set_read_idx(rd_idx_orig);
				if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
					return 0;
				} else {
					parent_->push_block(parent_->get_cid(), free_buf);
					continue;
				}
			}
			for(int i = 0; i < 4; i++){
				masking_key[i] = front_buf->read_uint8();
			}
		}

		size_t data_len = front_buf->readable_bytes();
		if (payload_length == 0 || payload_length > max_pack_size_) { /// 包长度标识为0, 包长度标识超过max_pack_size_, 即视为无效包, 异常, 关闭该连接
			LIB_LOG_ERROR("cid:%d data block len = %u", parent_->get_cid(), payload_length);
			front_buf->log_binary_data(512);
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
			return -1;
		}

		if (data_len == (size_t)payload_length) {
			Block_Buffer *data_buf = get_websocket_payload(payload_length, masking_key, front_buf);
			recv_block_list_.pop_front();
			block_vec.push_back(data_buf);
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			continue;
		} else {
			if (data_len < (size_t)payload_length) {		//半包，需要合并前两个包
				front_buf->set_read_idx(rd_idx_orig);
				if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
					return 0;
				} else {
					parent_->push_block(parent_->get_cid(), free_buf);
					continue;
				}
			}

			if (data_len > payload_length) {				//粘包，需要拆分包
				size_t wr_idx_orig = front_buf->get_write_idx();

				Block_Buffer *data_buf = get_websocket_payload(payload_length, masking_key, front_buf);
				block_vec.push_back(data_buf);

				size_t cid_idx = front_buf->get_read_idx() - sizeof(int32_t);
				front_buf->set_read_idx(cid_idx);
				front_buf->set_write_idx(cid_idx);
				front_buf->write_int32(parent_->get_cid());
				front_buf->set_write_idx(wr_idx_orig);

				continue;
			}
		}
	}

	return 0;
}

Block_Buffer *Svc_Websocket::get_websocket_payload(int16_t payload_length, uint8_t *masking_key, Block_Buffer *buf) {
	int cid = parent_->get_cid();
	Block_Buffer *data_buf = parent_->pop_block(cid);
	data_buf->reset();
	data_buf->write_int32(cid);
	data_buf->write_int16(payload_length);

	for(int i = 0; i < payload_length; i++){
		int j = i % 4;
		uint8_t c = buf->read_uint8() ^ masking_key[j];
		data_buf->write_uint8(c);
	}
	return data_buf;
}

int Svc_Websocket::websocket_handshake(Block_Buffer *buf) {
	boost::unordered_map<std::string, std::string> header_map;
	char buff[4096] = {};
	size_t size = buf->readable_bytes();
	memcpy(buff, buf->get_read_ptr(), size);
	for(uint i = 0; i < size; i++){
		if(!strncmp("\r\n\r\n", buff + i, 4)){
			buf->set_read_idx(buf->get_read_idx() + i + 4);
			break;
		}
	}
	if(buf->readable_bytes() <= 0){
		recv_block_list_.pop_front();
		buf->reset();
		parent_->push_block(parent_->get_cid(), buf);
	}

	std::istringstream s(buff);
	std::string request;
	char respond[512] = {};
	std::getline(s, request);
	if (request[request.size()-1] == '\r') {
		request.erase(request.end()-1);
	} else {
		return -1;
	}

	std::string header;
	std::string::size_type end;
	while (std::getline(s, header) && header != "\r") {
		if (header[header.size()-1] != '\r') {
			continue; //end
		} else {
			header.erase(header.end()-1);   //remove last char
		}

		end = header.find(": ",0);
		if (end != std::string::npos) {
			std::string key = header.substr(0,end);
			std::string value = header.substr(end+2);
			header_map[key] = value;
		}
	}

	strcat(respond, "HTTP/1.1 101 Switching Protocols\r\n");
	strcat(respond, "Connection: upgrade\r\n");
	strcat(respond, "Upgrade: websocket\r\n");
	strcat(respond, "Sec-WebSocket-Accept: ");
	std::string server_key = header_map["Sec-WebSocket-Key"];
	server_key += MAGIC_KEY;

	SHA1 sha;
	unsigned int message_digest[5];
	sha.Reset();
	sha << server_key.c_str();
	sha.Result(message_digest);
	for (int i = 0; i < 5; i++) {
		message_digest[i] = htonl(message_digest[i]);
	}
	server_key = base64_encode(reinterpret_cast<const unsigned char*>(message_digest),20);
	server_key += "\r\n\r\n";
	strcat(respond, server_key.c_str());
	
	websocket_connected_ = true;
	//握手包必然是该连接第一个包，这里直接将响应内容写入内核发送缓存
	write(parent_->get_fd(), respond, strlen(respond));
	return 0;
}

Block_Buffer *Svc_Websocket::make_websocket_frame(Block_Buffer *buf, uint8_t *op) {
	uint8_t first_byte, second_byte;
	uint8_t fin = 0; 
	uint8_t opcode = 0;
	uint8_t mask = 0;
	Block_Buffer *data_buf = parent_->pop_block(parent_->get_cid());
	data_buf->reset();
	if(op != NULL){
		fin = *(op);
		opcode = *(op + 1);
	}
	else {
		fin = FRAME_FINAL;
		opcode = OPCODE_BINARY;
	}
	first_byte = (fin << 7) | opcode;
	data_buf->write_uint8(first_byte);
	int16_t msg_len = buf->readable_bytes();
	if(msg_len <= 125){
		second_byte = (mask << 7) | (uint8_t)msg_len;
		data_buf->write_uint8(second_byte);
	} else if(msg_len > 125 && msg_len < 65535){
		second_byte = (mask << 7) | 0x7e;
		data_buf->write_uint8(second_byte);
		data_buf->write_int16(msg_len);
	} else {
		//单个包大小有限制
	}
	data_buf->copy(buf->get_read_ptr(), msg_len);
	return data_buf;
}
