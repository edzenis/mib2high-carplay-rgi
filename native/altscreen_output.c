#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "altscreen.h"
#include "framework/logging.h"

#define ALTSCREEN_OUTPUT_MODULE "AltScreenOutput"
#define ALTSCREEN_MAX_SESSIONS 4
#define NVSS_OUTPUT_CONFIG_OFFSET 10U
#define DISPLAY_TARGET_MAIN 59U
#define DISPLAY_TARGET_CLUSTER 58U
#define NVSS_DIAG_BYTES 20U
#define SCREEN_MASTER_KEY_LEN 16U

typedef struct {
    void *context;
    void *get_synchronized_ntp_time;
    void *get_up_ticks_near_synchronized_ntp_time;
} altscreen_time_sync;

typedef int (*screen_start_fn)(void *screen_session, void *delegate_context);
typedef void (*screen_delete_fn)(void *screen_session);
typedef void (*screen_time_sync_fn)(void *screen_session,
                                    const altscreen_time_sync *sync);
typedef int (*nvss_video_open_fn)(void *out_handle, void *config);
typedef void (*derive_screen_aes_fn)(const void *master_key,
                                     size_t master_key_len,
                                     uint64_t connection_id,
                                     uint8_t out_key[16],
                                     uint8_t out_iv[16]);

static pthread_mutex_t g_alt_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_alt_sessions[ALTSCREEN_MAX_SESSIONS];
static pthread_t g_patch_owner;
static int g_patch_owner_valid;
static unsigned int g_patch_depth;
static unsigned int g_output_patch_count;
static void *g_main_delegate_context;
static altscreen_time_sync g_main_time_sync;
static int g_main_time_sync_valid;
static uint8_t g_screen_master_key[SCREEN_MASTER_KEY_LEN];
static int g_screen_master_key_valid;

static screen_start_fn g_real_screen_start;
static screen_delete_fn g_real_screen_delete;
static screen_time_sync_fn g_real_time_sync;
static nvss_video_open_fn g_real_nvss_open;
static derive_screen_aes_fn g_real_derive_screen_aes;

static void resolve_symbols(void)
{
    if (g_real_screen_start == NULL) {
        g_real_screen_start = (screen_start_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_StartSession");
    }
    if (g_real_screen_delete == NULL) {
        g_real_screen_delete = (screen_delete_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_Delete");
    }
    if (g_real_time_sync == NULL) {
        g_real_time_sync = (screen_time_sync_fn)dlsym(
            RTLD_NEXT, "AirPlayReceiverSessionScreen_SetTimeSynchronizer");
    }
    if (g_real_nvss_open == NULL) {
        g_real_nvss_open = (nvss_video_open_fn)dlsym(
            RTLD_NEXT, "NvSSVideoOpen");
    }
    if (g_real_derive_screen_aes == NULL) {
        g_real_derive_screen_aes = (derive_screen_aes_fn)dlsym(
            RTLD_NEXT, "AirPlay_DeriveAESKeySHA512ForScreen");
    }
}

static int session_index_locked(void *screen_session)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_SESSIONS; ++i) {
        if (g_alt_sessions[i] == screen_session)
            return i;
    }
    return -1;
}

int altscreen_register_session(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);

    if (session_index_locked(screen_session) >= 0) {
        pthread_mutex_unlock(&g_alt_lock);
        return 1;
    }

    for (i = 0; i < ALTSCREEN_MAX_SESSIONS; ++i) {
        if (g_alt_sessions[i] == NULL) {
            g_alt_sessions[i] = screen_session;
            pthread_mutex_unlock(&g_alt_lock);
            LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                     "session register screen=%p slot=%d primary_untouched=1",
                     screen_session, i);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_alt_lock);
    LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
              "session register refused screen=%p reason=table_full primary_untouched=1",
              screen_session);
    return 0;
}

void altscreen_unregister_session(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return;

    pthread_mutex_lock(&g_alt_lock);
    i = session_index_locked(screen_session);
    if (i >= 0)
        g_alt_sessions[i] = NULL;
    pthread_mutex_unlock(&g_alt_lock);

    if (i >= 0) {
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "session unregister screen=%p slot=%d",
                 screen_session, i);
    }
}

int altscreen_is_session(void *screen_session)
{
    int found;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);
    found = session_index_locked(screen_session) >= 0;
    pthread_mutex_unlock(&g_alt_lock);
    return found;
}

