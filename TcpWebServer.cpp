#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string>
#include <string.h>
#include<fstream>
#include <ctime>

#define MAX_FILE_SIZE 20000
using namespace std;

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[1000];
	int len;
	int timeOfLastMsg = 0;
};

const int SERVER_PORT = 8080;
const int MAX_SOCKETS = 60;

const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;

const int OPTIONS = 1;
const int GET = 2;
const int HEAD = 3;
const int PUT = 4;
const int _DELETE = 5;
const int TRACE = 6;


bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
void PrepareFileResponse(string fileName, char* sendBuff, string connectionType);
void Options(char* SocketBuffer, char* sendBuff, string connectionType);
void Get(char* SocketBuffer, char* sendBuff, string connectionType);
void Head(char* SocketBuffer, char* sendBuff, string connectionType);
void Put(char* SocketBuffer, char* sendBuff, string connectionType);
void Delete(char* SocketBuffer, char* sendBuff, string connectionType);
void Trace(char * SocketBuffer, int socketBufferLen, char * sendBuff, string connectionType);
int GenerateHTTPHeader(char* sendBuff, const char* codeAndPhrase, int fileSize, string connectionType, string contentType = "text/html");
string GetConnectionType(string msg);
string GetFileName(char* buffer);
string GetBodyFromMessage(string msg);


struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Server: Error at WSAStartup()\n";
		return;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

 
	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	
	serverService.sin_addr.s_addr = INADDR_ANY;
	
	serverService.sin_port = htons(SERVER_PORT);

	
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&serverService, sizeof(serverService)))
	{
		cout << "Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	int currentTime = 0;
	cout << "Server is waiting: \n\n";
	while (true)
	{
		currentTime = (int)time(0);
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
			{
				FD_SET(sockets[i].id, &waitRecv);
			}
			else if (sockets[i].recv != LISTEN && sockets[i].recv != EMPTY && (currentTime - sockets[i].timeOfLastMsg) > 120)
			{
					removeSocket(i); // if two minutes past since last interaction with client, close the socket
			}
		}

		currentTime = (int)time(0);
		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
			{
				FD_SET(sockets[i].id, &waitSend);
			}
			else if (sockets[i].recv != LISTEN && sockets[i].recv != EMPTY && (currentTime - sockets[i].timeOfLastMsg) > 120)
			{
					removeSocket(i); // if two minutes past since last interaction with client, close the socket
			}
		}

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
				sockets[i].timeOfLastMsg = time(0); // every time a messgae arrive we log the time of the message to know if 2 minutes have past 
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	cout << "Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			sockets[i].timeOfLastMsg = time(0);
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	closesocket(sockets[index].id);
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr *)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;


		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = OPTIONS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[9], sockets[index].len - 9);
				sockets[index].len -= 9;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = GET;
				memcpy(sockets[index].buffer, &sockets[index].buffer[5], sockets[index].len - 5);
				sockets[index].len -= 5;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = HEAD;
				memcpy(sockets[index].buffer, &sockets[index].buffer[6], sockets[index].len - 6);
				sockets[index].len -= 6;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = PUT;
				memcpy(sockets[index].buffer, &sockets[index].buffer[5], sockets[index].len - 5);
				sockets[index].len -= 5;
				sockets[index].buffer[sockets[index].len] = '\0';
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = _DELETE;
				memcpy(sockets[index].buffer, &sockets[index].buffer[8], sockets[index].len - 8);
				sockets[index].len -= 8;
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = TRACE;
				return;
			}
		}
	}
}

