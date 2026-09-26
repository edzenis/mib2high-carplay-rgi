#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "altscreen.h"
#include "framework/logging.h"

#define ALTINFO_MODULE "AltScreenInfo"
#define ALTSCREEN_ENABLE_MARKER "/mnt/app/carplay_altscreen"
#define ALTSCREEN_URL_CARD "maps:/car/instrumentcluster/instructioncard"
#define ALTSCREEN_URL_BASE "maps:/car/instrumentcluster"
#define ALTSCREEN_DEFAULT_W 800
#define ALTSCREEN_DEFAULT_H 480
#define ALTSCREEN_DEFAULT_FPS 30
#define K_CFSTRING_UTF8 0x08000100U

typedef void *cf_ref;
typedef unsigned long cf_type_id;
typedef long cf_index;
typedef int os_status;

typedef cf_ref (*platform_copy_fn)(cf_ref, uint32_t, cf_ref, cf_ref, os_status *);
typedef cf_ref (*cf_string_create_fn)(cf_ref, const char *, uint32_t);
typedef cf_type_id (*cf_get_type_fn)(cf_ref);
typedef cf_type_id (*cf_type_getter_fn)(void);
typedef cf_ref (*cf_array_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_array_create_mutable_fn)(cf_ref, cf_index, const void *);
typedef void (*cf_array_append_fn)(cf_ref, const void *);
typedef cf_index (*cf_array_count_fn)(cf_ref);
typedef const void *(*cf_array_value_fn)(cf_ref, cf_index);
typedef cf_ref (*cf_dict_create_mutable_fn)(cf_ref, cf_index, const void *, const void *);
typedef void (*cf_dict_set_value_fn)(cf_ref, const void *, const void *);
typedef os_status (*cf_dict_set_i64_fn)(cf_ref, const void *, int64_t);
typedef void (*cf_release_fn)(cf_ref);

typedef struct {
    platform_copy_fn server_copy;
    platform_copy_fn session_copy;
    cf_string_create_fn string_create;
    cf_get_type_fn get_type;
    cf_type_getter_fn array_type;
    cf_type_getter_fn dict_type;
    cf_array_mutable_copy_fn array_mutable_copy;
    cf_array_create_mutable_fn array_create_mutable;
    cf_array_append_fn array_append;
    cf_array_count_fn array_count;
    cf_array_value_fn array_value;
    cf_dict_create_mutable_fn dict_create_mutable;
    cf_dict_set_value_fn dict_set_value;
    cf_dict_set_i64_fn dict_set_i64;
    cf_release_fn release;
    const void *dict_key_callbacks;
    const void *dict_value_callbacks;
    const void *array_callbacks;
    const void *boolean_true;
    const void *boolean_false;
    int resolved;
    int ready;
} alt_cf_api;

static alt_cf_api g_cf;
static int g_info_logged;

