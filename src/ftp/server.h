
#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include "./command.h"
#include "./data.h"

#include "generic/secure.h"
#include "utils/thread_pool.h"

#define FTP_SOCKET_BACKLOG_SIZE 10

#define FTP_MAX_QUEUE_SIZE 100

typedef uint16_t PortSize;
typedef struct {
	PortSize control;
	PortSize data;
} FTPPorts;

typedef struct {
	thread_pool* pool;
	myqueue* jobIds;
	ConnectionContext** contexts;
	int socketFd;
	const char* const global_folder;
	FTPPorts ports;
	DataController* data_controller;
} FTPControlThreadArgument;

typedef struct {
	thread_pool* pool;
	myqueue* jobIds;
	ConnectionContext** contexts;
	int socketFd;
	DataController* data_controller;
} FTPDataThreadArgument;

typedef struct {
	ConnectionContext** contexts;
	pthread_t listenerThread;
	int connectionFd;
	FTPState* state;
	RawNetworkAddress addr;
	FTPPorts ports;
	DataController* data_controller;
} FTPControlConnectionArgument;

NODISCARD bool ftp_process_command(ConnectionDescriptor* descriptor, FTPConnectAddr data_addr,
                                   FTPControlConnectionArgument*, const FTPCommand* command);

anyType(JobError*) ftp_control_socket_connection_handler(anyType(FTPControlConnectionArgument*) arg,
                                                         WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(ListenerError*)
    ftp_control_listener_thread_function(anyType(FTPControlThreadArgument*) arg);

anyType(ListenerError*) ftp_data_listener_thread_function(anyType(FTPDataThreadArgument*) arg);

NODISCARD int startFtpServer(PortSize control_port, PortSize data_port, char* folder);
