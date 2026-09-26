#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "altscreen.h"
#include "framework/logging.h"

#define ALTSCREEN_SESSION_MODULE "AltScreenSession"
#define ALTSCREEN_MAX_STATES 4
#define ALTSCREEN_TIMEOUT_SECS 10
#define K_CFSTRING_UTF8 0x08000100U

typedef void *cf_ref;
typedef long cf_index;
typedef unsigned long cf_type_id;
typedef int os_status;

typedef os_status (*session_setup_fn)(void *, cf_ref, cf_ref *);
typedef void (*session_teardown_fn)(void *, cf_ref, os_status, unsigned char *);
typedef os_status (*screen_create_fn)(void **);
typedef void (*screen_delete_fn)(void *);
typedef os_status (*screen_setup_fn)(void *, cf_ref, uint32_t);
typedef os_status (*screen_security_fn)(void *, const uint8_t[16], const uint8_t[16]);
typedef os_status (*screen_process_fn)(void *, void *, int);
typedef void (*screen_stop_fn)(void *);
typedef os_status (*screen_send_cmd_fn)(void *, char, const void *, size_t);
typedef os_status (*netsock_create_native_fn)(void **, int);
typedef os_status (*netsock_delete_fn)(void *);
typedef os_status (*screen_process_data_fn)(void *, const uint8_t *, size_t, uint64_t,
                                            cf_ref, void (*)(void *), void *);
typedef os_status (*send_command_fn)(void *, cf_ref, void (*)(os_status, cf_ref, void *), void *);

typedef cf_ref (*cf_string_create_fn)(cf_ref, const char *, uint32_t);
typedef cf_ref (*cf_dict_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_array_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_dict_create_mutable_fn)(cf_ref, cf_index, const void *, const void *);
typedef cf_ref (*cf_array_create_mutable_fn)(cf_ref, cf_index, const void *);
typedef const void *(*cf_dict_get_value_fn)(cf_ref, const void *);
typedef void (*cf_dict_set_value_fn)(cf_ref, const void *, const void *);
typedef int64_t (*cf_dict_get_i64_fn)(cf_ref, const void *, os_status *);
typedef os_status (*cf_dict_set_i64_fn)(cf_ref, const void *, int64_t);
typedef cf_index (*cf_array_count_fn)(cf_ref);
typedef const void *(*cf_array_value_fn)(cf_ref, cf_index);
typedef void (*cf_array_append_fn)(cf_ref, const void *);
typedef unsigned char (*cf_equal_fn)(cf_ref, cf_ref);
typedef cf_type_id (*cf_get_type_fn)(cf_ref);
typedef cf_type_id (*cf_type_getter_fn)(void);
typedef void (*cf_release_fn)(cf_ref);

typedef struct {
    session_setup_fn session_setup;
    session_teardown_fn session_teardown;
    screen_create_fn screen_create;
    screen_delete_fn screen_delete;
    screen_setup_fn screen_setup;
    screen_security_fn screen_security;
    screen_process_fn screen_process;
    screen_stop_fn screen_stop;
    screen_send_cmd_fn screen_send_cmd;
    netsock_create_native_fn netsock_create_native;
    netsock_delete_fn netsock_delete;
    screen_process_data_fn screen_process_data;
    send_command_fn send_command;

    cf_string_create_fn string_create;
    cf_dict_mutable_copy_fn dict_mutable_copy;
    cf_array_mutable_copy_fn array_mutable_copy;
    cf_dict_create_mutable_fn dict_create_mutable;
    cf_array_create_mutable_fn array_create_mutable;
    cf_dict_get_value_fn dict_get_value;
    cf_dict_set_value_fn dict_set_value;
    cf_dict_get_i64_fn dict_get_i64;
    cf_dict_set_i64_fn dict_set_i64;
    cf_array_count_fn array_count;
    cf_array_value_fn array_value;
    cf_array_append_fn array_append;
    cf_equal_fn equal;
    cf_get_type_fn get_type;
    cf_type_getter_fn array_type;
    cf_type_getter_fn dict_type;
    cf_release_fn release;
    const void *dict_key_callbacks;
    const void *dict_value_callbacks;
    const void *array_callbacks;
    int resolved;
    int ready;
} altscreen_api;

