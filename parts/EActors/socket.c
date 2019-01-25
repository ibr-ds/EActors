/**
\file
\brief Socket -- a communication channel on top of mboxes

Socket:
- Always includes 2 sides and 4 boxes
- There is a convention of assignments between mboxes and IN/OUT names
- Hides complexity of cross/inter enclave communications
- Produces cargos
*/


#include "socket.h"

extern void printa(const char *fmt, ...);

/**
\brief is it a cross enclave socket?

This varable is defined by configuration. Thus, technically, two actors located inside one enclave can also be configured as cross.

\param[in] *sock a socket to check
\return 1 if the socket bridges actors in different enclaves
*/
char is_cross(struct socket_s *sock) {
	return sock->cross;
}

/**
\brief is it an encrypted socket?

\param[in] *sock a socket to check
\return 1 if the socket is encrypted
*/
char is_enc(struct socket_s *sock) {
	return sock->enc;
}


/**
\brief Returns pointer to payload of a node

\param[in] *node
\return pointer to a payload
*/
char *get_content(node *node) {
	return node->payload;
}

/**
\brief Allocate a package from a socket' pool

\param[in] *sock a socket to alloc
\return pointer to a node object
*/
node *alloc_pkg(struct socket_s *sock) {
	return nalloc(sock->pool);
}

/**
\brief Return a package (node) back into a package pool

\param[in] *sock a socket in pool of which we need return a package
\param[in] *pkg an empty package
*/
void return_empty(struct socket_s *sock, node *pkg) {
	push_front(sock->pool, pkg);
}

/**
\brief Return a package (node) back into a package pool, but before deallocated a shadow buffer

\param[in] *sock a socket in pool of which we need return a package
\param[in] *pkg a package
\param[in] *msg a pointer to a shadow buffer assiciated with this package
*/
void return_pkg(struct socket_s *sock, node *pkg, char *msg) {
	if(is_enc(sock))
		free(msg);

	memset(pkg->payload, 0, sizeof(pkg->payload));
	push_front(sock->pool, pkg);
}

/**
\brief Put an existing package into my inbox.

\param[in] *sock a socket in which we push a package
\param[in] *pkg a pointer to a package belongs to a socket used for a transmission
\param[in] *src a pointer to a data need to be send via the socket
*/
void send_pkg_unpacked_myself(struct socket_s *sock, node *pkg, char *src) {
	push_back(sock->in, pkg);

	if(is_enc(sock))
		free(src);
}

/**
\brief Send a package without packing

\param[in] *sock a socket in which we push a package
\param[in] *pkg a package
*/
void send_pkg_unpacked(struct socket_s *sock, node *pkg) {
	push_back(sock->out, pkg);

}

/**
\brief Pack a message into a packet (if the socket is cross enclave) and then send it.

\param[in] *sock a socket in which we push a package
\param[in] *pkg a pointer to a package belongs to a socket used for a transmission
\param[in] *src a pointer to a data need to be send via the socket
\param[in] *size a size of a sending data
*/
void send_pkg(struct socket_s *sock, node *pkg, char *src, int size) {
	if(is_enc(sock))
		pack(sock, pkg->payload, src, size, sock->ctx, pkg->header.mac);

	push_back(sock->out, pkg);

	if(is_enc(sock))
		free(src);
#ifdef V2
	sock->count++;
#endif
}

/**
\brief Receive a packe from incomming queue without unpacking

\param[in] *sock a socket from which we are taking a package
\return pointer to node structure poped from an input
*/
node *recv_pkg_unpacked(struct socket_s *sock) {
	node *tmp = pop_front(sock->in);

	return tmp;
}

/**
\brief Receive a packe from incomming queue and unpack the content into existing shadow buffer

\param[in] *sock a socket from which we are taking a package
\param[out] *dst a pointer to an existing shadow buffer
\param[in] size a size of a receiving data
\return pointer to node structure poped from an input
*/
node *recv_pkg_no_allocate(struct socket_s *sock, char *dst, int size) {
	node *tmp = pop_front(sock->in);
	if(tmp == NULL)
		return NULL;

	unpack(sock, dst, tmp->payload, size, sock->ctx, tmp->header.mac);

	return tmp;
}

