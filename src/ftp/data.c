

#include "./data.h"
#include "utils/clock.h"
#include "utils/log.h"

#include <fcntl.h>
#include <pthread.h>

// the timeout is 30 seconds
#define DATA_CONNECTION_WAIT_FOR_INTERNAL_NEGOTIATION_TIMEOUT_S_D 30.0

typedef struct {
	DataConnection* associated_connection;
	FTPPortField port;
	bool available;
	bool reserved;
} PortMetadata;

struct DataControllerImpl {
	pthread_mutex_t mutex;
	// connections
	DataConnection** connections;
	size_t connections_size;
	// (passive)ports
	PortMetadata* ports;
	size_t port_amount;
};

/**
 * @enum value
 *
 * @note this could be a mask / flag, but it also works like this
 */
typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	DATA_CONNECTION_STATE_EMPTY = 0,
	DATA_CONNECTION_STATE_HAS_DESCRIPTOR,
	DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL,
	DATA_CONNECTION_STATE_HAS_BOTH
} DataConnectionState;

typedef enum C_23_NARROW_ENUM_TO(uint8_t) {
	DATA_CONNECTION_CONTROL_STATE_MISSING = 0,
	DATA_CONNECTION_CONTROL_STATE_RETRIEVED,
	DATA_CONNECTION_CONTROL_STATE_SENDING,
	DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE,
	DATA_CONNECTION_CONTROL_STATE_ERROR
} DataConnectionControlState;

typedef struct {
	bool active;
	union {
		FTPPortField port;
		FTPConnectAddr addr;
	} data;
} ConnectionTypeIdentifier;

typedef struct {
	int sockFd;
	struct sockaddr_in* connect_addr;
} ActiveResumeDataImpl;

typedef struct {
	int sockFd;
} ActiveConnectedDataImpl;

typedef struct {
	bool connected;
	union {
		ActiveResumeDataImpl resume_data;
		ActiveConnectedDataImpl conn_data;
	} data;
} ActiveConnectionData;

struct DataConnectionImpl {
	DataConnectionState state;
	DataConnectionControlState control_state;
	Time last_change;
	// data, dependend on state
	ConnectionDescriptor* descriptor;
	ConnectionTypeIdentifier identifier;
	// active only data
	ActiveConnectionData* active_data;
	// passive only data
	pthread_t associated_thread;
};

DataController* initialize_data_controller(size_t passive_port_amount) {

	DataController* controller = malloc(sizeof(DataController));

	if(!controller) {
		return NULL;
	}

	int result = pthread_mutex_init(&controller->mutex, NULL);
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to initialize the mutex for the data controller",
	    free(controller);
	    return NULL;);

	controller->connections = NULL;
	controller->connections_size = 0;

	controller->port_amount = passive_port_amount;
	PortMetadata* ports = (PortMetadata*)malloc(sizeof(PortMetadata) * passive_port_amount);

	if(!ports) {
		return NULL;
	}

	controller->ports = ports;
	for(size_t i = 0; i < controller->port_amount; ++i) {
		controller->ports[i].available = false;
		controller->ports[i].reserved = false;
		controller->ports[i].port = 0;
		controller->ports[i].associated_connection = NULL;
	}

	return controller;
}

NODISCARD bool data_connection_set_port_as_available(DataController* data_controller, size_t index,
                                                     FTPPortField port) {
	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return false;);

	bool success = false;

	{

		if(index >= data_controller->port_amount) {
			success = false;
			goto cleanup;
		}

		data_controller->ports[index].available = true;
		data_controller->ports[index].port = port;
		success = true;
	}

cleanup:
	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return false;);

	return success;
}

// NOTE: nts_internal_ stands for non thread safe internal

NODISCARD static bool nts_internal_set_last_change_to_now(DataConnection* connection) {
	Time current_time;
	bool clock_result = get_monotonic_time(&current_time);

	if(!clock_result) {
		LOG_MESSAGE(LogLevelError | LogPrintLocation, "Getting the time failed: %s\n",
		            strerror(errno));
		return false;
	}

	connection->last_change = current_time;

	return true;
}

