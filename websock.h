#ifndef _WEBSOCK_H
#define _WEBSOCK_H

#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <map>

#include <cerrno>
#include <cstring>

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
	bool readylisten;
	bool stoplisten;
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

//functions:
void perr(const std::string&);
bool sock_recvtimeout(int, int);
char sock_recvchar(int);
char sock_peekchar(int);
void getMyAddr(void);
#endif
