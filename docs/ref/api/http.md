# HTTP Support

NNG offers support for creation of HTTP clients, and servers. NNG supports HTTP/1.1 at present, and supports
a subset of functionality, but the support should be sufficient for simple clients, REST API servers, static content servers,
and gateways between HTTP and and other protocols. It also provides support for WebSocket based connections.

HTTP follows a request/reply model, where a client issues a request, and the server is expected to reply.
Every request is answered with a single reply.

## Header File

```c
#include <nng/http.h>
```

Unlike the rest of NNG, the HTTP API in NNG requires including `nng/http.h`. It is not necessary to include
the main `nng/nng.h` header, it will be included transitively by `nng/http.h`.

## Connection Object

```c
typedef struct nng_http nng_http;
```

The {{i:`nng_http`}} object represents a single logical HTTP connection to the server.
For HTTP/1.1 and earlier, this will correspond to a single TCP connection, but the object
also contains state relating to the transaction, such as the hostname used, HTTP method used,
request headers, response status, response headers, and so forth.

An `nng_http` object can be reused, unless closed, so that additional transactions can be
performed after the first transaction is complete.

At any given point in time, an `nng_http` object can only refer to a single HTTP transaction.
In NNG, these `nng_http` objects are used in both the client and server APIs.

The `nng_http` object is created by either [`nng_http_client_connect`] or by an HTTP server
object which then passes it to an [`nng_http_handler`] callback function.

### HTTP Method

```c
void nng_http_set_method(nng_http *conn, const char *method);
const char *nng_http_get_method(nng_http *conn);
```

Each HTTP transaction has a single verb, or method, that is used. The most common methods are "GET", "HEAD", and "POST",
but a number of others are possible.

The {{i:`nng_http_set_method`}} function specifies the HTTP method to use for the transaction.
The default is "GET". HTTP methods are case sensitive, and generally upper-case, such as "GET", "POST", "HEAD",
and so forth. This function silently truncates any method to 32-characters. (There are no defined methods longer than this.)

The {{i:`nng_http_get_method`}} function is used, typically on a server, to retrieve the method the client
set when issuing the transaction.

### HTTP Protocol Version

```c
int nng_http_set_version(nng_http *conn, const char *version);
const char *nng_http_get_version(nng_http *conn);
```

The {{i:`nng_http_set_version`}} function is used to select the HTTP protocol version to use for the
exchange. At present, only the values `NNG_HTTP_VERSION_1_0` and `NNG_HTTP_VERSION_1_1` (corresponding to
"HTTP/1.0" and "HTTP/1.1") are supported. NNG will default to using "HTTP/1.1" if this function is not called.
If an unsupported version is supplied, [`NNG_ENOTSUP`] will be returned, otherwise zero.

The {{i:`nng_http_get_version`}} function is used to determine the version the client selected. Normally
there is little need to use this, but there are some subtle semantic differences between HTTP/1.0 and HTTP/1.1.

> [!TIP]
> There are few, if any, remaining HTTP/1.0 implementations that are not also capable of HTTP/1.1.
> It might be easiest to just fail any request coming in that is not HTTP/1.1.

> [!NOTE]
> NNG does not support HTTP/2 or HTTP/3 at this time.

### HTTP Status

```c
uint16_t nng_http_get_status(nng_http *conn);
const char *nng_http_get_reason(nng_http_conn *conn);
void nng_http_set_status(nng_http *conn, uint16_t status, const char *reason);
```

The {{i:`nng_http_get_status`}} function obtains the numeric code (typipcally numbered from 100 through 599) returned
by the server in the last exchange on _conn_. (If no exchange has been performed yet, the result is undefined.)

A descriptive message matching the status code is returned by {{i:`nng_http_get_reason`}}.

The {{i:`nng_http_set_status`}} function is used on a server in a handler callback to set the status codethat will be
reported to the client to _status_, and the associated text (reason) to _reason_. If _reason_ is `NULL`,
then a built in reason based on the _status_ will be used instead.