NODISCARD DataConnection*
get_data_connection_for_data_thread_or_add_passive(DataController* const data_controller,
                                                   size_t port_index) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	DataConnection* connection = NULL;

	{

		if(port_index >= data_controller->port_amount) {
			goto cleanup;
		}

		PortMetadata* port_metadata = &data_controller->ports[port_index];

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(!current_conn->identifier.active &&
			   current_conn->identifier.data.port == port_metadata->port) {
				connection = current_conn;

				if(port_metadata->associated_connection == NULL) {
					port_metadata->associated_connection = current_conn;
				}
				break;
			}
		}

		if(connection == NULL) {

			connection = (DataConnection*)malloc(sizeof(DataConnection));

			if(!connection) {
				goto cleanup;
			}

			// NOTE: no check is performed, if this is a duplicate

			connection->identifier.active = false;
			connection->identifier.data.port = port_metadata->port;
			connection->state = DATA_CONNECTION_STATE_EMPTY;
			connection->descriptor = NULL;
			connection->control_state = DATA_CONNECTION_CONTROL_STATE_MISSING;
			connection->associated_thread = 0;
			if(!nts_internal_set_last_change_to_now(connection)) {
				free(connection);
				connection = NULL;
				goto cleanup;
			}

			if(port_metadata->associated_connection == NULL) {
				port_metadata->associated_connection = connection;
			}

			data_controller->connections_size++;
			DataConnection** new_conns = (DataConnection**)realloc(
			    (void*)data_controller->connections,
			    data_controller->connections_size * sizeof(DataConnection*));

			if(new_conns == NULL) {
				data_controller->connections_size--;
				free(connection);
				connection = NULL;
				goto cleanup;
			}

			data_controller->connections = new_conns;
			data_controller->connections[data_controller->connections_size - 1] = connection;
		}
	}

cleanup:
	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return connection;
}

bool data_controller_add_descriptor(DataController* data_controller,
                                    DataConnection* data_connection,
                                    ConnectionDescriptor* descriptor) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
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
				if(!nts_internal_set_last_change_to_now(data_connection)) {
					success = false;
					break;
				}

				success = true;
				data_connection->state =
				    data_connection->state == DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL
				        ? DATA_CONNECTION_STATE_HAS_BOTH
				        : DATA_CONNECTION_STATE_HAS_DESCRIPTOR;

				// TODO(Totto): maybe use this instead of signals:
				// https://linux.die.net/man/2/eventfd2

				if(data_connection->associated_thread != 0) {

					int pthread_res = pthread_kill(data_connection->associated_thread,
					                               FTP_PASSIVE_DATA_CONNECTION_SIGNAL);

					CHECK_FOR_THREAD_ERROR(
					    pthread_res,
					    "An Error occurred while trying to send a signal to the data "
					    "connections associated thread",
					    goto break_lbl;);
				}

				break;

			break_lbl:
				success = false;
				break;
			}
			case DATA_CONNECTION_STATE_HAS_DESCRIPTOR:
			case DATA_CONNECTION_STATE_HAS_BOTH:
			default: {
				success = false;
				break;
			}
		}
	}

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return success;
}

NODISCARD static bool nts_internal_should_close_connection(DataConnection* connection) {

	if(connection->state != DATA_CONNECTION_STATE_HAS_BOTH) {

		// Check timeout

		Time current_time;
		bool clock_result = get_monotonic_time(&current_time);

		if(!clock_result) {
			return false;
		}

		// same as: current_time - last_change;
		double diff_time = time_diff_in_exact_seconds(current_time, connection->last_change);

		return (diff_time >= DATA_CONNECTION_WAIT_FOR_INTERNAL_NEGOTIATION_TIMEOUT_S_D);
	}

	if(connection->control_state == DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE ||
	   connection->control_state == DATA_CONNECTION_CONTROL_STATE_ERROR) {
		return true;
	}

	return false;
}

