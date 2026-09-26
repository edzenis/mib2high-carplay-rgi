#ifndef K2161_ALTSCREEN_AIRPLAY_H
#define K2161_ALTSCREEN_AIRPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental path is intentionally opt-in on the unit. */
int k2161_altscreen_enabled(void);
int k2161_altscreen_info_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* K2161_ALTSCREEN_AIRPLAY_H */
