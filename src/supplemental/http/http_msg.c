//
// Copyright 2025 Staysail Systems, Inc. <info@staysail.tech>
// Copyright 2018 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/nng_impl.h"
#include "http_api.h"
#include "http_msg.h"
#include "nng/http.h"

static int
http_set_string(char **strp, const char *val)
{
	char *news;
	if (val == NULL) {
		news = NULL;
	} else if ((news = nni_strdup(val)) == NULL) {
		return (NNG_ENOMEM);
	}
	nni_strfree(*strp);
	*strp = news;
	return (0);
}

static void
http_headers_reset(nni_list *hdrs)
{
	http_header *h;
	while ((h = nni_list_first(hdrs)) != NULL) {
		nni_list_remove(hdrs, h);
		nni_strfree(h->name);
		nni_strfree(h->value);
		NNI_FREE_STRUCT(h);
	}
}

static void
http_entity_reset(nni_http_entity *entity)
{
	if (entity->own && entity->size) {
		nni_free(entity->data, entity->size);
	}
	entity->data = NULL;
	entity->size = 0;
	entity->own  = false;
}

void
nni_http_req_reset(nni_http_req *req)
{
	http_headers_reset(&req->hdrs);
	http_entity_reset(&req->data);
	nni_strfree(req->uri);
	req->uri = NULL;
	nni_free(req->buf, req->bufsz);
	(void) snprintf(req->meth, sizeof(req->meth), "GET");
	req->bufsz  = 0;
	req->buf    = NULL;
	req->parsed = false;
}

void
nni_http_res_reset(nni_http_res *res)
{
	http_headers_reset(&res->hdrs);
	http_entity_reset(&res->data);
	nni_strfree(res->rsn);
	res->vers   = NNG_HTTP_VERSION_1_1;
	res->rsn    = NULL;
	res->code   = 0;
	res->parsed = false;
	nni_free(res->buf, res->bufsz);
	res->buf   = NULL;
	res->bufsz = 0;
}

static int
http_del_header(nni_list *hdrs, const char *key)
{
	http_header *h;
	NNI_LIST_FOREACH (hdrs, h) {
		if (nni_strcasecmp(key, h->name) == 0) {
			nni_list_remove(hdrs, h);
			nni_strfree(h->name);
			nni_free(h->value, strlen(h->value) + 1);
			NNI_FREE_STRUCT(h);
			return (0);
		}
	}
	return (NNG_ENOENT);
}

int
nni_http_req_del_header(nni_http_req *req, const char *key)
{
	return (http_del_header(&req->hdrs, key));
}

int
nni_http_res_del_header(nni_http_res *res, const char *key)
{
	return (http_del_header(&res->hdrs, key));
}

static int
http_set_header(nni_list *hdrs, const char *key, const char *val)
{
	http_header *h;
	NNI_LIST_FOREACH (hdrs, h) {
		if (nni_strcasecmp(key, h->name) == 0) {
			char *news;
			if ((news = nni_strdup(val)) == NULL) {
				return (NNG_ENOMEM);
			}
			nni_strfree(h->value);
			h->value = news;
			return (0);
		}
	}

	if ((h = NNI_ALLOC_STRUCT(h)) == NULL) {
		return (NNG_ENOMEM);
	}
	if ((h->name = nni_strdup(key)) == NULL) {
		NNI_FREE_STRUCT(h);
		return (NNG_ENOMEM);
	}
	if ((h->value = nni_strdup(val)) == NULL) {
		nni_strfree(h->name);
		NNI_FREE_STRUCT(h);
		return (NNG_ENOMEM);
	}
	nni_list_append(hdrs, h);
	return (0);
}

int
nni_http_req_set_header(nni_http_req *req, const char *key, const char *val)
{
	return (http_set_header(&req->hdrs, key, val));
}

int
nni_http_res_set_header(nni_http_res *res, const char *key, const char *val)
{
	return (http_set_header(&res->hdrs, key, val));
}