static int begin_output_scope(void)
{
    pthread_t self = pthread_self();
    int ok = 0;

    pthread_mutex_lock(&g_alt_lock);

    if (!g_patch_owner_valid) {
        g_patch_owner = self;
        g_patch_owner_valid = 1;
        g_patch_depth = 1;
        ok = 1;
    } else if (pthread_equal(g_patch_owner, self)) {
        ++g_patch_depth;
        ok = 1;
    }

    pthread_mutex_unlock(&g_alt_lock);

    if (!ok) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "output scope refused reason=concurrent_owner primary_untouched=1");
    }
    return ok;
}

static void end_output_scope(void)
{
    pthread_t self = pthread_self();

    pthread_mutex_lock(&g_alt_lock);

    if (g_patch_owner_valid && pthread_equal(g_patch_owner, self)) {
        if (g_patch_depth > 1) {
            --g_patch_depth;
        } else {
            g_patch_depth = 0;
            g_patch_owner_valid = 0;
        }
    }

    pthread_mutex_unlock(&g_alt_lock);
}

static int output_scope_active_for_current_thread(void)
{
    pthread_t self = pthread_self();
    int active;

    pthread_mutex_lock(&g_alt_lock);
    active = g_patch_owner_valid &&
             g_patch_depth != 0 &&
             pthread_equal(g_patch_owner, self);
    pthread_mutex_unlock(&g_alt_lock);
    return active;
}

int altscreen_session_scope_active(void)
{
    return output_scope_active_for_current_thread();
}

void *altscreen_main_delegate_context(void)
{
    void *ctx;

    pthread_mutex_lock(&g_alt_lock);
    ctx = g_main_delegate_context;
    pthread_mutex_unlock(&g_alt_lock);
    return ctx;
}

int altscreen_time_sync_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_main_time_sync_valid && g_real_time_sync != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int altscreen_apply_main_time_sync(void *screen_session)
{
    altscreen_time_sync sync;
    int ready;

    if (screen_session == NULL)
        return 0;
    resolve_symbols();

    pthread_mutex_lock(&g_alt_lock);
    ready = g_main_time_sync_valid && g_real_time_sync != NULL;
    if (ready)
        sync = g_main_time_sync;
    pthread_mutex_unlock(&g_alt_lock);

    if (!ready) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "time sync refused reason=main_sync_not_captured");
        return 0;
    }

    g_real_time_sync(screen_session, &sync);
    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "time sync applied screen=%p context=%p",
             screen_session, sync.context);
    return 1;
}

int altscreen_aes_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_screen_master_key_valid && g_real_derive_screen_aes != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int altscreen_derive_aes(uint64_t stream_connection_id,
                         uint8_t out_key[16],
                         uint8_t out_iv[16])
{
    uint8_t master[SCREEN_MASTER_KEY_LEN];
    int ready;

    if (stream_connection_id == 0 || out_key == NULL || out_iv == NULL)
        return 0;

    resolve_symbols();

    pthread_mutex_lock(&g_alt_lock);
    ready = g_screen_master_key_valid && g_real_derive_screen_aes != NULL;
    if (ready)
        memcpy(master, g_screen_master_key, sizeof(master));
    pthread_mutex_unlock(&g_alt_lock);

    if (!ready) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "AES derive refused conn=%llu reason=master_key_not_captured",
                  (unsigned long long)stream_connection_id);
        return 0;
    }

    g_real_derive_screen_aes(master, sizeof(master), stream_connection_id,
                             out_key, out_iv);
    memset(master, 0, sizeof(master));

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "AES derived conn=%llu source=stock_screen_master",
             (unsigned long long)stream_connection_id);
    return 1;
}

int altscreen_output_ready(void)
{
    resolve_symbols();
    return g_real_screen_start != NULL && g_real_nvss_open != NULL;
}

unsigned int altscreen_output_patch_count(void)
{
    unsigned int count;

    pthread_mutex_lock(&g_alt_lock);
    count = g_output_patch_count;
    pthread_mutex_unlock(&g_alt_lock);
    return count;
}

int altscreen_start_session(void *screen_session, void *delegate_context)
{
    int rc;

    resolve_symbols();

    if (g_real_screen_start == NULL) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused reason=stock_start_missing");
        return -1;
    }
    if (!altscreen_is_session(screen_session)) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused screen=%p reason=not_registered",
                  screen_session);
        return -1;
    }
    if (!altscreen_apply_main_time_sync(screen_session)) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "session start refused screen=%p reason=time_sync_not_ready",
                  screen_session);
        return -1;
    }
    if (!begin_output_scope())
        return -1;

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "session start screen=%p output_scope=1 delegate=%p",
             screen_session, delegate_context);

    rc = g_real_screen_start(screen_session, delegate_context);
    end_output_scope();

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "session start return screen=%p rc=%d patches=%u",
             screen_session, rc, altscreen_output_patch_count());
    return rc;
}

