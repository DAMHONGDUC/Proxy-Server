#include "ProxyServer.h"


queue<SOCKET> Buff; //Hang doi.
HANDLE Queue_Mutex; //Mutex dung de han che quyen truy cap khi chay da luong.

//chuoi tra ve response "403 Forbidden" khi website nam trong file blacklist.
const char* forbidden = "HTTP/1.1 403 Forbidden\r\n\r\n<HTML>\r\n<BODY>\r\n<H1>403 Forbidden</H1>\r\n<H2>You don't have permission to access this server</H2>\r\n</BODY></HTML>\r\n";

//Load cac host nam trong file blacklist.conf vao vector<string> BlackList
vector<string> BlackList = Load_BlackList();

//Ham kiem tra request co la phuong thuc GET hay khong?
bool isGet_Method(const string& request)
{
	if (request.length() <= 2)
		return false;
	if ((request[0] == 'G') && (request[1] == 'E') && (request[2] == 'T'))
		return true;
	return false;
}

//Ham kiem tra request co la phuong thuc POST hay khong?
bool isPost_Method(const string& request)
{
	if (request.length() <= 3)
		return false;
	if ((request[0] == 'P') && (request[1] == 'O') && (request[2] == 'S') && (request[3] == 'T'))
		return true;
	return false;
}
// VD www.unikey.vn -> unikey.vn ; unikey.vn -> unikey.vn 
string getName(string s)
{
	string kq = "";

	if (s.substr(0, 4) == "www.") kq = s.substr(4, s.length() - 4);
	else kq = s;

	return kq;
}
//Ham tra ve vector<string> bao gom cac host nam trong blacklist.conf.
vector<string> Load_BlackList()
{
	vector<string> a;
	ifstream f;
	f.open("blacklist.conf", ios_base::in);
	if (f.is_open()) {
		while (!f.eof()) {
			string s;
			f >> s;
			a.push_back(getName(s));
		}
		f.close();
	}
	return a;
}

//Ham kiem tra 1 host co nam trong file blacklist ?
bool is_ForBidden(const string& host)
{
	for (int i = 0; i < BlackList.size(); i++)//Duyet qua tung phan tu trong vector.
	{
		if (getName(host) == BlackList[i]) //Neu host thuoc blackList
			return true;
	}
	return false;
}

//Ham lay thong tin tu request ve cho webserver : host, port 
void Get_WebServer(const string& request, string& host, string& port)
{
	int start = request.find("Host:") + 6; //vi tri bat dau cua host
	int end = request.substr(start).find("\r\n"); //vi tri ket thuc (so luong ky tu) cua host

	int k = (request.substr(start, end)).find(':');
	if (k == string::npos || k == -1)
	{ //Neu khong co dau ':' thi port mac dinh la 80
		host = request.substr(start, end);
		port = "80";
	}
	else //Nguoc lai, neu co ':' thi port sau dau ':'
	{
		host = request.substr(start, k);
		port = request.substr(start + k + 1, end - (k + 1));
	}
}

//Ham nhan du lieu tu Socket Proxy_WebServer.
DWORD Receive(SOCKET& Proxy_WebServer, WSABUF& proxyRecvBuf, WSAOVERLAPPED& proxyRecvOverlapped, DWORD& Flag, DWORD timeOut, bool wait)
{
	DWORD nread = 0;

	//Nhan du lieu
	int Wait = WSARecv(Proxy_WebServer, &proxyRecvBuf, 1, &nread, &Flag, &proxyRecvOverlapped, NULL);

	if (Wait == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING))
	{
		cout << "Error when receiving\n";
		return SOCKET_ERROR;
	}

	//Doi tin hieu
	Wait = WSAWaitForMultipleEvents(1, &proxyRecvOverlapped.hEvent, TRUE, timeOut, TRUE);

	if (Wait == WAIT_FAILED)
	{
		cout << "Wait failed" << endl;
		return SOCKET_ERROR;
	}
	// Nhan tin hieu.
	Wait = WSAGetOverlappedResult(Proxy_WebServer, &proxyRecvOverlapped, &nread, wait, &Flag);

	return nread;
}

