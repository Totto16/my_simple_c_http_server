

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
NODISCARD DataConnection* get_data_connection_for_client(const DataController*,
                                                         RawNetworkAddress addr);