typedef struct {
    void *receiver;
    void *screen;
    uint64_t connection_id;
    int listen_fd;
    int accepted_fd;
    uint16_t data_port;
    pthread_t worker;
    int worker_started;
    int screen_started;
    int show_sent;
    int active;
} altscreen_state;

static altscreen_api g_api;
static altscreen_state g_states[ALTSCREEN_MAX_STATES];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void *sym(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

static void resolve_api(void)
{
    if (g_api.resolved)
        return;

    g_api.resolved = 1;

    g_api.session_setup = (session_setup_fn)sym("AirPlayReceiverSessionSetup");
    g_api.session_teardown = (session_teardown_fn)sym("AirPlayReceiverSessionTearDown");
    g_api.screen_create = (screen_create_fn)sym("AirPlayReceiverSessionScreen_Create");
    g_api.screen_delete = (screen_delete_fn)sym("AirPlayReceiverSessionScreen_Delete");
    g_api.screen_setup = (screen_setup_fn)sym("AirPlayReceiverSessionScreen_Setup");
    g_api.screen_security = (screen_security_fn)sym("AirPlayReceiverSessionScreen_SetSecurityInfo");
    g_api.screen_process = (screen_process_fn)sym("AirPlayReceiverSessionScreen_ProcessFrames");
    g_api.screen_stop = (screen_stop_fn)sym("AirPlayReceiverSessionScreen_StopSession");
    g_api.screen_send_cmd = (screen_send_cmd_fn)sym("AirPlayReceiverSessionScreen_SendCommand");
    g_api.netsock_create_native = (netsock_create_native_fn)sym("NetSocket_CreateWithNative");
    g_api.netsock_delete = (netsock_delete_fn)sym("NetSocket_Delete");
    g_api.screen_process_data = (screen_process_data_fn)sym("ScreenStreamProcessData");
    g_api.send_command = (send_command_fn)sym("AirPlayReceiverSessionSendCommand");

    g_api.string_create = (cf_string_create_fn)sym("CFStringCreateWithCString");
    g_api.dict_mutable_copy = (cf_dict_mutable_copy_fn)sym("CFDictionaryCreateMutableCopy");
    g_api.array_mutable_copy = (cf_array_mutable_copy_fn)sym("CFArrayCreateMutableCopy");
    g_api.dict_create_mutable = (cf_dict_create_mutable_fn)sym("CFDictionaryCreateMutable");
    g_api.array_create_mutable = (cf_array_create_mutable_fn)sym("CFArrayCreateMutable");
    g_api.dict_get_value = (cf_dict_get_value_fn)sym("CFDictionaryGetValue");
    g_api.dict_set_value = (cf_dict_set_value_fn)sym("CFDictionarySetValue");
    g_api.dict_get_i64 = (cf_dict_get_i64_fn)sym("CFDictionaryGetInt64");
    g_api.dict_set_i64 = (cf_dict_set_i64_fn)sym("CFDictionarySetInt64");
    g_api.array_count = (cf_array_count_fn)sym("CFArrayGetCount");
    g_api.array_value = (cf_array_value_fn)sym("CFArrayGetValueAtIndex");
    g_api.array_append = (cf_array_append_fn)sym("CFArrayAppendValue");
    g_api.equal = (cf_equal_fn)sym("CFEqual");
    g_api.get_type = (cf_get_type_fn)sym("CFGetTypeID");
    g_api.array_type = (cf_type_getter_fn)sym("CFArrayGetTypeID");
    g_api.dict_type = (cf_type_getter_fn)sym("CFDictionaryGetTypeID");
    g_api.release = (cf_release_fn)sym("CFRelease");
    g_api.dict_key_callbacks = sym("kCFTypeDictionaryKeyCallBacks");
    g_api.dict_value_callbacks = sym("kCFTypeDictionaryValueCallBacks");
    g_api.array_callbacks = sym("kCFTypeArrayCallBacks");

    g_api.ready =
        g_api.session_setup != NULL &&
        g_api.session_teardown != NULL &&
        g_api.screen_create != NULL &&
        g_api.screen_delete != NULL &&
        g_api.screen_setup != NULL &&
        g_api.screen_security != NULL &&
        g_api.screen_process != NULL &&
        g_api.screen_stop != NULL &&
        g_api.screen_send_cmd != NULL &&
        g_api.netsock_create_native != NULL &&
        g_api.netsock_delete != NULL &&
        g_api.screen_process_data != NULL &&
        g_api.send_command != NULL &&
        g_api.string_create != NULL &&
        g_api.dict_mutable_copy != NULL &&
        g_api.array_mutable_copy != NULL &&
        g_api.dict_create_mutable != NULL &&
        g_api.array_create_mutable != NULL &&
        g_api.dict_get_value != NULL &&
        g_api.dict_set_value != NULL &&
        g_api.dict_get_i64 != NULL &&
        g_api.dict_set_i64 != NULL &&
        g_api.array_count != NULL &&
        g_api.array_value != NULL &&
        g_api.array_append != NULL &&
        g_api.equal != NULL &&
        g_api.get_type != NULL &&
        g_api.array_type != NULL &&
        g_api.dict_type != NULL &&
        g_api.release != NULL &&
        g_api.dict_key_callbacks != NULL &&
        g_api.dict_value_callbacks != NULL &&
        g_api.array_callbacks != NULL;
}

static cf_ref cfs(const char *s)
{
    if (s == NULL || g_api.string_create == NULL)
        return NULL;
    return g_api.string_create(NULL, s, K_CFSTRING_UTF8);
}

static int is_array(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.array_type();
}

static int is_dict(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.dict_type();
}

static cf_ref dict_get(cf_ref dict, const char *name)
{
    cf_ref key;
    cf_ref value = NULL;

    if (dict == NULL || name == NULL)
        return NULL;
    key = cfs(name);
    if (key == NULL)
        return NULL;
    value = (cf_ref)g_api.dict_get_value(dict, key);
    g_api.release(key);
    return value;
}

static int64_t dict_get_i64(cf_ref dict, const char *name, int *ok)
{
    cf_ref key;
    os_status err = -1;
    int64_t value = 0;

    if (ok != NULL)
        *ok = 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    value = g_api.dict_get_i64(dict, key, &err);
    g_api.release(key);
    if (ok != NULL)
        *ok = (err == 0);
    return value;
}

static int dict_set_obj(cf_ref dict, const char *name, cf_ref value)
{
    cf_ref key;

    if (dict == NULL || value == NULL)
        return 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    g_api.dict_set_value(dict, key, value);
    g_api.release(key);
    return 1;
}

static int dict_set_i64(cf_ref dict, const char *name, int64_t value)
{
    cf_ref key;
    os_status rc;

    key = cfs(name);
    if (key == NULL)
        return 0;
    rc = g_api.dict_set_i64(dict, key, value);
    g_api.release(key);
    return rc == 0;
}

static int dict_set_string(cf_ref dict, const char *name, const char *value)
{
    cf_ref s;
    int ok;

    s = cfs(value);
    if (s == NULL)
        return 0;
    ok = dict_set_obj(dict, name, s);
    g_api.release(s);
    return ok;
}

static cf_ref new_dict(void)
{
    return g_api.dict_create_mutable(NULL, 0,
                                     g_api.dict_key_callbacks,
                                     g_api.dict_value_callbacks);
}

static cf_ref new_array(void)
{
    return g_api.array_create_mutable(NULL, 0, g_api.array_callbacks);
}

static int array_contains_string(cf_ref array, const char *value)
{
    cf_ref needle;
    cf_index i, n;
    int found = 0;

    if (!is_array(array))
        return 0;
    needle = cfs(value);
    if (needle == NULL)
        return 0;
    n = g_api.array_count(array);
    for (i = 0; i < n; ++i) {
        cf_ref item = (cf_ref)g_api.array_value(array, i);
        if (item != NULL && g_api.equal(item, needle)) {
            found = 1;
            break;
        }
    }
    g_api.release(needle);
    return found;
}

static int append_string(cf_ref array, const char *value)
{
    cf_ref s = cfs(value);

    if (s == NULL)
        return 0;
    g_api.array_append(array, s);
    g_api.release(s);
    return 1;
}

static cf_ref build_stock_only_request(cf_ref request, cf_ref *out_alt_desc)
{
    cf_ref streams;
    cf_ref stock_request = NULL;
    cf_ref stock_streams = NULL;
    cf_index i, n;
    int alt_count = 0;

    if (out_alt_desc != NULL)
        *out_alt_desc = NULL;
    if (!is_dict(request))
        return NULL;

    streams = dict_get(request, "streams");
    if (!is_array(streams))
        return NULL;

    n = g_api.array_count(streams);
    stock_request = g_api.dict_mutable_copy(NULL, 0, request);
    stock_streams = new_array();
    if (stock_request == NULL || stock_streams == NULL)
        goto fail;

    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type;

        if (!is_dict(desc)) {
            g_api.array_append(stock_streams, desc);
            continue;
        }
        type = dict_get_i64(desc, "type", &ok);
        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN) {
            ++alt_count;
            if (out_alt_desc != NULL)
                *out_alt_desc = desc;
            continue;
        }
        g_api.array_append(stock_streams, desc);
    }

    if (alt_count == 0) {
        g_api.release(stock_streams);
        g_api.release(stock_request);
        return NULL;
    }
    if (alt_count != 1) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "request split refused alt_count=%d expected=1 primary_untouched=1",
                  alt_count);
        goto fail;
    }

    if (!dict_set_obj(stock_request, "streams", stock_streams))
        goto fail;

    g_api.release(stock_streams);
    return stock_request;

