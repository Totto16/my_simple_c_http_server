#include <errno.h>
#include <signal.h>

#include "./folder.h"
#include "./hpack.h"
#include "./send.h"
#include "./server.h"
#include "generic/helper.h"
#include "generic/read.h"
#include "generic/secure.h"
#include "generic/signal_fd.h"
#include "http/header.h"
#include "http/mime.h"
#include "http/parser.h"
#include "http/v2.h"
#include "utils/clock.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/thread_pool.h"
#include "utils/utils.h"
#include "ws/handler.h"
#include "ws/thread_manager.h"
#include "ws/ws.h"

#ifdef _SIMPLE_SERVER_USE_OPENSSL
	#include <openssl/crypto.h>
#endif

static volatile sig_atomic_t
    g_signal_received = // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    0;

// only setting the volatile sig_atomic_t g_signal_received' in here
static void receive_signal(int signal_number) {
	g_signal_received = signal_number;
}

#define SUPPORTED_HTTP_METHODS "GET, POST, HEAD, OPTIONS, CONNECT"

#define FREE_AT_END() \
	do { \
	} while(false)

#define INT_ERROR_FROM_VOID_PTR(ERR) (-((int)((uintptr_t)(ERR))))

NODISCARD static int process_http_error(const HttpRequestError error,
                                        ConnectionDescriptor* const descriptor,
                                        HTTPGeneralContext* general_context,
                                        const SendSettings send_settings, const bool send_body) {

	if(error.is_advanced) {

		StringBuilder* string_builder = string_builder_init();

		string_builder_append_single(string_builder, "Bad Request: ");

		string_builder_append_single(string_builder, error.value.advanced);

		HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
			                           .body = http_response_body_from_string_builder(
			                               &string_builder, send_body),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

		return send_http_message_to_connection(general_context, descriptor, to_send, send_settings);
	}

	switch(error.value.enum_value) {
		case HttpRequestErrorTypeInvalidHttpVersion: {

			HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

			{

				Time now;

				bool success = get_current_time(&now);

				if(success) {
					char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
					if(date_str != NULL) {
						add_http_header_field_const_key_dynamic_value(
						    &additional_headers, HTTP_HEADER_NAME(date), date_str);
					}
				}
			}

			HTTPResponseToSend to_send = { .status = HttpStatusHttpVersionNotSupported,
				                           .body = http_response_body_from_static_string(
				                               "Only HTTP/1.1 is supported atm", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = additional_headers };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
		case HttpRequestErrorTypeMethodNotSupported: {

			HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

			{

				{ // all 405 have to have a Allow filed according to spec

					add_http_header_field_const_key_const_value(
					    &additional_headers, HTTP_HEADER_NAME(allow), SUPPORTED_HTTP_METHODS);
				}

				{
					Time now;

					bool success = get_current_time(&now);

					if(success) {
						char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
						if(date_str != NULL) {
							add_http_header_field_const_key_dynamic_value(
							    &additional_headers, HTTP_HEADER_NAME(date), date_str);
						}
					}
				}
			}

			HTTPResponseToSend to_send = {
				.status = HttpStatusMethodNotAllowed,
				.body = http_response_body_from_static_string(
				    "This primitive HTTP Server only supports GET, POST, "
				    "HEAD, OPTIONS and CONNECT requests",
				    send_body),
				.mime_type = MIME_TYPE_TEXT,
				.additional_headers = additional_headers
			};

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
		case HttpRequestErrorTypeInvalidNonEmptyBody: {
			HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
				                           .body = http_response_body_from_static_string(
				                               "A GET, HEAD or OPTIONS Request can't have a body",
				                               send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
		case HttpRequestErrorTypeInvalidHttp2Preface: {
			return http2_send_connection_error(descriptor, Http2ErrorCodeProtocolError,
			                                   "invalid http2 preface");
		}
		case HttpRequestErrorTypeLengthRequired: {
			HTTPResponseToSend to_send = { .status = HttpStatusLengthRequired,
				                           .body = http_response_body_empty(),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
		case HttpRequestErrorTypeProtocolError: {
			HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
				                           .body = http_response_body_from_static_string(
				                               "Protocol Error", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
		case HttpRequestErrorTypeNotSupported: {
			HTTPResponseToSend to_send = { .status = HttpStatusBadRequest,
				                           .body = http_response_body_from_static_string(
				                               "Not Supported", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
			break;
		}
		default: {
			HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
				                           .body = http_response_body_from_static_string(
				                               "Internal Server Error 2", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

			return send_http_message_to_connection(general_context, descriptor, to_send,
			                                       send_settings);
		}
	}
}

NODISCARD static JobError
process_http_request(const HttpRequest http_request, ConnectionDescriptor* const descriptor,
                     HTTPGeneralContext* general_context, const RouteManager* const route_manager,
                     HTTPConnectionArgument* argument, const WorkerInfo worker_info,
                     const RequestSettings request_settings, const IPAddress address) {

	ConnectionContext* context =
	    TVEC_AT(ConnectionContextPtr, argument->contexts, worker_info.worker_index);

	// To test this error codes you can use '-X POST' with curl or
	// '--http2' (doesn't work, since http can only be HTTP/1.1, https can be HTTP 2 or QUIC
	// alias HTTP 3)

	// if the request is supported then the "beautiful" website is sent, if the path is /shutdown
	// a shutdown is issued

	const bool send_body = http_request.head.request_line.method != HTTPRequestMethodHead;

	SendSettings send_settings = get_send_settings(request_settings);
	HttpRequestProperties http_properties = request_settings.http_properties;

	SelectedRoute* selected_route =
	    route_manager_get_route_for_request(route_manager, http_properties, http_request, address);

	if(selected_route == NULL) {

		int result = 0;

		switch(http_request.head.request_line.method) {
			case HTTPRequestMethodGet:
			case HTTPRequestMethodPost:
			case HTTPRequestMethodHead: {

				HTTPResponseToSend to_send = { .status = HttpStatusNotFound,
					                           .body = http_response_body_from_static_string(
					                               "File not Found", send_body),
					                           .mime_type = MIME_TYPE_TEXT,
					                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

				result = send_http_message_to_connection(general_context, descriptor, to_send,
				                                         send_settings);
				break;
			}
			case HTTPRequestMethodOptions: {
				HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

				{

					{ // all 405 have to have a Allow filed according to spec

						add_http_header_field_const_key_const_value(
						    &additional_headers, HTTP_HEADER_NAME(allow), SUPPORTED_HTTP_METHODS);
					}

					{
						Time now;

						bool success = get_current_time(&now);

						if(success) {
							char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
							if(date_str != NULL) {
								add_http_header_field_const_key_dynamic_value(
								    &additional_headers, HTTP_HEADER_NAME(date), date_str);
							}
						}
					}
				}

				HTTPResponseToSend to_send = { .status = HttpStatusOk,
					                           .body = http_response_body_empty(),
					                           .mime_type = NULL,
					                           .additional_headers = additional_headers };

				result = send_http_message_to_connection(general_context, descriptor, to_send,
				                                         send_settings);

				break;
			}
			case HTTPRequestMethodConnect: {
				HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

				{

					{ // all 405 have to have a Allow filed according to spec

						add_http_header_field_const_key_const_value(
						    &additional_headers, HTTP_HEADER_NAME(allow), SUPPORTED_HTTP_METHODS);
					}

					{
						Time now;

						bool success = get_current_time(&now);

						if(success) {
							char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
							if(date_str != NULL) {
								add_http_header_field_const_key_dynamic_value(
								    &additional_headers, HTTP_HEADER_NAME(date), date_str);
							}
						}
					}
				}

				HTTPResponseToSend to_send = { .status = HttpStatusOk,
					                           .body = http_response_body_empty(),
					                           .mime_type = NULL,
					                           .additional_headers = additional_headers };

				result = send_http_message_to_connection(general_context, descriptor, to_send,
				                                         send_settings);

				break;
			}
			default: {
				HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
					                           .body = http_response_body_from_static_string(
					                               "Internal Server Error 1", send_body),
					                           .mime_type = MIME_TYPE_TEXT,
					                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

				result = send_http_message_to_connection(general_context, descriptor, to_send,
				                                         send_settings);
				break;
			}
		}

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "Error in sending response\n");
		}

		return JOB_ERROR_NONE;
	}

	HTTPSelectedRoute selected_route_data = get_selected_route_data(selected_route);

	HTTPRouteData route_data = selected_route_data.data;

	int result = -1;

	switch(route_data.type) {
		case HTTPRouteTypeSpecial: {

			switch(route_data.value.special.type) {
				case HTTPRouteSpecialDataTypeShutdown: {

					HTTPResponseBody body;
					HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

					if(http_request.head.request_line.method == HTTPRequestMethodGet) {
						body = http_response_body_from_static_string("Shutting Down", send_body);
					} else if(http_request.head.request_line.method == HTTPRequestMethodHead) {
						body = http_response_body_empty();

						{

							add_http_header_field_const_key_const_value(
							    &additional_headers, HTTP_HEADER_NAME(x_shutdown), "true");
						}

					} else {

						{ // all 405 have to have a Allow filed according to spec

							add_http_header_field_const_key_const_value(
							    &additional_headers, HTTP_HEADER_NAME(allow), "GET, HEAD");
						}

						HTTPResponseToSend to_send = {
							.status = HttpStatusMethodNotAllowed,
							.body = http_response_body_from_static_string(
							    "Only GET and HEAD supported to this URL", send_body),
							.mime_type = MIME_TYPE_TEXT,
							.additional_headers = additional_headers
						};

						result = send_http_message_to_connection(general_context, descriptor,
						                                         to_send, send_settings);

						break;
					}

					LOG_MESSAGE_SIMPLE(LogLevelInfo, "Shutdown requested!\n");

					HTTPResponseToSend to_send = { .status = HttpStatusOk,
						                           .body = body,
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = additional_headers };

					result = send_http_message_to_connection(general_context, descriptor, to_send,
					                                         send_settings);

					// just cancel the listener thread, then no new connection are accepted
					// and the main thread cleans the pool and queue, all jobs are finished
					// so shutdown gracefully
					int cancel_result = pthread_cancel(argument->listener_thread);
					CHECK_FOR_ERROR(cancel_result, "While trying to cancel the listener Thread", {
						FREE_AT_END();
						return JOB_ERROR_THREAD_CANCEL;
					});

					break;
				}
				case HTTPRouteSpecialDataTypeWs: {

					if(http_request.head.request_line.method != HTTPRequestMethodGet) {
						HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

						{ // all 405 have to have a Allow filed according to spec

							add_http_header_field_const_key_const_value(
							    &additional_headers, HTTP_HEADER_NAME(allow), "GET");
						}

						HTTPResponseToSend to_send = {
							.status = HttpStatusMethodNotAllowed,
							.body = http_response_body_from_static_string(
							    "Only GET supported to this URL", send_body),
							.mime_type = MIME_TYPE_TEXT,
							.additional_headers = additional_headers
						};

						result = send_http_message_to_connection(general_context, descriptor,
						                                         to_send, send_settings);

						break;
					}

					WSExtensions extensions = TVEC_EMPTY(WSExtension);

					int ws_request_successful = handle_ws_handshake(
					    http_request, descriptor, general_context, send_settings, &extensions);

					WsConnectionArgs websocket_args =
					    get_ws_args_from_http_request(selected_route_data.path, extensions);

					if(ws_request_successful >= 0) {
						// move the context so that we can use it in the long standing web
						// socket thread
						ConnectionContext* new_context = copy_connection_context(context);

						auto _ = TVEC_SET_AT(ConnectionContextPtr, &(argument->contexts),
						                     worker_info.worker_index, new_context);
						UNUSED(_);

						if(!thread_manager_add_connection(argument->web_socket_manager, descriptor,
						                                  context, websocket_function,
						                                  websocket_args)) {

							TVEC_FREE(WSExtension, &extensions);
							free_selected_route(selected_route);
							FREE_AT_END();

							return JOB_ERROR_CONNECTION_ADD;
						}

						// finally free everything necessary

						free_selected_route(selected_route);
						FREE_AT_END();

						return JOB_ERROR_NONE;
					}

					// the error was already sent, just close the descriptor and free the
					// http request, this is done at the end of this big if else statements
					break;
				}
				default: {
					// TODO(Totto): refactor all these arbitrary -<int> error numbers into
					// some error enum, e.g also -11
					result =
					    -10; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
					break;
				}
			}

			break;
		}

		case HTTPRouteTypeNormal: {

			result = route_manager_execute_route(route_manager, route_data.value.normal, descriptor,
			                                     general_context, send_settings, http_request,
			                                     context, selected_route_data.path,
			                                     selected_route_data.auth_user, address);

			break;
		}
		case HTTPRouteTypeInternal: {
			result = send_http_message_to_connection(general_context, descriptor,
			                                         route_data.value.internal.send, send_settings);
			break;
		}
		case HTTPRouteTypeServeFolder: {
			const HTTPRouteServeFolder data = route_data.value.serve_folder;

			ServeFolderResult* serve_folder_result =
			    get_serve_folder_content(http_properties, data, selected_route_data, send_body);

			if(serve_folder_result == NULL) {
				HTTPResponseToSend to_send = {
					.status = HttpStatusInternalServerError,
					.body = http_response_body_from_static_string(
					    "Internal Server Error: Folder Server Request failed", send_body),
					.mime_type = MIME_TYPE_TEXT,
					.additional_headers = TVEC_EMPTY(HttpHeaderField)
				};

				result = send_http_message_to_connection(general_context, descriptor, to_send,
				                                         send_settings);
				break;
			}

			switch(serve_folder_result->type) {
				case ServeFolderResultTypeNotFound: {

					HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

					{
						Time now;

						bool success = get_current_time(&now);

						if(success) {
							char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
							if(date_str != NULL) {

								add_http_header_field_const_key_const_value(
								    &additional_headers, HTTP_HEADER_NAME(date), date_str);
							}
						}
					}

					// TODO(Totto): send a info page
					HTTPResponseToSend to_send = { .status = HttpStatusNotFound,
						                           .body = http_response_body_empty(),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers = additional_headers };

					result = send_http_message_to_connection(general_context, descriptor, to_send,
					                                         send_settings);

					break;
				}
				case ServeFolderResultTypeServerError: {

					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal Server Error: 3", send_body),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers =
						                               TVEC_EMPTY(HttpHeaderField) };

					result = send_http_message_to_connection(general_context, descriptor, to_send,
					                                         send_settings);

					break;
				}
				case ServeFolderResultTypeFile: {
					const ServeFolderFileInfo file = serve_folder_result->data.file;

					HttpHeaderFields additional_headers = TVEC_EMPTY(HttpHeaderField);

					{

						add_http_header_field_const_key_const_value(
						    &additional_headers, HTTP_HEADER_NAME(content_transfer_encoding),
						    "binary");

						add_http_header_field_const_key_const_value(
						    &additional_headers, HTTP_HEADER_NAME(content_description),
						    "File Transfer");

						{
							char* content_disposition_buffer = NULL;
							FORMAT_STRING(
							    &content_disposition_buffer,
							    {
								    TVEC_FREE(HttpHeaderField, &additional_headers);
								    return NULL;
							    },
							    "attachment; filename=\"%s\"", file.file_name);

							add_http_header_field_const_key_dynamic_value(
							    &additional_headers, HTTP_HEADER_NAME(content_disposition),
							    content_disposition_buffer);
						}

						{
							Time now;

							bool success = get_current_time(&now);

							if(success) {
								char* date_str = get_date_string(now, TimeFormatHTTP1Dot1);
								if(date_str != NULL) {

									add_http_header_field_const_key_dynamic_value(
									    &additional_headers, HTTP_HEADER_NAME(date), date_str);
								}
							}
						}
					}

					HTTPResponseBody body = http_response_body_from_data(
					    file.file_content.data, file.file_content.size, send_body);

					HTTPResponseToSend to_send = { .status = HttpStatusOk,
						                           .body = body,
						                           .mime_type = file.mime_type,
						                           .additional_headers = additional_headers };

					// TODO: files on nginx send also this:
					//  we also need to accapt range request (detect the header field),
					//  this should not be done in theadditional headers, as it should
					//  be done in the generic handler, so we need a send binary handler

					/* HTTP/1.1 200 OK
					Server: nginx
					Date: Sat, 31 Jan 2026 06:10:13 GMT
					Content-Type: application/octet-stream
					Content-Length: 3135
					Last-Modified: Sat, 31 Jan 2026 02:17:18 GMT
					Connection: keep-alive
					ETag: "697d662e-c3f"
					Accept-Ranges: bytes */

					result = send_http_message_to_connection(general_context, descriptor, to_send,
					                                         send_settings);

					{ // setup the value of the file, so that it isn't freed twice, as
					  // sending
						// the body frees it!
						serve_folder_result->data.file.file_content.data = NULL;
					}

					break;
				}
				case ServeFolderResultTypeFolder: {
					const ServeFolderFolderInfo folder_info = serve_folder_result->data.folder;

					if(http_properties.type != HTTPPropertyTypeNormal) {
						HTTPResponseToSend to_send = {
							.status = HttpStatusInternalServerError,
							.body = http_response_body_from_static_string(
							    "Internal Server Error: Not allowed internal type: "
							    "HTTPPropertyType",
							    send_body),
							.mime_type = MIME_TYPE_TEXT,
							.additional_headers = TVEC_EMPTY(HttpHeaderField)
						};

						result = send_http_message_to_connection(general_context, descriptor,
						                                         to_send, send_settings);
						break;
					}

					auto const normal_data = http_properties.data.normal;

					StringBuilder* html_string_builder =
					    folder_content_to_html(folder_info, normal_data.path);

					if(html_string_builder == NULL) {
						HTTPResponseToSend to_send = {
							.status = HttpStatusInternalServerError,
							.body = http_response_body_from_static_string(
							    "Internal Server Error: 4", send_body),
							.mime_type = MIME_TYPE_TEXT,
							.additional_headers = TVEC_EMPTY(HttpHeaderField)
						};

						result = send_http_message_to_connection(general_context, descriptor,
						                                         to_send, send_settings);
					} else {

						HTTPResponseBody body =
						    http_response_body_from_string_builder(&html_string_builder, send_body);

						if(http_request.head.request_line.method == HTTPRequestMethodHead) {
							body.send_body_data = false;
						}

						HTTPResponseToSend to_send = { .status = HttpStatusOk,
							                           .body = body,
							                           .mime_type = MIME_TYPE_HTML,
							                           .additional_headers =
							                               TVEC_EMPTY(HttpHeaderField) };

						result = send_http_message_to_connection(general_context, descriptor,
						                                         to_send, send_settings);
					}
					break;
				}
				default: {
					HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
						                           .body = http_response_body_from_static_string(
						                               "Internal Server Error: 5", send_body),
						                           .mime_type = MIME_TYPE_TEXT,
						                           .additional_headers =
						                               TVEC_EMPTY(HttpHeaderField) };

					result = send_http_message_to_connection(general_context, descriptor, to_send,
					                                         send_settings);
					break;
				}
			}

			free_serve_folder_result(serve_folder_result);

			break;
		}
		default: {
			HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
				                           .body = http_response_body_from_static_string(
				                               "Internal error: Implementation error", send_body),
				                           .mime_type = MIME_TYPE_TEXT,
				                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };
			result = send_http_message_to_connection(general_context, descriptor, to_send,
			                                         send_settings);
			break;
		}
	}

	free_selected_route(selected_route);

	if(result < 0) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
		                   "Error in sending response\n");
	}

	return JOB_ERROR_NONE;
}

#undef FREE_AT_END

// the connectionHandler, that ist the thread spawned by the listener, or better said by the thread
// pool, but the listener adds it
// it receives all the necessary information and also handles the html parsing and response

ANY_TYPE(JobError*)
http_socket_connection_handler(ANY_TYPE(HTTPConnectionArgument*) arg_ign,
                               const WorkerInfo worker_info) {

	// attention arg is malloced!
	HTTPConnectionArgument* argument = (HTTPConnectionArgument*)arg_ign;

	ConnectionContext* context =
	    TVEC_AT(ConnectionContextPtr, argument->contexts, worker_info.worker_index);

	char* thread_name_buffer = NULL;
	FORMAT_STRING(&thread_name_buffer, return JOB_ERROR_STRING_FORMAT;
	              , "connection handler %lu", worker_info.worker_index);
	set_thread_name(thread_name_buffer);

	const RouteManager* route_manager = argument->route_manager;

#define FREE_AT_END() \
	do { \
		unset_thread_name(); \
		free(thread_name_buffer); \
		free(argument); \
	} while(false)

	bool sig_result = setup_sigpipe_signal_handler();

	if(!sig_result) {
		FREE_AT_END();
		return NULL;
	}

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting Connection handler\n");

	ConnectionDescriptor* const descriptor =
	    get_connection_descriptor(context, argument->connection_fd);

	if(descriptor == NULL) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "get_connection_descriptor failed\n");

		FREE_AT_END();
		return JOB_ERROR_DESC;
	}

	JobError job_error = JOB_ERROR_NONE;

	HTTPReader* http_reader = initialize_http_reader_from_connection(descriptor);

	if(!http_reader) {
		HTTPResponseToSend to_send = { .status = HttpStatusInternalServerError,
			                           .body = http_response_body_from_static_string(
			                               "Internal Server Error: 0x1", true),
			                           .mime_type = MIME_TYPE_TEXT,
			                           .additional_headers = TVEC_EMPTY(HttpHeaderField) };

		int result = send_http_message_to_connection(
		    NULL, // not yet available!
		    descriptor, to_send,
		    (SendSettings){
		        .compression_to_use = CompressionTypeNone,
		        .protocol_to_use = DEFAULT_RESPONSE_PROTOCOL_VERSION,
		    });

		if(result < 0) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
			                   "Error in sending response\n");
		}

		goto cleanup;
	}

	HTTPGeneralContext* general_context = http_reader_get_general_context(http_reader);

	do {

		// raw_http_request gets freed in here
		// TODO: replace with proper error handling instead of NULL or value, thats the same as
		// optional<> ws expected<>
		HttpRequestResult http_request_result = get_http_request(http_reader);

		if(http_request_result.is_error) {

			SendSettings default_send_settings = {
				.compression_to_use = CompressionTypeNone,
				.protocol_to_use = DEFAULT_RESPONSE_PROTOCOL_VERSION,
			};

			int result = process_http_error(http_request_result.value.error, descriptor,
			                                general_context, default_send_settings, true);

			if(result < 0) {
				LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation),
				                   "Error in sending response\n");
			}

			goto cleanup;
		}

		const HTTPResultOk http_result = http_request_result.value.result;
		const HttpRequest http_request = http_result.request;

		JobError process_error =
		    process_http_request(http_request, descriptor, general_context, route_manager, argument,
		                         worker_info, http_result.settings, argument->address);

		free_http_request_result(http_result);

		if(process_error == JOB_ERROR_CLEANUP_CONNECTION) {
			job_error = JOB_ERROR_NONE;
			goto cleanup;
		}

		if(process_error != JOB_ERROR_NONE) {
			job_error = process_error;
			goto cleanup;
		}
	} while(http_reader_more_available(http_reader));

cleanup:

	// TODO(Totto): should we log, if the reader had an error or what the reason for the exit of the
	// loop was?

	bool finished_cleanly = finish_reader(http_reader, context);

	// free the malloced stuff
	// needs to be called at the very end, as some things here are in use by the http_reader
	FREE_AT_END();

	if(!finished_cleanly) {
		job_error = JOB_ERROR_CLOSE;
	}

	return job_error;
}