NODISCARD static ConnectionsToClose
nts_internal_data_connections_to_close(DataController* data_controller, DataConnection* filter) {
	ConnectionsToClose connections = STBDS_ARRAY_EMPTY;

	if(!connections) {
		goto cleanup;
	}

	{

		size_t current_keep_index = 0;

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(nts_internal_should_close_connection( // NOLINT(readability-implicit-bool-conversion)
			       current_conn) &&
			   (filter == NULL || current_conn == filter)) {

				if(!current_conn->identifier.active) {
					// dealloc port

					bool port_found = false;

					for(size_t port_idx = 0; port_idx < data_controller->port_amount; ++port_idx) {
						PortMetadata* port_metadata = &data_controller->ports[port_idx];
						if(port_metadata->associated_connection == current_conn) {
							port_found = true;

							port_metadata->associated_connection = NULL,
							port_metadata->reserved = false;

							break;
						}
					}

					if(!port_found) {
						LOG_MESSAGE_SIMPLE(LogLevelError, "Error in closing connection, couldn't "
						                                  "find passive port metadata to clean!\n");
					}
				}

				stbds_arrput(connections, current_conn->descriptor);
			} else {

				data_controller->connections[current_keep_index] = current_conn;

				current_keep_index++;
			}
		}

		data_controller->connections_size = current_keep_index;
		if(current_keep_index == 0) {
			free((void*)data_controller->connections);
			data_controller->connections = NULL;
		} else {

			DataConnection** new_conns = (DataConnection**)realloc(
			    (void*)data_controller->connections,
			    data_controller->connections_size * sizeof(DataConnection*));

			if(new_conns == NULL) {
				// just ignore, the array memory area might be to big but it'S no hard error
			} else {
				data_controller->connections = new_conns;
			}
		}
	}

cleanup:
	return connections;
}

ConnectionsToClose data_connections_to_close(DataController* data_controller) {
	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	ConnectionsToClose connections = nts_internal_data_connections_to_close(data_controller, NULL);

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return connections;
}

NODISCARD static ConnectionTypeIdentifier
nts_internal_conn_identifier_from_settings(FTPDataSettings settings) {

	switch(settings.mode) {
		case FTP_DATA_MODE_ACTIVE: {
			return (ConnectionTypeIdentifier){ .active = true, .data = { .addr = settings.addr } };
		}
		case FTP_DATA_MODE_PASSIVE: {
			return (ConnectionTypeIdentifier){ .active = false,
				                               .data = { .port = settings.addr.port } };
		}
		case FTP_DATA_MODE_NONE:
		default: {
			return (ConnectionTypeIdentifier){ .active = false, .data = { .port = 0 } };
		}
	}

	UNREACHABLE(); // NOLINT(cert-dcl03-c,misc-static-assert)
}

NODISCARD static bool
nts_internal_try_if_active_connection_is_connected(ActiveConnectionData* active_conn_data) {

	if(active_conn_data->connected) {
		return true;
	}

	ActiveResumeDataImpl* resume_data = &active_conn_data->data.resume_data;

	int result = connect(resume_data->sockFd, (struct sockaddr*)resume_data->connect_addr,
	                     sizeof(*resume_data->connect_addr));

	if(result == 0) {
		goto connected;
	}

	// previous connect calls aare not yet completed
	if(errno == EALREADY) {
		return true;
	}

	// the connection can't be completed atm
	if(errno == EINPROGRESS) {
		return true;
	}

	// ignore signals and just wait for the next call
	if(errno == EINTR) {
		return true;
	}

	if(errno == EISCONN) {
		goto connected;
	}

	// unexpected errno
	return false;

connected:

	active_conn_data->connected = true;

	active_conn_data->data.conn_data.sockFd = resume_data->sockFd;
	free(resume_data->connect_addr);

	return true;
}

