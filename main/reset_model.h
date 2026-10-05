#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RESET_DAYS 28
#define RESET_TEXT 512
#define RESET_CACHE_VERSION 1
/* No platform or LVGL dependencies: also exercised by host tests. */
typedef enum { DAY_UNKNOWN, DAY_UPCOMING, DAY_OPEN, DAY_IMPROVEMENT, DAY_RESET, DAY_BOTH, DAY_CHECKING, DAY_CLOSED } day_state_t;
typedef struct {
    int64_t reset_at, fetched_at;
    uint32_t total;
    float average_days;
    bool valid, banked, scheduled;
    char text[RESET_TEXT];
} reset_status_t;
typedef struct {
    day_state_t days[RESET_DAYS];
    int64_t start_day, fetched_at;
    bool valid;
} challenge_t;
typedef struct {
    char tag[768];
    size_t length;
    unsigned count;
    bool in_tag, overflow, invalid, saw_start;
    challenge_t value;
} challenge_parser_t;
typedef struct { unsigned page, selected_day; bool detail; unsigned scroll; } reset_nav_t;
typedef enum { KEY_UP, KEY_DOWN, KEY_OK, KEY_BACK } reset_key_t;
int64_t reset_parse_iso(const char *s);
int64_t reset_civil_day(int year, unsigned month, unsigned day);
int reset_challenge_day(const challenge_t *c, int64_t now);
int reset_pacific_offset(int64_t now);
void reset_navigation(reset_nav_t *n, reset_key_t key);
void challenge_parser_init(challenge_parser_t *p);
void challenge_parser_feed(challenge_parser_t *p, const char *data, size_t length);
bool challenge_parser_finish(challenge_parser_t *p, challenge_t *out);
const char *reset_day_label(day_state_t state);
bool reset_font_codepoint(uint32_t cp);
void reset_copy_text(char *dst, size_t capacity, const char *src);