#undef FREE_AT_END

// implemented specifically for the http Server, it just gets the internal value, but it's better to
// not access that, since additional steps can be required, like  boundary checks!
static int myqueue_size(Myqueue* queue) {
	if(queue->size < 0) {
		LOG_MESSAGE(LogLevelCritical,
		            "internal size implementation error in the queue, value negative: %d!",
		            queue->size);
	}
	return queue->size;
}

// this is the function, that runs in the listener, it receives all necessary information
// trough the argument
ANY_TYPE(ListenerError*) http_listener_thread_function(ANY_TYPE(HTTPThreadArgument*) arg) {

	set_thread_name("listener thread");

	LOG_MESSAGE_SIMPLE(LogLevelTrace, "Starting\n");

	HTTPThreadArgument argument = *((HTTPThreadArgument*)arg);

	RUN_LIFECYCLE_FN(argument.fns.startup_fn);

#define POLL_FD_AMOUNT 2

	struct pollfd poll_fds[POLL_FD_AMOUNT] = {};
	// initializing the structs for poll
	poll_fds[0].fd = argument.socket_fd;
	poll_fds[0].events = POLLIN;

	int sig_fd = get_signal_like_fd(SIGINT);
	// TODO(Totto): don't exit here
	CHECK_FOR_ERROR(sig_fd, "While trying to cancel the listener Thread on signal",
	                exit(EXIT_FAILURE););

	poll_fds[1].fd = sig_fd;
	poll_fds[1].events = POLLIN;
	// loop and accept incoming requests
	while(true) {

		// TODO(Totto): Set cancel state in correct places!

		// the function poll makes the heavy lifting, the timeout 5000 is completely
		// arbitrary and should not be to short, but otherwise it doesn't matter that much,
		// since it aborts on POLLIN from the socket_fd or the signalFd
		int status = 0;
		while(status == 0) {
			status = poll(
			    poll_fds, POLL_FD_AMOUNT,
			    5000); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			if(status < 0) {
				LOG_MESSAGE(LogLevelError, "poll failed: %s\n", strerror(errno));
				continue;
			}
		}

		if(poll_fds[1].revents == POLLIN || g_signal_received != 0) {
			// TODO(Totto): This fd isn't closed, when pthread_cancel is called from somewhere else,
			// fix that somehow
			close(poll_fds[1].fd);
			RUN_LIFECYCLE_FN(argument.fns.shutdown_fn);
			int result = pthread_cancel(pthread_self());
			CHECK_FOR_ERROR(result, "While trying to cancel the listener Thread on signal",
			                return LISTENER_ERROR_THREAD_CANCEL;);
			return LISTENER_ERROR_THREAD_AFTER_CANCEL;
		}

		// the poll didn't see a POLLIN event in the argument.socket_fd fd, so the accept
		// will fail, just redo the poll
		if(poll_fds[0].revents != POLLIN) {
			continue;
		}

		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		// would be better to set cancel state in the right places!!
		int connection_fd = accept(argument.socket_fd, (struct sockaddr*)&client_addr, &addr_len);
		CHECK_FOR_ERROR(connection_fd, "While Trying to accept a socket",
		                return LISTENER_ERROR_ACCEPT;);

		IPAddress address = from_ipv4(client_addr.sin_addr);

		HTTPConnectionArgument* connection_argument =
		    (HTTPConnectionArgument*)malloc(sizeof(HTTPConnectionArgument));

		if(!connection_argument) {
			LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
			                   "Couldn't allocate memory!\n");
			return LISTENER_ERROR_MALLOC;
		}

		// to have longer lifetime, that is needed here, since otherwise it would be "dead"
		connection_argument->contexts = argument.contexts;
		connection_argument->connection_fd = connection_fd;
		connection_argument->listener_thread = pthread_self();
		connection_argument->web_socket_manager = argument.web_socket_manager;
		connection_argument->route_manager = argument.route_manager;
		connection_argument->address = address;

		// push to the queue, but not await, since when we wait it wouldn't be fast and
		// ready to accept new connections
		if(myqueue_push(argument.job_ids, pool_submit(argument.pool, http_socket_connection_handler,
		                                              connection_argument)) < 0) {
			return LISTENER_ERROR_QUEUE_PUSH;
		}

		// not waiting directly, but when the queue grows to fast, it is reduced, then the
		// listener thread MIGHT block, but probably are these first jobs already finished,
		// so its super fast,but if not doing that, the queue would overflow, nothing in
		// here is a cancellation point, so it's safe to cancel here, since only accept then
		// really cancels
		int size = myqueue_size(argument.job_ids);
		if(size > HTTP_MAX_QUEUE_SIZE) {
			int boundary = size / 2;
			while(size > boundary) {

				JobId* job_id = (JobId*)myqueue_pop(argument.job_ids);

				JobError result = pool_await(job_id);

				if(is_job_error(result)) {
					if(result != JOB_ERROR_NONE) {
						print_job_error(result);
					}
				} else if(result == PTHREAD_CANCELED) {
					LOG_MESSAGE_SIMPLE(LogLevelError, "A connection thread was cancelled!\n");
				} else {
					LOG_MESSAGE(LogLevelError,
					            "A connection thread was terminated with wrong error: %p!\n",
					            result);
				}

				--size;
			}
		}

		// gets cancelled in accept, there it also is the most time!
		// otherwise if it would cancel other functions it would be baaaad, but only accept
		// is here a cancel point!
	}

	RUN_LIFECYCLE_FN(argument.fns.shutdown_fn);
}