NODISCARD static ActiveConnectionData*
nts_internal_setup_new_active_connection(FTPConnectAddr addr) {

	ActiveConnectionData* active_conn_data =
	    (ActiveConnectionData*)malloc(sizeof(ActiveConnectionData));

	if(!active_conn_data) {
		return NULL;
	}

#ifdef __linux
	int sockFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	CHECK_FOR_ERROR(sockFd, "While Trying to create a active connection", {
		free(active_conn_data);
		return NULL;
	});
#else
	int sockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	CHECK_FOR_ERROR(sockFd, "While Trying to create a active connection", return NULL;);

	int fcntl_flags = fcntl(sockFd, F_GETFL, 0);
	fcntl(sockFd, F_SETFL, fcntl_flags | O_NONBLOCK);
#endif

	struct sockaddr_in* connect_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));

	connect_addr->sin_family = AF_INET;
	// hto functions are used for networking, since there every number is BIG ENDIAN and linux
	// has Little Endian
	connect_addr->sin_port = htons(addr.port);

	connect_addr->sin_addr.s_addr = htonl(addr.addr);

	active_conn_data->connected = false;

	active_conn_data->data.resume_data.sockFd = sockFd;
	active_conn_data->data.resume_data.connect_addr = connect_addr;

	if(!nts_internal_try_if_active_connection_is_connected(active_conn_data)) {
		free(active_conn_data);
		return NULL;
	}

	return active_conn_data;
}

NODISCARD static bool nts_internal_addr_eq(FTPConnectAddr addr1, FTPConnectAddr addr2) {
	// NOTE: only port and address are compared

	if(addr1.port != addr2.port) {
		return false;
	}

	return addr1.addr == addr2.addr;
}

NODISCARD static bool nts_internal_conn_identifier_eq(ConnectionTypeIdentifier ident1,
                                                      ConnectionTypeIdentifier ident2) {

	if(ident1.active != ident2.active) {
		return false;
	}

	if(ident1.active) {
		return nts_internal_addr_eq(ident1.data.addr, ident2.data.addr);
	}

	return ident1.data.port == ident2.data.port;
}

NODISCARD DataConnection*
get_data_connection_for_control_thread_or_add(DataController* const data_controller,
                                              FTPDataSettings settings) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return NULL;);

	DataConnection* connection = NULL;

	{

		ConnectionTypeIdentifier identifier = nts_internal_conn_identifier_from_settings(settings);

		if(!identifier.active && identifier.data.port == 0) {
			goto cleanup;
		}

		for(size_t i = 0; i < data_controller->connections_size; ++i) {

			DataConnection* current_conn = data_controller->connections[i];

			if(nts_internal_conn_identifier_eq(current_conn->identifier, identifier)) {
				connection = current_conn;

				switch(connection->state) {

					case DATA_CONNECTION_STATE_EMPTY: {
						connection->state = DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL;
						// an error here means not the world end
						bool _ignore = nts_internal_set_last_change_to_now(connection);
						UNUSED(_ignore);
						connection->associated_thread = pthread_self();
						connection = NULL;
						break;
					}
					case DATA_CONNECTION_STATE_HAS_DESCRIPTOR: {
						connection->state = DATA_CONNECTION_STATE_HAS_BOTH;
						connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
						connection->associated_thread = pthread_self();
						break;
					}
					case DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL: {
						connection = NULL;
						break;
					}
					case DATA_CONNECTION_STATE_HAS_BOTH: {
						connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
						connection->associated_thread = pthread_self();
						break;
					}
				}

				if(current_conn // NOLINT(readability-implicit-bool-conversion)
				       ->identifier.active &&
				   current_conn->active_data != NULL) {

					if(!nts_internal_try_if_active_connection_is_connected(
					       current_conn->active_data)) {
						goto cleanup;
					}

					if(current_conn->active_data->connected) {
						connection = current_conn;
						connection->state = DATA_CONNECTION_STATE_HAS_BOTH;
						connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
						bool _ignore = nts_internal_set_last_change_to_now(connection);
						UNUSED(_ignore);
						// TODO(Totto): where do we get eventual ssl conetxts here?
						const SecureOptions* const options =
						    initialize_secure_options(false, "", "");

						ConnectionContext* context = get_connection_context(options);

						ConnectionDescriptor* const descriptor = get_connection_descriptor(
						    context, connection->active_data->data.conn_data.sockFd);

						connection->descriptor = descriptor;
						// TODO(Totto): free appropriately
						connection->active_data = NULL;
						goto cleanup;
					}
				}

				goto cleanup;
			}
		}

		// add a new one!
		{

			DataConnection* new_connection = (DataConnection*)malloc(sizeof(DataConnection));

			if(!new_connection) {
				goto cleanup;
			}

			new_connection->identifier = identifier;
			new_connection->state = DATA_CONNECTION_STATE_HAS_ASSOCIATED_CONTROL;
			new_connection->descriptor = NULL;
			new_connection->control_state = DATA_CONNECTION_CONTROL_STATE_MISSING;
			new_connection->associated_thread = pthread_self();
			if(!nts_internal_set_last_change_to_now(new_connection)) {
				free(new_connection);
				new_connection = NULL;
				goto cleanup;
			}

			data_controller->connections_size++;
			DataConnection** new_conns = (DataConnection**)realloc(
			    (void*)data_controller->connections,
			    data_controller->connections_size * sizeof(DataConnection*));

			if(new_conns == NULL) {
				data_controller->connections_size--;
				free(new_connection);
				new_connection = NULL;
				goto cleanup;
			}

			data_controller->connections = new_conns;
			data_controller->connections[data_controller->connections_size - 1] = new_connection;
			connection = NULL;

			if(identifier.active) {

				// setup connect_data

				ActiveConnectionData* active_data =
				    nts_internal_setup_new_active_connection(identifier.data.addr);

				if(active_data == NULL) {
					goto cleanup;
				}

				new_connection->active_data = active_data;

				if(new_connection->active_data->connected) {
					connection = new_connection;
					connection->state = DATA_CONNECTION_STATE_HAS_BOTH;
					connection->control_state = DATA_CONNECTION_CONTROL_STATE_RETRIEVED;
					bool _ignore = nts_internal_set_last_change_to_now(connection);
					UNUSED(_ignore);
					goto cleanup;
				}
			}
		}
	}

