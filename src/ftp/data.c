

#include "./data.h"

#include <pthread.h>

// the timeout is 30 seconds
#define DATA_CONNECTION_WAIT_FOR_INTERNAL_NEGOTIATION_TIMEOUT_S 30

struct DataControllerImpl {
	pthread_mutex_t mutex;
	DataConnection** connections;
	size_t connections_size;
};

/**
 * @enum value
 *
 * @note this could be a mask / flag, but it also works like this
 */
typedef enum {
	DATA_CONNECTION_STATE_EMPTY = 0,
	DATA_CONNECTION_STATE_HAS_FD,
	DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL,
	DATA_CONNECTION_STATE_HAS_BOTH
} DataConnectionState;

typedef enum {
	DATA_CONNECTION_CONTROL_STATE_MISSING = 0,
	DATA_CONNECTION_CONTROL_STATE_RETRIEVED,
	DATA_CONNECTION_CONTROL_STATE_SENDING,
	DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE,
	DATA_CONNECTION_CONTROL_STATE_ERROR
} DataConnectionControlState;

struct DataConnectionImpl {
	RawNetworkAddress addr;
	DataConnectionState state;
	ConnectionDescriptor* descriptor;
	DataConnectionControlState control_state;
	time_t last_change;
};

DataController* initialize_data_controller() {

	DataController* controller = malloc(sizeof(DataController));

	if(!controller) {
		return NULL;
	}

	int result = pthread_mutex_init(&controller->mutex, NULL);
	checkForThreadError(
	    result, "An Error occurred while trying to initialize the mutex for the data controller",
	    free(controller);
	    return NULL;);

	controller->connections = NULL;
	controller->connections_size = 0;

	return controller;
}

NODISCARD bool addr_eq(RawNetworkAddress addr1, RawNetworkAddress addr2) {
	// NOTE: only port and address are compared

	if(addr1.sin_port != addr2.sin_port) {
		return false;
	}

	return addr1.sin_addr.s_addr == addr2.sin_addr.s_addr;
}

NODISCARD DataConnection* get_data_connection_for_client(DataController* const data_controller,
                                                         RawNetworkAddress addr) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	DataConnection* connection = NULL;

	{

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(addr_eq(current_conn->addr, addr)) {
				connection = current_conn;
				break;
			}
		}
	}

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return connection;
}

NODISCARD bool internal_set_last_change_to_now(DataConnection* connection) {
	time_t current_time = time(NULL);

	if(current_time == ((time_t)-1)) {
		return false;
	}

	connection->last_change = current_time;

	return true;
}

NODISCARD DataConnection* data_controller_add_entry(DataController* data_controller,
                                                    RawNetworkAddress addr) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	DataConnection* new_connection = NULL;

	{

		new_connection = malloc(sizeof(DataConnection));

		if(!new_connection) {
			goto cleanup;
		}

		// NOTE: no check is performed, if this is a duplicate

		new_connection->addr = addr;
		new_connection->state = DATA_CONNECTION_STATE_EMPTY;
		new_connection->descriptor = NULL;
		new_connection->control_state = DATA_CONNECTION_CONTROL_STATE_MISSING;
		if(!internal_set_last_change_to_now(new_connection)) {
			free(new_connection);
			new_connection = NULL;
			goto cleanup;
		}

		data_controller->connections_size++;
		DataConnection** new_conns = (DataConnection**)realloc(data_controller->connections,
		                                                       data_controller->connections_size);

		if(new_conns == NULL) {
			data_controller->connections_size--;
			free(new_connection);
			new_connection = NULL;
			goto cleanup;
		}

		data_controller->connections = new_conns;
		new_conns[data_controller->connections_size - 1] = new_connection;
	}

cleanup:

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return new_connection;
}

bool data_controller_add_descriptor(DataController* data_controller,
                                    DataConnection* data_connection,
                                    ConnectionDescriptor* descriptor) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	bool success = false;

	// NOTE: no check is performed, if the data_connection is actually inside the
	// data_controller->connections

	{

		switch(data_connection->state) {
			case DATA_CONNECTION_STATE_EMPTY:
			case DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL: {
				data_connection->descriptor = descriptor;
				if(!internal_set_last_change_to_now(data_connection)) {
					success = false;
					break;
				}
				success = true;
				data_connection->state =
				    data_connection->state == DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL
				        ? DATA_CONNECTION_STATE_HAS_BOTH
				        : DATA_CONNECTION_STATE_HAS_FD;
				break;
			}
			case DATA_CONNECTION_STATE_HAS_FD:
			case DATA_CONNECTION_STATE_HAS_BOTH:
			default: {
				success = false;
				break;
			}
		}
	}

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return success;
}

