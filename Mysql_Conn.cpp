/*
 * Mysql_Conn.cpp
 *
 *  Created on: 2016年1月4日
 *      Author: zhangyalei
 */

#include <iostream>
#include <sstream>
#include <stdio.h>
#include "Mysql_Conn.h"
#include "Lib_Log.h"

#define MIN_DB_CONN_CNT		2

Mysql_DB_Manager* Mysql_DB_Manager::mysql_db_manager = NULL;

Mysql_DB_Conn::Mysql_DB_Conn(Mysql_DB_Pool* pPool) {
	m_pDBPool = pPool;
	conn = NULL;
	stmt = NULL;
}

Mysql_DB_Conn::~Mysql_DB_Conn() {
	if (conn){
		delete conn;
		conn = NULL;
	}

	if (stmt){
		delete stmt;
		stmt = NULL;
	}
}

int Mysql_DB_Conn::Init() {
	std::stringstream stream;
	stream << m_pDBPool->GetDBServerPort();
	std::string url = std::string("tcp://") + m_pDBPool->GetDBServerIP() + ":" + stream.str();
	try {
			conn = m_pDBPool->GetDriver()->connect(url.c_str(), m_pDBPool->GetUsername(), m_pDBPool->GetPasswrod());
			conn->setSchema(m_pDBPool->GetDBName());
			stmt = conn->createStatement();
			if (conn->isClosed()) {
				LOG_DEBUG("connect mysql error, url = [%s], user = [%s], pw = [%s]", url.c_str(), m_pDBPool->GetUsername().c_str(), m_pDBPool->GetPasswrod().c_str());
				return 1;
			}
	}
	catch (sql::SQLException &e) {
			int err_code = e.getErrorCode();
			LOG_DEBUG("SQLException, MySQL Error Code = %d, SQLState = [%s], [%s]", err_code, e.getSQLState().c_str(), e.what());
			return 2;
	}

	return 0;
}

sql::ResultSet* Mysql_DB_Conn::ExecuteQuery(const char* sql_query) {
	sql::ResultSet  *res;

	try {
		res = stmt->executeQuery(sql_query);
	}catch (sql::SQLException &e) {
			int err_code = e.getErrorCode();
			LOG_DEBUG("SQLException, MySQL Error Code = %d, SQLState = [%s], [%s]", err_code, e.getSQLState().c_str(), e.what());
			return NULL;
	}

	return res;
}

int Mysql_DB_Conn::ExecuteUpdate(const char* sql_query) {
	int ret;

	try {
		ret = stmt->executeUpdate(sql_query);
	}catch (sql::SQLException &e) {
		int err_code = e.getErrorCode();
		LOG_DEBUG("SQLException, MySQL Error Code = %d, SQLState = [%s], [%s]", err_code, e.getSQLState().c_str(), e.what());
		return -1;
	}

	return ret;
}

bool Mysql_DB_Conn::Execute(const char* sql_query) {
	int ret;

	try {
		ret = stmt->execute(sql_query);
	}catch (sql::SQLException &e) {
		int err_code = e.getErrorCode();
		LOG_DEBUG("SQLException, MySQL Error Code = %d, SQLState = [%s], [%s]", err_code, e.getSQLState().c_str(), e.what());
		return -1;
	}

	return ret;
}

std::string& Mysql_DB_Conn::GetPoolName() {
	return m_pDBPool->GetPoolName();
}

////////////////
Mysql_DB_Pool::Mysql_DB_Pool(std::string& pool_name,  std::string& db_server_ip, uint32_t db_server_port,
		std::string& username,  std::string& password,  std::string& db_name, int32_t max_conn_cnt) {
	m_pool_name = pool_name;
	m_db_server_ip = db_server_ip;
	m_db_server_port = db_server_port;
	m_username = username;
	m_password = password;
	m_db_name = db_name;
	m_db_max_conn_cnt = max_conn_cnt;
	m_db_cur_conn_cnt = MIN_DB_CONN_CNT;

	try {
	        driver = get_driver_instance();
	    } catch (sql::SQLException&e) {
	        perror("驱动连接出错;\n");
	    } catch (std::runtime_error&e) {
	        perror("运行出错了\n");
	   }
}