> [!TIP]
> Callbacks used on the server may wish to use [`nng_http_server_set_error`] or [`nng_http_server_set_redirect`] instead of
> `nng_http_set_status`, because those functions will also set the response body to a suitable HTML document
> for display to users.

Status codes are defined by the IETF. Here are defininitions that NNG provides for convenience:

| Name                                                                                             | Code | Reason Text                     | Notes                                                 |
| ------------------------------------------------------------------------------------------------ | ---- | ------------------------------- | ----------------------------------------------------- |
| `NNG_HTTP_STATUS_CONTINUE`<a name="#NNG_HTTP_STATUS_CONTINUE"></a>                               | 100  | Continue                        | Partial transfer, client may send body.               |
| `NNG_HTTP_STATUS_SWITCHING`<a name="#NNG_HTTP_STATUS_SWITCHING"></a>                             | 101  | Switching Protocols             | Used when upgrading or hijacking a connection.        |
| `NNG_HTTP_STATUS_PROCESSING`<a name="#NNG_HTTP_STATUS_PROCESSING"></a>                           | 102  | Processing                      |
| `NNG_HTTP_STATUS_OK`<a name="#NNG_HTTP_STATUS_OK"></a>                                           | 200  | OK                              | Successful result.                                    |
| `NNG_HTTP_STATUS_CREATED`<a name="#NNG_HTTP_STATUS_CREATED"></a>                                 | 201  | Created                         | Resource created successfully.                        |
| `NNG_HTTP_STATUS_ACCEPTED`<a name="#NNG_HTTP_STATUS_ACCEPTED"></a>                               | 202  | Created                         | Request accepted for future processing.               |
| `NNG_HTTP_STATUS_NOT_AUTHORITATIVE`<a name="#NNG_HTTP_STATUS_NOT_AUTHORITATIVE"></a>             | 203  | Not Authoritative               | Request successful, but modified by proxy.            |
| `NNG_HTTP_STATUS_NO_CONTENT`<a name="#NNG_HTTP_STATUS_NO_CONTENT"></a>                           | 204  | No Content                      | Request successful, no content returned.              |
| `NNG_HTTP_STATUS_RESET_CONTENT`<a name="#NNG_HTTP_STATUS_NO_CONTENT"></a>                        | 205  | Reset Content                   | Request successful, client should reload content.     |
| `NNG_HTTP_STATUS_PARTIAL_CONTENT`<a name="#NNG_HTTP_STATUS_NO_CONTENT"></a>                      | 206  | Partial Content                 | Response to a range request.                          |
| `NNG_HTTP_STATUS_MULTI_STATUS`<a name="#NNG_HTTP_STATUS_MULTI_STATUS"></a>                       | 207  | Multi-Status                    | Used with WebDAV.                                     |
| `NNG_HTTP_STATUS_ALREADY_REPORTED`<a name="#NNG_HTTP_STATUS_ALREADY_REPORTED"></a>               | 208  | Already Reported                | Used with WebDAV.                                     |
| `NNG_HTTP_STATUS_IM_USED`<a name="#NNG_HTTP_STATUS_IM_USED"></a>                                 | 226  | IM Used                         | Used with delta encodings, rarely supported.          |
| `NNG_HTTP_STATUS_MULTIPLE_CHOICES`<a name="#NNG_HTTP_STATUS_MULTIPLE_CHOICES"></a>               | 300  | Multiple Choices                | Multiple responses possible, client should choose.    |
| `NNG_HTTP_STATUS_MOVED_PERMANENTLY`<a name="#NNG_HTTP_STATUS_MOVED_PERMANENTLY"></a>             | 301  | Moved Permanently               | Permanent redirection, may be saved by client.        |
| `NNG_HTTP_STATUS_FOUND`<a name="#NNG_HTTP_STATUS_FOUND"></a>                                     | 302  | Found                           | Temporary redirection, client may switch to GET.      |
| `NNG_HTTP_STATUS_SEE_OTHER`<a name="#NNG_HTTP_STATUS_SEE_OTHER"></a>                             | 303  | See Other                       | Redirect, perhaps after a success POST or PUT.        |
| `NNG_HTTP_STATUS_NOT_MODIFIED`<a name="#NNG_HTTP_STATUS_NOT_MODIFIED"></a>                       | 304  | Not Modified                    | Resource not modified, client may use cached version. |
| `NNG_HTTP_STATUS_USE_PROXY`<a name="#NNG_HTTP_STATUS_USE_PROXY"></a>                             | 305  | Use Proxy                       |
| `NNG_HTTP_STATUS_TEMPORARY_REDIRECT`<a name="#NNG_HTTP_STATUS_TEMPORARY_REDIRECT"></a>           | 307  | Temporary Redirect              | Temporary redirect, preserves method.                 |
| `NNG_HTTP_STATUS_PERMANENT_REDIRECT`<a name="#NNG_HTTP_STATUS_PERMANENT_REDIRECT"></a>           | 308  | Permanent Redirect              | Permanent redirect.                                   |
| `NNG_HTTP_STATUS_BAD_REQUEST`<a name="#NNG_HTTP_STATUS_BAD_REQUEST"></a>                         | 400  | Bad Request                     | Generic problem with the request.                     |
| `NNG_HTTP_STATUS_UNAUTHORIZED`<a name="#NNG_HTTP_STATUS_UNAUTHORIZED"></a>                       | 401  | Unauthorized                    | Indicates a problem with authentication.              |
| `NNG_HTTP_STATUS_PAYMENT_REQUIRED`<a name="#NNG_HTTP_STATUS_PAYMENT_REQUIRED"></a>               | 402  | Payment Required                |
| `NNG_HTTP_STATUS_FORBIDDEN`<a name="#NNG_HTTP_STATUS_FORBIDDEN"></a>                             | 403  | Forbidden                       | No permission to access resource.                     |
| `NNG_HTTP_STATUS_NOT_FOUND`<a name="#NNG_HTTP_STATUS_NOT_FOUND"></a>                             | 404  | Not Found                       | Resource does not exist.                              |
| `NNG_HTTP_STATUS_METHOD_NOT_ALLOWED`<a name="#NNG_HTTP_STATUS_METHOD_NOT_ALLOWED"></a>           | 405  | Method Not Allowed              | Resource does not support the method.                 |
| `NNG_HTTP_STATUS_METHOD_NOT_ACCEPTABLE`<a name="#NNG_HTTP_STATUS_METHOD_NOT_ACCEPTABLE"></a>     | 406  | Not Acceptable                  | Could not satisfy accept requirements.                |
| `NNG_HTTP_STATUS_PROXY_AUTH_REQUIRED`<a name="#NNG_HTTP_STATUS_PROXY_AUTH_REQUIRED"></a>         | 407  | Proxy Authentication Required   | Proxy requires authentication.                        |
| `NNG_HTTP_STATUS_REQUEST_TIMEOUT`<a name="#NNG_HTTP_STATUS_REQUEST_TIMEOUT"></a>                 | 408  | Request Timeout                 | Timed out waiting for request.                        |
| `NNG_HTTP_STATUS_CONFLICT`<a name="#NNG_HTTP_STATUS_CONFLICT"></a>                               | 409  | Conflict                        | Conflicting request.                                  |
| `NNG_HTTP_STATUS_GONE`<a name="#NNG_HTTP_STATUS_GONE"></a>                                       | 410  | Gone                            | Resource no longer exists.                            |
| `NNG_HTTP_STATUS_LENGTH_REQUIRED`<a name="#NNG_HTTP_STATUS_LENGTH_REQUIRED"></a>                 | 411  | Length Required                 | Missing Content-Length.                               |
| `NNG_HTTP_STATUS_PRECONDITION_FAILED`<a name="#NNG_HTTP_STATUS_PRECONDITION_FAILED"></a>         | 412  | Precondition Failed             |                                                       |
| `NNG_HTTP_STATUS_CONTENT_TOO_LARGE`<a name="#NNG_HTTP_STATUS_PAYLOAD_TOO_LARGE"></a>             | 413  | Content Too Large               |                                                       |
| `NNG_HTTP_STATUS_URI_TOO_LONG`<a name="#NNG_HTTP_STATUS_URI_TOO_LONG"></a>                       | 414  | URI Too Long                    |                                                       |
| `NNG_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE`<a name="#NNG_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE"></a>   | 415  | Unsupported Media Type          |
| `NNG_HTTP_STATUS_RANGE_NOT_SATISFIABLE`<a name="#NNG_HTTP_STATUS_RANGE_NOT_SATISFIABLE"></a>     | 416  | Range Not Satisfiable           |
| `NNG_HTTP_STATUS_EXPECTATION_FAILED`<a name="#NNG_HTTP_STATUS_EXPECTATION_FAILED"></a>           | 417  | Expectation Failed              |
| `NNG_HTTP_STATUS_TEAPOT`<a name="#NNG_HTTP_STATUS_TEAPOT"></a>                                   | 418  | I Am A Teapot                   | RFC 2324.                                             |
| `NNG_HTTP_STATUS_UNPROCESSABLE_ENTITY`<a name="#NNG_HTTP_STATUS_UNPROCESSABLE_ENTITY"></a>       | 422  | Unprocessable Entity            |
| `NNG_HTTP_STATUS_LOCKED`<a name="#NNG_HTTP_STATUS_LOCKED"></a>                                   | 423  | Locked                          |
| `NNG_HTTP_STATUS_FAILED_DEPENDENCY`<a name="#NNG_HTTP_STATUS_FAILED_DEPEDNENCY"></a>             | 424  | Failed Dependency               |
| `NNG_HTTP_STATUS_TOO_EARLY`<a name="#NNG_HTTP_STATUS_TOO_EARLY"></a>                             | 425  | Too Early                       |
| `NNG_HTTP_STATUS_UPGRADE_REQUIRED`<a name="#NNG_HTTP_STATUS_UPGRADE_REQUIRED"></a>               | 426  | Upgrade Required                |
| `NNG_HTTP_STATUS_PRECONDITION_REQUIRED`<a name="#NNG_HTTP_STATUS_PRECONDITION_REQUIRED"></a>     | 428  | Precondition Required           |                                                       |
| `NNG_HTTP_STATUS_TOO_MANY_REQUESTS`<a name="#NNG_HTTP_STATUS_TOO_MANY_REQUESTS"></a>             | 429  | Too Many Requests               |                                                       |
| `NNG_HTTP_STATUS_HEADERS_TOO_LARGE`<a name="#NNG_HTTP_STATUS_HEADERS_TOO_LARGE"></a>             | 431  | Headers Too Large               |                                                       |
| `NNG_HTTP_STATUS_UNAVAIL_LEGAL_REASONS`<a name="#NNG_HTTP_STATUS_UNAVAIL_LEGAL_REASONS"></a>     | 451  | Unavailabe For Legal Reasons    |                                                       |
| `NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR`<a name="#NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR"></a>     | 500  | Internal Server Error           |
| `NNG_HTTP_STATUS_NOT_IMPLEMENTED`<a name="#NNG_HTTP_STATUS_NOT_IMPLEMENTED"></a>                 | 501  | Not Implemented                 | Server does not implement method.                     |
| `NNG_HTTP_STATUS_BAD_GATEWAY`<a name="#NNG_HTTP_STATUS_BAD_GATEWAY"></a>                         | 502  | Bad Gateway                     |
| `NNG_HTTP_STATUS_SERVICE_UNAVAILALE`<a name="#NNG_HTTP_STATUS_SERVICE_UNAVAILABLE"></a>          | 503  | Service Unavailable             |
| `NNG_HTTP_STATUS_GATEWAY_TIMEOUT`<a name="#NNG_HTTP_STATUS_GATEWAY_TIMEOUT"></a>                 | 504  | Gateway TImeout                 |
| `NNG_HTTP_STATUS_HTTP_VERSION_NOT_SUPP`<a name="#NNG_HTTP_STATUS_HTTP_VERSION_NOT_SUPP"></a>     | 505  | HTTP Version Not Supported      |
| `NNG_HTTP_STATUS_VARIANT_ALSO_NEGOTIATES`<a name="#NNG_HTTP_STATUS_VARIANT_ALSO_NEGOTIATES"></a> | 506  | Variant Also Negotiates         |
| `NNG_HTTP_STATUS_INSUFFICIENT_STORAGE`<a name="#NNG_HTTP_STATUS_INSUFFICIENT_STORAGE"></a>       | 507  | Variant Also Negotiates         |
| `NNG_HTTP_STATUS_LOOP_DETECTED`<a name="#NNG_HTTP_STATUS_LOOP_DETECTED"></a>                     | 508  | Loop Detected                   |
| `NNG_HTTP_STATUS_NOT_EXTENDED`<a name="#NNG_HTTP_STATUS_NOT_EXTENDED"></a>                       | 510  | Not Extended                    |
| `NNG_HTTP_STATUS_NETWORK_AUTH_REQUIRED`<a name="#NNG_HTTP_STATUS_NETWORK_AUTH_REQUIRED"></a>     | 511  | Network Authentication Required |

