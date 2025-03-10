

#pragma once

#include "generic/secure.h"
#include "utils/utils.h"

#include <netinet/in.h>

// opaque type
typedef struct DataControllerImpl DataController;

// opaque type
typedef struct DataConnectionImpl DataConnection;

ARRAY_STRUCT(ConnectionsToClose, ConnectionDescriptor*);

typedef struct sockaddr_in RawNetworkAddress;

NODISCARD DataController* initialize_data_controller();

// TODO free DataController

// thread save
NODISCARD DataConnection* get_data_connection_for_data_thread_or_add(DataController*,
                                                                     RawNetworkAddress addr);

// thread save
NODISCARD bool data_controller_add_descriptor(DataController*, DataConnection*,
                                              ConnectionDescriptor*);

// thread save
NODISCARD ConnectionsToClose* data_connections_to_close(DataController*);

// thread save
NODISCARD DataConnection* get_data_connection_for_control_thread_or_add(DataController*,
                                                                        RawNetworkAddress addr);
// thread save
NODISCARD ConnectionDescriptor* data_connection_get_descriptor_to_send_to(DataController*,
                                                                          DataConnection*);

// thread save
NODISCARD bool data_connection_set_should_close(DataController*, DataConnection*);
