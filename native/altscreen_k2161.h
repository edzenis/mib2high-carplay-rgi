#ifndef K2161_ALTSCREEN_K2161_H
#define K2161_ALTSCREEN_K2161_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * K2161 native AltScreen bridge.
 *
 * Stock K2161 ScreenStreamStart() opens NvSS with output byte 59.  The
 * experimental AltScreen path keeps the complete stock ScreenStream/NvSS
 * decoder path and changes only that output byte to 58, but only while an
 * explicitly registered private type-111 screen is being started.
 */

int k2161_altscreen_register_private_screen(void *screen_session);
void k2161_altscreen_unregister_private_screen(void *screen_session);
int k2161_altscreen_is_private_screen(void *screen_session);

/* Start a registered private screen using the stock K2161 implementation. */
int k2161_altscreen_private_start(void *screen_session, void *delegate_context);

/* True only on the thread currently starting an explicitly private screen. */
int k2161_altscreen_private_scope_active(void);

/* Diagnostics. */
int k2161_altscreen_native58_ready(void);
unsigned int k2161_altscreen_nvss_patch_count(void);

#ifdef __cplusplus
}
#endif

#endif /* K2161_ALTSCREEN_K2161_H */