### Closing the Connection

```c
void nng_http_close(nng_http *conn);
```

The {{i:`nng_http_close`}} function closes the supplied HTTP connection _conn_,
including any disposing of any underlying file descriptors or related resources.

Once this function, no further access to the _conn_ structure may be made.

### Reset Connection State

```c
void nng_http_reset(nng_http *conn);
```

The {{i:`nng_http_reset`}} function resets the request and response state of the
the connection _conn_.

The "Host" parameter will be retained for client connections, but the URI will not.

The intended purpose of this function is to clear the object state before reusing the _conn_ for
subsequent transactions.

### Request and Response Headers

```c
int nng_http_request_add_header(nng_http *conn, const char *key, const char *val);
int nng_http_request_set_header(nng_http *conn, const char *key, const char *val);
```

### Direct Read and Write

```c
void nng_http_read(nng_http *conn, nng_aio *aio);
void nng_http_write(nng_http *conn, nng_aio *aio);
void nng_http_read_all(nng_http *conn, nng_aio *aio);
void nng_http_write_all(nng_http *conn, nng_aio *aio);
```

The {{i:`nng_http_read`}} and {{i:`nng_http_write`}} functions read or write data asynchronously from or to the
connection _conn_, using the [`nng_iov`] that is set in _aio_ with [`nng_aio_set_iov`].
These functions will complete as soon as any data is transferred.
Use [`nng_aio_get_count`] to determine how much data was actually transferred.

