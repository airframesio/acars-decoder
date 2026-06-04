/* ads_escape_hatches.h — prototypes for per-plugin custom functions.
 *
 * Implementations live in each language repo's src/escape_hatches/
 * (the central runtime can't implement them — they're plugin-specific).
 *
 * Three hatch shapes:
 *
 *   Whole-plugin parse hatch: called from a generated plugin's decode():
 *     ads_decode_result_t *ads_hatch_<name>(const ads_message_t *msg,
 *                                           ads_decode_result_t *result,
 *                                           const ads_options_t *opts);
 *
 *   Field-level decode hatch: called from a generated field assignment:
 *     ads_value_t *ads_hatch_<name>(const char *value, const char *args_json);
 *     (or const ads_value_t * for value-from-prior-step variants)
 *
 *   Formatter hatch: called from a generated formatter item:
 *     void ads_hatch_<name>(ads_decode_result_t *result);
 */
#ifndef ADS_ESCAPE_HATCHES_H
#define ADS_ESCAPE_HATCHES_H

#include "ads_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Whole-plugin parse hatches (61 plugins) ────────────────────────────── */

ads_decode_result_t *ads_hatch_arinc_702_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_cband_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_colon_comma_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_10_ldr_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_10_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_12_n_space_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_12_pos_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_13_18_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_15_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_15_fst_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_16_autpos_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_16_honeywell_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_16_n_space_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_16_posa1_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_16_tod_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_1l_070_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_1l_3line_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_1l_660_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_1l_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_1m_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_20_cfb01_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_20_pos_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_21_pos_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_22_off_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_22_pos_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_24_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_2p_fm3_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_2p_fm4_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_2p_fm5_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_30_slash_ea_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_44_slash_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4a_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4a_01_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4a_dis_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4a_door_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4a_slash_01_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4n_decode(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4t_agfsr_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_4t_eta_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_58_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_5z_slash_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_80_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_83_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_8e_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_b6_forwardslash_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_atis_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_ezf_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_flr_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_m_pos_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_ofp_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_paren_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_starpos_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h1_wrn_parse(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_h2_02e_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_hx_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_ma_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_qp_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_qq_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_qr_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_qs_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);
ads_decode_result_t *ads_hatch_label_sq_dispatch(const ads_message_t *msg, ads_decode_result_t *result, const ads_options_t *opts);

/* ─── Field-level decode hatches ──────────────────────────────────────────── */

ads_value_t *ads_hatch_parse_flight_level_or_ground(const char *value, const char *args_json);
ads_value_t *ads_hatch_flight_level_to_altitude_feet(const ads_value_t *value, const char *args_json);
ads_value_t *ads_hatch_ohma_unwrap_message(const char *value, const char *args_json);

/* ─── Formatter-level hatches ─────────────────────────────────────────────── */

void ads_hatch_ohma_message_item(ads_decode_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* ADS_ESCAPE_HATCHES_H */