int AirPlayReceiverSessionScreen_StartSession(void *screen_session,
                                               void *delegate_context)
{
    int rc;
    int alt_session;

    resolve_symbols();
    if (g_real_screen_start == NULL)
        return -1;

    alt_session = altscreen_is_session(screen_session);
    if (!alt_session) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_delegate_context = delegate_context;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary delegate captured context=%p",
                 delegate_context);
        return g_real_screen_start(screen_session, delegate_context);
    }

    if (!begin_output_scope())
        return -1;
    rc = g_real_screen_start(screen_session, delegate_context);
    end_output_scope();
    return rc;
}

void AirPlayReceiverSessionScreen_SetTimeSynchronizer(
    void *screen_session,
    const altscreen_time_sync *sync)
{
    int alt_session;

    resolve_symbols();
    if (g_real_time_sync == NULL)
        return;

    alt_session = altscreen_is_session(screen_session);
    if (!alt_session && sync != NULL) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_time_sync = *sync;
        g_main_time_sync_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary time sync captured screen=%p context=%p",
                 screen_session, sync->context);
    }

    g_real_time_sync(screen_session, sync);
}

void AirPlayReceiverSessionScreen_Delete(void *screen_session)
{
    resolve_symbols();
    altscreen_unregister_session(screen_session);
    if (g_real_screen_delete != NULL)
        g_real_screen_delete(screen_session);
}

void AirPlay_DeriveAESKeySHA512ForScreen(const void *master_key,
                                         size_t master_key_len,
                                         uint64_t connection_id,
                                         uint8_t out_key[16],
                                         uint8_t out_iv[16])
{
    resolve_symbols();

    if (master_key != NULL && master_key_len == SCREEN_MASTER_KEY_LEN) {
        pthread_mutex_lock(&g_alt_lock);
        memcpy(g_screen_master_key, master_key, SCREEN_MASTER_KEY_LEN);
        g_screen_master_key_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
                 "primary screen AES master captured conn=%llu len=%u",
                 (unsigned long long)connection_id,
                 (unsigned int)master_key_len);
    }

    if (g_real_derive_screen_aes != NULL) {
        g_real_derive_screen_aes(master_key, master_key_len, connection_id,
                                 out_key, out_iv);
    }
}

int NvSSVideoOpen(void *out_handle, void *config)
{
    uint8_t *cfg = (uint8_t *)config;
    uint8_t saved;
    int rc;

    resolve_symbols();
    if (g_real_nvss_open == NULL)
        return -1;

    if (!output_scope_active_for_current_thread() || cfg == NULL)
        return g_real_nvss_open(out_handle, config);

    saved = cfg[NVSS_OUTPUT_CONFIG_OFFSET];

    LOG_HEXDUMP(ALTSCREEN_OUTPUT_MODULE, "NvSS open stock config",
                cfg, NVSS_DIAG_BYTES);

    if (saved != DISPLAY_TARGET_MAIN) {
        LOG_ERROR(ALTSCREEN_OUTPUT_MODULE,
                  "output patch refused offset=0x%x expected=%u actual=%u primary_untouched=1",
                  (unsigned int)NVSS_OUTPUT_CONFIG_OFFSET,
                  (unsigned int)DISPLAY_TARGET_MAIN,
                  (unsigned int)saved);
        return g_real_nvss_open(out_handle, config);
    }

    cfg[NVSS_OUTPUT_CONFIG_OFFSET] = DISPLAY_TARGET_CLUSTER;

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "output patch applied offset=0x%x %u->%u altscreen=1",
             (unsigned int)NVSS_OUTPUT_CONFIG_OFFSET,
             (unsigned int)DISPLAY_TARGET_MAIN,
             (unsigned int)DISPLAY_TARGET_CLUSTER);
    LOG_HEXDUMP(ALTSCREEN_OUTPUT_MODULE, "NvSS open patched config",
                cfg, NVSS_DIAG_BYTES);

    rc = g_real_nvss_open(out_handle, config);
    cfg[NVSS_OUTPUT_CONFIG_OFFSET] = saved;

    pthread_mutex_lock(&g_alt_lock);
    ++g_output_patch_count;
    pthread_mutex_unlock(&g_alt_lock);

    LOG_INFO(ALTSCREEN_OUTPUT_MODULE,
             "NvSS open return rc=%d config_restored=1 patches=%u",
             rc, altscreen_output_patch_count());
    return rc;
}
