/*
 * SPDX-License-Identifier: ISC
 *
 * Minimal Telegram HTTPS POST transport. Request data and optional proxy
 * credentials are read from a mode-0600 file which is unlinked immediately
 * after opening, so none of them appear in process arguments.
 */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INPUT_LIMIT (16 * 1024)
#define RESPONSE_LIMIT (256 * 1024)
#define TELEGRAM_CA_BUNDLE "/etc/ssl/certs/ca-certificates.crt"

static char response[RESPONSE_LIMIT + 1];
static size_t response_len;
static int response_overflow;

static size_t response_write(char *data, size_t size, size_t count, void *unused)
{
	size_t length;

	(void)unused;
	if (size != 0 && count > (size_t)-1 / size)
		return 0;
	length = size * count;
	if (response_len + length > RESPONSE_LIMIT) {
		response_overflow = 1;
		return 0;
	}
	memcpy(response + response_len, data, length);
	response_len += length;
	return length;
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

static int valid_proxy_type(const char *value)
{
	return strcmp(value, "none") == 0 || strcmp(value, "http") == 0 ||
	       strcmp(value, "socks5") == 0;
}

static int valid_proxy_host(const char *value)
{
	const unsigned char *cursor = (const unsigned char *)value;
	size_t length = strlen(value);

	if (length < 1 || length > 253)
		return 0;
	for (; *cursor; cursor++)
		if (!((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= 'a' && *cursor <= 'z') ||
		      (*cursor >= '0' && *cursor <= '9') || *cursor == '.' || *cursor == '-' ||
		      *cursor == '_' || *cursor == ':' || *cursor == '%'))
			return 0;
	return 1;
}

static int valid_credential(const char *value)
{
	const unsigned char *cursor = (const unsigned char *)value;

	if (strlen(value) > 256)
		return 0;
	for (; *cursor; cursor++)
		if (*cursor < 32 || *cursor == 127)
			return 0;
	return 1;
}

static int parse_port(const char *value, long *port)
{
	char *end = NULL;
	long result;

	if (!value[0])
		return 0;
	result = strtol(value, &end, 10);
	if (!end || *end != '\0' || result < 1 || result > 65535)
		return 0;
	*port = result;
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

static char *take_line(char **cursor)
{
	char *line = *cursor;
	char *newline = strchr(line, '\n');

	if (!newline)
		return NULL;
	*newline = '\0';
	*cursor = newline + 1;
	return line;
}

static int set_common_options(CURL *curl, const char *url, const char *body,
			      struct curl_slist *headers)
{
	return curl_easy_setopt(curl, CURLOPT_URL, url) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_CAINFO, TELEGRAM_CA_BUNDLE) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_USERAGENT, "sms-to-telegram/1.2") == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_POST, 1L) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body)) == CURLE_OK &&
	       curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write) == CURLE_OK;
}

int main(int argc, char **argv)
{
	char *input = NULL, *cursor, *token, *method, *proxy_type, *proxy_host;
	char *proxy_port_text, *proxy_username, *proxy_password, *body, *url = NULL;
	struct curl_slist *headers = NULL;
	CURL *curl = NULL;
	CURLcode curl_result;
	size_t input_size = 0;
	long proxy_port = 0, status = 0;
	int result = 1;

	if (argc != 2)
		return 1;
	signal(SIGPIPE, SIG_IGN);
	input = read_secret_file(argv[1], &input_size);
	if (!input)
		goto cleanup;
	cursor = input;
	token = take_line(&cursor);
	method = take_line(&cursor);
	proxy_type = take_line(&cursor);
	proxy_host = take_line(&cursor);
	proxy_port_text = take_line(&cursor);
	proxy_username = take_line(&cursor);
	proxy_password = take_line(&cursor);
	body = cursor;
	if (!token || !method || !proxy_type || !proxy_host || !proxy_port_text ||
	    !proxy_username || !proxy_password || !valid_token(token) ||
	    (strcmp(method, "sendMessage") != 0 && strcmp(method, "getUpdates") != 0) ||
	    !valid_proxy_type(proxy_type) || !valid_credential(proxy_username) ||
	    !valid_credential(proxy_password) || body[0] != '{')
		goto cleanup;
	if (strcmp(proxy_type, "none") != 0 &&
	    (!valid_proxy_host(proxy_host) || !parse_port(proxy_port_text, &proxy_port)))
		goto cleanup;
	if (asprintf(&url, "https://api.telegram.org/bot%s/%s", token, method) < 0)
		goto cleanup;
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
		goto cleanup;
	curl = curl_easy_init();
	if (!curl)
		goto cleanup_curl;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!headers || !set_common_options(curl, url, body, headers))
		goto cleanup_easy;
	if (strcmp(proxy_type, "none") != 0) {
		long type = strcmp(proxy_type, "http") == 0 ? CURLPROXY_HTTP : CURLPROXY_SOCKS5_HOSTNAME;
		if (curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host) != CURLE_OK ||
		    curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy_port) != CURLE_OK ||
		    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, type) != CURLE_OK ||
		    (type == CURLPROXY_HTTP &&
		     curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L) != CURLE_OK) ||
		    (proxy_username[0] &&
		     curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxy_username) != CURLE_OK) ||
		    (proxy_password[0] &&
		     curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxy_password) != CURLE_OK))
			goto cleanup_easy;
	}
	curl_result = curl_easy_perform(curl);
	if (curl_result != CURLE_OK || response_overflow ||
	    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK)
		goto cleanup_easy;
	response[response_len] = '\0';
	printf("%ld\n", status);
	if (response_len)
		fwrite(response, 1, response_len, stdout);
	result = 0;

cleanup_easy:
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
cleanup_curl:
	curl_global_cleanup();
cleanup:
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