fail:
    if (stock_streams != NULL) g_api.release(stock_streams);
    if (stock_request != NULL) g_api.release(stock_request);
    if (out_alt_desc != NULL) *out_alt_desc = NULL;
    return NULL;
}

static cf_ref merge_altscreen_response(cf_ref stock_response, uint16_t port)
{
    cf_ref response = NULL;
    cf_ref streams = NULL;
    cf_ref out_streams = NULL;
    cf_ref alt_stream = NULL;
    cf_ref enabled = NULL;
    cf_ref out_enabled = NULL;

    if (stock_response != NULL && is_dict(stock_response))
        response = g_api.dict_mutable_copy(NULL, 0, stock_response);
    else
        response = new_dict();
    if (response == NULL)
        goto fail;

    streams = dict_get(response, "streams");
    if (is_array(streams))
        out_streams = g_api.array_mutable_copy(NULL, g_api.array_count(streams) + 1, streams);
    else
        out_streams = new_array();
    alt_stream = new_dict();
    if (out_streams == NULL || alt_stream == NULL)
        goto fail;

    if (!dict_set_i64(alt_stream, "type", AIRPLAY_STREAM_TYPE_ALT_SCREEN) ||
        !dict_set_i64(alt_stream, "dataPort", (int64_t)port))
        goto fail;

    g_api.array_append(out_streams, alt_stream);
    if (!dict_set_obj(response, "streams", out_streams))
        goto fail;

    enabled = dict_get(response, "enabledFeatures");
    if (is_array(enabled))
        out_enabled = g_api.array_mutable_copy(NULL, g_api.array_count(enabled) + 2, enabled);
    else
        out_enabled = new_array();
    if (out_enabled == NULL)
        goto fail;

    if (!array_contains_string(out_enabled, "altScreen") &&
        !append_string(out_enabled, "altScreen"))
        goto fail;
    if (!array_contains_string(out_enabled, "viewAreas") &&
        !append_string(out_enabled, "viewAreas"))
        goto fail;
    if (!dict_set_obj(response, "enabledFeatures", out_enabled))
        goto fail;

    g_api.release(out_enabled);
    g_api.release(alt_stream);
    g_api.release(out_streams);
    return response;

fail:
    if (out_enabled != NULL) g_api.release(out_enabled);
    if (alt_stream != NULL) g_api.release(alt_stream);
    if (out_streams != NULL) g_api.release(out_streams);
    if (response != NULL) g_api.release(response);
    return NULL;
}