static int
http_add_header(nni_list *hdrs, const char *key, const char *val)
{
	http_header *h;
	NNI_LIST_FOREACH (hdrs, h) {
		if (nni_strcasecmp(key, h->name) == 0) {
			char *news;
			int   rv;
			rv = nni_asprintf(&news, "%s, %s", h->value, val);
			if (rv != 0) {
				return (rv);
			}
			nni_strfree(h->value);
			h->value = news;
			return (0);
		}
	}

	if ((h = NNI_ALLOC_STRUCT(h)) == NULL) {
		return (NNG_ENOMEM);
	}
	if ((h->name = nni_strdup(key)) == NULL) {
		NNI_FREE_STRUCT(h);
		return (NNG_ENOMEM);
	}
	if ((h->value = nni_strdup(val)) == NULL) {
		nni_strfree(h->name);
		NNI_FREE_STRUCT(h);
		return (NNG_ENOMEM);
	}
	nni_list_append(hdrs, h);
	return (0);
}

int
nni_http_req_add_header(nni_http_req *req, const char *key, const char *val)
{
	return (http_add_header(&req->hdrs, key, val));
}

int
nni_http_res_add_header(nni_http_res *res, const char *key, const char *val)
{
	return (http_add_header(&res->hdrs, key, val));
}

static const char *
http_get_header(const nni_list *hdrs, const char *key)
{
	http_header *h;
	NNI_LIST_FOREACH (hdrs, h) {
		if (nni_strcasecmp(h->name, key) == 0) {
			return (h->value);
		}
	}
	return (NULL);
}

const char *
nni_http_req_get_header(const nni_http_req *req, const char *key)
{
	return (http_get_header(&req->hdrs, key));
}

const char *
nni_http_res_get_header(const nni_http_res *res, const char *key)
{
	return (http_get_header(&res->hdrs, key));
}

// http_entity_set_data sets the entity, but does not update the
// content-length header.
static void
http_entity_set_data(nni_http_entity *entity, const void *data, size_t size)
{
	if (entity->own) {
		nni_free(entity->data, entity->size);
	}
	entity->data = (void *) data;
	entity->size = size;
	entity->own  = false;
}

static int
http_entity_alloc_data(nni_http_entity *entity, size_t size)
{
	void *newdata;
	if (size != 0) {
		if ((newdata = nni_zalloc(size)) == NULL) {
			return (NNG_ENOMEM);
		}
	}
	http_entity_set_data(entity, newdata, size);
	entity->own = true;
	return (0);
}

static int
http_entity_copy_data(nni_http_entity *entity, const void *data, size_t size)
{
	int rv;
	if ((rv = http_entity_alloc_data(entity, size)) == 0) {
		memcpy(entity->data, data, size);
	}
	return (rv);
}

static int
http_set_content_length(nni_http_entity *entity, nni_list *hdrs)
{
	char buf[16];
	(void) snprintf(buf, sizeof(buf), "%u", (unsigned) entity->size);
	return (http_set_header(hdrs, "Content-Length", buf));
}

static void
http_entity_get_data(nni_http_entity *entity, void **datap, size_t *sizep)
{
	*datap = entity->data;
	*sizep = entity->size;
}

void
nni_http_req_get_data(nni_http_req *req, void **datap, size_t *sizep)
{
	http_entity_get_data(&req->data, datap, sizep);
}

void
nni_http_res_get_data(nni_http_res *res, void **datap, size_t *sizep)
{
	http_entity_get_data(&res->data, datap, sizep);
}

int
nni_http_req_set_data(nni_http_req *req, const void *data, size_t size)
{
	int rv;

	http_entity_set_data(&req->data, data, size);
	if ((rv = http_set_content_length(&req->data, &req->hdrs)) != 0) {
		http_entity_set_data(&req->data, NULL, 0);
	}
	return (rv);
}

int
nni_http_res_set_data(nni_http_res *res, const void *data, size_t size)
{
	int rv;

	http_entity_set_data(&res->data, data, size);
	if ((rv = http_set_content_length(&res->data, &res->hdrs)) != 0) {
		http_entity_set_data(&res->data, NULL, 0);
	}
	res->iserr = false;
	return (rv);
}

int
nni_http_req_copy_data(nni_http_req *req, const void *data, size_t size)
{
	int rv;

	if (((rv = http_entity_copy_data(&req->data, data, size)) != 0) ||
	    ((rv = http_set_content_length(&req->data, &req->hdrs)) != 0)) {
		http_entity_set_data(&req->data, NULL, 0);
		return (rv);
	}
	return (0);
}

