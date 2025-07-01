
#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include "./command.h"
#include "./data.h"

#include "generic/secure.h"
#include "utils/thread_pool.h"

#define FTP_SOCKET_BACKLOG_SIZE 10

#define FTP_MAX_QUEUE_SIZE 100

typedef struct {
	ThreadPool* pool;
	Myqueue* jobIds;
	ConnectionContext** contexts;
	int socketFd;
	const char* const global_folder;
	DataController* data_controller;
} FTPControlThreadArgument;

typedef struct {
	ConnectionContext** contexts;
	pthread_t listenerThread;
	int connectionFd;
	FTPState* state;
	RawNetworkAddress addr;
	DataController* data_controller;
	pthread_t data_orchestrator;
} FTPControlConnectionArgument;

typedef struct {
	DataController* data_controller;
	FTPPortField* ports;
	size_t port_amount;
} FTPDataOrchestratorArgument;

typedef struct {
	DataController* data_controller;
	FTPPortField port;
	size_t port_index;
	int fd;
} FTPDataThreadArgument;

typedef struct {
	pthread_t thread_ref;
	FTPPortField port;
	bool success;
} FTPPassivePortStatus;

NODISCARD bool ftp_process_command(ConnectionDescriptor* descriptor, FTPAddrField server_addr,
                                   FTPControlConnectionArgument*, const FTPCommand* command);

ANY_TYPE(JobError*) ftp_control_socket_connection_handler(ANY_TYPE(FTPControlConnectionArgument*) arg,
                                                         WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
ANY_TYPE(ListenerError*)
    ftp_control_listener_thread_function(ANY_TYPE(FTPControlThreadArgument*) arg);

ANY_TYPE(ListenerError*) ftp_data_listener_thread_function(ANY_TYPE(FTPDataThreadArgument*) arg);

ANY_TYPE(ListenerError*)
    ftp_data_orchestrator_thread_function(ANY_TYPE(FTPDataOrchestratorArgument*) arg);

NODISCARD int startFtpServer(FTPPortField control_port, char* folder, SecureOptions* options);