static int open_listener(uint16_t *out_port)
{
    int fd = -1;
    int one = 1;
    struct sockaddr_in addr;
    socklen_t len;

    if (out_port == NULL)
        return -1;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;

    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        goto fail;
    if (listen(fd, 1) != 0)
        goto fail;

    len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
        goto fail;

    *out_port = ntohs(addr.sin_port);
    if (*out_port == 0)
        goto fail;
    return fd;

fail:
    close(fd);
    return -1;
}

static altscreen_state *find_state_receiver_locked(void *receiver)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].receiver == receiver)
            return &g_states[i];
    }
    return NULL;
}

static altscreen_state *find_state_worker_locked(pthread_t thread)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].worker_started &&
            pthread_equal(g_states[i].worker, thread))
            return &g_states[i];
    }
    return NULL;
}

static altscreen_state *alloc_state_locked(void *receiver)
{
    int i;

    if (find_state_receiver_locked(receiver) != NULL)
        return NULL;
    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (!g_states[i].active) {
            memset(&g_states[i], 0, sizeof(g_states[i]));
            g_states[i].receiver = receiver;
            g_states[i].listen_fd = -1;
            g_states[i].accepted_fd = -1;
            g_states[i].active = 1;
            return &g_states[i];
        }
    }
    return NULL;
}