int
nni_http_req_alloc_data(nni_http_req *req, size_t size)
{
	int rv;

	if ((rv = http_entity_alloc_data(&req->data, size)) != 0) {
		return (rv);
	}
	return (0);
}

int
nni_http_res_copy_data(nni_http_res *res, const void *data, size_t size)
{
	int rv;

	if (((rv = http_entity_copy_data(&res->data, data, size)) != 0) ||
	    ((rv = http_set_content_length(&res->data, &res->hdrs)) != 0)) {
		http_entity_set_data(&res->data, NULL, 0);
		return (rv);
	}
	res->iserr = false;
	return (0);
}

// nni_http_res_alloc_data allocates the data region, but does not update any
// headers.  The intended use is for client implementations that want to
// allocate a buffer to receive the entity into.
int
nni_http_res_alloc_data(nni_http_res *res, size_t size)
{
	int rv;

	if ((rv = http_entity_alloc_data(&res->data, size)) != 0) {
		return (rv);
	}
	return (0);
}

bool
nni_http_res_is_error(nni_http_res *res)
{
	return (res->iserr);
}

static int
http_parse_header(nni_list *hdrs, void *line)
{
	char *key = line;
	char *val;
	char *end;

	// Find separation between key and value
	if ((val = strchr(key, ':')) == NULL) {
		return (NNG_EPROTO);
	}

	// Trim leading and trailing whitespace from header
	*val = '\0';
	val++;
	while (*val == ' ' || *val == '\t') {
		val++;
	}
	end = val + strlen(val);
	end--;
	while ((end > val) && (*end == ' ' || *end == '\t')) {
		*end = '\0';
		end--;
	}

	return (http_add_header(hdrs, key, val));
}

// http_sprintf_headers makes headers for an HTTP request or an HTTP response
// object.  Each header is dumped from the list. If the buf is NULL,
// or the sz is 0, then a dryrun is done, in order to allow the caller to
// determine how much space is needed. Returns the size of the space needed,
// not including the terminating NULL byte.  Truncation occurs if the size
// returned is >= the requested size.
static size_t
http_sprintf_headers(char *buf, size_t sz, nni_list *list)
{
	size_t       rv = 0;
	http_header *h;

	if (buf == NULL) {
		sz = 0;
	}

	NNI_LIST_FOREACH (list, h) {
		size_t l;
		l = snprintf(buf, sz, "%s: %s\r\n", h->name, h->value);
		if (buf != NULL) {
			buf += l;
		}
		sz = (sz > l) ? sz - l : 0;
		rv += l;
	}
	return (rv);
}

static int
http_asprintf(char **bufp, size_t *szp, nni_list *hdrs, const char *fmt, ...)
{
	va_list ap;
	size_t  len;
	size_t  n;
	char   *buf;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	len += http_sprintf_headers(NULL, 0, hdrs);
	len += 3; // \r\n\0

	if (len <= *szp) {
		buf = *bufp;
	} else {
		if ((buf = nni_alloc(len)) == NULL) {
			return (NNG_ENOMEM);
		}
		nni_free(*bufp, *szp);
		*bufp = buf;
		*szp  = len;
	}
	va_start(ap, fmt);
	n = vsnprintf(buf, len, fmt, ap);
	va_end(ap);
	buf += n;
	len -= n;
	n = http_sprintf_headers(buf, len, hdrs);
	buf += n;
	len -= n;
	snprintf(buf, len, "\r\n");
	NNI_ASSERT(len == 3);
	return (0);
}

static int
http_req_prepare(nni_http_req *req)
{
	int rv;
	if (req->uri == NULL) {
		return (NNG_EINVAL);
	}
	rv = http_asprintf(&req->buf, &req->bufsz, &req->hdrs, "%s %s %s\r\n",
	    req->meth, req->uri, req->vers);
	return (rv);
}

static int
http_res_prepare(nng_http *conn)
{
	int           rv;
	nng_http_res *res = nni_http_conn_res(conn);

	if (res->code == 0) {
		res->code = NNG_HTTP_STATUS_OK;
	}
	rv = http_asprintf(&res->buf, &res->bufsz, &res->hdrs, "%s %d %s\r\n",
	    res->vers, res->code, nni_http_conn_get_reason(conn));
	return (rv);
}