int start_http_server(uint16_t port, SecureOptions* const options,
                      AuthenticationProviders* const auth_providers, HTTPRoutes* routes) {

	// using TCP  and not 0, which is more explicit about what protocol to use
	// so essentially a socket is created, the protocol is AF_INET alias the IPv4 Prototol,
	// the socket type is SOCK_STREAM, meaning it has reliable read and write capabilities,
	// all other types are not that well suited for that example
	int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	CHECK_FOR_ERROR(socket_fd, "While Trying to create socket", return EXIT_FAILURE;);

	// set the reuse port option to the socket, so it can be reused
	const int optval = 1;
	int option_return = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	CHECK_FOR_ERROR(option_return, "While Trying to set socket option 'SO_REUSEPORT'",
	                return EXIT_FAILURE;);

	// creating the sockaddr_in struct, each number that is used in context of network has
	// to be converted into ntework byte order (Big Endian, linux uses Little Endian) that
	// is relevant for each multibyte value, essentially everything but char, so htox is
	// used, where x stands for different lengths of numbers, s for int, l for long
	struct sockaddr_in* addr =
	    (struct sockaddr_in*)malloc_with_memset(sizeof(struct sockaddr_in), true);

	if(!addr) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	addr->sin_family = AF_INET;
	// hto functions are used for networking, since there every number is BIG ENDIAN and
	// linux has Little Endian
	addr->sin_port = htons(port);
	// INADDR_ANY is 0.0.0.0, which means every port, but when nobody forwards it,
	// it means, that by default only localhost can be used to access it
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

	// since bind is generic, the specific struct has to be casted, and the actual length
	// has to be given, this is a function signature, just to satisfy the typings, the real
	// requirements are given in the responsive protocol man page, here ip(7) also note that
	// ports below 1024 are  privileged ports, meaning, that you require special permissions
	// to be able to bind to them ( CAP_NET_BIND_SERVICE capability) (the simple way of
	// getting that is being root, or executing as root: sudo ...)
	int result = bind(socket_fd, (struct sockaddr*)addr, sizeof(*addr));
	CHECK_FOR_ERROR(result, "While trying to bind socket to port", return EXIT_FAILURE;);

	// SOCKET_BACKLOG_SIZE is used, to be able to change it easily, here it denotes the
	// connections that can be unaccepted in the queue, to be accepted, after that is full,
	// the protocol discards these requests listen starts listening on that socket, meaning
	// new connections can be accepted
	result = listen(socket_fd, HTTP_SOCKET_BACKLOG_SIZE);
	CHECK_FOR_ERROR(result, "While trying to listen on socket", return EXIT_FAILURE;);

	const char* protocol_string =
	    is_secure(options) ? "https" : "http"; // NOLINT(readability-implicit-bool-conversion)

	LOG_MESSAGE(LogLevelInfo, "To use this simple Http Server visit '%s://localhost:%d'.\n",
	            protocol_string, port);

	if(routes == NULL) {
		return EXIT_FAILURE;
	}

	LOG_MESSAGE(LogLevelTrace, "Defined Routes (%zu):\n", TVEC_LENGTH(HTTPRoute, routes->routes));
	if(log_should_log(LogLevelTrace)) {
		for(size_t i = 0; i < TVEC_LENGTH(HTTPRoute, routes->routes); ++i) {
			HTTPRoute route = TVEC_AT(HTTPRoute, routes->routes, i);

			LOG_MESSAGE(LogLevelTrace, "Route %zu:\n", i);

			{ // Path
				LOG_MESSAGE(LogLevelTrace, "\tPath: %s ", route.path.data);

				switch(route.path.type) {
					case HTTPRoutePathTypeExact: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "(Exact)\n");
						break;
					}
					case HTTPRoutePathTypeStartsWith: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "(StarsWith)\n");
						break;
					}
					default: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "(<Unknown>)\n");
						break;
					}
				}
			}

			{ // Method
				LOG_MESSAGE_SIMPLE(LogLevelTrace, "\tMethod: ");

				switch(route.method) {
					case HTTPRequestRouteMethodGet: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "GET\n");
						break;
					}
					case HTTPRequestRouteMethodPost: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "POST\n");
						break;
					}
					default: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "<Unknown>\n");
						break;
					}
				}
			}

			{ // Auth
				LOG_MESSAGE_SIMPLE(LogLevelTrace, "\tAuth: ");

				switch(route.auth.type) {
					case HTTPAuthorizationTypeNone: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "None\n");
						break;
					}
					case HTTPAuthorizationTypeSimple: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "Simple\n");
						break;
					}
					case HTTPAuthorizationTypeComplicated: {
						const HTTPAuthorizationComplicatedData data = route.auth.data.complicated;

						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						            "Complicated: (TODO %d)\n", data.todo);

						break;
					}
					default: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "<Unknown>\n");
						break;
					}
				}
			}

			{ // Handler
				LOG_MESSAGE_SIMPLE(LogLevelTrace, "\tHandler: ");

				switch(route.data.type) {
					case HTTPRouteTypeNormal: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "Normal ");

						const HTTPRouteFn data = route.data.value.normal;

						switch(data.type) {
							case HTTPRouteFnTypeExecutor: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "(Executor Fn)\n");
								break;
							}
							case HTTPRouteFnTypeExecutorExtended: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "(Extended Executor Fn)\n");
								break;
							}
							case HTTPRouteFnTypeExecutorAuth: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "(Auth Executor Fn)\n");
								break;
							}
							default: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "<Unknown>)\n");
								break;
							}
						}

						break;
					}
					case HTTPRouteTypeSpecial: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "Special: ");

						const HTTPRouteSpecialData data = route.data.value.special;

						switch(data.type) {
							case HTTPRouteSpecialDataTypeShutdown: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "Shutdown\n");
								break;
							}
							case HTTPRouteSpecialDataTypeWs: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude), "WS\n");
								break;
							}
							default: {
								LOG_MESSAGE_SIMPLE(
								    COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
								    "<Unknown>\n");
								break;
							}
						}

						break;
					}
					case HTTPRouteTypeInternal: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "Internal (Static data)\n");

						break;
					}
					case HTTPRouteTypeServeFolder: {

						const HTTPRouteServeFolder data = route.data.value.serve_folder;

						LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						            "Serve Folder: %s\n", data.folder_path);

						break;
					}
					default: {
						LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelTrace, LogPrintNoPrelude),
						                   "<Unknown>\n");
						break;
					}
				}
			}
		}
	}

	// set up the signal handler
	// just create a sigaction structure, then add the handler
	struct sigaction action = {};

	action.sa_handler = receive_signal;
	// initialize the mask to be empty
	int empty_set_result = sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGINT);
	int result1 = sigaction(SIGINT, &action, NULL);
	if(result1 < 0 || empty_set_result < 0) {
		LOG_MESSAGE(LogLevelError, "Couldn't set signal interception: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// create pool and queue! then initializing both!
	// the pool is created and destroyed outside of the listener, so the listener can be
	// cancelled and then the main thread destroys everything accordingly
	ThreadPool pool;
	int create_result = pool_create_dynamic(&pool);
	if(create_result < 0) {
		print_create_error(-create_result);
		return EXIT_FAILURE;
	}

	// this is a internal synchronized queue! myqueue_init creates a semaphore that handles
	// that
	Myqueue job_ids;
	if(myqueue_init(&job_ids) < 0) {
		return EXIT_FAILURE;
	};

	// this is an array of pointers
	ConnectionContextPtrs contexts = TVEC_EMPTY(ConnectionContextPtr);

	const TvecResult allocate_result =
	    TVEC_ALLOCATE_UNINITIALIZED(ConnectionContextPtr, &contexts, pool.worker_threads_amount);

	if(allocate_result == TvecResultErr) {
		LOG_MESSAGE_SIMPLE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation),
		                   "Couldn't allocate memory!\n");
		return EXIT_FAILURE;
	}

	for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
		ConnectionContext* context = get_connection_context(options);

		auto _ = TVEC_SET_AT(ConnectionContextPtr, &contexts, i, context);
		UNUSED(_);
	}

	WebSocketThreadManager* web_socket_manager = initialize_thread_manager();

	if(!web_socket_manager) {
		for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
			ConnectionContext* context = TVEC_AT(ConnectionContextPtr, contexts, i);
			free_connection_context(context);
		}
		TVEC_FREE(ConnectionContextPtr, &contexts);

		return EXIT_FAILURE;
	}

	RouteManager* route_manager = initialize_route_manager(routes, auth_providers);

	if(!route_manager) {
		for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
			ConnectionContext* context = TVEC_AT(ConnectionContextPtr, contexts, i);
			free_connection_context(context);
		}
		TVEC_FREE(ConnectionContextPtr, &contexts);

		if(!free_thread_manager(web_socket_manager)) {
			return EXIT_FAILURE;
		}

		return EXIT_FAILURE;
	}

	// create global http arguments
	global_initialize_http_global_data();

	// initializing the thread Arguments for the single listener thread, it receives all
	// necessary arguments
	pthread_t listener_thread = {};
	HTTPThreadArgument thread_argument = { .pool = &pool,
		                                   .job_ids = &job_ids,
		                                   .contexts = contexts,
		                                   .socket_fd = socket_fd,
		                                   .web_socket_manager = web_socket_manager,
		                                   .route_manager = route_manager,
		                                   .fns = { .startup_fn = NULL, .shutdown_fn = NULL } };

	// creating the thread
	result =
	    pthread_create(&listener_thread, NULL, http_listener_thread_function, &thread_argument);
	CHECK_FOR_THREAD_ERROR(result, "An Error occurred while trying to create a new Thread",
	                       return EXIT_FAILURE;);

	// wait for the single listener thread to finish, that happens when he is cancelled via
	// shutdown request
	ListenerError return_value = LISTENER_ERROR_NONE;
	result = pthread_join(listener_thread, &return_value);
	CHECK_FOR_THREAD_ERROR(result, "An Error occurred while trying to wait for a Thread",
	                       return EXIT_FAILURE;);

	if(is_listener_error(return_value)) {
		if(return_value != LISTENER_ERROR_NONE) {
			print_listener_error(return_value);
		}
	} else if(return_value != PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelError, "The http listener thread wasn't cancelled properly!\n");
	} else if(return_value == PTHREAD_CANCELED) {
		LOG_MESSAGE_SIMPLE(LogLevelInfo, "The http listener thread was cancelled properly!\n");
	} else {
		LOG_MESSAGE(LogLevelError,
		            "The http listener thread was terminated with wrong error: %p!\n",
		            return_value);
	}

	// since the listener doesn't wait on the jobs, the main thread has to do that work!
	// the queue can be filled, which can lead to a problem!!
	while(!myqueue_is_empty(&job_ids)) {
		JobId* job_id = (JobId*)myqueue_pop(&job_ids);

		JobError job_result = pool_await(job_id);

		if(is_job_error(job_result)) {
			if(job_result != JOB_ERROR_NONE) {
				print_job_error(job_result);
			}
		} else if(job_result == PTHREAD_CANCELED) {
			LOG_MESSAGE_SIMPLE(LogLevelError, "A connection thread was cancelled!\n");
		} else {
			LOG_MESSAGE(LogLevelError, "A connection thread was terminated with wrong error: %p!\n",
			            job_result);
		}
	}

	// then after all were awaited the pool is destroyed
	if(pool_destroy(&pool) < 0) {
		return EXIT_FAILURE;
	}

	// then the queue is destroyed
	if(myqueue_destroy(&job_ids) < 0) {
		return EXIT_FAILURE;
	}

	// finally closing the whole socket, so that the port is useable by other programs or by
	// this again, NOTES: ip(7) states :" A TCP local socket address that has been bound is
	// unavailable for  some time after closing, unless the SO_REUSEADDR flag has been set.
	// Care should be taken when using this flag as it makes TCP less reliable." So
	// essentially saying, also correctly closed sockets aren't available after a certain
	// time, even if closed correctly!
	result = close(socket_fd);
	CHECK_FOR_ERROR(result, "While trying to close the socket", return EXIT_FAILURE;);

	// and freeing the malloced sockaddr_in, could be done (probably, since the receiver of
	// this option has already got that argument and doesn't read data from that pointer
	// anymore) sooner.
	free(addr);

	for(size_t i = 0; i < pool.worker_threads_amount; ++i) {
		ConnectionContext* context = TVEC_AT(ConnectionContextPtr, contexts, i);
		free_connection_context(context);
	}

	if(!thread_manager_remove_all_connections(web_socket_manager)) {
		return EXIT_FAILURE;
	}

	if(!free_thread_manager(web_socket_manager)) {
		return EXIT_FAILURE;
	}

	free_route_manager(route_manager);

	TVEC_FREE(ConnectionContextPtr, &contexts);

	free_secure_options(options);

	free_authentication_providers(auth_providers);

#ifdef _SIMPLE_SERVER_USE_OPENSSL
	openssl_cleanup_global_state();
#endif

	global_free_http_global_data();

	return EXIT_SUCCESS;
}

void global_initialize_http_global_data(void) {
	global_initialize_mime_map();
	global_initialize_locale_for_http();
	global_initialize_http2_hpack_data();
}

void global_free_http_global_data(void) {
	global_free_mime_map();
	global_free_locale_for_http();
	global_free_http2_hpack_data();
}