The {{i:`nng_http_read_all`}} and {{`nng_http_write_all`}} functions perform the same task, but will keep resubmitting
operations until the the entire amount of data requested by the [`nng_iov`] is transferred.

> [!NOTE]
> These functions perform no special handling for chunked transfers.

These functions are most likely to be useful after hijacking the connection with [`nng_http_hijack`].
They can be used to transfer request or response body data as well.

### Hijacking Connections

```c
void nng_http_hijack(nng_http_conn *conn);
```

TODO: This API will change to convert the conn into a stream object.

The {{i:`nng_http_hijack`}} function hijacks the connection _conn_, causing it
to be disassociated from the HTTP server where it was created.

The purpose of this function is the creation of HTTP upgraders (such as
WebSocket), where the underlying HTTP connection will be taken over for
some other purpose, and should not be used any further by the server.

This function is most useful when called from a handler function.
(See [`nng_http_handler_alloc`].)

> [!NOTE]
> It is the responsibility of the caller to dispose of the underlying connection when it is no longer needed.
> Furthermore, the HTTP server will no longer send any responses to the hijacked connection, so the caller should do that as well if appropriate.
> (See [`nng_http_conn_write_res`].)

> [!TIP]
> This function is intended to facilitate uses cases that involve changing the protocol from HTTP, such as WebSocket.
> Most applications will never need to use this function.