cleanup:
	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return connection;
}

NODISCARD ConnectionDescriptor*
data_connection_get_descriptor_to_send_to(DataController* data_controller,
                                          DataConnection* connection) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
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
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return NULL;);

	return descriptor;
}

NODISCARD bool nts_internal_close_connection(DataController* data_controller,
                                             DataConnection* connection) {

	ConnectionsToClose connections_to_close =
	    nts_internal_data_connections_to_close(data_controller, connection);

	if(stbds_arrlenu(connections_to_close) > 1) {
		LOG_MESSAGE_SIMPLE(LogLevelError | LogPrintLocation,
		                   "ASSERT: maximal one connection should be closed, if we set a filter\n");

		stbds_arrfree(connections_to_close);
		return false;
	}

	for(size_t i = 0; i < stbds_arrlenu(connections_to_close); ++i) {
		ConnectionDescriptor* connection_to_close = connections_to_close[i];
		close_connection_descriptor(connection_to_close);
	}

	stbds_arrfree(connections_to_close);
	return true;
}

NODISCARD bool data_connection_close(DataController* data_controller, DataConnection* connection) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
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

		connection->control_state = DATA_CONNECTION_CONTROL_STATE_SHOULD_CLOSE;

		success = nts_internal_close_connection(data_controller, connection);
	}
cleanup:

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return false;);

	return success;
}

FTPPortField get_available_port_for_passive_mode(DataController* data_controller) {

	int result = pthread_mutex_lock(&data_controller->mutex);
	CHECK_FOR_THREAD_ERROR(result,
	                    "An Error occurred while trying to lock the mutex for the data_controller",
	                    return false;);

	FTPPortField resulting_port = 0;

	{

		for(size_t i = 0; i < data_controller->port_amount; ++i) {
			if(!data_controller->ports[i].available) {
				continue;
			}

			if(data_controller->ports[i].reserved) {
				continue;
			}

			data_controller->ports[i].reserved = true;
			resulting_port = data_controller->ports[i].port;
			break;
		}
	}

	result = pthread_mutex_unlock(&data_controller->mutex);
	// TODO(Totto): better report error
	CHECK_FOR_THREAD_ERROR(
	    result, "An Error occurred while trying to unlock the mutex for the data_controller",
	    return false;);

	return resulting_port;
}
