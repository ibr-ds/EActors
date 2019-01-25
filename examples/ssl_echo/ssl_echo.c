/**
\file
\brief An example of TCP-IP echo service working over SSL

This is a echo services. Just use following command to connect: 

openssl s_client -connect localhost:5001

and type something. This example requires system actors 'acceptor', 'writer', 'reader' and 'closer'.

WARNING: This is a very, very scematic example, just to play with mbedtls. This example does not support multiple connections and very bed written.
See the http example for a better design and application.

*/

#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);

enum state_e {
	INIT,
	ACCEPTING,
	READY,
	WAIT_READING,
	WAIT_WRITING,
};

struct tsoc_desc {
	queue *queue;
	queue *accepter;
	queue *reader;
	queue *closer;
	queue *writer;
	queue *me;

	int sock;
	enum state_e state;
};


#define DEBUG


#include <mbedtls/config.h>

#define mbedtls_time       time
#define mbedtls_time_t     time_t
#define mbedtls_fprintf    fprintf
#define mbedtls_printf     printa

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/certs.h>
#include <mbedtls/x509.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>


#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif

#define DEBUG_LEVEL 0

static void my_debug( void *ctx, int level, const char *file, int line, const char *str ) {
	((void) level);
	printa("%s:%04d: %s\n", file, line, str );
}


int ret, len;
mbedtls_net_context listen_fd, client_fd;
unsigned char buf2[1024];
const char *pers = "ssl_server";

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_x509_crt srvcert;
mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif

struct actor_s *gself = NULL;

void mbedtls_net_init( mbedtls_net_context *ctx ) {
	printa("%s\n", __func__);
	ctx->fd = -1;
}

static int net_prepare( void ) {
	printa("%s\n", __func__);
	return( 0 );
}

int mbedtls_net_bind( mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto ) {
	int n, ret;

	printa("%s: IP:%s, PORT=%s, proto=%d\n", __func__, bind_ip, port, proto);

	if( ( ret = net_prepare() ) != 0 )
	    return( ret );

	return( ret );
}

void mbedtls_net_free( mbedtls_net_context *ctx ) {
	printa("%s\n", __func__);
	if( ctx->fd == -1 )
	    return;

#if 0
	shutdown( ctx->fd, 2 );
	close( ctx->fd );
#endif
	ctx->fd = -1;
}


