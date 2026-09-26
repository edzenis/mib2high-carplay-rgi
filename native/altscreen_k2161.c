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

typedef int (*screen_start_fn)(void *screen_session, void *delegate_context);
typedef void (*screen_delete_fn)(void *screen_session);
typedef int (*nvss_video_open_fn)(void *out_handle, void *config);

static pthread_mutex_t g_alt_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_private_screens[ALTSCREEN_MAX_PRIVATE_SCREENS];
static pthread_t g_patch_owner;
static int g_patch_owner_valid;
static unsigned int g_patch_depth;
static unsigned int g_nvss_patch_count;

static screen_start_fn g_real_screen_start;
static screen_delete_fn g_real_screen_delete;
static nvss_video_open_fn g_real_nvss_open;

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
    if (g_real_nvss_open == NULL) {
        g_real_nvss_open = (nvss_video_open_fn)dlsym(
            RTLD_NEXT, "NvSSVideoOpen");
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

/*
 * Deterministic entry point for the future private type-111 setup path.
 * The stock StartSession creates and starts its own ScreenStream synchronously;
 * while it is executing, the NvSSVideoOpen interposer below is allowed to
 * rewrite only the private stream's output target.
 */
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
             "private111 start screen=%p native58_scope=1",
             screen_session);

    rc = g_real_screen_start(screen_session, delegate_context);
    end_native58_scope();

    LOG_INFO(ALTSCREEN_MODULE,
             "private111 start return screen=%p rc=%d patches=%u",
             screen_session, rc, k2161_altscreen_nvss_patch_count());
    return rc;
}

/*
 * Optional symbol interposer.  It is still fail-closed because an ordinary
 * stock screen is never in g_private_screens.  Private111 code may call
 * k2161_altscreen_private_start() directly instead of relying on ELF
 * preemption of libairplay's internal call.
 */
int AirPlayReceiverSessionScreen_StartSession(void *screen_session,
                                               void *delegate_context)
{
    int rc;
    int private_screen;

    resolve_symbols();
    if (g_real_screen_start == NULL)
        return -1;

    private_screen = k2161_altscreen_is_private_screen(screen_session);
    if (!private_screen)
        return g_real_screen_start(screen_session, delegate_context);

    if (!begin_native58_scope())
        return -1;
    rc = g_real_screen_start(screen_session, delegate_context);
    end_native58_scope();
    return rc;
}

void AirPlayReceiverSessionScreen_Delete(void *screen_session)
{
    resolve_symbols();
    k2161_altscreen_unregister_private_screen(screen_session);
    if (g_real_screen_delete != NULL)
        g_real_screen_delete(screen_session);
}

/*
 * Stock K2161 ScreenStreamStart() constructs the NvSS open block on its stack.
 * Reverse engineering of MHI2_ER_AUG22_K2161_MU1421 shows byte +0x0a is 59.
 * We deliberately require that exact stock value before changing it to 58.
 * Any unexpected firmware/layout therefore falls through untouched.
 */
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

    /* The open block is owned by the stock caller. Restore it immediately. */
    cfg[K2161_NVSS_OUTPUT_OFFSET] = saved;

    pthread_mutex_lock(&g_alt_lock);
    ++g_nvss_patch_count;
    pthread_mutex_unlock(&g_alt_lock);

    LOG_INFO(ALTSCREEN_MODULE,
             "native58 NvSS open return rc=%d config_restored=1 patches=%u",
             rc, k2161_altscreen_nvss_patch_count());
    return rc;
}