char *
nni_http_req_headers(nni_http_req *req)
{
	char  *s;
	size_t len;

	len = http_sprintf_headers(NULL, 0, &req->hdrs) + 1;
	if ((s = nni_alloc(len)) != NULL) {
		http_sprintf_headers(s, len, &req->hdrs);
	}
	return (s);
}

char *
nni_http_res_headers(nni_http_res *res)
{
	char  *s;
	size_t len;

	len = http_sprintf_headers(NULL, 0, &res->hdrs) + 1;
	if ((s = nni_alloc(len)) != NULL) {
		http_sprintf_headers(s, len, &res->hdrs);
	}
	return (s);
}

int
nni_http_req_get_buf(nni_http_req *req, void **data, size_t *szp)
{
	int rv;

	if ((req->buf == NULL) && (rv = http_req_prepare(req)) != 0) {
		return (rv);
	}
	*data = req->buf;
	*szp  = req->bufsz - 1; // exclude terminating NUL
	return (0);
}

int
nni_http_res_get_buf(nni_http_conn *conn, void **data, size_t *szp)
{
	int           rv;
	nni_http_res *res = nni_http_conn_res(conn);

	if ((res->buf == NULL) && (rv = http_res_prepare(conn)) != 0) {
		return (rv);
	}
	*data = res->buf;
	*szp  = res->bufsz - 1; // exclude terminating NUL
	return (0);
}

void
nni_http_req_init(nni_http_req *req)
{
	NNI_LIST_INIT(&req->hdrs, http_header, node);
	req->buf       = NULL;
	req->bufsz     = 0;
	req->data.data = NULL;
	req->data.size = 0;
	req->data.own  = false;
	req->uri       = NULL;
	(void) snprintf(req->meth, sizeof(req->meth), "GET");
}

int
nni_http_req_set_url(nni_http_req *req, const nng_url *url)
{
	if (url == NULL) {
		return (0);
	}
	const char *host;
	char        host_buf[264]; // 256 + 8 for port
	int         rv;
	rv = nni_asprintf(&req->uri, "%s%s%s%s%s", url->u_path,
	    url->u_query ? "?" : "", url->u_query ? url->u_query : "",
	    url->u_fragment ? "#" : "",
	    url->u_fragment ? url->u_fragment : "");
	if (rv != 0) {
		return (NNG_ENOMEM);
	}

	// Add a Host: header since we know that from the URL. Also,
	// only include the :port portion if it isn't the default port.
	if (nni_url_default_port(url->u_scheme) == url->u_port) {
		host = url->u_hostname;
	} else {
		if (strchr(url->u_hostname, ':')) {
			snprintf(host_buf, sizeof(host_buf), "[%s]:%u",
			    url->u_hostname, url->u_port);
		} else {
			snprintf(host_buf, sizeof(host_buf), "%s:%u",
			    url->u_hostname, url->u_port);
		}
		host = host_buf;
	}
	if ((rv = nni_http_req_set_header(req, "Host", host)) != 0) {
		return (rv);
	}
	return (0);
}

void
nni_http_res_init(nni_http_res *res)
{
	NNI_LIST_INIT(&res->hdrs, http_header, node);
	res->buf       = NULL;
	res->bufsz     = 0;
	res->data.data = NULL;
	res->data.size = 0;
	res->data.own  = false;
	res->vers      = NNG_HTTP_VERSION_1_1;
	res->rsn       = NULL;
	res->code      = 0;
}

const char *
nni_http_req_get_uri(const nni_http_req *req)
{
	return (req->uri != NULL ? req->uri : "");
}

int
nni_http_req_set_uri(nni_http_req *req, const char *uri)
{
	return (http_set_string(&req->uri, uri));
}

void
nni_http_res_set_status(nni_http_res *res, uint16_t status)
{
	res->code = status;
}

uint16_t
nni_http_res_get_status(const nni_http_res *res)
{
	return (res->code);
}