//Ham nhan Response tu WebServer va gui ve cho Client 
void Get_Response_From_WebServer(const char* HostName, const char* request, SOCKET& Client_ProxyServer)
{
	//Tao socket Proxy_WebServer.
	SOCKET Proxy_WebServer;
	Proxy_WebServer = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	//
	struct hostent* pHost;
	pHost = gethostbyname(HostName);
	//
	SOCKADDR_IN clientAddr;
	memset(&clientAddr, 0, sizeof(clientAddr));

	int addrLen = sizeof(clientAddr);

	//Khoi tao cau truc SOCKADDR_IN
	clientAddr.sin_family = AF_INET; // SOCKET kieu IPv4.
	memcpy(&clientAddr.sin_addr, pHost->h_addr, pHost->h_length);
	clientAddr.sin_port = htons(80); // chuyen port tu host-byte order sang net-byte order.

	//Ket noi den WebServer thong qua socket Proxy_WebServer.
	if (connect(Proxy_WebServer, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR_IN)) != 0)
	{
		closesocket(Proxy_WebServer);
		return;
	}

	WSAOVERLAPPED SendOverlapped;
	DWORD Flag = 0;
	DWORD nSend = 0;
	WSABUF Client_Request_Buf;
	WSAEVENT sendEvent[1];

	//Thiet lap Client_Request_Buf.
	Client_Request_Buf.buf = (char*)request;
	Client_Request_Buf.len = strlen(request);

	//Thiet lap cau truc SendOverlapped
	sendEvent[0] = WSACreateEvent();
	SendOverlapped.hEvent = sendEvent[0];

	//Gui request len WebServer
	int Wait = WSASend(Proxy_WebServer, &Client_Request_Buf, 1, &nSend, Flag, &SendOverlapped, NULL);

	if (Wait == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING))
	{
		cout << "Error when receiving!\n";
		return;
	}

	DWORD nread = 0;
	char buf[BuffSize + 1];
	char sendBuf[BuffSize + 1];
	memset(buf, 0, sizeof(buf));
	memset(sendBuf, 0, sizeof(sendBuf));

	WSAOVERLAPPED Proxy_Recv_Overlapped;
	WSABUF Proxy_Recv_Buf;
	WSAEVENT Proxy_Recv_Event[1];
	//Thiet lap Proxy_Recv_Event.
	Proxy_Recv_Buf.buf = buf;
	Proxy_Recv_Buf.len = sizeof(buf);

	//Thiet lap Proxy_Recv_Overlapped.
	Proxy_Recv_Event[0] = WSACreateEvent();
	Proxy_Recv_Overlapped.hEvent = Proxy_Recv_Event[0];

	//Doi su kien sendEvent
	WSAWaitForMultipleEvents(1, sendEvent, FALSE, WSA_INFINITE, FALSE);

	Wait = WSAGetOverlappedResult(Proxy_WebServer, &SendOverlapped, &nSend, FALSE, &Flag);

	//Nhan response tu Web Server
	nread = recv(Proxy_WebServer, Proxy_Recv_Buf.buf, BuffSize, 0);

	do
	{
		memcpy(sendBuf, buf, sizeof(buf));
		send(Client_ProxyServer, sendBuf, nread, 0); //chuyen tiep cho client

		memset(buf, 0, sizeof(buf));
		//Tiep tuc nhan response tu WebServer voi timeout = 60s.
		nread = Receive(Proxy_WebServer, Proxy_Recv_Buf, Proxy_Recv_Overlapped, Flag, 60, FALSE);

		if ((nread <= 0) || (nread == SOCKET_ERROR))
		{
			cout << "Close socket!\n";
			break;
		}

	} while (true);

	//Dong socket
	cout << "Close socket!\n";
	shutdown(Proxy_WebServer, 2);
	closesocket(Proxy_WebServer);

	return;
}

