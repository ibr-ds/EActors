/**
\file
\brief Node manipulations
*/

#include "node.h"
#include "spinlock.h"

/**
\brief initialise a queue

\param[in] *q queue

*/
void queue_init(queue *q) {
	q->top = NULL;
	q->bottom = NULL;
	q->lock = 0;
}

/**
\brief Enqueue as FIFO

\param[in] *q queue
\param[in] *new_node node

*/
void push_back(queue *q, node *new_node) {
	new_node->header.prev = NULL;
	new_node->header.next = NULL;

	spin_lock(&q->lock);

	if(q->bottom == NULL) {
		q->bottom = new_node;
		q->top = new_node;
	} else {
		q->bottom->header.prev = new_node;
		new_node->header.next = q->bottom;
		q->bottom = new_node;
	}

	spin_unlock(&q->lock);
}

/**
\brief Enqueue as LIFO

\param[in] *q queue
\param[in] *new_node node

*/
void push_front(queue *q, node *new_node) {
	new_node->header.prev = NULL;
	new_node->header.next = NULL;

	spin_lock(&q->lock);

	if(q->top == NULL) {
		q->top = new_node;
		q->bottom = new_node;
	} else {
		q->top->header.next = new_node;
		new_node->header.prev = q->top;
		q->top = new_node;
	}

	spin_unlock(&q->lock);
}

/**
\brief Dequeue as LIFO

\param[in] *q queue
\return node
*/
node *pop_front(queue *q) {
	if(q->top == NULL)
		return NULL;

	node *node;

	spin_lock(&q->lock);

	node = q->top;
	if(node->header.prev)
		node->header.prev->header.next = NULL;

	q->top = node->header.prev;

	if(q->top == NULL)
		q->bottom = NULL;

	spin_unlock(&q->lock);

	node->header.prev = NULL;
	node->header.next = NULL;

	return node;
}

/**
\brief Dequeue the latest element

\param[in] *q queue
\return node
*/
node *pop_back(queue *q) {
	if(q->bottom == NULL)
		return NULL;

	node *node;

	spin_lock(&q->lock);

	node = q->bottom;
	if(node->header.next)
		node->header.next->header.prev = NULL;
	q->bottom = node->header.next;

	if(q->bottom == NULL)
		q->top = NULL;

	spin_unlock(&q->lock);

	node->header.prev = NULL;
	node->header.next = NULL;

	return node;
}

/**
\brief Allocate a node (LIFO style)

\param[in] *s queue
\return node
*/
node *nalloc(queue *s) {
	node *tmp = pop_front(s);
	if(tmp != NULL)
		tmp->header.pool = s;

	return tmp;
}

/**
\brief Return a node (LIFO style)

\param[in] *n node to return
*/
void nfree(node *n) {
	push_front(n->header.pool, n);
}
