/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_ATHEROS_ATH10K_WOW_H_
#define SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_ATHEROS_ATH10K_WOW_H_

struct ath10k_wow {
    uint32_t max_num_patterns;
    sync_completion_t wakeup_completed;
#if 0   // NEEDS PORTING
    struct wiphy_wowlan_support wowlan_support;
#endif  // NEEDS PORTING
};

#ifdef CONFIG_PM

int ath10k_wow_init(struct ath10k* ar);
#if 0   // NEEDS PORTING
int ath10k_wow_op_suspend(struct ieee80211_hw* hw,
                          struct cfg80211_wowlan* wowlan);
int ath10k_wow_op_resume(struct ieee80211_hw* hw);
#endif  // NEEDS PORTING

#else

static inline int ath10k_wow_init(struct ath10k* ar) {
    return 0;
}

#endif  /* CONFIG_PM */
#endif  // SRC_CONNECTIVITY_WLAN_DRIVERS_THIRD_PARTY_ATHEROS_ATH10K_WOW_H_
