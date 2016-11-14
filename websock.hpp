#ifndef _WEBSOCK_H
#define _WEBSOCK_H

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <fstream>

#include <cerrno>
#include <cstring>
#include <ctime>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

class TcpServer{
private:
	int sfd;
	std::map<int, sockaddr_storage> recvnode;
	bool readylisten = false;
	bool stoplisten = false;
	std::mutex mux;
public:
	TcpServer(unsigned short, bool=false);
	void startlisten(int, void(*accept_callbac)(int, const sockaddr_storage&), bool=false);
	void stopListen(void){stoplisten=false;}
	void printcurlist(void);
};

class ReqHeader{
private:
	std::string _method;
	std::string _path;
	std::string _ver;
	std::map<std::string, std::string> _prop;
	std::string _content;
public:
	static const size_t method_name_max = 7;//Longest method name is CONNECT, which has 7 characters
	static const size_t path_length_max = 512;//I don't think I need more than 512.
	static const size_t ver_length_max = 8;// HTTP/1.1 would be the most often case.
	static const size_t prop_name_max = 64;
	static const size_t prop_value_max = 256;
	ReqHeader(void);
	bool readfromsock(int sfd);
	std::string method(void);
	std::string path(void);
	std::string ver(void);
	std::string salvage(void);
	std::string operator[](const std::string& pname);
};

class RespMsg{
private:
	std::string _msg;
	size_t _len;
	std::string _ver = "HTTP/1.1";
	std::string _code = "200";
	std::string _status = "OK";
	std::map<std::string,std::string> _prop;
public:
	RespMsg(const std::string& _message);
	RespMsg(void);
	bool send(int sfd);
	void setprop(const std::string& pname, const std::string& pvalue);
	void setstatus(const std::string&, const std::string&);
	std::string operator[](const std::string& pname);
};

class RespFile{
private:
	static const size_t msg_size_max = 2048;
	std::ifstream& _file;
	size_t _len;
	bool _ready = false;
	std::string _ver = "HTTP/1.1";
	std::string _code = "200";
	std::string _status = "OK";
	std::map<std::string,std::string> _prop;
public:
	RespFile(std::ifstream& file);
	void setprop(const std::string& pname, const std::string& pvalue);
	std::string operator[](const std::string& pname);
	bool getready(void);
	bool ready(void);
	bool send(int sfd);
};

//functions:
void perr(const std::string&);
bool sock_recvtimeout(int, int);
char sock_recvchar(int);
char sock_peekchar(int);
void getMyAddr(void);
std::string gmtstr(time_t *t);
#endif
