# HTTP Support

NNG offers support for creation of HTTP clients, and servers. NNG supports HTTP/1.1 at present, and supports
a subset of functionality, but the support should be sufficient for simple clients, REST API servers, static content servers,
and gateways between HTTP and and other protocols. It also provides support for WebSocket based connections.

HTTP follows a request/reply model, where a client issues a request, and the server is expected to reply.
Every request is answered with a single reply.

## Connection Object

The {{i:`nng_http`}} object represents a single logical HTTP connection to the server.
For HTTP/1.1 and earlier, this will correspond to a single TCP connection, but the object
also contains state relating to the transaction, such as the hostname used, HTTP method used,
request headers, response status, response headers, and so forth.

An `nng_http` object can be reused, unless closed, so that additional transactions can be
performed after the first transaction is complete.

At any given point in time, an `nng_http` object can only refer to a single HTTP transaction.
In NNG, these `nng_http` objects are used in both the client and server APIs.

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

## Client API

The NNG client API consists of an API for creating connections, and an API for performing
transactions on those connections.

### Client Object

### Creating a Client

### Destroy a Client

```c
#include <nng/http.h>

void nng_http_client_free(nng_http_client *client);
```

The {{i:`nng_http_client_free`}} connection destroys the client object and any
of its resources.

> [!NOTE]
> Any connections created by [`nng_http_client_client`] are not affected by this function,
> and must be closed explicitly as needed.

### Creating HTTP Connections

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
