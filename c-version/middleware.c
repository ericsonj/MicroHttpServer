#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "middleware.h"

/* Route */
typedef struct _Route {
	char *uri;
	HTTPREQ_CALLBACK callback;
} Route;

Route routes[MAX_HTTP_ROUTES];
int routes_used = 0;

/* Add an URI and the corresponding callback into the route table. */
int AddRoute(char *uri, HTTPREQ_CALLBACK callback) {
	if(routes_used < MAX_HTTP_ROUTES) {
		routes[routes_used].uri = uri;
		routes[routes_used].callback = callback;
		routes_used++;

		return routes_used;
	}
	else {
		return 0;
	}
}

/* Try to read static files under static folder. */
uint8_t _ReadStaticFiles(HTTPReqMessage *req, HTTPResMessage *res) {
	uint8_t found = 0;
	int8_t depth = 0;
	char *uri = req->Header.URI;
	size_t n = strlen(uri);
	size_t i;

	FILE *fp;
	int size;
	char path[128];

	char header1[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n";
	char header2[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";

	/* Prevent Path Traversal. */
	for(i=0; i<n; i++) {
		if(uri[i] == '/') {
			if(((n-i) > 2) && (uri[i+1] == '.') && ((uri[i+2] == '.'))) {
				depth -= 1;
				if(depth < 0)
					break;
			}
			else if (((n-i) > 1) && (uri[i+1] == '.'))
				continue;
			else
				depth += 1;
		}
	}

	if(depth >= 0) {
		/* Try to open and load the static file. */
		sprintf(path, "%s%s", STATIC_FILE_FOLDER, uri);
		fp = fopen(path, "r");
		if(fp != NULL) {
			fseek(fp, 0, SEEK_END);
			size = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			if(size < MAX_BODY_SIZE) {
				/* Build HTTP OK header. */
				n = strlen(header1);
				memcpy(res->_buf, header1, n);
				i = n;
				n = strlen(header2);
				memcpy(res->_buf + i, header2, n);
				i += n;

				/* Build HTTP body. */
				n = fread(res->_buf + i, 1, size, fp);
				i += n;

				res->_index = i;

				found = 1;
			}
			fclose(fp);
		}
	}

	return found;
}

void _NotFound(HTTPReqMessage *req, HTTPResMessage *res) {
	uint8_t n;
	char header[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";

	/* Build HTTP OK header. */
	n = strlen(header);
	memcpy(res->_buf, header, n);
	res->_index = n;
}

/* Dispatch an URI according to the route table. */
void Dispatch(HTTPReqMessage *req, HTTPResMessage *res) {
	uint16_t i, j;
	size_t n;
	char *req_uri;
	uint8_t found = 0;

	/* Check the routes. */
	req_uri = req->Header.URI;
	for(i=0; i<routes_used; i++) {
		n = strlen(routes[i].uri);
		for(j=0; j<n; j++) {
			if((req_uri[j] != '\0') && (req_uri[j] == routes[i].uri[j])) {
				found = 1;
			}
			else {
				found = 0;
				break;
			}
		}

		if((found == 1) && (req_uri[j] == '\0')) {
			routes[i].callback(req, res);
			break;
		}
		else {
			found = 0;
		}
	}

	/* Check static files. */
	if(found != 1)
		found = _ReadStaticFiles(req, res);

	/* It is really not found. */
	if(found != 1)
		_NotFound(req, res);
}