Mysql_DB_Pool::~Mysql_DB_Pool() {
	for (std::list<Mysql_DB_Conn*>::iterator it = m_free_list.begin(); it != m_free_list.end(); it++) {
		Mysql_DB_Conn* pConn = *it;
		delete pConn;
	}

	m_free_list.clear();
}

int Mysql_DB_Pool::Init() {
	for (int i = 0; i < m_db_cur_conn_cnt; i++) {
		Mysql_DB_Conn* pDBConn = new Mysql_DB_Conn(this);
		int ret = pDBConn->Init();
		if (ret) {
			delete pDBConn;
			return ret;
		}
		m_free_list.push_back(pDBConn);
	}

	LOG_DEBUG("db pool: %s, size: %d", m_pool_name.c_str(), (int)m_free_list.size());
	return 0;
}

Mysql_DB_Conn* Mysql_DB_Pool::GetDBConn() {
	m_thread_notify.Lock();

	while (m_free_list.empty()) {
		if (m_db_cur_conn_cnt >= m_db_max_conn_cnt) {
			m_thread_notify.Wait();
		} else {
			Mysql_DB_Conn* pDBConn = new Mysql_DB_Conn(this);
			int ret = pDBConn->Init();
			if (ret) {
				delete pDBConn;
				m_thread_notify.Unlock();
				return NULL;
			} else {
				m_free_list.push_back(pDBConn);
				m_db_cur_conn_cnt++;
				LOG_DEBUG("new db connection: %s, conn_cnt: %d", m_pool_name.c_str(), m_db_cur_conn_cnt);
			}
		}
	}

	Mysql_DB_Conn* pConn = m_free_list.front();
	m_free_list.pop_front();
	m_thread_notify.Unlock();

	return pConn;
}

void Mysql_DB_Pool::RelDBConn(Mysql_DB_Conn* pConn) {
	m_thread_notify.Lock();

	std::list<Mysql_DB_Conn*>::iterator it = m_free_list.begin();
	for (; it != m_free_list.end(); it++) {
		if (*it == pConn) {
			break;
		}
	}

	if (it == m_free_list.end()) {
		m_free_list.push_back(pConn);
	}

	m_thread_notify.Signal();
	m_thread_notify.Unlock();
}

/////////////////
Mysql_DB_Manager::Mysql_DB_Manager() {}

Mysql_DB_Manager::~Mysql_DB_Manager() {
	std::map<std::string, Mysql_DB_Pool*>::iterator 	it;
	for(it=m_dbpool_map.begin(); it!=m_dbpool_map.end(); ++it){
		delete it->second;
	}
	m_dbpool_map.clear();
}

Mysql_DB_Manager* Mysql_DB_Manager::instance() {
	if (!mysql_db_manager) {
		mysql_db_manager = new Mysql_DB_Manager();
	}

	return mysql_db_manager;
}

int Mysql_DB_Manager::Init(std::string& db_host, int db_port, std::string& db_username, std::string& db_password, std::string& db_name, std::string& pool_name, int db_maxconncnt) {
		Mysql_DB_Pool* pDBPool = new Mysql_DB_Pool(pool_name, db_host, db_port, db_username, db_password, db_name, db_maxconncnt);

		if (pDBPool->Init()) {
			delete pDBPool;
			LOG_ABORT("init db instance failed db_host:%s, db_port:%d, db_username:%s, db_password:%s, db_name:%s, pool_name:%s",
					db_host.c_str(), db_port, db_username.c_str(), db_password.c_str(), db_name.c_str(), pool_name.c_str());
			return -1;
		}
		m_dbpool_map.insert(make_pair(pool_name, pDBPool));

	return 0;
}

Mysql_DB_Conn* Mysql_DB_Manager::GetDBConn(std::string& dbpool_name) {
	std::map<std::string, Mysql_DB_Pool*>::iterator it = m_dbpool_map.find(dbpool_name);
	if (it == m_dbpool_map.end()) {
		return NULL;
	} else {
		return it->second->GetDBConn();
	}
}

void Mysql_DB_Manager::RelDBConn(Mysql_DB_Conn* pConn) {
	if (!pConn) {
		return;
	}

	std::map<std::string, Mysql_DB_Pool*>::iterator it = m_dbpool_map.find(pConn->GetPoolName());
	if (it != m_dbpool_map.end()) {
		it->second->RelDBConn(pConn);
	}
}