int mbedtls_net_accept( mbedtls_net_context *bind_ctx, mbedtls_net_context *client_ctx, void *client_ip, size_t buf_size, size_t *ip_len ) {
	printa("%s\n", __func__);
	return( 0 );
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv( void *ctx, unsigned char *buf, size_t len ) {
	int ret = 0;
	struct tsoc_desc *desc = (struct tsoc_desc *) gself->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

	printa("NET_RECV: %d\n", len);

	tmp = pop_front(desc->queue);
//todo return
	to_read = (struct req *) tmp->payload;
	memset(to_read->data, 0 , sizeof(to_read->data));
	to_read->size = len;
	to_read->sockfd = desc->sock;
	to_read->mbox = desc->me;

	push_back(desc->reader, tmp);
///
	asleep(100);
///
	tmp = NULL;
	while(tmp == NULL)
		tmp = pop_front(desc->me);

	to_read = (struct req *) tmp->payload;
	if (to_read->size == -2) {
		push_front(desc->queue, tmp);
		return MBEDTLS_ERR_SSL_WANT_READ;
	}

	if (to_read->size == -1) {
		push_front(desc->queue, tmp);
    		return MBEDTLS_ERR_NET_RECV_FAILED;
	}


#ifdef DEBUG
	printa("incomming message '%s' size = %d\n", to_read->data, to_read->size);
#endif

	memcpy(buf, to_read->data, to_read->size);

	push_front(desc->queue, tmp);

	return to_read->size;
}

int mbedtls_net_send( void *ctx, const unsigned char *buf, size_t len ) {
	int ret = 0;
	struct tsoc_desc *desc = (struct tsoc_desc *) gself->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

	printa("NET_SEND: %d\n", len);

	tmp = pop_front(desc->queue);

	to_read = (struct req *) tmp->payload;
	memset(to_read->data, 0 , sizeof(to_read->data));
	to_read->size = len;
	to_read->sockfd = desc->sock;
	to_read->mbox = desc->me;
	memcpy(to_read->data, buf, len);

	push_back(desc->writer, tmp);
///
	asleep(100);
///
	tmp = NULL;
	while(tmp == NULL)
		tmp = pop_front(desc->me);

	to_write = (struct req *) tmp->payload;
	if (to_write->size < 0) {
		push_front(desc->queue, tmp);
		return MBEDTLS_ERR_NET_SEND_FAILED;
	}

	push_front(desc->queue, tmp);

	return( to_write->size );
}

static int ea_entropy_func( void *data, unsigned char *output, size_t len, size_t *olen )
{
	((void) data);

	sgx_read_rand((unsigned char *)output, len);

	*olen = len;

	return( 0 );
}



int server(struct actor_s *self) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

	switch(desc->state) {
		case INIT:
			tmp = pop_front(desc->queue);
			if (tmp == NULL)
				return 1;

			tcp_sock = (struct tcp_sock *) tmp->payload;
			tcp_sock->sockfd = -1;
			tcp_sock->mbox = desc->me;

			push_back(desc->accepter, tmp);
			desc->state = ACCEPTING;
			return 1;
		case ACCEPTING:
			tmp = pop_front(desc->me);
			if (tmp == NULL)
				return 1;

			tcp_sock = (struct tcp_sock *) tmp->payload;
			if(tcp_sock->sockfd == -1) {
				push_back(desc->queue, tmp);
				desc->state = INIT;
				return 1;
			}

			printa("got accept sock = %d\n", tcp_sock->sockfd);
			desc->sock = tcp_sock->sockfd;

///// SSL
			/*
			 * 3. Wait until a client connects
			 */
			mbedtls_printf( "  . Waiting for a remote connection ...\n" );
			
			if( ( ret = mbedtls_net_accept( &listen_fd, &client_fd, NULL, 0, NULL ) ) != 0 ) {
			        mbedtls_printf( " failed\n  ! mbedtls_net_accept returned %d\n\n", ret );
			}

			client_fd.fd = tcp_sock->sockfd;
			mbedtls_ssl_set_bio( &ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL );
			mbedtls_printf( " ok\n" );
/////
			push_back(desc->queue, tmp);
			desc->state = READY;
			return 1;
		case READY:
			/*
		         * 5. Handshake
		         */
			mbedtls_printf( "  . Performing the SSL/TLS handshake...\n" );

			if( (ret = mbedtls_ssl_handshake( &ssl ) ) != 0 ) {
				if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE ) {
					mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned %d\n\n", ret );
				}

				return 1;
			}

			mbedtls_printf( "handshake done\n" );

			desc->state = WAIT_READING;
			return 1;
		case WAIT_READING:

			/*
			 * 6. Read the HTTP Request
			 */
			mbedtls_printf( "  < Read from client:\n" );

			do {
				len = sizeof( buf ) - 1;
				memset( buf, 0, sizeof( buf ) );
				ret = mbedtls_ssl_read( &ssl, buf, len );

				if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
					return 1;

				if( ret <= 0 ) {
					switch( ret ) {
						case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
							mbedtls_printf( " connection was closed gracefully\n" );
							break;
						case MBEDTLS_ERR_NET_CONN_RESET:
							mbedtls_printf( " connection was reset by peer\n" );
							break;
						default:
							mbedtls_printf( " mbedtls_ssl_read returned -0x%x\n", -ret );
							break;
					}
					break;
				}

				mbedtls_printf( " %d bytes read\n\n%s", len, (char *) buf );

				if( ret > 0 )
		        		break;
			} while( 1 );

			desc->state = WAIT_WRITING;
			return 1;
		case WAIT_WRITING:

			/*
			 * 7. Write the 200 Response
			 */
			mbedtls_printf( "  > Write to client:\n" );

			len = snprintf( (char *) buf2, sizeof( buf2 ), "echo sais '%s'", buf);

			while( ( ret = mbedtls_ssl_write( &ssl, buf2, len ) ) <= 0 ) {
				if( ret == MBEDTLS_ERR_NET_CONN_RESET ) {
					mbedtls_printf( " failed\n  ! peer closed the connection\n\n" );
					break;
				}

				if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE ) {
					mbedtls_printf( " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret );
					break;
				}
			}

			len = ret;
			mbedtls_printf( " %d bytes written\n\n%s\n", len, (char *) buf );


			desc->state = WAIT_READING;
			return 1;
	}
	return 1;
}

