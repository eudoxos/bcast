#include <gio/gio.h>
#include <glib/gprintf.h>
#include <gio/gnetworking.h>

void set_broadcast(GSocket* sock, gboolean enable){
	int result=setsockopt(g_socket_get_fd(sock),SOL_SOCKET,SO_BROADCAST,(char*)&enable,sizeof(enable));
	if(result!=0) g_critical("setsockopt failed.");
}
void set_buffer_size(GPollFD* gpfd, int bufsize){
	#ifndef G_OS_WIN32
		int result = setsockopt (gpfd->fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof (bufsize));
	#else
		DWORD _buffer_size=bufsize;
		int result=setsockopt(gpfd->fd,SOL_SOCKET,SO_RCVBUF,(const char*) &_buffer_size,sizeof(_buffer_size));
	#endif
	if(result!=0) g_critical("setsockopt failed.");
}
#if 0
// DHCP discover packet
char payload[]={
/* op, htype, hlen, hops */ 0x01,0x01,0x06,0x00, 
/* id */ 0xaa,0xbb,0xcc,0xdd, 
/* secs, reserved */ 0,0,0,0, 
/* client ip */ 0,0,0,0,
/* your ip */   0,0,0,0,
/* server ip */ 0,0,0,0,
/* relay ip */  0,0,0,0,
/* MAC */ 0xaa,0xbb,0xcc,0xdd,0xee,0xff, 
/* padding */ 0,0,0,0, 0,0,0,0, 0,0, 
/* 64b: server hostname */
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
/* 128b: bootp filename */
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
/* magic cookie */ 0x63,0x82,0x53,0x63,
/* option: discover */ 0x35,0x01,0x03,
/* end */ 0xff,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};
const int dst_port=67;
const int listen_port=68;
#else
// GVCP discover packet
const char payload[]={/*type*/0x42,/*flags*/0x01,/*command*/0x00,0x02,/*size*/0x00,0x00,/*id*/0xff,0xff};
const int listen_port=3956;
//const int dst_port=3956;
#endif

/* there is bunch of leaks in here, I know */
int main(int argc, char** argv){
	if(argc<2){
		fprintf(stderr,"usage: %s IFACE_IP_ADDRESS\n",argv[0]);
		exit(1);
	}
	int result;
	/* create GPollFD, with one address only, to be polled later */
	GError *error=NULL;
	GSocket *sock=g_socket_new(G_SOCKET_FAMILY_IPV4,G_SOCKET_TYPE_DATAGRAM,G_SOCKET_PROTOCOL_UDP,NULL);
	GPollFD *fds=g_new(GPollFD,1);
	fds[0].fd=g_socket_get_fd(sock);
	fds[0].events=G_IO_IN;
	fds[0].revents=0;
	set_buffer_size(&fds[0],256*1024);
	/* bind to the interface */
	GSocketAddress *addr=g_inet_socket_address_new_from_string(argv[1],listen_port);
	if(!addr) { g_critical("parsing IP address %s failed.",argv[1]); return 1; }
	result=g_socket_bind(sock,addr,FALSE,&error);
	if(!result){ g_critical("g_socket_bind failed: %s",error->message); return 1; }

#if 0
	set_broadcast(sock,TRUE);
	/* send discovery packet */
	GSocketAddress* baddr = g_inet_socket_address_new_from_string("255.255.255.255",dst_port);
	g_socket_send_to(sock,baddr,&payload[0],sizeof(payload),NULL,&error);
	if(error){ g_critical("g_socket_send failed: %s",error->message); return 1; }
	fprintf(stderr,"Sent %d bytes discover to 255.255.255.255 over %s\n",(int)sizeof(payload),argv[1]);
	set_broadcast(sock,FALSE);
#endif

	/* poll sockets (just one, really) for 10s to receive reply */
	result=g_poll(&fds[0],1,10000);

	if(result==0){ g_critical("g_poll timed out."); exit(1); }
	else if(result<0){ g_critical("g_poll returned %d (error or call interrupted)",result); return 1; }

	/* got reply, print some information */
	char buffer[256*2014];
	g_socket_set_blocking(sock,FALSE);
	GInetSocketAddress* sender=g_malloc(sizeof(GInetSocketAddress));
	int count=g_socket_receive_from(sock,(GSocketAddress**)&sender,buffer,sizeof(buffer),NULL,&error);
	if(count==0){ g_critical("g_socket_received returned 0 (connection closed by peer)"); return 1; }
	if(count<0){ g_critical("g_socket_received redurned %d: %s",count,error->message); return 1; }
	char* saddr=g_inet_address_to_string(g_inet_socket_address_get_address(sender));
	fprintf(stderr,"Received %d bytes from %s.\n",count,saddr);

	return 0;
};
