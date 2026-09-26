#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "altscreen_k2161.h"
#include "framework/logging.h"

#define ALTSCREEN_MODULE "AltScreenK2161"
#define ALTSCREEN_MAX_PRIVATE_SCREENS 4
#define K2161_NVSS_OUTPUT_OFFSET 10U
#define K2161_NVSS_OUTPUT_MAIN 59U
#define K2161_NVSS_OUTPUT_CLUSTER 58U
#define K2161_NVSS_DIAG_BYTES 20U
#define K2161_SCREEN_MASTER_KEY_LEN 16U

typedef struct {
    void *context;
    void *get_synchronized_ntp_time;
    void *get_up_ticks_near_synchronized_ntp_time;
} k2161_screen_time_sync;

typedef int (*screen_start_fn)(void *screen_session, void *delegate_context);
typedef void (*screen_delete_fn)(void *screen_session);
typedef void (*screen_time_sync_fn)(void *screen_session,
                                    const k2161_screen_time_sync *sync);
typedef int (*nvss_video_open_fn)(void *out_handle, void *config);
typedef void (*derive_screen_aes_fn)(const void *master_key,
                                     size_t master_key_len,
                                     uint64_t connection_id,
                                     uint8_t out_key[16],
                                     uint8_t out_iv[16]);

static pthread_mutex_t g_alt_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_private_screens[ALTSCREEN_MAX_PRIVATE_SCREENS];
static pthread_t g_patch_owner;
static int g_patch_owner_valid;
static unsigned int g_patch_depth;
static unsigned int g_nvss_patch_count;
static void *g_main_delegate_context;
static k2161_screen_time_sync g_main_time_sync;
static int g_main_time_sync_valid;
static uint8_t g_screen_master_key[K2161_SCREEN_MASTER_KEY_LEN];
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

static int private_index_locked(void *screen_session)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_PRIVATE_SCREENS; ++i) {
        if (g_private_screens[i] == screen_session)
            return i;
    }
    return -1;
}

int k2161_altscreen_register_private_screen(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);

    if (private_index_locked(screen_session) >= 0) {
        pthread_mutex_unlock(&g_alt_lock);
        return 1;
    }

    for (i = 0; i < ALTSCREEN_MAX_PRIVATE_SCREENS; ++i) {
        if (g_private_screens[i] == NULL) {
            g_private_screens[i] = screen_session;
            pthread_mutex_unlock(&g_alt_lock);
            LOG_INFO(ALTSCREEN_MODULE,
                     "private111 register screen=%p slot=%d main110_untouched=1",
                     screen_session, i);
            return 1;
        }
    }

    pthread_mutex_unlock(&g_alt_lock);
    LOG_ERROR(ALTSCREEN_MODULE,
              "private111 register refused screen=%p reason=table_full main110_untouched=1",
              screen_session);
    return 0;
}

void k2161_altscreen_unregister_private_screen(void *screen_session)
{
    int i;

    if (screen_session == NULL)
        return;

    pthread_mutex_lock(&g_alt_lock);
    i = private_index_locked(screen_session);
    if (i >= 0)
        g_private_screens[i] = NULL;
    pthread_mutex_unlock(&g_alt_lock);

    if (i >= 0) {
        LOG_INFO(ALTSCREEN_MODULE,
                 "private111 unregister screen=%p slot=%d",
                 screen_session, i);
    }
}

int k2161_altscreen_is_private_screen(void *screen_session)
{
    int found;

    if (screen_session == NULL)
        return 0;

    pthread_mutex_lock(&g_alt_lock);
    found = private_index_locked(screen_session) >= 0;
    pthread_mutex_unlock(&g_alt_lock);
    return found;
}

static int begin_native58_scope(void)
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
        LOG_ERROR(ALTSCREEN_MODULE,
                  "native58 scope refused reason=concurrent_owner main110_untouched=1");
    }
    return ok;
}

static void end_native58_scope(void)
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

static int native58_scope_active_for_current_thread(void)
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

int k2161_altscreen_private_scope_active(void)
{
    return native58_scope_active_for_current_thread();
}

void *k2161_altscreen_main_delegate_context(void)
{
    void *ctx;

    pthread_mutex_lock(&g_alt_lock);
    ctx = g_main_delegate_context;
    pthread_mutex_unlock(&g_alt_lock);
    return ctx;
}

int k2161_altscreen_time_sync_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_main_time_sync_valid && g_real_time_sync != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int k2161_altscreen_apply_main_time_sync(void *screen_session)
{
    k2161_screen_time_sync sync;
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
        LOG_ERROR(ALTSCREEN_MODULE,
                  "private111 time sync refused reason=main_sync_not_captured");
        return 0;
    }

    g_real_time_sync(screen_session, &sync);
    LOG_INFO(ALTSCREEN_MODULE,
             "private111 time sync applied screen=%p context=%p",
             screen_session, sync.context);
    return 1;
}

int k2161_altscreen_private_aes_ready(void)
{
    int ready;

    resolve_symbols();
    pthread_mutex_lock(&g_alt_lock);
    ready = g_screen_master_key_valid && g_real_derive_screen_aes != NULL;
    pthread_mutex_unlock(&g_alt_lock);
    return ready;
}

int k2161_altscreen_derive_private_aes(uint64_t stream_connection_id,
                                       uint8_t out_key[16],
                                       uint8_t out_iv[16])
{
    uint8_t master[K2161_SCREEN_MASTER_KEY_LEN];
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
        LOG_ERROR(ALTSCREEN_MODULE,
                  "private111 AES derive refused conn=%llu reason=master_key_not_captured",
                  (unsigned long long)stream_connection_id);
        return 0;
    }

    g_real_derive_screen_aes(master, sizeof(master), stream_connection_id,
                             out_key, out_iv);
    memset(master, 0, sizeof(master));

    LOG_INFO(ALTSCREEN_MODULE,
             "private111 AES derived conn=%llu source=stock_screen_master",
             (unsigned long long)stream_connection_id);
    return 1;
}

