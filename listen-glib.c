#include <gio/gio.h>
#include <glib/gprintf.h>
#include <gio/gnetworking.h>
#include <glib/gmain.h>

#ifdef G_OS_WIN32
#include "gpoll_local.h"
#endif

/* there is bunch of leaks in here, I know */
int main(int argc, char** argv){
	// address is localhost by default
	char* addr_str=argc>1?argv[1]:"127.0.0.1";
	int listen_port=argc>2?atoi(argv[2]):3956;
	int result;

	/*
	Sets GLib's _g_main_poll_debug to true if env G_MAIN_POLL_DEBUG is defined, under Windows;
	that enables debugging messages from g_poll; the context is otherwise not used at all.
	*/
	GMainContext *ctx=g_main_context_new();

	/* create GPollFD, with one address only, to be polled later */
	GError *error=NULL;
	GSocket *sock=g_socket_new(G_SOCKET_FAMILY_IPV4,G_SOCKET_TYPE_DATAGRAM,G_SOCKET_PROTOCOL_UDP,NULL);
	GPollFD *fds=g_new(GPollFD,1);
	fds[0].fd=g_socket_get_fd(sock);
	fds[0].events=G_IO_IN;
	fds[0].revents=0;

	/* bind to the interface */
	GSocketAddress *addr=g_inet_socket_address_new_from_string(addr_str,listen_port);
	if(!addr) { g_critical("parsing IP address %s failed.",addr_str); return 1; }
	result=g_socket_bind(sock,addr,FALSE,&error);
	if(!result){ g_critical("g_socket_bind failed: %s",error->message); return 1; }
	fprintf(stderr,"Bound to %s:%d\n",addr_str,listen_port);

	/* poll sockets (just one, really) for 30s to receive reply */
	#ifdef G_OS_WIN32
		/* call local version, copied and adjusted from glib sources */
		result=L_g_poll(&fds[0],1,1000);
	#else
		result=g_poll(&fds[0],1,1000);
	#endif
	/* some error */
	if(result==0){ g_critical("g_poll timed out."); exit(1); }
	else if(result<0){ g_critical("g_poll returned %d (error or call interrupted)",result); return 1; }
	/* no error, print some information */
	char buffer[1024];
	g_socket_set_blocking(sock,FALSE);
	GInetSocketAddress* sender=g_malloc(sizeof(GInetSocketAddress));
	int count=g_socket_receive_from(sock,(GSocketAddress**)&sender,buffer,sizeof(buffer),NULL,&error);
	if(count==0){ g_critical("g_socket_received returned 0 (connection closed by peer)"); return 1; }
	if(count<0){ g_critical("g_socket_received returned %d: %s",count,error->message); return 1; }
	char* saddr=g_inet_address_to_string(g_inet_socket_address_get_address(sender));
	fprintf(stderr,"Received %d bytes from %s.\n",count,saddr);

	return 0;
};
