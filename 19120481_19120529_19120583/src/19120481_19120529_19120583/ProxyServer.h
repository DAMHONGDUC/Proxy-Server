#pragma once
#ifndef _PROXYSERVER_H_
#define _PROXYSERVER_H_

#pragma comment(lib, "Ws2_32.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <WinSock2.h>
#include <process.h>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <ctime>


using namespace std;

#define PROXY_PORT  8888 //Port cua ProxyServer
#define BuffSize 4096 //Kich co cua mang buffer
#define MAX_THREADS	30 //So luong thread toi da


//Ham kiem tra request co la phuong thuc GET hay khong?
bool isGet_Method(const string& request);

//Ham kiem tra request co la phuong thuc POST hay khong?
bool isPost_Method(const string& request);

// VD www.unikey.vn -> unikey.vn ; unikey.vn -> unikey.vn 
string getName(string s);
//Ham load cac host nam trong file blacklist.conf vao vector<string>
//Ham tra ve vector<string> la mang gom cac host nam trong blacklist.conf.
vector<string> Load_BlackList();

//Ham kiem tra 1 host co nam trong file blacklist.conf ?
bool is_ForBidden(const string& host);

//Ham lay thong tin tu request ve cho webserver: port, host.
void Get_WebServer(const string& request, string& host, string& port);

//Ham nhan du lieu tu Socket Proxy_WebServer.
DWORD Receive(SOCKET& Proxy_WebServer, WSABUF& proxyRecvBuf, WSAOVERLAPPED& proxyRecvOverlapped, DWORD& Flag, DWORD timeOut, bool wait);

//Ham nhan Response tu Web Server va gui ve cho client.
void Get_Response_From_WebServer(const char* HostName, const char* request, SOCKET& Client_ProxyServer);

//Ham lay tung Socket Client_ProxyServer trong hang doi ra de xu ly.
//Nhan request tu Client, kiem tra request co tu host trong blacklist hay khong?

unsigned __stdcall requestThread(void*);

//Ham chay ProxyServer bao gom cac cong viec cua ProxyServer.
void runServer();

#endif