static cf_ref make_event_request(const char *type, int include_url)
{
    cf_ref request = NULL;
    cf_ref params = NULL;

    request = new_dict();
    params = new_dict();
    if (request == NULL || params == NULL)
        goto fail;

    if (!dict_set_string(request, "type", type) ||
        !dict_set_string(params, "uuid", ALTSCREEN_UUID))
        goto fail;
    if (include_url && !dict_set_string(params, "url", ALTSCREEN_INITIAL_URL))
        goto fail;
    if (!dict_set_obj(request, "params", params))
        goto fail;

    g_api.release(params);
    return request;

fail:
    if (params != NULL) g_api.release(params);
    if (request != NULL) g_api.release(request);
    return NULL;
}

static os_status send_event(void *receiver, const char *type, int include_url)
{
    cf_ref request;
    os_status rc = -1;

    if (receiver == NULL || g_api.send_command == NULL)
        return -1;
    request = make_event_request(type, include_url);
    if (request == NULL)
        return -1;

    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "event type=%s rc=%d uuid=%s url=%s",
             type, rc, ALTSCREEN_UUID, include_url ? ALTSCREEN_INITIAL_URL : "-");
    return rc;
}

static void request_cluster(altscreen_state *state)
{
    os_status rc;

    if (state == NULL)
        return;
    rc = send_event(state->receiver, "showUI", 1);
    if (rc == 0)
        (void)send_event(state->receiver, "forceKeyFrame", 0);
}

static void *altscreen_worker(void *arg)
{
    altscreen_state *state = (altscreen_state *)arg;
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int fd;
    void *net = NULL;
    void *delegate_context;
    os_status rc;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "worker waiting receiver=%p port=%u",
             state->receiver, (unsigned int)state->data_port);

    do {
        fd = accept(state->listen_fd, (struct sockaddr *)&peer, &peer_len);
    } while (fd < 0 && errno == EINTR);

    pthread_mutex_lock(&g_lock);
    if (state->listen_fd >= 0) {
        close(state->listen_fd);
        state->listen_fd = -1;
    }
    state->accepted_fd = fd;
    pthread_mutex_unlock(&g_lock);

    if (fd < 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "accept failed receiver=%p errno=%d",
                  state->receiver, errno);
        return NULL;
    }

    delegate_context = altscreen_main_delegate_context();
    rc = altscreen_start_session(state->screen, delegate_context);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "session start failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);
    state->screen_started = 1;
    pthread_mutex_unlock(&g_lock);

    rc = g_api.netsock_create_native(&net, fd);
    if (rc != 0 || net == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "NetSocket create failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        g_api.screen_stop(state->screen);
        pthread_mutex_lock(&g_lock);
        state->screen_started = 0;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing begin receiver=%p screen=%p delegate=%p",
             state->receiver, state->screen, delegate_context);

    rc = g_api.screen_process(state->screen, net, ALTSCREEN_TIMEOUT_SECS);

    pthread_mutex_lock(&g_lock);
    state->accepted_fd = -1;
    pthread_mutex_unlock(&g_lock);

    (void)g_api.netsock_delete(net);
    g_api.screen_stop(state->screen);

    pthread_mutex_lock(&g_lock);
    state->screen_started = 0;
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing end receiver=%p rc=%d",
             state->receiver, rc);
    return NULL;
}

