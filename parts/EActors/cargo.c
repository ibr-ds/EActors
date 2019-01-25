/**
\file
\brief The cargo is a compound object which is used to hide difference between encrypted and non-encrypted private/public communications. Produced by connectors.
*/
#include "cargo.h"

/**
\brief Send a cargo myself
\param[in] *cargo a cargo object to send
\note this routine uses previously encrypted data
\note cargo object can not be used after send
*/
void hold_on_cargo(struct cargo_s *cargo) {
	send_pkg_unpacked_myself(cargo->sock, cargo->node, cargo->data);
}

/**
\brief A helper function, it helps to know should we repack a cargo. In other words -- can the same cargo be used for different sockets.
\param[in] *sockA the first socket
\param[in] *sockB the second socket
\return 1 we need to repack. If: sockets are different types, or sockets have different payload size
\return 0 if we can reuse the cargo
*/
int need_repack(struct socket_s *sockA, struct socket_s *sockB) {
	if(is_cross(sockA) ^ is_cross(sockB))
		return 1;

	if( sizeof(sockA->pool->top->payload) != sizeof(sockA->pool->top->payload))
		return 1;

//this is a workaround for tripong2.xml. something does not work and we need to repack our cargos.
	if(sockA->type != sockB->type)
		return 1;

	return 0;
}

/**
\brief Free a cargo object
\param[in] *cargo a cargo object to free
*/
void free_cargo(struct cargo_s *cargo) {
	free(cargo);
}

/**
\brief Return a cargo into a socket. Cargo cannot not be used after return
\param[in] *cargo a cargo object to return
*/
void return_cargo(struct cargo_s *cargo) {
	return_pkg(cargo->sock, cargo->node, (char *)cargo->data);
}

/**
\brief Receive a package from a socket and associate a cargo with received data. With known payload size decryption will be faster
\param[in] *sock a socket from which we receive a package
\param[out] *cargo a cargo object to assiciate
\param[in] size a payload size
\return 1 if there is no package
\return 0 if we got a message
*/
int recv_cargo_ks(struct socket_s *sock, struct cargo_s *cargo, unsigned int size) {
	if(sock->type == EMPTY) {
		printa("Socket was not inited, die (%d)\n", __LINE__);while(1);
	}

	cargo->node = recv_pkg(sock, (char **)&cargo->data, size);
	if(cargo->node == NULL)
		return 1;

	cargo->sock = sock;
	return 0;
}

/**
\brief Send a cargo with a known size of data
\param[in] *cargo a cargo object to send
\param[in] size a payload size
\note cargo object can not be used after send
*/
void send_cargo_ks(struct cargo_s *cargo, unsigned int size) {
	send_pkg(cargo->sock, cargo->node, cargo->data, size);
}

/**
\brief Receive a package from a socket and associate a cargo with a received data.
\param[in] *sock a socket from which we receive a package
\param[out] *cargo a cargo object to assiciate
\return 1 if there is no package
\return 0 if we got a message
\note this routine decrypts whole payload
*/
int recv_cargo(struct socket_s *sock, struct cargo_s *cargo) {
	if(sock->type == EMPTY) {
		printa("Socket was not inited, die (%d)\n", __LINE__);while(1);
	}

	cargo->node = recv_pkg(sock, (char **)&cargo->data, sizeof(sock->pool->top->payload));
	if(cargo->node == NULL)
		return 1;

	cargo->sock = sock;
	return 0;
}

/**
\brief Send a cargo
\param[in] *cargo a cargo object to send
\note this routing encrypts whole package payload
\note cargo object can not be used after send
*/
void send_cargo(struct cargo_s *cargo) {
	send_pkg(cargo->sock, cargo->node, cargo->data, sizeof(cargo->sock->pool->top->payload));
}

/**
\brief Get a free package from a socket and assosiate a cargo object with this data
\param[in] *sock a socket where we take an empty package
\param[out] *cargo a cargo object to send
\return 1 if we do not have a free packages in a socket
\return 0 on success
*/
int create_cargo(struct socket_s *sock, struct cargo_s *cargo) {
	cargo->node = create_pkg(sock, (char **)&cargo->data);
	if(cargo->node == NULL) {
		return 1;
	}

	cargo->sock = sock;

	return 0;
}

