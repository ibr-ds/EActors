/**
\file
\brief 

*/

#include <fw.h>
#include "payload.h"

#include <mbedtls/config.h>

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/certs.h>
#include <mbedtls/x509.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>

#define HTTP_200 \
    "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n"

#define HTTP_400 \
    "HTTP/1.0 400 Bad Request"

#define HTTP_404 \
    "HTTP/1.0 404 Not Found"


#include "dir.h"

//#define ADEBUG

extern void printa(const char *fmt, ...);

enum state_e {
	INIT,
	ACCEPTING,
	READY,
	WAIT_READING,
	WAIT_WRITING,
};

struct ssl_server_s {
	mbedtls_net_context listen_fd, client_fd;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt srvcert;
	mbedtls_x509_crt cachain;
	mbedtls_pk_context pkey;
};

struct tsoc_desc {
	queue *pool;
	queue *accepter;
	queue *reader;
	queue *closer;
	queue *writer;
	queue *me;
	queue *conn;

	int id;

	queue *online;
	queue online_pool;

	queue my_tasks;

	enum state_e state;

	struct ssl_server_s ssl_serv;
};

#define MAX_CLIENTS	(MAX_GBOXES-200)

enum c_state {
	C_READY = 0,
	C_WAIT_READING,
	C_WAIT_WRITING,
	C_NULL,
};

enum c_ssl_state {
	C_SSL_INIT = 0,
	C_SSL_HANDSHAKE,
	C_SSL_READ,
	C_SSL_WRITE,
	C_SSL_WRITE_REST,
	C_SSL_CLOSE,
	C_SSL_NONE
};

enum mime_type {
	MIME_HTML = 0,
	MIME_SVG,
	MIME_PNG,
	MIME_JPG,
	MIME_GIF,
	MIME_JS,
	MIME_CSS,
	MIME_FILE
};

char *mimes[] = {
	"text/html",
	"image/svg+xml",
	"image/png",
	"image/jpeg",
	"image/gif",
	"application/javascript",
	"text/css",
	"application/octet-stream",
};


struct client_s {
	int sock_fd;

	int id;
	queue *mbox;
	enum c_state state;
	enum c_ssl_state ssl_state;
	queue *pool;

	mbedtls_ssl_context ssl;
	struct actor_s *owner;

	char *file;
	int len;
	int step;
	enum mime_type mime;
};


enum mime_type parse_mime(char *in) {
	if(strstr(in, ".htm") != NULL)
		return MIME_HTML;

	if(strstr(in, ".svg") != NULL)
		return MIME_SVG;

	if(strstr(in, ".png") != NULL)
		return MIME_PNG;

	if(strstr(in, ".jpeg") != NULL)
		return MIME_JPG;

	if(strstr(in, ".gif") != NULL)
		return MIME_GIF;

	if(strstr(in, ".js") != NULL)
		return MIME_JS;

	if(strstr(in, ".css") != NULL)
		return MIME_CSS;

	return MIME_FILE;
}

int parse_fname(char *dst, char *in) {
	char *begin = strstr(in, "/");
	if(begin == NULL)
		return -1;

	char *end = strstr(begin, " ");
	if(end == NULL)
		return -1;

	memcpy(dst, &begin[0], end-begin);
	dst[end-begin] = '\0';
	return end-begin;
}