static int prepare_altscreen(void *receiver, cf_ref alt_desc, uint16_t *out_port)
{
    altscreen_state *state = NULL;
    void *screen = NULL;
    uint8_t key[16];
    uint8_t iv[16];
    uint64_t connection_id;
    int ok = 0;
    os_status rc;
    int listen_fd = -1;
    uint16_t port = 0;

    connection_id = (uint64_t)dict_get_i64(alt_desc, "streamConnectionID", &ok);
    if (!ok || connection_id == 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused reason=missing_connection_id");
        return 0;
    }

    if (!altscreen_aes_ready()) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused conn=%llu reason=stock_AES_not_ready primary_preserved=1",
                  (unsigned long long)connection_id);
        return 0;
    }

    pthread_mutex_lock(&g_lock);
    state = alloc_state_locked(receiver);
    pthread_mutex_unlock(&g_lock);
    if (state == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused receiver=%p reason=state_collision_or_full",
                  receiver);
        return 0;
    }

    rc = g_api.screen_create(&screen);
    if (rc != 0 || screen == NULL)
        goto fail;

    rc = g_api.screen_setup(screen, alt_desc, 0);
    if (rc != 0)
        goto fail;

    if (!altscreen_derive_aes(connection_id, key, iv))
        goto fail;
    rc = g_api.screen_security(screen, key, iv);
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    if (rc != 0)
        goto fail;

    if (!altscreen_register_session(screen))
        goto fail;

    listen_fd = open_listener(&port);
    if (listen_fd < 0)
        goto fail_registered;

    pthread_mutex_lock(&g_lock);
    state->screen = screen;
    state->connection_id = connection_id;
    state->listen_fd = listen_fd;
    state->data_port = port;
    pthread_mutex_unlock(&g_lock);

    rc = pthread_create(&state->worker, NULL, altscreen_worker, state);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "worker create failed receiver=%p rc=%d",
                  receiver, rc);
        goto fail_listener;
    }

    pthread_mutex_lock(&g_lock);
    state->worker_started = 1;
    pthread_mutex_unlock(&g_lock);

    *out_port = port;
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "READY receiver=%p screen=%p conn=%llu dataPort=%u primary_untouched=1",
             receiver, screen, (unsigned long long)connection_id,
             (unsigned int)port);
    return 1;

fail_listener:
    close(listen_fd);
    pthread_mutex_lock(&g_lock);
    state->listen_fd = -1;
    pthread_mutex_unlock(&g_lock);
fail_registered:
    altscreen_unregister_session(screen);
