#ifndef HYLE_SOURCE_PICKER_H
#define HYLE_SOURCE_PICKER_H

/**
 * @file picker.h
 * @brief Picker view descriptors for selection widgets.
 *
 * Describes single/multi selection over a target dataset with search,
 * pagination and inline-add support, consumed by SSR forms and WASM.
 */

#include <stddef.h>

/** @brief Maximum selectable option count per page. */
#define HYLE_PICKER_MAX_OPTS 64
/** @brief Maximum pinned selection count. */
#define HYLE_PICKER_MAX_SEL 64
/** @brief Maximum picker fields per view. */
#define HYLE_PICKER_MAX_FIELDS 8
/** @brief Query-string budget for picker search requests. */
#define HYLE_PICKER_QS_BUDGET 2048

/**
 * @brief One selectable option.
 */
typedef struct {
	/** Option id. */
	const char *id;
	/** Option display label. */
	const char *label;
} hyle_option_t;

/**
 * @brief Descriptor of one picker field.
 */
typedef struct {
	/** Form field name. */
	const char *key;
	/** Human label. */
	const char *label;
	/** Target dataset id, e.g. "grp.items". */
	const char *source;
	/** 0 = radio (single), 1 = checkbox (multi). */
	int multi;
	/** Sibling GET form id. */
	const char *get_form_id;
	/** Fragment URL with {q},{page},{sel} slots. */
	const char *url_tmpl;
	/** Current page options (borrowed). */
	const hyle_option_t *page_opts; int npage;
	/** Pinned selections (borrowed). */
	const hyle_option_t *sel;       int nsel;
	/** Current search term. */
	const char *q;
	/** Pagination state. */
	int page, per_page, total;
	/** Custom search input name; NULL defaults to "pick_q_<key>". */
	const char *search_param;
	/** Custom page input name; NULL defaults to "pick_page_<key>". */
	const char *page_param;
	/** 1 = inline record creation enabled, 0 = selection only. */
	int allow_add;
} hyle_picker_desc_t;

/**
 * @brief Compact picker field entry used by multi-field views.
 */
typedef struct {
	/** Form field name. */
	const char *key;
	/** Human label. */
	const char *label;
	/** Target dataset id. */
	const char *target;
	/** 0 = single, 1 = multi. */
	int multi;
	/** Custom search input name, or NULL. */
	const char *search_param;
	/** Custom page input name, or NULL. */
	const char *page_param;
	/** Current page options (borrowed). */
	const hyle_option_t *page_opts; int npage;
	/** Pinned selections (borrowed). */
	const hyle_option_t *sel;       int nsel;
	/** Current search term. */
	const char *q;
	/** Pagination state. */
	int page, per_page, total;
	/** Inline-add enabled flag. */
	int allow_add;
} hyle_picker_entry_t;

/**
 * @brief A picker view over up to HYLE_PICKER_MAX_FIELDS fields.
 */
typedef struct {
	/** Entry count. */
	int n;
	/** Field entries. */
	hyle_picker_entry_t entries[HYLE_PICKER_MAX_FIELDS];
} hyle_picker_view_t;

/**
 * @brief Scratch buffers backing a picker render.
 */
typedef struct {
	/** Option id/label storage. */
	hyle_option_t opts[HYLE_PICKER_MAX_OPTS];
	/** Selection id/label storage. */
	hyle_option_t sel[HYLE_PICKER_MAX_OPTS];
	/** Option id scratch. */
	char opt_ids[HYLE_PICKER_MAX_OPTS][128];
	/** Option label scratch. */
	char opt_labels[HYLE_PICKER_MAX_OPTS][256];
	/** Selection id scratch. */
	char sel_ids[HYLE_PICKER_MAX_OPTS][128];
	/** Selection label scratch. */
	char sel_labels[HYLE_PICKER_MAX_OPTS][256];
} hyle_picker_buffer_t;

#endif