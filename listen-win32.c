#include <gio/gio.h>
#include <glib/gprintf.h>
#include <gio/gnetworking.h>


// https://stackoverflow.com/a/16416421/761090
int main(int argc,char** argv){
	int listen_port=argc>2?atoi(argv[2]):3956;
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult=WSAStartup(MAKEWORD(2,2),&wsaData);
	if(iResult!=NO_ERROR){
		printf("Error at WSAStartup()\n");
		return 1;
	}

	// Create a socket.
	SOCKET m_socket[1];
	m_socket[0]=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

	if(m_socket[0]==INVALID_SOCKET){
		 printf("Error at socket(): %d\n",WSAGetLastError());
		 WSACleanup();
		 return 1;
	}

	// Bind the socket on 127.0.0.1
	struct sockaddr_in service[1];
	service[0].sin_family=AF_INET;
	service[0].sin_addr.s_addr=htonl(0x7f000001);
	service[0].sin_port=htons(listen_port);

	if(bind(m_socket[0],(SOCKADDR*)&service[0],sizeof(service[0]))==SOCKET_ERROR){
		printf( "bind() failed.\n" );
		closesocket(m_socket[0]);
		return 1;
	}
	fprintf(stderr,"Bound to %s: %d\n",inet_ntoa(service[0].sin_addr),ntohs(service[0].sin_port));

	char data[256];
	int bytes;

	WSAEVENT hEvent=WSACreateEvent();
	WSANETWORKEVENTS events;
	WSAEventSelect(*m_socket,hEvent,FD_READ | FD_WRITE);
	/* return value ignored */
	WSAWaitForMultipleEvents(1,&hEvent,FALSE,30000,FALSE);
	while(1){
		if(WSAEnumNetworkEvents(*m_socket,hEvent,&events) == SOCKET_ERROR) {
			fprintf(stderr,"Error");
		} else {
			if(events.lNetworkEvents & FD_READ){
				bytes=recv(*m_socket,data,256,0);
				fprintf(stderr,"Received %d bytes\n",bytes);
				return 0;
			}
		}
	}
	WSACloseEvent(hEvent);
}
