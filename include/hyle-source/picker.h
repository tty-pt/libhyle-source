#ifndef HYLE_SOURCE_PICKER_H
#define HYLE_SOURCE_PICKER_H

#include <stddef.h>

#define HYLE_PICKER_MAX_OPTS 64
#define HYLE_PICKER_MAX_SEL 64
#define HYLE_PICKER_MAX_FIELDS 8
#define HYLE_PICKER_QS_BUDGET 2048

typedef struct {
	const char *id;
	const char *label;
} hyle_option_t;

typedef struct {
	const char *key;         /* form field name */
	const char *label;       /* human label */
	const char *source;      /* target dataset id, e.g. "grp.items" */
	int multi;               /* 0 = radio (single), 1 = checkbox (multi) */
	const char *get_form_id; /* sibling GET form id */
	const char *url_tmpl;    /* fragment URL with {q},{page},{sel} slots */
	const hyle_option_t *page_opts; int npage; /* current page (borrowed) */
	const hyle_option_t *sel;       int nsel;  /* pinned selections */
	const char *q;
	int page, per_page, total;
	const char *search_param;/* custom search input name; NULL defaults to "pick_q_<key>" */
	const char *page_param;  /* custom page input name; NULL defaults to "pick_page_<key>" */
	int allow_add;           /* 1 = inline record creation enabled (catalog/tag datasets), 0 = selection only */
} hyle_picker_desc_t;

typedef struct {
	const char *key;
	const char *label;
	const char *target;
	int multi;
	const char *search_param;
	const char *page_param;
	const hyle_option_t *page_opts; int npage;
	const hyle_option_t *sel;       int nsel;
	const char *q;
	int page, per_page, total;
	int allow_add;
} hyle_picker_entry_t;

typedef struct {
	int n;
	hyle_picker_entry_t entries[HYLE_PICKER_MAX_FIELDS];
} hyle_picker_view_t;

typedef struct {
	hyle_option_t opts[HYLE_PICKER_MAX_OPTS];
	hyle_option_t sel[HYLE_PICKER_MAX_OPTS];
	char opt_ids[HYLE_PICKER_MAX_OPTS][128];
	char opt_labels[HYLE_PICKER_MAX_OPTS][256];
	char sel_ids[HYLE_PICKER_MAX_OPTS][128];
	char sel_labels[HYLE_PICKER_MAX_OPTS][256];
} hyle_picker_buffer_t;

#endif