int connector(struct actor_s *self) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;
	node *tmp = NULL, *tmp2 = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];
	struct client_s *client = NULL;
	int id, fail, filled = 0;

	switch(desc->state) {
		case READY:
			asleep(1000000);
		case INIT:
			tmp = pop_front(desc->conn);
			if( tmp != NULL)
				push_front(&desc->online_pool, tmp);

			tmp = pop_front(desc->pool);
			if (tmp == NULL) {
				printa("wtf>!\n");while(1);
			}

			tcp_sock = (struct tcp_sock *) tmp->payload;
			tcp_sock->sockfd = -1;
			tcp_sock->mbox = desc->me;
			push_front(desc->accepter, tmp);

			desc->state = ACCEPTING;
			return 1;
		case ACCEPTING:
			tmp = pop_front(desc->me);
			if (tmp == NULL) {
				asleep(10000);
				return 1;
			}

			tcp_sock = (struct tcp_sock *) tmp->payload;
			if(tcp_sock->sockfd == -1) {
				push_front(desc->pool, tmp);
				asleep(10000);
				desc->state = INIT;
				return 1;
			}

			tmp2 = pop_front(&desc->online_pool);
			client = (struct client_s *) tmp2->payload;
#ifdef ADEBUG
			printa("got accept sock = %d, belongs to %d\n", tcp_sock->sockfd, client->id);
#endif
			client->sock_fd = tcp_sock->sockfd;
			client->state = C_READY;
			client->step = 0;
			client->ssl_state = C_SSL_INIT;
			push_front(desc->online, tmp2);
			push_front(desc->pool, tmp);

			desc->state = INIT;
			return 1;
	}
}