int k2161_altscreen_native58_ready(void)
{
    resolve_symbols();
    return g_real_screen_start != NULL && g_real_nvss_open != NULL;
}

unsigned int k2161_altscreen_nvss_patch_count(void)
{
    unsigned int count;

    pthread_mutex_lock(&g_alt_lock);
    count = g_nvss_patch_count;
    pthread_mutex_unlock(&g_alt_lock);
    return count;
}

int k2161_altscreen_private_start(void *screen_session, void *delegate_context)
{
    int rc;

    resolve_symbols();

    if (g_real_screen_start == NULL) {
        LOG_ERROR(ALTSCREEN_MODULE,
                  "private111 start refused reason=stock_start_missing");
        return -1;
    }
    if (!k2161_altscreen_is_private_screen(screen_session)) {
        LOG_ERROR(ALTSCREEN_MODULE,
                  "private111 start refused screen=%p reason=not_registered",
                  screen_session);
        return -1;
    }
    if (!begin_native58_scope())
        return -1;

    LOG_INFO(ALTSCREEN_MODULE,
             "private111 start screen=%p native58_scope=1 delegate=%p",
             screen_session, delegate_context);

    rc = g_real_screen_start(screen_session, delegate_context);
    end_native58_scope();

    LOG_INFO(ALTSCREEN_MODULE,
             "private111 start return screen=%p rc=%d patches=%u",
             screen_session, rc, k2161_altscreen_nvss_patch_count());
    return rc;
}

int AirPlayReceiverSessionScreen_StartSession(void *screen_session,
                                               void *delegate_context)
{
    int rc;
    int private_screen;

    resolve_symbols();
    if (g_real_screen_start == NULL)
        return -1;

    private_screen = k2161_altscreen_is_private_screen(screen_session);
    if (!private_screen) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_delegate_context = delegate_context;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_MODULE,
                 "main110 delegate captured context=%p",
                 delegate_context);
        return g_real_screen_start(screen_session, delegate_context);
    }

    if (!begin_native58_scope())
        return -1;
    rc = g_real_screen_start(screen_session, delegate_context);
    end_native58_scope();
    return rc;
}

void AirPlayReceiverSessionScreen_SetTimeSynchronizer(
    void *screen_session,
    const k2161_screen_time_sync *sync)
{
    int private_screen;

    resolve_symbols();
    if (g_real_time_sync == NULL)
        return;

    private_screen = k2161_altscreen_is_private_screen(screen_session);
    if (!private_screen && sync != NULL) {
        pthread_mutex_lock(&g_alt_lock);
        g_main_time_sync = *sync;
        g_main_time_sync_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_MODULE,
                 "main110 time sync captured screen=%p context=%p",
                 screen_session, sync->context);
    }

    g_real_time_sync(screen_session, sync);
}

void AirPlayReceiverSessionScreen_Delete(void *screen_session)
{
    resolve_symbols();
    k2161_altscreen_unregister_private_screen(screen_session);
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

    if (master_key != NULL && master_key_len == K2161_SCREEN_MASTER_KEY_LEN) {
        pthread_mutex_lock(&g_alt_lock);
        memcpy(g_screen_master_key, master_key, K2161_SCREEN_MASTER_KEY_LEN);
        g_screen_master_key_valid = 1;
        pthread_mutex_unlock(&g_alt_lock);
        LOG_INFO(ALTSCREEN_MODULE,
                 "main110 screen AES master captured conn=%llu len=%u",
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

    if (!native58_scope_active_for_current_thread() || cfg == NULL)
        return g_real_nvss_open(out_handle, config);

    saved = cfg[K2161_NVSS_OUTPUT_OFFSET];

    LOG_HEXDUMP(ALTSCREEN_MODULE, "private111 NvSS open stock config",
                cfg, K2161_NVSS_DIAG_BYTES);

    if (saved != K2161_NVSS_OUTPUT_MAIN) {
        LOG_ERROR(ALTSCREEN_MODULE,
                  "native58 patch refused offset=0x%x expected=%u actual=%u main110_untouched=1",
                  (unsigned int)K2161_NVSS_OUTPUT_OFFSET,
                  (unsigned int)K2161_NVSS_OUTPUT_MAIN,
                  (unsigned int)saved);
        return g_real_nvss_open(out_handle, config);
    }

    cfg[K2161_NVSS_OUTPUT_OFFSET] = K2161_NVSS_OUTPUT_CLUSTER;

    LOG_INFO(ALTSCREEN_MODULE,
             "native58 patch applied offset=0x%x %u->%u private111=1",
             (unsigned int)K2161_NVSS_OUTPUT_OFFSET,
             (unsigned int)K2161_NVSS_OUTPUT_MAIN,
             (unsigned int)K2161_NVSS_OUTPUT_CLUSTER);
    LOG_HEXDUMP(ALTSCREEN_MODULE, "private111 NvSS open patched config",
                cfg, K2161_NVSS_DIAG_BYTES);

    rc = g_real_nvss_open(out_handle, config);
    cfg[K2161_NVSS_OUTPUT_OFFSET] = saved;

    pthread_mutex_lock(&g_alt_lock);
    ++g_nvss_patch_count;
    pthread_mutex_unlock(&g_alt_lock);

    LOG_INFO(ALTSCREEN_MODULE,
             "native58 NvSS open return rc=%d config_restored=1 patches=%u",
             rc, k2161_altscreen_nvss_patch_count());
    return rc;
}