## Client API

The NNG client API consists of an API for creating connections, and an API for performing
transactions on those connections.

### Client Object

```c
typedef struct nng_http_client nng_http_client;
```

The {{i:`nng_http_client`}} object is the client side creator for [`nng_http`] objects.
It is analogous to a [dialer] object used elsewhere in NNG, but it specifically is only for HTTP.

### Create a Client

```c
#include <nng/http.h>

void nng_http_client_alloc(nng_http_client *clientp, const nng_url *url);
```

The {{i:`nng_http_client_alloc`}} allocates an HTTP client suitable for
connecting to the server identified by _url_ and stores a pointer to
it in the location referenced by _clientp_.

### Destroy a Client

```c
#include <nng/http.h>

void nng_http_client_free(nng_http_client *client);
```

The {{i:`nng_http_client_free`}} connection destroys the client object and any
of its resources.

> [!NOTE]
> Any connections created by [`nng_http_client_connect`] are not affected by this function,
> and must be closed explicitly as needed.

### Client TLS

```c
int nng_http_client_get_tls(nng_http_client *client, nng_tls_config **tlsp);
int nng_http_client_set_tls(nng_http_client *client, nng_tls_config *tls);
```

The {{i:`nng_http_client_get_tls`}} and {{i:`nng_http_client_set_tls`}} functions are used to
retrieve or change the [TLS configuration][`nng_tls_config`] used when making outbound connections, enabling
{{i:TLS}} as a result.

