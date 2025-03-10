

#pragma once

#include "generic/secure.h"
#include "utils/utils.h"

#include <netinet/in.h>

// opaque type
typedef struct DataControllerImpl DataController;

// opaque type
typedef struct DataConnectionImpl DataConnection;

NODISCARD DataController* initialize_data_controller();

// TODO free DataController

typedef struct sockaddr_in RawNetworkAddress;

// thread save
NODISCARD DataConnection* get_data_connection_for_client(DataController*, RawNetworkAddress addr);

// thread save
NODISCARD DataConnection* data_controller_add_entry(DataController*, RawNetworkAddress addr);

// thread save
NODISCARD bool data_controller_add_descriptor(DataController*, DataConnection*,
                                              ConnectionDescriptor*);

// thread save
NODISCARD int data_connections_to_close(DataController*, ConnectionDescriptor***);

// thread save
NODISCARD DataConnection* add_data_connection_ready_for_control(DataController*,
                                                                RawNetworkAddress addr);
// thread save
NODISCARD ConnectionDescriptor* data_connection_get_descriptor_to_send_to(DataController*,
                                                                          DataConnection*);

// thread save
NODISCARD bool data_connection_set_should_close(DataController*, DataConnection*);
