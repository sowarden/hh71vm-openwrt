/*
 * SPDX-License-Identifier: ISC
 *
 * Minimal HTTPS POST transport built against the uclient API shipped by the
 * pinned OpenWrt 19.07 tree. Secret request data is read from a mode-0600 file,
 * which is unlinked immediately after opening; neither the bot token nor the SMS
 * body appears in process arguments.
 */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glob.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libubox/uloop.h>
#include <libubox/ustream-ssl.h>
#include <libubox/uclient.h>

#define INPUT_LIMIT (16 * 1024)
#define RESPONSE_LIMIT (256 * 1024)

static struct ustream_ssl_ctx *ssl_ctx;
static const struct ustream_ssl_ops *ssl_ops;
static char response[RESPONSE_LIMIT + 1];
static size_t response_len;
static int failed;

static void finish(struct uclient *client)
{
	uclient_disconnect(client);
	uloop_end();
}

static void header_done(struct uclient *client)
{
	if (client->status_code < 100 || client->status_code > 599) {
		failed = 1;
		finish(client);
	}
}

static void data_read(struct uclient *client)
{
	char buffer[2048];
	int length;

	while ((length = uclient_read(client, buffer, sizeof(buffer))) > 0) {
		if (response_len + (size_t)length > RESPONSE_LIMIT) {
			failed = 1;
			finish(client);
			return;
		}
		memcpy(response + response_len, buffer, (size_t)length);
		response_len += (size_t)length;
	}
}

static void data_eof(struct uclient *client)
{
	if (!client->data_eof)
		failed = 1;
	finish(client);
}

static void request_error(struct uclient *client, int code)
{
	(void)code;
	failed = 1;
	finish(client);
}

static const struct uclient_cb callbacks = {
	.header_done = header_done,
	.data_read = data_read,
	.data_eof = data_eof,
	.error = request_error,
};

static int init_tls(void)
{
	glob_t certificates;
	void *library;
	size_t index;

	library = dlopen("libustream-ssl.so", RTLD_LAZY | RTLD_LOCAL);
	if (!library)
		return -1;
	ssl_ops = dlsym(library, "ustream_ssl_ops");
	if (!ssl_ops)
		return -1;
	ssl_ctx = ssl_ops->context_new(false);
	if (!ssl_ctx)
		return -1;
	if (glob("/etc/ssl/certs/*.crt", 0, NULL, &certificates) != 0)
		return -1;
	if (certificates.gl_pathc == 0) {
		globfree(&certificates);
		return -1;
	}
	for (index = 0; index < certificates.gl_pathc; index++)
		ssl_ops->context_add_ca_crt_file(ssl_ctx, certificates.gl_pathv[index]);
	globfree(&certificates);
	return 0;
}

static int valid_token(const char *token)
{
	const char *separator = strchr(token, ':');
	const unsigned char *cursor;
	size_t length = strlen(token);

	if (length < 30 || length > 128 || !separator || separator == token)
		return 0;
	for (cursor = (const unsigned char *)token; cursor < (const unsigned char *)separator; cursor++)
		if (*cursor < '0' || *cursor > '9')
			return 0;
	if (*(separator + 1) == '\0')
		return 0;
	for (cursor = (const unsigned char *)separator + 1; *cursor; cursor++)
		if (!((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= 'a' && *cursor <= 'z') ||
		      (*cursor >= '0' && *cursor <= '9') || *cursor == '_' || *cursor == '-'))
			return 0;
	return 1;
}

static char *read_secret_file(const char *path, size_t *size)
{
	struct stat metadata;
	char *buffer;
	ssize_t received;
	size_t offset = 0;
	int descriptor;

	descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (descriptor < 0)
		return NULL;
	if (unlink(path) != 0 || fstat(descriptor, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
	    metadata.st_uid != geteuid() || (metadata.st_mode & 077) != 0 ||
	    metadata.st_size < 1 || metadata.st_size > INPUT_LIMIT) {
		close(descriptor);
		return NULL;
	}
	buffer = calloc(1, (size_t)metadata.st_size + 1);
	if (!buffer) {
		close(descriptor);
		return NULL;
	}
	while (offset < (size_t)metadata.st_size) {
		received = read(descriptor, buffer + offset, (size_t)metadata.st_size - offset);
		if (received <= 0) {
			explicit_bzero(buffer, (size_t)metadata.st_size + 1);
			free(buffer);
			close(descriptor);
			return NULL;
		}
		offset += (size_t)received;
	}
	close(descriptor);
	*size = offset;
	return buffer;
}

int main(int argc, char **argv)
{
	char *input = NULL, *token, *method, *body, *first, *second, *url = NULL;
	struct uclient *client = NULL;
	size_t input_size = 0;
	int result = 1, status = 0;

	if (argc != 2)
		return 1;
	signal(SIGPIPE, SIG_IGN);
	input = read_secret_file(argv[1], &input_size);
	if (!input)
		goto cleanup;
	first = strchr(input, '\n');
	if (!first)
		goto cleanup;
	*first = '\0';
	second = strchr(first + 1, '\n');
	if (!second)
		goto cleanup;
	*second = '\0';
	token = input;
	method = first + 1;
	body = second + 1;
	if (!valid_token(token) || (strcmp(method, "sendMessage") != 0 && strcmp(method, "getUpdates") != 0) ||
	    body[0] != '{')
		goto cleanup;
	if (asprintf(&url, "https://api.telegram.org/bot%s/%s", token, method) < 0)
		goto cleanup;
	if (init_tls() != 0 || uloop_init() != 0)
		goto cleanup;
	client = uclient_new(url, NULL, &callbacks);
	if (!client)
		goto cleanup_uloop;
	uclient_set_timeout(client, 25000);
	uclient_http_set_ssl_ctx(client, ssl_ops, ssl_ctx, true);
	if (uclient_connect(client) != 0 || uclient_http_set_request_type(client, "POST") != 0 ||
	    uclient_http_reset_headers(client) != 0 ||
	    uclient_http_set_header(client, "User-Agent", "sms-to-telegram/1.0") != 0 ||
	    uclient_http_set_header(client, "Content-Type", "application/json") != 0 ||
	    uclient_write(client, body, strlen(body)) < 0 || uclient_request(client) != 0)
		goto cleanup_client;
	uloop_run();
	status = client->status_code;
	if (!failed) {
		response[response_len] = '\0';
		printf("%d\n", status);
		if (response_len)
			fwrite(response, 1, response_len, stdout);
		result = 0;
	}

cleanup_client:
	uclient_free(client);
cleanup_uloop:
	uloop_done();
cleanup:
	if (ssl_ctx)
		ssl_ops->context_free(ssl_ctx);
	if (url) {
		explicit_bzero(url, strlen(url));
		free(url);
	}
	if (input) {
		explicit_bzero(input, input_size + 1);
		free(input);
	}
	explicit_bzero(response, sizeof(response));
	return result;
}