/**
\brief Receive a packe from incomming queue, allocate a shadow buffer for a corr-enclave socket and unpack a package content into this buffer

\param[in] *sock a socket from which we are taking a package
\param[out] **dst a pointer to a pointer in which we allocate a shadow buffer
\param[in] size a size of a receiving data
\return pointer to node structure poped from an input
*/
node *recv_pkg(struct socket_s *sock, char **dst, int size) {
	node *tmp = pop_front(sock->in);
	if(tmp == NULL)
		return NULL;

	if(is_enc(sock)) {
		*dst = malloc(PL_SIZE);
		if(dst == NULL) {
			printa(" cannot allocate memory, %d\n", __LINE__);while(1);
		}
		unpack(sock, *dst, tmp->payload, size, sock->ctx, tmp->header.mac);
	} else
		*dst = (char *) get_content(tmp);

	return tmp;
}

/**
\brief Create an ampty package from a socket and allocate shadow buffer if need

\param[in] *sock a socket from pool of which we are taking a free package
\param[out] **pl a pointer to a pointer in which we will allocate a shadow buffer
\return pointer to node structure poped from an input
*/
node *create_pkg(struct socket_s *sock, char **pl) {
	node *cargo=alloc_pkg(sock);
	if(cargo == NULL)
		return NULL;

	if(is_enc(sock)) {
		*pl = malloc(PL_SIZE);
	} else
		*pl = (char *) get_content(cargo);

	return cargo;
}

/**
\brief Setup a socket. De-references and calls the corresponding allocator of an encryption context

\param[in] *sock a socket which we want to setup
\param[in] enc is it encrypted?
\param[in] cross is it cross-enclave?
\param[in] type encryption type?
*/
void setup_socket(struct socket_s *sock, int enc, int cross, enum etype type) {
	sock->enc = enc;
	sock->cross = cross;
	sock->type = type;
	sock->mid = 0;

	//todo: it is better to implement 'destroy'

	if(sock->ctx != NULL)
		return;

	if(!sock->enc)
		return;

	switch(sock->type) {
		case GCM:
			alloc_gcm((struct ctx_gcm_s **) &sock->ctx);
			break;
		case CTR:
			alloc_ctr((struct ctx_ctr_s **) &sock->ctx);
			break;
		case MEMCPY:
			alloc_memcpy((struct ctx_memcpy_s **) &sock->ctx);
			break;
		default:
			printa("[%d] wrong type of the socket, die\n", __LINE__); while(1);
	}
}

/**
\brief decrypt data of a cargo

\param[in] *sock a socket which produced an orgiginal cargo
\param[out] *dst where decrypt
\param[in] *src what decrypt
\param[in] size size of the data
\param[in] *ctx encryption context
\param[in] *mac mac, can be NULL if not used
*/
void unpack(struct socket_s *sock, char *dst, char *src, int size, void *ctx, char *mac) {
	switch(sock->type) {
		case GCM:
			unpack_gcm(dst, src, size, (struct ctx_gcm_s *)ctx, mac);
			break;
		case CTR:
			unpack_ctr(dst, src, size, (struct ctx_ctr_s *)ctx, mac);
			break;
		case MEMCPY:
			unpack_memcpy(dst, src, size);
			break;
		default:
			printa("[%d] wrong type of the socket, die\n", __LINE__); while(1);
	}
}

/**
\brief encrypt data of a cargo

\param[in] *sock a socket which produced an orgiginal cargo
\param[out] *dst where to store
\param[in] *src what to encrypt
\param[in] size size of the data
\param[in] *ctx encryption context
\param[in] *mac mac, can be NULL if not used
*/
void pack(struct socket_s *sock, char *dst, char *src, int size, void *ctx, char *mac) {
	switch(sock->type) {
		case GCM:
			pack_gcm(dst, src, size, (struct ctx_gcm_s *)ctx, mac);
			break;
		case CTR:
			pack_ctr(dst, src, size, (struct ctx_ctr_s *)ctx, mac);
			break;
		case MEMCPY:
			pack_memcpy(dst, src, size);
			break;
		default:
			printa("[%d] wrong type of the socket, die\n", __LINE__); while(1);
	}
}


/**
\brief different contextes may have different sizes of keys

\param[in] *sock a socket which produced an orgiginal cargo
\return the size of encryption keys
*/
int pack_get_size(struct socket_s *sock) {
	switch(sock->type) {
		case GCM:
			return pack_get_size_gcm();
			break;
		case CTR:
			return pack_get_size_ctr();
			break;
		case MEMCPY:
			return pack_get_size_memcpy();
			break;
		default:
			printa("[%d] wrong type of the socket, die\n", __LINE__); while(1);
	}

}
