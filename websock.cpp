#include "websock.hpp"



TcpServer::TcpServer(unsigned short port, bool detach_if){
	std::thread start_thread([&](unsigned short _port){
		while((sfd = socket(AF_INET, SOCK_STREAM, 0))==-1){
			perr("socket");
			std::cout<<"try socket() in a second"<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout<<"socket created successfully"<<std::endl;
		struct sockaddr_in myaddr;
		memset(&myaddr, 0, sizeof myaddr);
		myaddr.sin_family = AF_INET;
		myaddr.sin_port = htons(_port);
		myaddr.sin_addr.s_addr = INADDR_ANY;
		while(bind(sfd, (struct sockaddr *)&myaddr, sizeof(myaddr))==-1){
			perr("bind");
			std::cout<<"try bind() in a second"<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout<<"bound on port "<<_port<<std::endl;
		readylisten = true;
	}, port);
	if(detach_if)start_thread.detach();
	else start_thread.join();
}

void TcpServer::startlisten(int backlog, void (*accept_callback)(int, const sockaddr_storage&), bool detach_if){
	while(!readylisten){
		std::cout<<"TcpServer not ready"<<std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	std::thread listen_thread([&](int _backlog, void (*_accept_callback)(int, const sockaddr_storage&)){
		while(listen(sfd, _backlog)==-1){
			perr("listen");
			std::cout<<"try listen() in a second"<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		std::cout<<"starts listening"<<std::endl;
		int nfd;
		struct sockaddr_storage client_addr;
		socklen_t client_addr_size = sizeof client_addr;
		while(!stoplisten){
			nfd=accept(sfd, (struct sockaddr *)&client_addr, &client_addr_size);
			if(nfd==-1){
				perr("accept");
				std::cout<<"try accept() in a second"<<std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
			recvnode[nfd]=client_addr;
			std::cout<<"connection established for "<<nfd<<std::endl;
			std::thread recvthread([&](int _nfd, const sockaddr_storage& _client_addr){
				_accept_callback(_nfd, _client_addr);
				mux.lock();
				::close(_nfd);
				std::cout<<"connection "<<_nfd<<" is closed"<<std::endl;
				recvnode.erase(_nfd);
				mux.unlock();
				//printcurlist();//so the problem is not recvnode. it is about passing reference!!!!
			}, nfd, recvnode[nfd]);
			recvthread.detach();
		}
		::close(sfd);
	}, backlog, accept_callback);
	if(detach_if)listen_thread.detach();
	else listen_thread.join();
}

void TcpServer::printcurlist(void){
	std::cout<<"current sockets:"<<std::endl;
	for(auto const& sock: recvnode){
		std::cout<<" socket:"<<sock.first<<std::endl;
	}
}

ReqHeader::ReqHeader(void):_method(""), _path(""), _ver(""), _content(""){_prop.clear();}

bool ReqHeader::readfromsock(int sfd){
	if(sock_peekchar(sfd)==0)return false;
	char chbuf;
	while((chbuf=sock_recvchar(sfd))!=32){
		_method+=chbuf;
		if(_method.size()>method_name_max) return false;//so now, it is 8-char-long. One more than we need. It shouldn't be a http request any more. We'll need to stop the process and salvage the data that we have already read from the socket.
	}
	while((chbuf=sock_recvchar(sfd))!=32){
		_path+=chbuf;
		if(_path.size()>path_length_max) return false;
	}
	while((chbuf=sock_recvchar(sfd))!=13){
		_ver+=chbuf;
		if(_ver.size()>ver_length_max) return false;
	}
	if(sock_recvchar(sfd)!=10) return false;
	while(1){
		std::string namebuf = "";
		while((chbuf=sock_recvchar(sfd))!=':'){
			namebuf+=chbuf;
			if(namebuf.size()>prop_name_max)return false;
		}
		_prop[namebuf] = "";
		while(sock_peekchar(sfd)==32)sock_recvchar(sfd);
		while((chbuf=sock_recvchar(sfd))!=13){
			_prop[namebuf]+=chbuf;
			if(_prop[namebuf].size()>prop_value_max)return false;
		}
		if(sock_recvchar(sfd)!=10)return false;
		if(sock_peekchar(sfd)==13){sock_recvchar(sfd);break;}
	}
	if(sock_recvchar(sfd)!=10)return false;
	return true;
}

std::string ReqHeader::method(void){ return _method;}
std::string ReqHeader::path(void){ return _path;}
std::string ReqHeader::ver(void){ return _ver;}
std::string ReqHeader::salvage(void){
	std::string salv = "";
	salv+=(_method+' ');
	salv+=(_path+' ');
	salv+=(_ver+"\r\n");
	for(auto const& property: _prop){
		salv+=(property.first+": ");
		salv+=(property.second+"\r\n");
	}
	salv+="\r\n";
	return salv;
}
std::string ReqHeader::operator[](const std::string& pname){ return _prop[pname];}

RespMsg::RespMsg(void):_msg(""),_len(0){
}

RespMsg::RespMsg(const std::string& _message):_msg(_message){
	_len = _msg.size();
}

bool RespMsg::send(int sfd){
	const std::string firstline(_ver+' '+_code+' '+_status+"\r\n");
	if((size_t)::send(sfd, firstline.data(), firstline.size(), 0)!=firstline.size()){perr("send()");return false;}
	_prop["Content-Length"] = std::to_string(_len);
	time_t now;
	time(&now);
	_prop["Date"] = gmtstr(&now);
	for(const auto& property: _prop){
		const std::string propline(property.first+": "+property.second+"\r\n");
		if((size_t)::send(sfd, propline.data(), propline.size(), 0)!=propline.size()){perr("send()");return false;}
	}
	const char lastline[2] = {'\r', '\n'};
	if(::send(sfd, &lastline, 2, 0)!=2){perr("send()");return false;}
	if((size_t)::send(sfd, _msg.data(), _len, 0)!=_len){perr("send()");return false;}
	return true;
}

void RespMsg::setprop(const std::string& pname, const std::string& pvalue){
	_prop[pname] = pvalue;
}

void RespMsg::setstatus(const std::string& code, const std::string& status){
	_code = code;
	_status = status;
}


RespFile::RespFile(std::ifstream& file):_file(file){
	getready();
}

bool RespFile::getready(void){
	if(_file.is_open()){
		_file.seekg(0, _file.end);
		_len = _file.tellg();
		_file.seekg(0, _file.beg);
		_ready = true;
		return true;
	}
	return false;
}

bool RespFile::ready(void){return _ready;}

bool RespFile::send(int sfd){
	const std::string firstline(_ver+' '+_code+' '+_status+"\r\n");
	if((size_t)::send(sfd, firstline.data(), firstline.size(), 0)!=firstline.size()){perr("send()-RespFile-1-");return false;}
	_prop["Content-Length"] = std::to_string(_len);
	time_t now;
	time(&now);
	_prop["Date"] = gmtstr(&now);
	for(const auto& property: _prop){
		const std::string propline(property.first+": "+property.second+"\r\n");
		if((size_t)::send(sfd, propline.data(), propline.size(), 0)!=propline.size()){perr("send()-RespFile-2-");return false;}
	}
	const char lastline[2] = {'\r', '\n'};
	if(::send(sfd, &lastline, 2, 0)!=2){perr("send()-RespFile-3-");return false;}
	char sendbuf[msg_size_max];
	do{
		memset(&sendbuf, 0, sizeof sendbuf);
		_file.read(sendbuf, msg_size_max);
		if(::send(sfd, sendbuf, _file.gcount(), 0)<(int)_file.gcount()){perr("send()-RespFile-4-");return false;}
	}while(_file.gcount()==msg_size_max);
	return true;
}

void RespFile::setprop(const std::string& pname, const std::string& pvalue){
	_prop[pname] = pvalue;
}

std::string RespFile::operator[](const std::string& pname){return _prop[pname];}
	

void perr(const std::string& message){
	std::cout<<message<<':'<<strerror(errno)<<std::endl;
}

bool sock_recvtimeout(int sfd, int seconds){
	timeval tlen;
	tlen.tv_sec = seconds;
	tlen.tv_usec = 0;
	if(setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tlen, sizeof tlen)==-1){
		perr("setsockopt:SO_RCVTIMEO");
		return false;
	}
	return true;
}

char sock_recvchar(int sockfd){
	char x;
	if(recv(sockfd, &x, sizeof x, 0)==-1) if(errno!=EWOULDBLOCK)perr("recv-recvchar");
	return x;
}

char sock_peekchar(int sockfd){
	char x;
	if(recv(sockfd, &x, sizeof x, MSG_PEEK)==-1) if(errno!=EWOULDBLOCK)perr("recv_peekchar");
	return x;
}

std::string gmtstr(time_t *t){
	char ts[30];
	memset(ts, 0, 30);
	tm *tt = gmtime(t);
	strftime(ts, 30, "%a, %d %b %Y %T GMT", tt);
	return std::string(ts);
}


/*
void getMyAddr(void){
	addrinfo myai;
	myai.ai_flags = AI_PASSIVE;
}
*/
