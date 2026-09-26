#ifndef K2161_ALTSCREEN_K2161_H
#define K2161_ALTSCREEN_K2161_H

#include <stdint.h>

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

/*
 * Stock main110 StartSession provides the application delegate context that
 * ScreenStreamSetDelegateContext receives.  We remember it rather than
 * guessing an AirPlayReceiverSessionPrivate structure offset.
 */
void *k2161_altscreen_main_delegate_context(void);

/*
 * Stock main110 installs a copied-by-value three-word time synchronizer on its
 * SessionScreen before processing frames.  Capture that exact object and
 * install the same callbacks/context on private111.
 */
int k2161_altscreen_time_sync_ready(void);
int k2161_altscreen_apply_main_time_sync(void *screen_session);

/*
 * AirPlay_DeriveAESKeySHA512ForScreen is observed during the stock main110
 * setup.  The captured screen master key is then reused through the stock
 * derivation helper for the independent type111 streamConnectionID.
 */
int k2161_altscreen_private_aes_ready(void);
int k2161_altscreen_derive_private_aes(uint64_t stream_connection_id,
                                        uint8_t out_key[16],
                                        uint8_t out_iv[16]);

/* Diagnostics. */
int k2161_altscreen_native58_ready(void);
unsigned int k2161_altscreen_nvss_patch_count(void);

#ifdef __cplusplus
}
#endif

#endif /* K2161_ALTSCREEN_K2161_H */