//Ham lay tung Socket Client_ProxyServer trong hang doi ra de xu ly.
//Nhan request tu Clientva check request co tu host trong file blacklist ?
//Gui lai response cho Client.
unsigned __stdcall requestThread(void*)
{
	while (true)
	{
		SOCKET Client_Proxy;

		//Hang doi Queue_Mutex.
		WaitForSingleObject(Queue_Mutex, INFINITE);

		if (Buff.size() < 1)
		{
			ReleaseMutex(Queue_Mutex);
			continue;
		}
		// Co gang thu lai.
		try
		{
			Client_Proxy = Buff.front();
			Buff.pop();
			ReleaseMutex(Queue_Mutex);
		}
		catch (exception)
		{
			ReleaseMutex(Queue_Mutex);
			continue;
		}

		try {
			char buf[BuffSize + 1];
			memset(buf, 0, sizeof(buf));

			string data = "";

			//Nhan request tu client
			int recvLen = recv(Client_Proxy, buf, BuffSize, 0);
			if (recvLen <= 0)
			{
				return 0;
			}
			data += buf;

			while (recvLen >= BuffSize)
			{
				memset(buf, 0, sizeof(buf));
				recvLen = recv(Client_Proxy, buf, BuffSize, 0);
				data += buf;
			}

			string HostName;
			string Port;
			Get_WebServer(data, HostName, Port);

			if ((Port == "80") && ((isGet_Method(data)) || (isPost_Method(data)))) { //Chi ho tro HTTP (Port 80), GET va POST
				cout << data << endl;

				if (is_ForBidden(HostName)) //neu nam trong blacklist.
					send(Client_Proxy, forbidden, strlen(forbidden), 0); //Tra ve "403 forbidden".
				else
					Get_Response_From_WebServer(HostName.c_str(), data.c_str(), Client_Proxy);
			}
			shutdown(Client_Proxy, 2);
			closesocket(Client_Proxy);
		}
		catch (exception)
		{
			shutdown(Client_Proxy, 2);
			closesocket(Client_Proxy);
		}
	}
	return 1;

}
void runServer()
{
	WSAData wsaData;
	//Khoi tao Winsock2.2
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData))
	{
		cout << "Version not supported!\n";
		return;
	}

	SOCKET ProxyServer, Client_Proxy;
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	//Tao socket ProxyServer
	ProxyServer = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (ProxyServer == INVALID_SOCKET)
	{
		cout << "Invalid socket!\n";
		return;
	}

	//Khoi tao cau truc SOCKADDR_IN cua ProxyServer.
	serverAddr.sin_family = AF_INET; // ho cua dia chi
	serverAddr.sin_port = htons(PROXY_PORT); // port 8888
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // su dung bat ki giao dien nao

	//Bind socket cua ProxyServer.
	if (bind(ProxyServer, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cout << "Cannot bind to socket!\n";
		return;
	}

	//Chuyen doi trang thai ket noi sang listen.
	if (listen(ProxyServer, 25))
	{
		cout << "Listen failed!\n";
		return;
	}

	Queue_Mutex = CreateMutex(NULL, FALSE, NULL);
	const int ThreadNumber = MAX_THREADS;
	HANDLE threads[ThreadNumber];
	unsigned int threadId[ThreadNumber];

	// khoi tao va chay cac luong.
	for (int i = 0; i < ThreadNumber; ++i)
		threads[i] = (HANDLE)_beginthreadex(NULL, 0, requestThread, NULL, 0, &threadId[i]);

	//Lan luot nhan cac yeu cau va cho vao Queue_Mutex.
	while (true)
	{
		Client_Proxy = accept(ProxyServer, NULL, NULL);
		cout << "Accept" << endl;
		WaitForSingleObject(Queue_Mutex, INFINITE);
		Buff.push(Client_Proxy);
		ReleaseMutex(Queue_Mutex);
	}

	shutdown(ProxyServer, 2); //Dong ket noi.
	closesocket(ProxyServer);//Dong socket ProxyServer.
	WSACleanup();//Giai phong WinSock.
}