static int
http_scan_line(void *vbuf, size_t n, size_t *lenp)
{
	size_t len;
	char   lc;
	char  *buf = vbuf;

	lc = 0;
	for (len = 0; len < n; len++) {
		char c = buf[len];
		if (c == '\n') {
			// Technically we should be receiving CRLF, but
			// debugging is easier with just LF, so we behave
			// following Postel's Law.
			if (lc != '\r') {
				buf[len] = '\0';
			} else {
				buf[len - 1] = '\0';
			}
			*lenp = len + 1;
			return (0);
		}
		// If we have a control character (other than CR), or a CR
		// followed by anything other than LF, then its an error.
		if (((c < ' ') && (c != '\r')) || (lc == '\r')) {
			return (NNG_EPROTO);
		}
		lc = c;
	}
	// Scanned the entire content, but did not find a line.
	return (NNG_EAGAIN);
}

static int
http_req_parse_line(nng_http *conn, nni_http_req *req, void *line)
{
	int   rv;
	char *method;
	char *uri;
	char *version;

	method = line;
	if ((uri = strchr(method, ' ')) == NULL) {
		return (NNG_EPROTO);
	}
	*uri = '\0';
	uri++;

	if ((version = strchr(uri, ' ')) == NULL) {
		return (NNG_EPROTO);
	}
	*version = '\0';
	version++;

	nni_http_conn_set_method(conn, method);
	if (((rv = nni_http_req_set_uri(req, uri)) != 0) ||
	    ((rv = nni_http_conn_set_version(conn, version)) != 0)) {
		return (rv);
	}
	req->parsed = true;
	return (0);
}

static int
http_res_parse_line(nng_http *conn, uint8_t *line)
{
	int           rv;
	char         *reason;
	char         *codestr;
	char         *version;
	int           status;
	nng_http_res *res = nni_http_conn_res(conn);

	version = (char *) line;
	if ((codestr = strchr(version, ' ')) == NULL) {
		return (NNG_EPROTO);
	}
	*codestr = '\0';
	codestr++;

	if ((reason = strchr(codestr, ' ')) == NULL) {
		return (NNG_EPROTO);
	}
	*reason = '\0';
	reason++;

	status = atoi(codestr);
	if ((status < 100) || (status > 999)) {
		return (NNG_EPROTO);
	}

	nni_http_conn_set_status(conn, (uint16_t) status);

	if (((rv = nni_http_conn_set_version(conn, version)) != 0) ||
	    ((rv = nni_http_conn_set_reason(conn, reason)) != 0)) {
		return (rv);
	}
	res->parsed = true;
	return (0);
}

// nni_http_req_parse parses a request (but not any attached entity data).
// The amount of data consumed is returned in lenp.  Returns zero on
// success, NNG_EPROTO on parse failure, NNG_EAGAIN if more data is
// required, or NNG_ENOMEM on memory exhaustion.  Note that lenp may
// be updated even in the face of errors (esp. NNG_EAGAIN, which is
// not an error so much as a request for more data.)
int
nni_http_req_parse(nng_http *conn, void *buf, size_t n, size_t *lenp)
{

	size_t        len = 0;
	size_t        cnt;
	int           rv  = 0;
	nni_http_req *req = nni_http_conn_req(conn);

	for (;;) {
		uint8_t *line;
		if ((rv = http_scan_line(buf, n, &cnt)) != 0) {
			break;
		}

		len += cnt;
		line = buf;
		buf  = line + cnt;
		n -= cnt;

		if (*line == '\0') {
			break;
		}

		if (req->parsed) {
			rv = http_parse_header(&req->hdrs, line);
		} else {
			rv = http_req_parse_line(conn, req, line);
		}

		if (rv != 0) {
			break;
		}
	}

	*lenp = len;
	return (rv);
}

int
nni_http_res_parse(nng_http *conn, void *buf, size_t n, size_t *lenp)
{

	size_t        len = 0;
	size_t        cnt;
	int           rv  = 0;
	nng_http_res *res = nni_http_conn_res(conn);
	for (;;) {
		uint8_t *line;
		if ((rv = http_scan_line(buf, n, &cnt)) != 0) {
			break;
		}

		len += cnt;
		line = buf;
		buf  = line + cnt;
		n -= cnt;

		if (*line == '\0') {
			break;
		}

		if (res->parsed) {
			rv = http_parse_header(&res->hdrs, line);
		} else {
			rv = http_res_parse_line(conn, line);
		}

		if (rv != 0) {
			break;
		}
	}

	*lenp = len;
	return (rv);
}
