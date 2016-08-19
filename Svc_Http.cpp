/*
 * Svc_Http.cpp
 *
 *  Created on: Aug 16,2016
 *      Author: zhangyalei
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <string.h>
#include "Svc_Http.h"
#include "Http_Parser_Wrap.h"

Svc_Http::Svc_Http(): Svc_Handler() {}

Svc_Http::~Svc_Http() {}

int Svc_Http::handle_recv(void) {
	if (parent_->is_closed())
		return 0;

	int cid = parent_->get_cid();
	Block_Buffer *buf = parent_->pop_block(cid);
	if (! buf) {
		LIB_LOG_ERROR("http pop_block fail, cid:%d", cid);
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

			LIB_LOG_ERROR("tcp read < 0 cid:%d fd=%d,n:%d", cid, parent_->get_fd(), n);
			parent_->push_block(cid, buf);
			parent_->handle_close();
			return 0;
		} else if (n == 0) { /// EOF
			LIB_LOG_ERROR("tcp read eof close cid:%d fd=%d", cid, parent_->get_fd());
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

int Svc_Http::handle_send(void) {
	if (parent_->is_closed())
		return 0;

	int cid = parent_->get_cid();
	Block_Buffer *front_buf = 0;
	while (!send_block_list_.empty()) {
		front_buf = send_block_list_.front();

		//构建http消息头
		make_http_head(front_buf);
		size_t sum_bytes = front_buf->readable_bytes();
		int ret = ::write(parent_->get_fd(), front_buf->get_read_ptr(), sum_bytes);
		if (ret == -1) {
			LIB_LOG_ERROR("write cid:%d ip:%s port:%d errno:%d", cid, parent_->get_peer_ip().c_str(), parent_->get_peer_port(), errno);
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
				send_block_list_.pop_front();
				continue;
			} else { //未写完, 丢掉已发送的数据, 下一次超时再写,front_buf没有发送成功，留到下一次继续发送
				return ret;
			}
		}
	}

	return 0;
}

int Svc_Http::handle_pack(Block_Vector &block_vec) {
	int32_t cid = 0;
	Block_Buffer *front_buf = 0;

	while (! recv_block_list_.empty()) {
		front_buf = recv_block_list_.front();
		if (! front_buf) {
			LIB_LOG_ERROR("front_buf == 0");
			continue;
		}

		cid = front_buf->read_int32();
		if (front_buf->readable_bytes() <= 0) { /// 数据块异常, 关闭该连接
			LIB_LOG_ERROR("cid:%d fd:%d, data block read bytes<0", cid, parent_->get_fd());
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
			return -1;
		}

		Http_Parser_Wrap http_parser;
		http_parser.parse_http_content(front_buf->get_read_ptr(), front_buf->readable_bytes());

		Block_Buffer *data_buf = parent_->pop_block(cid);
		data_buf->reset();
		data_buf->write_int32(cid);
		data_buf->write_string(http_parser.get_body_content());
		block_vec.push_back(data_buf);
		recv_block_list_.pop_front();
	}

	return 0;
}

void Svc_Http::make_http_head(Block_Buffer *buf) {
	std::string str_content = buf->read_string();
	int content_len = str_content.length();
  char str_http[1024];
  snprintf(str_http, 1024, HTTP_RESPONSE_HTML, content_len, str_content.c_str());
	buf->write_string(str_http);
	LIB_LOG_INFO("http msg:%s%s", "\r\n", str_http);
}