int server_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;

	desc->queue = gpool;
	desc->accepter = &gboxes[ACCEPTER];
	desc->writer = &gboxes[WRITER];
	desc->reader = &gboxes[READER];
	desc->closer = &gboxes[CLOSER];
	desc->me = &gboxes[0];

	desc->sock = -1;
	desc->state = INIT;

////// SSL
	gself = self;

	mbedtls_net_init( &listen_fd );
	mbedtls_net_init( &client_fd );
	mbedtls_ssl_init( &ssl );
	mbedtls_ssl_config_init( &conf );
#if defined(MBEDTLS_SSL_CACHE_C)
	mbedtls_ssl_cache_init( &cache );
#endif
	mbedtls_x509_crt_init( &srvcert );
	mbedtls_pk_init( &pkey );
	mbedtls_entropy_init( &entropy );
	mbedtls_ctr_drbg_init( &ctr_drbg );

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold( DEBUG_LEVEL );
#endif

	/*
	 * 1. Load the certificates and private RSA key
        */
	mbedtls_printf( "\n  . Loading the server cert. and key...\n" );

        /*
         * This demonstration program uses embedded test certificates.
         * Instead, you may want to use mbedtls_x509_crt_parse_file() to read the
         * server and CA certificates, as well as mbedtls_pk_parse_keyfile().
         */
        ret = mbedtls_x509_crt_parse( &srvcert, (const unsigned char *) mbedtls_test_srv_crt,
                          mbedtls_test_srv_crt_len );
        if( ret != 0 ) {
		mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret );
		goto exit;
        }

	ret = mbedtls_x509_crt_parse( &srvcert, (const unsigned char *) mbedtls_test_cas_pem,
                          mbedtls_test_cas_pem_len );
	if( ret != 0 ) {
		mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret );
		goto exit;
	}

	ret =  mbedtls_pk_parse_key( &pkey, (const unsigned char *) mbedtls_test_srv_key,
                         mbedtls_test_srv_key_len, NULL, 0 );
	if( ret != 0 ) {
		mbedtls_printf( " failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret );
		goto exit;
	}

	mbedtls_printf( " ok\n" );

	/*
	* 2. Setup the listening TCP socket
	*/
	mbedtls_printf( "  . Bind on https://localhost:4433/ ...\n" );

	if( ( ret = mbedtls_net_bind( &listen_fd, NULL, "4433", MBEDTLS_NET_PROTO_TCP ) ) != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_net_bind returned %d\n\n", ret );
		goto exit;
	}

	mbedtls_printf( " ok\n" );

	/*
	 * 3. Seed the RNG
        */
	mbedtls_printf( "  . Seeding the random number generator...\n" );

	if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, ea_entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
		goto exit;
	}

	mbedtls_printf( " ok\n" );

	/*
	 * 4. Setup stuff
        */
	mbedtls_printf( "  . Setting up the SSL data.... \n" );

	if( ( ret = mbedtls_ssl_config_defaults( &conf,
                    MBEDTLS_SSL_IS_SERVER,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
		goto exit;
	}

	mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
	mbedtls_ssl_conf_dbg( &conf, my_debug, NULL );

	mbedtls_ssl_conf_ca_chain( &conf, srvcert.next, NULL );
	if( ( ret = mbedtls_ssl_conf_own_cert( &conf, &srvcert, &pkey ) ) != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret );
		while(1);
	}

	if( ( ret = mbedtls_ssl_setup( &ssl, &conf ) ) != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret );
		while(1);
	}

	mbedtls_printf( " ok\n" );

	return 0;
}
