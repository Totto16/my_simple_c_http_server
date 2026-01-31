

#pragma once

#include "./state.h"
#include "generic/secure.h"
#include "utils/utils.h"
#include <zvec/zvec.h>

#include <netinet/in.h>
#include <signal.h>

#define FTP_PASSIVE_DATA_CONNECTION_SIGNAL SIGUSR1

// opaque type
typedef struct DataControllerImpl DataController;

// opaque type
typedef struct DataConnectionImpl DataConnection;

ZVEC_DEFINE_VEC_TYPE_EXTENDED(ConnectionDescriptor*, ConnectionDescriptorPtr)

typedef ZVEC_TYPENAME(ConnectionDescriptorPtr) ConnectionsToClose;

typedef struct sockaddr_in RawNetworkAddress;

NODISCARD DataController* initialize_data_controller(size_t passive_port_amount);

void free_data_controller(DataController* data_controller);

// thread save
NODISCARD DataConnection*
get_data_connection_for_data_thread_or_add_passive(DataController* data_controller,
                                                   size_t port_index);

// thread save
NODISCARD bool data_controller_add_descriptor(DataController* data_controller,
                                              DataConnection* data_connection,
                                              ConnectionDescriptor* descriptor);

// thread save
NODISCARD ConnectionsToClose* data_connections_to_close(DataController* data_controller);

// thread save
NODISCARD DataConnection*
get_data_connection_for_control_thread_or_add(DataController* data_controller,
                                              FTPDataSettings settings);
// thread save
NODISCARD ConnectionDescriptor*
data_connection_get_descriptor_to_send_to(DataController* data_controller,
                                          DataConnection* connection);
// thread save
NODISCARD bool data_connection_close(DataController* data_controller, DataConnection* connection);

// thread save
NODISCARD FTPPortField get_available_port_for_passive_mode(DataController* data_controller);

// thread save
NODISCARD bool data_connection_set_port_as_available(DataController* data_controller, size_t index,
                                                     FTPPortField port);