NODISCARD bool should_close_connection(DataConnection* connection) {

	if(connection->state != DATA_CONNECTION_STATE_HAS_BOTH) {

		// Check timeout

		time_t current_time = time(NULL);

		if(current_time == ((time_t)-1)) {
			return false;
		}

		time_t diff_time = current_time - connection->last_change;

		if(diff_time >= DATA_CONNECTION_WAIT_FOR_INTERNAL_NEGOTIATION_TIMEOUT_S) {
			return true;
		}

		return false;
	}

	if(connection->control_state == DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE ||
	   connection->control_state == DATA_CONNECTION_CONTROL_STATE_ERROR) {
		return true;
	}

	return false;
}

int data_connections_to_close(DataController* data_controller,
                              ConnectionDescriptor*** descriptors) {
	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return -1;);

	int close_amount = 0;

	{

		size_t current_keep_index = 0;

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(should_close_connection(current_conn)) {
				close_amount++;

				ConnectionDescriptor** new_descriptors =
				    (ConnectionDescriptor**)realloc((void*)(*descriptors), close_amount);

				if(new_descriptors == NULL) {
					*descriptors = NULL;
					close_amount = -1;
					goto cleanup;
				}

				*descriptors = new_descriptors;

				new_descriptors[close_amount - 1] = current_conn->descriptor;
			} else {

				data_controller->connections[current_keep_index] = current_conn;

				current_keep_index++;
			}
		}

		data_controller->connections_size = current_keep_index;
		DataConnection** new_conns = (DataConnection**)realloc(data_controller->connections,
		                                                       data_controller->connections_size);

		if(new_conns == NULL) {
			// just ignore, the array memory area might be to big but it'S no hard error
		} else {
			data_controller->connections = new_conns;
		}
	}

cleanup:

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return -1;);

	return close_amount;
}

NODISCARD DataConnection*
add_data_connection_ready_for_control(DataController* const data_controller,
                                      RawNetworkAddress addr) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	DataConnection* connection = NULL;

	{

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(addr_eq(current_conn->addr, addr)) {
				connection = current_conn;

				switch(connection->state) {

					case DATA_CONNECTION_STATE_EMPTY: {
						connection->state = DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL;
						// an error here means not the world end
						bool _ignore = internal_set_last_change_to_now(connection);
						UNUSED(_ignore);
						connection = NULL;
						break;
					}
					case DATA_CONNECTION_STATE_HAS_FD: {
						connection->state = DATA_CONNECTION_STATE_HAS_BOTH;
						connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
						break;
					}
					case DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL: {
						connection = NULL;
						break;
					}
					case DATA_CONNECTION_STATE_HAS_BOTH: {
						connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
						break;
					}
				}

				goto cleanup;
			}
		}

		// add a new one!
		{

			DataConnection* new_connection = malloc(sizeof(DataConnection));

			if(!new_connection) {
				goto cleanup;
			}

			new_connection->addr = addr;
			new_connection->state = DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL;
			new_connection->descriptor = NULL;
			new_connection->control_state = DATA_CONNECTION_CONTROL_STATE_MISSING;
			if(!internal_set_last_change_to_now(new_connection)) {
				free(new_connection);
				new_connection = NULL;
				goto cleanup;
			}

			data_controller->connections_size++;
			DataConnection** new_conns = (DataConnection**)realloc(
			    data_controller->connections, data_controller->connections_size);

			if(new_conns == NULL) {
				data_controller->connections_size--;
				free(new_connection);
				new_connection = NULL;
				goto cleanup;
			}

			data_controller->connections = new_conns;
			connection = NULL;
		}
	}

cleanup:
	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return connection;
}

NODISCARD ConnectionDescriptor*
data_connection_get_descriptor_to_send_to(DataController* data_controller,
                                          DataConnection* connection) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	ConnectionDescriptor* descriptor = NULL;

	{

		if(connection->state != DATA_CONNECTION_STATE_HAS_BOTH) {
			descriptor = NULL;
			connection->control_state = DATA_CONNECTION_CONTROL_STATE_ERROR;
			goto cleanup;
		}

		if(connection->control_state != DATA_CONNECTION_CONTROL_STATE_RETRIEVED) {
			descriptor = NULL;
			connection->control_state = DATA_CONNECTION_CONTROL_STATE_ERROR;
			goto cleanup;
		}

		descriptor = connection->descriptor;
		connection->control_state = DATA_CONNECTION_CONTROL_STATE_SENDING;
	}
cleanup:

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return descriptor;
}

NODISCARD bool data_connection_set_should_close(DataController* data_controller,
                                                DataConnection* connection) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	checkForThreadError(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return false;);

	bool success = true;

	{

		if(connection->state != DATA_CONNECTION_STATE_HAS_BOTH) {
			success = false;
			connection->control_state = DATA_CONNECTION_CONTROL_STATE_ERROR;
			goto cleanup;
		}

		if(connection->control_state != DATA_CONNECTION_CONTROL_STATE_SENDING) {
			success = false;
			connection->control_state = DATA_CONNECTION_CONTROL_STATE_ERROR;
			goto cleanup;
		}

		success = true;
		connection->control_state = DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE;
	}
cleanup:

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	checkForThreadError(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return false;);

	return success;
}
