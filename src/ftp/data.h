

#pragma once

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
NODISCARD bool data_controller_add_fd(DataController*, DataConnection*, int);

// thread save
NODISCARD int data_connections_to_close(DataController*, int**);
