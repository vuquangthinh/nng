# HTTP Support

NNG offers support for creation of HTTP clients, and servers. NNG supports HTTP/1.1 at present, and supports
a subset of functionality, but the support should be sufficient for simple clients, REST API servers, static content servers,
and gateways between HTTP and and other protocols. It also provides support for WebSocket based connections.

HTTP follows a request/reply model, where a client issues a request, and the server is expected to reply.
Every request is answered with a single reply.

## Client API

The NNG client API consists of an API for creating connections, and an API for performing
transactions on those connections.

### Client Object

### Connection Object

The {{i:`nng_http`}} object represents a single logical HTTP connection to the server.
For HTTP/1.1 and earlier, this will correspond to a single TCP connection, but the object
also contains state relating to the transaction, such as the hostname used, HTTP method used,
request headers, response status, response headers, and so forth.

An `nng_http` object can be reused, unless closed, so that additional transactions can be
performed after the first transaction is complete.

### Creating a Client

### Destroy a Client

### Creating HTTP Connections

### Preparing a Transaction

```c
int nng_http_set_version(nng_http *conn, const char *version);
void nng_http_set_method(nng_http *conn, const char *method);
int nng_http_set_url(nng_http *conn, const nng_url *url);
```

The {{i:`nng_http_set_version`}} function is used to select the HTTP protocol version to use for the
exchange. At present, only the values `NNG_HTTP_VERSION_1_0` and `NNG_HTTP_VERSION_1_1` (corresponding to
"HTTP/1.0" and "HTTP/1.1") are supported. NNG will default to using "HTTP/1.1" if this function is not called.
If an unsupported version is supplied, [`NNG_ENOTSUP`] will be returned, otherwise zero.

The {{i:`nng_http_set_method`}} function specifies the HTTP method to use for the transaction.
The default is "GET". HTTP methods are case sensitive, and generally upper-case, such as "GET", "POST", "HEAD",
and so forth. This function silently truncates any method longer than 32-characters to 32-characters. (There are
no defined methods that are so long.)

The {{i:`nng_http_set_url`}} function provides a URL for the transaction. This will be used to
set the URI (aka path) for the request, as well as the "Host" header. The "Host" header may be overridden later.

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

### Response Body

## Server API

### Handlers