void sendMessage(int index)
{
	char sendBuff[MAX_FILE_SIZE];
	sendBuff[0] = '\0';
	int bytesSent = 0;

	string connectionType = GetConnectionType(sockets[index].buffer);
	SOCKET msgSocket = sockets[index].id;

	if (sockets[index].sendSubType == OPTIONS)
	{
		Options(sockets[index].buffer, sendBuff, connectionType);
	}
	else if (sockets[index].sendSubType == GET)
	{
		Get(sockets[index].buffer, sendBuff, connectionType);
	}
	else if (sockets[index].sendSubType == HEAD)
	{
		Head(sockets[index].buffer, sendBuff, connectionType);
	}
	else if (sockets[index].sendSubType == PUT)
	{
		Put(sockets[index].buffer, sendBuff, connectionType);
	}
	else if (sockets[index].sendSubType == _DELETE)
	{
		Delete(sockets[index].buffer, sendBuff, connectionType);
	}
	else if (sockets[index].sendSubType == TRACE)
	{
		Trace(sockets[index].buffer, sockets[index].len, sendBuff, connectionType);
	}


	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}
	
	cout << "Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n\n";

	if (connectionType == "Connection: close" || connectionType == "Connection: Close")
	{
		removeSocket(index);
	}

	sockets[index].buffer[0] = '\0';
	sockets[index].len = 0;
	sockets[index].send = IDLE;
}

void PrepareFileResponse(string fileName, char* sendBuff, string connectionType)
{
	ifstream inFile(fileName, ifstream::binary);

	if (inFile)
	{
		inFile.seekg(0, inFile.end);
		int FileLength = inFile.tellg();
		inFile.seekg(0, inFile.beg);

		GenerateHTTPHeader(sendBuff, "200 OK", FileLength, connectionType);
		
		if (FileLength != 0)
		{
			strcat(sendBuff, "\0");

			char temp[MAX_FILE_SIZE];
			inFile.read(temp, FileLength);
			temp[FileLength] = '\0';

			strcat(sendBuff, temp);
		}
		inFile.close();
	}
	else
	{
		GenerateHTTPHeader(sendBuff, "404 Not Found", 0, connectionType);
	}
	
}

void Options(char* SocketBuffer, char* sendBuff, string connectionType)
{
	if (SocketBuffer[0] == ' ' || SocketBuffer[0] == '*')
	{
		GenerateHTTPHeader(sendBuff, "200 OK", 0, connectionType);
		return;
	}
	else
	{
		PrepareFileResponse("C:/Users/Arye/Desktop/files/options.txt", sendBuff, connectionType);
		return;
	}
}

void Get(char* SocketBuffer, char* sendBuff, string connectionType)
{
	string fileName = GetFileName(SocketBuffer);
	PrepareFileResponse(fileName, sendBuff, connectionType);
}

void Head(char * SocketBuffer, char * sendBuff, string connectionType)
{
	string fileName = GetFileName(SocketBuffer);

	ifstream inFile(fileName, ifstream::binary);

	if (inFile)
	{
		inFile.seekg(0, inFile.end);
		int FileLength = inFile.tellg();
		inFile.seekg(0, inFile.beg);

		GenerateHTTPHeader(sendBuff, "200 OK", FileLength, connectionType);
	inFile.close();
	}
	else
	{
		GenerateHTTPHeader(sendBuff, "404 Not Found", 0, connectionType);
	}
	
}

void Put(char * SocketBuffer, char * sendBuff, string connectionType)
{
	string fileName = GetFileName(SocketBuffer);
	char temp;
	int fileNameSize = fileName.size();

	if (fileNameSize >= 28)
	{
		temp = fileName[27];
		fileName[27] = '\0';
	}

	if (fileNameSize < 28 || strcmp(fileName.c_str(), "C:/Users/Arye/Desktop/files") != 0) // can create and change files only in one folder
	{
		GenerateHTTPHeader(sendBuff, "501 Not Implemented", 24, connectionType);
		strcat(sendBuff, "illigal folder in server");
		return;
	}
	fileName[27] = temp;
	
	string body = GetBodyFromMessage(SocketBuffer);

	bool isExist;
	ifstream inFile(fileName);
	if (inFile)
	{
		isExist = true;
		inFile.close();
	}
	else
	{
		isExist = false;
	}

	ofstream outFile(fileName, ofstream::binary);
	outFile << body;
	outFile.close();

	if(isExist)
	{
		GenerateHTTPHeader(sendBuff, "204 No Content", 0, connectionType);
	}
	else
	{
		GenerateHTTPHeader(sendBuff, "201 Created", 0, connectionType);
	}

	return;
}