static void *sym(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

static void resolve_cf(void)
{
    if (g_cf.resolved)
        return;

    g_cf.resolved = 1;
    g_cf.server_copy = (platform_copy_fn)sym("AirPlayReceiverServerPlatformCopyProperty");
    g_cf.session_copy = (platform_copy_fn)sym("AirPlayReceiverSessionPlatformCopyProperty");
    g_cf.string_create = (cf_string_create_fn)sym("CFStringCreateWithCString");
    g_cf.get_type = (cf_get_type_fn)sym("CFGetTypeID");
    g_cf.array_type = (cf_type_getter_fn)sym("CFArrayGetTypeID");
    g_cf.dict_type = (cf_type_getter_fn)sym("CFDictionaryGetTypeID");
    g_cf.array_mutable_copy = (cf_array_mutable_copy_fn)sym("CFArrayCreateMutableCopy");
    g_cf.array_create_mutable = (cf_array_create_mutable_fn)sym("CFArrayCreateMutable");
    g_cf.array_append = (cf_array_append_fn)sym("CFArrayAppendValue");
    g_cf.array_count = (cf_array_count_fn)sym("CFArrayGetCount");
    g_cf.array_value = (cf_array_value_fn)sym("CFArrayGetValueAtIndex");
    g_cf.dict_create_mutable = (cf_dict_create_mutable_fn)sym("CFDictionaryCreateMutable");
    g_cf.dict_set_value = (cf_dict_set_value_fn)sym("CFDictionarySetValue");
    g_cf.dict_set_i64 = (cf_dict_set_i64_fn)sym("CFDictionarySetInt64");
    g_cf.release = (cf_release_fn)sym("CFRelease");
    g_cf.dict_key_callbacks = sym("kCFTypeDictionaryKeyCallBacks");
    g_cf.dict_value_callbacks = sym("kCFTypeDictionaryValueCallBacks");
    g_cf.array_callbacks = sym("kCFTypeArrayCallBacks");
    {
        const void **p = (const void **)sym("kCFBooleanTrue");
        const void **q = (const void **)sym("kCFBooleanFalse");
        g_cf.boolean_true = p != NULL ? *p : NULL;
        g_cf.boolean_false = q != NULL ? *q : NULL;
    }

    g_cf.ready =
        g_cf.string_create != NULL &&
        g_cf.get_type != NULL &&
        g_cf.array_type != NULL &&
        g_cf.dict_type != NULL &&
        g_cf.array_mutable_copy != NULL &&
        g_cf.array_create_mutable != NULL &&
        g_cf.array_append != NULL &&
        g_cf.array_count != NULL &&
        g_cf.array_value != NULL &&
        g_cf.dict_create_mutable != NULL &&
        g_cf.dict_set_value != NULL &&
        g_cf.dict_set_i64 != NULL &&
        g_cf.release != NULL &&
        g_cf.dict_key_callbacks != NULL &&
        g_cf.dict_value_callbacks != NULL &&
        g_cf.array_callbacks != NULL &&
        g_cf.boolean_true != NULL &&
        g_cf.boolean_false != NULL;
}

int altscreen_enabled(void)
{
    const char *env = getenv("CARPLAY_ALTSCREEN");

    if (env != NULL && env[0] == '1' && env[1] == '\0')
        return 1;
    return access(ALTSCREEN_ENABLE_MARKER, F_OK) == 0;
}

int altscreen_info_ready(void)
{
    resolve_cf();
    return g_cf.ready;
}

static cf_ref cfs(const char *s)
{
    if (s == NULL || g_cf.string_create == NULL)
        return NULL;
    return g_cf.string_create(NULL, s, K_CFSTRING_UTF8);
}

static int property_is(cf_ref property, const char *name)
{
    typedef unsigned char (*cf_equal_fn)(cf_ref, cf_ref);
    static cf_equal_fn equal_fn;
    cf_ref key;
    int equal = 0;

    if (property == NULL)
        return 0;
    if (equal_fn == NULL)
        equal_fn = (cf_equal_fn)sym("CFEqual");
    if (equal_fn == NULL)
        return 0;

    key = cfs(name);
    if (key != NULL) {
        equal = equal_fn(property, key) != 0;
        g_cf.release(key);
    }
    return equal;
}

static int is_array(cf_ref obj)
{
    return obj != NULL && g_cf.get_type(obj) == g_cf.array_type();
}

static int is_dict(cf_ref obj)
{
    return obj != NULL && g_cf.get_type(obj) == g_cf.dict_type();
}

static cf_ref new_dict(void)
{
    return g_cf.dict_create_mutable(NULL, 0,
                                    g_cf.dict_key_callbacks,
                                    g_cf.dict_value_callbacks);
}

static cf_ref new_array(void)
{
    return g_cf.array_create_mutable(NULL, 0, g_cf.array_callbacks);
}

static int set_i64(cf_ref dict, const char *name, int64_t value)
{
    cf_ref key = cfs(name);
    os_status rc;

    if (key == NULL)
        return 0;
    rc = g_cf.dict_set_i64(dict, key, value);
    g_cf.release(key);
    return rc == 0;
}

static int set_obj(cf_ref dict, const char *name, cf_ref value)
{
    cf_ref key = cfs(name);

    if (key == NULL || value == NULL) {
        if (key != NULL)
            g_cf.release(key);
        return 0;
    }
    g_cf.dict_set_value(dict, key, value);
    g_cf.release(key);
    return 1;
}

static int set_string(cf_ref dict, const char *name, const char *value)
{
    cf_ref str = cfs(value);
    int ok;

    if (str == NULL)
        return 0;
    ok = set_obj(dict, name, str);
    g_cf.release(str);
    return ok;
}

static int set_bool(cf_ref dict, const char *name, int value)
{
    return set_obj(dict, name,
                   (cf_ref)(value ? g_cf.boolean_true : g_cf.boolean_false));
}

static cf_ref make_safe_area(int w, int h)
{
    cf_ref safe = new_dict();

    if (safe == NULL)
        return NULL;
    if (!set_i64(safe, "originXPixels", 0) ||
        !set_i64(safe, "originYPixels", 0) ||
        !set_i64(safe, "widthPixels", w) ||
        !set_i64(safe, "heightPixels", h) ||
        !set_bool(safe, "drawUIOutsideSafeArea", 0)) {
        g_cf.release(safe);
        return NULL;
    }
    return safe;
}

static cf_ref make_view_areas(int w, int h)
{
    cf_ref areas = NULL;
    cf_ref area = NULL;
    cf_ref safe = NULL;

    areas = new_array();
    area = new_dict();
    safe = make_safe_area(w, h);
    if (areas == NULL || area == NULL || safe == NULL)
        goto fail;

    if (!set_i64(area, "originXPixels", 0) ||
        !set_i64(area, "originYPixels", 0) ||
        !set_i64(area, "widthPixels", w) ||
        !set_i64(area, "heightPixels", h) ||
        !set_obj(area, "safeArea", safe))
        goto fail;

    g_cf.array_append(areas, area);
    g_cf.release(safe);
    g_cf.release(area);
    return areas;

fail:
    if (safe != NULL) g_cf.release(safe);
    if (area != NULL) g_cf.release(area);
    if (areas != NULL) g_cf.release(areas);
    return NULL;
}

static cf_ref make_alt_display(void)
{
    cf_ref alt = NULL;
    cf_ref view_areas = NULL;
    cf_ref adjacent = NULL;
    int w = ALTSCREEN_DEFAULT_W;
    int h = ALTSCREEN_DEFAULT_H;

    alt = new_dict();
    view_areas = make_view_areas(w, h);
    adjacent = new_array();
    if (alt == NULL || view_areas == NULL || adjacent == NULL)
        goto fail;

    if (!set_string(alt, "uuid", ALTSCREEN_UUID) ||
        !set_i64(alt, "features", 0) ||
        !set_i64(alt, "primaryInputDevice", 0) ||
        !set_i64(alt, "maxFPS", ALTSCREEN_DEFAULT_FPS) ||
        !set_i64(alt, "widthPixels", w) ||
        !set_i64(alt, "heightPixels", h) ||
        !set_i64(alt, "widthPhysical", 0) ||
        !set_i64(alt, "heightPhysical", 0) ||
        !set_i64(alt, "initialViewArea", 0) ||
        !set_obj(alt, "adjacentViewAreas", adjacent) ||
        !set_obj(alt, "viewAreas", view_areas) ||
        !set_i64(alt, "type", AIRPLAY_STREAM_TYPE_ALT_SCREEN) ||
        !set_bool(alt, "showsInstruments", 1) ||
        !set_string(alt, "initialURL", ALTSCREEN_INITIAL_URL))
        goto fail;

    g_cf.release(adjacent);
    g_cf.release(view_areas);
    return alt;

fail:
    if (adjacent != NULL) g_cf.release(adjacent);
    if (view_areas != NULL) g_cf.release(view_areas);
    if (alt != NULL) g_cf.release(alt);
    return NULL;
}

static cf_ref add_alt_display(cf_ref stock)
{
    cf_ref out = NULL;
    cf_ref alt = NULL;
    cf_index count;

    if (!is_array(stock)) {
        LOG_ERROR(ALTINFO_MODULE,
                  "refused property=displays reason=stock_not_array primary_untouched=1");
        return NULL;
    }

    count = g_cf.array_count(stock);
    if (count < 1 || !is_dict((cf_ref)g_cf.array_value(stock, 0))) {
        LOG_ERROR(ALTINFO_MODULE,
                  "refused property=displays reason=no_main_display primary_untouched=1");
        return NULL;
    }

    out = g_cf.array_mutable_copy(NULL, count + 1, stock);
    alt = make_alt_display();
    if (out == NULL || alt == NULL) {
        if (out != NULL) g_cf.release(out);
        if (alt != NULL) g_cf.release(alt);
        return NULL;
    }

    g_cf.array_append(out, alt);
    g_cf.release(alt);

    if (!g_info_logged) {
        g_info_logged = 1;
        LOG_WARN(ALTINFO_MODULE,
                 "display advertised uuid=%s geometry=%dx%d fps=%d primary_displays=%ld",
                 ALTSCREEN_UUID, ALTSCREEN_DEFAULT_W, ALTSCREEN_DEFAULT_H,
                 ALTSCREEN_DEFAULT_FPS, (long)count);
    }
    return out;
}

static cf_ref make_cluster_urls(void)
{
    cf_ref out = new_array();
    cf_ref s = NULL;
    const char *urls[3] = {
        ALTSCREEN_INITIAL_URL,
        ALTSCREEN_URL_CARD,
        ALTSCREEN_URL_BASE
    };
    int i;

    if (out == NULL)
        return NULL;
    for (i = 0; i < 3; ++i) {
        s = cfs(urls[i]);
        if (s == NULL) {
            g_cf.release(out);
            return NULL;
        }
        g_cf.array_append(out, s);
        g_cf.release(s);
    }
    return out;
}

static cf_ref patch_property_result(cf_ref stock, cf_ref property)
{
    cf_ref patched;

    if (!altscreen_enabled() || !g_cf.ready)
        return stock;

    if (property_is(property, "displays")) {
        patched = add_alt_display(stock);
        if (patched != NULL) {
            if (stock != NULL) g_cf.release(stock);
            return patched;
        }
        return stock;
    }

    if (property_is(property, "altScreenURLs") ||
        property_is(property, "altScreenSuggestUIURLs")) {
        patched = make_cluster_urls();
        if (patched != NULL) {
            if (stock != NULL) g_cf.release(stock);
            LOG_INFO(ALTINFO_MODULE, "cluster URL capability returned");
            return patched;
        }
    }
    return stock;
}

cf_ref AirPlayReceiverServerPlatformCopyProperty(cf_ref server,
                                                  uint32_t flags,
                                                  cf_ref property,
                                                  cf_ref qualifier,
                                                  os_status *out_err)
{
    cf_ref stock;

    resolve_cf();
    if (g_cf.server_copy == NULL)
        return NULL;
    stock = g_cf.server_copy(server, flags, property, qualifier, out_err);
    return patch_property_result(stock, property);
}

cf_ref AirPlayReceiverSessionPlatformCopyProperty(cf_ref session,
                                                   uint32_t flags,
                                                   cf_ref property,
                                                   cf_ref qualifier,
                                                   os_status *out_err)
{
    cf_ref stock;

    resolve_cf();
    if (g_cf.session_copy == NULL)
        return NULL;
    stock = g_cf.session_copy(session, flags, property, qualifier, out_err);
    return patch_property_result(stock, property);
}