static int ea_entropy_func( void *data, unsigned char *output, size_t len, size_t *olen ) {
	((void) data);
	sgx_read_rand((unsigned char *)output, len);
	*olen = len;
	return 0;
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv( void *ctx, unsigned char *buf, size_t len ) {
	int ret = 0;
	struct client_s *client = (struct client_s *)ctx;
	struct actor_s *gself = client->owner;
	struct tsoc_desc *desc = (struct tsoc_desc *) gself->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

#ifdef ADEBUG
	printa("NET_RECV: %d, %d, %d, %p %p\n", len, client->sock_fd, client->state, client->mbox,  desc->reader);
#endif

	if(len > REQ_SIZE)
		len = REQ_SIZE;


	switch(client->state) {
		case C_READY:
			tmp = pop_front(desc->pool);
//todo return
			to_read = (struct req *) tmp->payload;
			memset(to_read->data, 0 , sizeof(to_read->data));
			to_read->size = len;
			to_read->sockfd = client->sock_fd;
			to_read->mbox = client->mbox;

			push_back(desc->reader, tmp);
			client->state = C_WAIT_READING;

			return MBEDTLS_ERR_SSL_WANT_READ;
		case C_WAIT_READING:
			tmp = pop_front(client->mbox);
			if(tmp == NULL) {
				return MBEDTLS_ERR_SSL_WANT_READ;
			}

			to_read = (struct req *) tmp->payload;
			if (to_read->size == -2) {
				client->state = C_READY;
				push_front(desc->pool, tmp);
				return MBEDTLS_ERR_SSL_WANT_READ;
			}

			if (to_read->size == -1) {
				client->state = C_READY;
				push_front(desc->pool, tmp);
    				return MBEDTLS_ERR_NET_RECV_FAILED;
			}

#ifdef ADEBUG
			printa("incomming message '%s' size = %d\n", to_read->data, to_read->size);
#endif

			memcpy(buf, to_read->data, to_read->size);

			push_front(desc->pool, tmp);

			client->state = C_READY;
			return to_read->size;
		default:
			printa("[%d] wrong state, redevelop\n",__LINE__);while(1);
	}
}

int mbedtls_net_send( void *ctx, const unsigned char *buf, size_t len ) {
	int ret = 0;
	struct client_s *client = (struct client_s *)ctx;
	struct actor_s *gself = client->owner;
	struct tsoc_desc *desc = (struct tsoc_desc *) gself->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

#ifdef ADEBUG
	printa("NET_SEND: %d, %d, %d, %p %p\n", len, client->sock_fd, client->state, client->mbox,  desc->writer);
#endif

	if(len > REQ_SIZE)
		len = REQ_SIZE;

	switch(client->state) {
		case C_READY:
			tmp = pop_front(desc->pool);

			to_read = (struct req *) tmp->payload;
			memset(to_read->data, 0 , sizeof(to_read->data));
			to_read->size = len;
			to_read->sockfd = client->sock_fd;
			to_read->mbox = client->mbox;
			memcpy(to_read->data, buf, len);

			push_back(desc->writer, tmp);
			client->state = C_WAIT_WRITING;
///
			return MBEDTLS_ERR_SSL_WANT_WRITE;
		case C_WAIT_WRITING:
			tmp = pop_front(client->mbox);
			if(tmp == NULL) {
				return MBEDTLS_ERR_SSL_WANT_WRITE;
			}
			to_write = (struct req *) tmp->payload;

			if (to_write->size < 0) {
				client->state = C_READY;
				push_front(desc->pool, tmp);
    				return MBEDTLS_ERR_NET_SEND_FAILED;
			}

			client->state = C_READY;
			push_front(desc->pool, tmp);
			return(to_write->size);
		default:
			printa("[%d] wrong state, redevelop\n",__LINE__);while(1);
	}
}

int act_ssl(struct client_s *client, struct ssl_server_s *ssl_serv) {
	int ret = 0;
	int len;
	char buf[PL_SIZE];

	unsigned long size;
	void *file = NULL;

	const char *current_filename;
	struct cpio_header *header, *next;
	void *result;
	int error;

	char fname[200];

	switch(client->ssl_state) {
		case C_SSL_INIT:
			mbedtls_ssl_init( &client->ssl );
#ifdef ADEBUG
			printa( " Setting up SSL/TLS data\n");
#endif
			if( ( ret = mbedtls_ssl_setup( &client->ssl, &ssl_serv->conf ) ) != 0 ) {
				printa( "  failed: mbedtls_ssl_setup returned -0x%04x\n", -ret ); while(1);
			}

			mbedtls_ssl_set_bio( &client->ssl, client, mbedtls_net_send, mbedtls_net_recv, NULL );

			client->ssl_state = C_SSL_HANDSHAKE;
			break;
		case C_SSL_HANDSHAKE:
#ifdef ADEBUG
			printa( "  [ #%ld ] Performing the SSL/TLS handshake\n", client->sock_fd);
#endif
			if ( (ret = mbedtls_ssl_handshake( &client->ssl )) != 0 ) {
    				if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE ) {
        					printa( "  [ #%ld ]  failed: mbedtls_ssl_handshake returned -0x%04x\n", client->sock_fd, -ret );
						client->ssl_state = C_SSL_CLOSE;
						return 1;
    				}
				return 1;
			}
			client->ssl_state = C_SSL_READ;
			break;
		case C_SSL_READ:
#ifdef ADEBUG
			printa( "  [ #%ld ]  < Read from client\n", client->sock_fd );
#endif

			len = sizeof( buf ) - 1;
			memset( buf, 0, sizeof( buf ) );
			ret = mbedtls_ssl_read( &client->ssl, buf, len );

			if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
				return 1;

			if( ret <= 0 ) {
				switch( ret ) {
					case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
						printa( " connection was closed gracefully\n");
						client->ssl_state = C_SSL_CLOSE;
						return 1;
					case MBEDTLS_ERR_NET_CONN_RESET:
						printa( " connection was reset by peer\n");
						client->ssl_state = C_SSL_CLOSE;
						return 1;
					default:
						printa( "  [ #%ld ]  mbedtls_ssl_read returned -0x%04x\n", client->sock_fd, -ret );
						client->ssl_state = C_SSL_CLOSE;
						return 1;
				}
			}

			len = ret;
#ifdef ADEBUG
			printa( "  [ #%ld ]  %d bytes read\n=====\n%s\n=====\n",  client->sock_fd, len, (char *) buf );
#endif

//todo parse GET
			memset(fname, 0,sizeof(fname));
			if(parse_fname(fname, buf) <=0) {
				printa("malformed request '%s'\n", buf); //should be 400 but I am lasy
				client->ssl_state = C_SSL_CLOSE;
				return 1;
			} 
			do {
				printa("find file name '%s'\n", fname);

				if(fname[strlen(fname)-1] == '/') {
					memcpy(&fname[strlen(fname)], "index.html", strlen("index.html"));
					break;
				}

				if(strstr(fname, ".") == NULL) {
					memcpy(&fname[strlen(fname)], "/index.html", strlen("/index.html"));
					break;
				}
			} while(0);

#ifdef ADEBUG
			printa("final requested file: %s\n", fname);
#endif

//one more check
			if(fname[0] != '/') {
				printa("wrong name %s\n", fname); while(1);
			}
//get file from cpio
			file = cpio_get_file(dir_cpio, &fname[1], &size);
			if (file == NULL) {
				printa("Cannot find file '%s', my content:\n", &fname[1]);
				header = (struct cpio_header *) dir_cpio;
				while(header != NULL) {
					error = cpio_parse_header(header, &current_filename, &size, &result, &next);
					if (error)
						break;

					printa("%s\n", current_filename);
					header = next;
				}
				client->step = -1;
				client->len = -1;
				client->file = NULL;
			} else {
				printa("Found file '%s' at %p from %p size %d\n", fname, file, dir_cpio, size);
				client->file = file;
				client->len = size;
				client->step = 0;
				client->mime = parse_mime(fname);
			}

			client->ssl_state = C_SSL_WRITE;
			break;
		case C_SSL_WRITE:
#ifdef ADEBUG
			printa( "  [ #%ld ]  > Write to client:\n", client->sock_fd );
#endif

			if (client->file == NULL) {
				memcpy(buf, &HTTP_404, sizeof(HTTP_404));
				len = sizeof(HTTP_404);
			} else {
				snprintf(buf, sizeof(buf), HTTP_200, mimes[client->mime],client->len);
				len = strlen(buf);
			}

			ret = mbedtls_ssl_write( &client->ssl, buf, len );

			if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
				return 1;

			if(ret == MBEDTLS_ERR_NET_CONN_RESET) {
				printa( "  [ #%ld ]  failed: peer closed the connection\n", client->sock_fd);
				client->ssl_state = C_SSL_CLOSE;
				return 1;
			}

#ifdef ADEBUG
			printa( "  [%d][ #%ld ]  %d bytes written\n=====\n%s\n=====\n", __LINE__, client->sock_fd, len, (char *) buf );
#endif
			if(client->file == NULL)
				client->ssl_state = C_SSL_CLOSE;
			else
				client->ssl_state = C_SSL_WRITE_REST;
			break;
		case C_SSL_WRITE_REST:
			len = client->len / ((client->step+1)*REQ_SIZE) == 0 ? client->len % REQ_SIZE : REQ_SIZE;
			ret = mbedtls_ssl_write( &client->ssl, &client->file[client->step*REQ_SIZE], len );

			if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
				return 1;

			if( ret <= 0 ) {
//maybe we need to close the connection in some situations but not just drop the connection? e.g. reset?
				printa( "  [ #%ld ]  mbedtls_ssl_write returned -0x%04x\n", client->sock_fd, -ret );

				mbedtls_ssl_free( &client->ssl );

				client->ssl_state = C_SSL_NONE;
				return 1;
			}

#ifdef ADEBUG
			printa( "  [%d][ #%ld ]  %d bytes written\n=====\n%s\n=====\n", __LINE__, client->sock_fd, len, (char *) &client->file[client->step*1000] );
#endif

			if( (client->step+1) * REQ_SIZE < client->len ) {
				client->step++;
				client->ssl_state = C_SSL_WRITE_REST;
			} else
				client->ssl_state = C_SSL_CLOSE;
			break;
		case C_SSL_CLOSE:
#ifdef ADEBUG
			printa( "  [ #%ld ]  . Closing the connection...\n", client->sock_fd );
#endif
			ret = mbedtls_ssl_close_notify( &client->ssl );
			if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
				return 1;

			if(ret != 0) {
				printa( "  [ #%ld ]  failed: mbedtls_ssl_close_notify returned -0x%04x\n", client->sock_fd, ret );
//				while(1);
			}

			mbedtls_ssl_free( &client->ssl );

			client->ssl_state = C_SSL_NONE;
		case C_SSL_NONE:
			return 0;
		default:
			printa("unknown state %d at line %d\n", client->ssl_state, __LINE__);
			while(1);
	}
	return 1;
}