void Delete(char * SocketBuffer, char * sendBuff, string connectionType)
{
	string fileName = GetFileName(SocketBuffer);
	char temp;
	int fileNameSize = fileName.size();

	if (fileNameSize >= 28)
	{
		temp = fileName[27];
		fileName[27] = '\0';
	}

	if (fileNameSize < 28 || strcmp(fileName.c_str(), "C:/Users/Arye/Desktop/files") != 0) // can delete files only in one folder
	{
		GenerateHTTPHeader(sendBuff, "501 Not Implemented", 24, connectionType);
		strcat(sendBuff, "illigal folder in server");
		return;
	}
	fileName[27] = temp;

	ifstream checkIfFileExist(fileName, ifstream::binary);
	bool exist;
	if(checkIfFileExist)
	{
		checkIfFileExist.close();
		exist = true;
	}
	else
	{
		exist = false;
	}

	if(!exist)
	{
		GenerateHTTPHeader(sendBuff, "404 Not Found", 0, connectionType);
		return;
	}

	else
	{
		int errorInDelete = remove(fileName.c_str());

		if (!errorInDelete)
		{
			GenerateHTTPHeader(sendBuff, "204 No Content", 0, connectionType);
		}
		else
		{
			GenerateHTTPHeader(sendBuff, "501 Not Implemented", 35, connectionType);
			strcat(sendBuff, "Could not delete the file specified");
		}
	}

	return;
}

void Trace(char * SocketBuffer,int socketBufferLen, char * sendBuff, string connectionType)
{
	SocketBuffer[socketBufferLen] = '\0';
	GenerateHTTPHeader(sendBuff, "200 OK", socketBufferLen, connectionType, "message/http");
	strcat(sendBuff, SocketBuffer);
	return;
}

int GenerateHTTPHeader(char* sendBuff, const char* codeAndPhrase, int bodySize, string connectionType, string typeOfContent)
{
	int size = 0;

	time_t _time = time(0);

	strcat(sendBuff, "HTTP/1.1 ");
	strcat(sendBuff, codeAndPhrase);
	strcat(sendBuff, "\r\nServer: Arye's Server");
	strcat(sendBuff, "\r\nDate: ");
	char* TheTime = ctime(&_time);
	TheTime[strlen(TheTime) - 1] = '\0';
	strcat(sendBuff, TheTime);
	strcat(sendBuff, "\r\nContent-Type: ");
	strcat(sendBuff, typeOfContent.c_str());
	strcat(sendBuff, "\r\nContent-Length: ");


	if (bodySize != 0)
	{
		strcat(sendBuff, to_string(bodySize).c_str());
	}
	else
	{
		strcat(sendBuff, "0");
	}
	if (connectionType.size() != 0)
	{
		strcat(sendBuff, "\r\n");
		strcat(sendBuff, connectionType.c_str());
	}
	strcat(sendBuff, "\r\n\r\n");
	return strlen(sendBuff);
}

string GetConnectionType(string msg)
{
	int pos = msg.find("Connection: ");
	string result;

	if (pos != 0)
	{

		for (int i = pos; msg[i] != '\r'; i++)
		{
			result.push_back(msg[i]);
		}
		result.push_back('\0');


	}
	return result;
}

string GetFileName(char* buffer)
{
	char fileName[100];
	int i = 0;

	while (buffer[i] != ' ' && i < 99)
	{
		fileName[i] = buffer[i];
		i++;
	}
	fileName[i] = '\0';

	return fileName;
}

string GetBodyFromMessage(string msg)
{
	int pos = msg.find("\r\n\r\n");

	if (pos == -1)
	{
		return nullptr;
	}
	pos += 4;

	string result;
	int size = msg.size();

	for (int i = pos; i < size; i++)
	{
		result.push_back(msg[i]);
	}

	return result;
}