If TLS has not been previously configured on _client_, then `nng_http_client_get_tls` will return [`NNG_EINVAL`].
Both functions will return [`NNG_ENOTSUP`] if either HTTP or TLS is not supported.

Calling `nng_http_client_set_tls` invalidates any client previously obtained with
`nng_http_client_get_tls`, unless a separate hold on the object was obtained.

Once TLS is enabled for an `nng_http_client`, it is not possible to disable TLS.

> [!NOTE]
> The TLS configuration itself cannnot be changed once it has been used to create a connection,
> such as by calling [`nng_http_client_connect`], but a new one can be installed in the client.
> Existing connections will use the TLS configuration that there were created with.

### Creating Connections

```c
#include <nng/http.h>

void nng_http_client_connect(nng_http_client *client, nng_aio *aio);
```

The {{i:`nng_http_client_connect`}} function makes an outgoing connection to the
server configured for _client_, and creates an [`nng_http`] object for the connection.

This is done asynchronously, and when the operation succeseds the connection may be
retried from the _aio_ using [`nng_aio_get_output`] with index 0.

#### Example 1: Connecting to Google

```c
nng_aio *aio;
nng_url *url;
nng_http_client *client;
nng_http_conn *conn;
int rv;

// Error checks elided for clarity.
nng_url_parse(&url, "http://www.google.com");
nng_aio_alloc(&aio, NULL, NULL);
nng_http_client_alloc(&client, url);

nng_http_client_connect(client, aio);

// Wait for connection to establish (or attempt to fail).
nng_aio_wait(aio);

if ((rv = nng_aio_result(aio)) != 0) {
    printf("Connection failed: %s\n", nng_strerror(rv));
} else {
    // Connection established, get it.
    conn = nng_aio_get_output(aio, 0);

    // ... do something with it here

    // Close the connection when done to avoid leaking it.
    nng_http_close(conn);
}
```

### Closing Connections

```c
void nng_http_close(nng_http *conn);
```

### Preparing a Transaction

```c
int nng_http_set_version(nng_http *conn, const char *version);
int nng_http_set_url(nng_http *conn, const nng_url *url);
```

The {{i:`nng_http_set_url`}} function provides a URL for the transaction. This will be used to
set the URI (aka path) for the request, as well as the "Host" header. The "Host" header may be overridden later.

### Request Headers

### Request Body

### Submitting the Transaction

```c
int nng_http_transact(nng_http *conn, nng_aio *aio);
```

The HTTP request is issued, and the response processed, asynchronously by the {{i:`nng_http_transact`}} function.
When the function is complete, the _aio_ will be notified.

### Getting the Results

```c
uint16_t nng_http_get_status(nng_http *conn);
const char *nng_http_get_reason(nng_http_conn *conn);
```

Once the transaction is complete, the HTTP status code for the transaction is
available by calling {{i:`nng_http_get_status`}}.

A descriptive message matching the status code is returned by {{i:`nng_http_get_reason`}}.

### Response Headers

### Response Body

## Server API

### Handlers

{{#include ../xref.md}}