int server(struct actor_s *self) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;
	node *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL, *close_node = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL, *to_close = NULL;
	struct req *to_write = NULL;
	struct batched_req *br = NULL;
	char msg[512];
	struct client_s *client = NULL;
	struct batched_head *bhead = NULL;
	int id, fail, filled = 0, i;
/// SSL
	int ret;
	const char pers[] = "ssl_pthread_server";
///
	switch(desc->state) {
		case INIT:
////////////////
			mbedtls_x509_crt_init(&desc->ssl_serv.srvcert);
			mbedtls_x509_crt_init(&desc->ssl_serv.cachain);
			mbedtls_ssl_config_init(&desc->ssl_serv.conf);
			mbedtls_ctr_drbg_init( &desc->ssl_serv.ctr_drbg);

			mbedtls_entropy_init(&desc->ssl_serv.entropy);

			printa( "\n  . Loading the server cert. and key..." );
			ret = mbedtls_x509_crt_parse( &desc->ssl_serv.srvcert, (const unsigned char *) mbedtls_test_srv_crt, mbedtls_test_srv_crt_len );
			if( ret != 0 ) {
				printa( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret ); while(1);
			}

			ret = mbedtls_x509_crt_parse( &desc->ssl_serv.cachain, (const unsigned char *) mbedtls_test_cas_pem, mbedtls_test_cas_pem_len );
			if( ret != 0 ) {
				printa( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret ); while(1);
			}

			mbedtls_pk_init( &desc->ssl_serv.pkey );
			ret =  mbedtls_pk_parse_key( &desc->ssl_serv.pkey, (const unsigned char *) mbedtls_test_srv_key, mbedtls_test_srv_key_len, NULL, 0 );
			if( ret != 0 ) {
				printa( " failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret ); while(1);
			}

			printa( " ok\n" );

			printa( "  . Seeding the random number generator..." );

			if( ( ret = mbedtls_ctr_drbg_seed( &desc->ssl_serv.ctr_drbg, ea_entropy_func, &desc->ssl_serv.entropy, (const unsigned char *) pers, strlen( pers ) ) ) != 0 ) {
	    			    printa( " failed: mbedtls_ctr_drbg_seed returned -0x%04x\n", -ret ); while(1);
			}

			printa( " ok\n" );

			printa( "  . Setting up the SSL data...." );

			if( ( ret = mbedtls_ssl_config_defaults( &desc->ssl_serv.conf,
			        MBEDTLS_SSL_IS_SERVER,
			        MBEDTLS_SSL_TRANSPORT_STREAM,
			        MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 ) {
					printa( " failed: mbedtls_ssl_config_defaults returned -0x%04x\n", -ret ); while(1);
			}

			mbedtls_ssl_conf_rng( &desc->ssl_serv.conf, mbedtls_ctr_drbg_random, &desc->ssl_serv.ctr_drbg );

			mbedtls_ssl_conf_ca_chain( &desc->ssl_serv.conf, &desc->ssl_serv.cachain, NULL );
			if( ( ret = mbedtls_ssl_conf_own_cert( &desc->ssl_serv.conf, &desc->ssl_serv.srvcert, &desc->ssl_serv.pkey ) ) != 0 ) {
				printa( " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret );while(1);
			}

			printa( " ok\n" );

			printa( "  [ main ]  Waiting for a remote connection\n" );

///////////////
			desc->state = READY;
			break;
		case READY:
//firstly, we need to check a new clients and add the first one from the top
			tmp = pop_front(desc->online);
			if(tmp != NULL)
				push_front(&desc->my_tasks, tmp);

			tmp = pop_front(&desc->my_tasks);

			if(tmp == NULL) {
				asleep(100); //relax a bit
				return 1;
			}

			client = (struct client_s *) tmp->payload;
			client->owner = self; //should be inside
			ret = act_ssl(client, &desc->ssl_serv);

			if(ret == 0) {
				close_node = pop_front(desc->pool);

				to_close = (struct req *) close_node->payload;
				to_close->sockfd = client->sock_fd;
				to_close->mbox = desc->pool;
				push_front(desc->closer, close_node);

				client->state = C_NULL;

				push_back(desc->conn, tmp);
			} else
				push_back(&desc->my_tasks, tmp);

			desc->state = READY;
			break;
	}

	return 1;
}

int connector_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;

	desc->id = 0;

	desc->pool = gpool;
	desc->me = &gboxes[0];
	desc->conn = &gboxes[188];

	int i;
//
	queue_init(&desc->my_tasks);
	queue_init(&desc->online_pool);

	desc->online = (queue *) &gboxes[189];

	queue_init(desc->online);

	for(i = 0; i < MAX_CLIENTS; i++) {
		node *tmp = nalloc(desc->pool);
		struct client_s *client = (struct client_s *) tmp->payload;
		client->id = MAX_CLIENTS - i - 1; //because of queue
		client->state = C_NULL;
		client->mbox = &gboxes[200 + i];
		client->pool = &self->gpool_v2[i];

		push_front(&desc->online_pool, tmp);
	}
//spawn closer
	node *tmp_closer = nalloc(self->gboxes_v2);
	queue *closer_mbox = (queue *) tmp_closer->payload;
	spawn_service(gpool, gboxes, F_CLOSER, 0, 0, closer_mbox);
	desc->closer = closer_mbox;
//spawn reader
	node *tmp_reader = nalloc(self->gboxes_v2);
	queue *reader_mbox = (queue *) tmp_reader->payload;
	spawn_service(gpool, gboxes, F_READER, 0, 0, reader_mbox);
	desc->reader = reader_mbox;
//spawn writer
	node *tmp_writer = nalloc(self->gboxes_v2);
	queue *writer_mbox = (queue *) tmp_writer->payload;
	spawn_service(gpool, gboxes, F_WRITER, 0, 0, writer_mbox);
	desc->writer = writer_mbox;
//spawn accepter
	node *tmp_accepter = nalloc(self->gboxes_v2);
	queue *accepter_mbox = (queue *) tmp_accepter->payload;
	spawn_service(gpool, gboxes, F_ACCEPTER, 0, 4443, accepter_mbox);
	desc->accepter = accepter_mbox;
//
	desc->state = INIT;

	return 0;
}

int server_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;

	desc->id = 1;
	queue_init(&desc->my_tasks);
	desc->pool = gpool;
	desc->accepter = NULL;//I do not accept connections
	desc->me = &gboxes[desc->id];
	desc->conn = &gboxes[188];

	desc->online = (queue *)&gboxes[189];

	node *tmp_closer = nalloc(self->gboxes_v2);
	queue *closer_mbox = (queue *) tmp_closer->payload;
	spawn_service(gpool, gboxes, F_CLOSER, 0, 0, closer_mbox);
	desc->closer = closer_mbox;

	node *tmp_reader = nalloc(self->gboxes_v2);
	queue *reader_mbox = (queue *) tmp_reader->payload;
	spawn_service(gpool, gboxes, F_READER, 0, 0, reader_mbox);
	desc->reader = reader_mbox;

	node *tmp_writer = nalloc(self->gboxes_v2);
	queue *writer_mbox = (queue *) tmp_writer->payload;
	spawn_service(gpool, gboxes, F_WRITER, 0, 0, writer_mbox);
	desc->writer = writer_mbox;

	desc->state = INIT;
	return 0;
}

