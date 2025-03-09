

#include "./data.h"

#include <pthread.h>

struct DataControllerImpl {
	pthread_mutex_t mutex;
};

// opaque type
struct DataConnectionImpl {
	int todo;
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

	return controller;
}

NODISCARD DataConnection*
get_data_connection_for_client(const DataController* const data_controller,
                               RawNetworkAddress addr) {

	// TODO
	UNUSED(data_controller);
	UNUSED(addr);
	return NULL;
}
