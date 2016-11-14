#include <iostream>
#include <string>
#include "websock.hpp"
#include <exception>

using namespace std;

RespMsg fof;

const int max_numfileperpage 6;

void accepthandle(int nfd, const sockaddr_storage& client_addr){
	
	if(!sock_recvtimeout(nfd, 3))return;
	//longest method is CONNECT, 7 chars
	ReqHeader req;
	if(!req.readfromsock(nfd)){
		cout<<"bad req\n"<<endl;
		fof.send(nfd);
		return;
	}
	if(req.method()=="GET"){
		string reqpath = "";
		if(req.path()=="/")reqpath = "./web/index.html";
		else reqpath = "."+req.path();
		//cout<<reqpath<<endl;
		ifstream reqfile(reqpath);
		RespFile reqfileresp(reqfile);
		if(!reqfileresp.ready()){
			fof.send(nfd);
			reqfile.close();
			return;
		}
		reqfileresp.send(nfd);
		reqfile.close();
		return;
	}
	fof.send(nfd);
}

int main(void){
	fof.setstatus("404","NotFound");
	TcpServer testserver(9999, true);
	testserver.startlisten(5, accepthandle, true);
	while(1){
		char userinput;
		cin>>userinput;
		if(userinput=='q'){testserver.stopListen();break;}
		else if(userinput=='l')testserver.printcurlist();
	}
	return 0;
}
