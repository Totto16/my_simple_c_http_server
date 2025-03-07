
#pragma once

#include <stdint.h>

#include "./command.h"
#include "generic/secure.h"
#include "utils/thread_pool.h"

#define FTP_SOCKET_BACKLOG_SIZE 10

#define FTP_MAX_QUEUE_SIZE 100

typedef struct {
	thread_pool* pool;
	myqueue* jobIds;
	ConnectionContext** contexts;
	int socketFd;
	const char* const global_folder;
} FTPThreadArgument;

typedef struct {
	ConnectionContext** contexts;
	pthread_t listenerThread;
	int connectionFd;
	FTPState* state;
} FTPConnectionArgument;

bool ftp_process_command(ConnectionDescriptor* const descriptor, FTPState*, const FTPCommand*);

anyType(JobError*)
    ftp_socket_connection_handler(anyType(FTPConnectionArgument*) arg, WorkerInfo workerInfo);

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
anyType(NULL) ftp_listener_thread_function(anyType(FTPThreadArgument*) arg);

int startFtpServer(uint16_t port, char* folder);