fail:
    if (screen != NULL)
        g_api.screen_delete(screen);
    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static void cleanup_altscreen(void *receiver)
{
    altscreen_state *state;
    pthread_t worker;
    int worker_started;
    int listen_fd;
    int accepted_fd;
    void *screen;
    int screen_started;
    int show_sent;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state == NULL) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    worker = state->worker;
    worker_started = state->worker_started;
    listen_fd = state->listen_fd;
    accepted_fd = state->accepted_fd;
    screen = state->screen;
    screen_started = state->screen_started;
    show_sent = state->show_sent;
    if (listen_fd >= 0) {
        close(listen_fd);
        state->listen_fd = -1;
    }
    if (accepted_fd >= 0)
        (void)shutdown(accepted_fd, SHUT_RDWR);
    pthread_mutex_unlock(&g_lock);

    if (show_sent)
        (void)send_event(receiver, "stopUI", 0);
    if (screen_started && screen != NULL && g_api.screen_send_cmd != NULL)
        (void)g_api.screen_send_cmd(screen, 'q', NULL, 0);

    if (worker_started && !pthread_equal(worker, pthread_self()))
        (void)pthread_join(worker, NULL);

    if (screen != NULL) {
        g_api.screen_stop(screen);
        altscreen_unregister_session(screen);
        g_api.screen_delete(screen);
    }

    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "cleanup receiver=%p route_stop=%d",
             receiver, show_sent);
}

os_status AirPlayReceiverSessionSetup(void *receiver,
                                      cf_ref request,
                                      cf_ref *out_response)
{
    cf_ref alt_desc = NULL;
    cf_ref stock_request = NULL;
    cf_ref stock_response = NULL;
    cf_ref final_response = NULL;
    uint16_t alt_port = 0;
    os_status rc;

    resolve_api();
    if (g_api.session_setup == NULL)
        return -1;

    if (!altscreen_enabled() || !g_api.ready || out_response == NULL)
        return g_api.session_setup(receiver, request, out_response);

    stock_request = build_stock_only_request(request, &alt_desc);
    if (stock_request == NULL || alt_desc == NULL)
        return g_api.session_setup(receiver, request, out_response);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "requested receiver=%p stock_only_request=%p descriptor=%p primary_protected=1",
             receiver, stock_request, alt_desc);

    rc = g_api.session_setup(receiver, stock_request, &stock_response);
    g_api.release(stock_request);
    stock_request = NULL;
    if (rc != 0) {
        if (stock_response != NULL) g_api.release(stock_response);
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "stock setup failed receiver=%p rc=%d",
                  receiver, rc);
        return rc;
    }

    if (!prepare_altscreen(receiver, alt_desc, &alt_port)) {
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare failed receiver=%p stock_response_preserved=1",
                  receiver);
        return 0;
    }

    final_response = merge_altscreen_response(stock_response, alt_port);
    if (final_response == NULL) {
        cleanup_altscreen(receiver);
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "response merge failed receiver=%p stock_response_preserved=1",
                  receiver);
        return 0;
    }

    if (stock_response != NULL)
        g_api.release(stock_response);
    *out_response = final_response;

    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "SETUP_READY receiver=%p dataPort=%u enabledFeatures=altScreen,viewAreas primary_untouched=1",
             receiver, (unsigned int)alt_port);
    return 0;
}

void AirPlayReceiverSessionTearDown(void *receiver,
                                    cf_ref request,
                                    os_status reason,
                                    unsigned char *out_done)
{
    resolve_api();
    if (altscreen_enabled() && g_api.ready)
        cleanup_altscreen(receiver);
    if (g_api.session_teardown != NULL)
        g_api.session_teardown(receiver, request, reason, out_done);
}

os_status ScreenStreamProcessData(void *stream,
                                  const uint8_t *data,
                                  size_t len,
                                  uint64_t display_ticks,
                                  cf_ref options,
                                  void (*completion)(void *),
                                  void *completion_context)
{
    os_status rc;
    altscreen_state *state = NULL;
    pthread_t self;
    int should_show = 0;

    resolve_api();
    if (g_api.screen_process_data == NULL)
        return -1;

    rc = g_api.screen_process_data(stream, data, len, display_ticks,
                                   options, completion, completion_context);
    if (rc != 0 || !altscreen_enabled())
        return rc;

    self = pthread_self();
    pthread_mutex_lock(&g_lock);
    state = find_state_worker_locked(self);
    if (state != NULL && state->screen_started && !state->show_sent) {
        state->show_sent = 1;
        should_show = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (should_show) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "FIRST_REAL_FRAME receiver=%p stream=%p len=%u route=cluster",
                 state->receiver, stream, (unsigned int)len);
        request_cluster(state);
    }
    return rc;
}
