// SPDX-License-Identifier: MIT
// Deliberately vendored in each independent project; keep copies identical.
#ifndef SAMELOOP_COMMON_H
#define SAMELOOP_COMMON_H
#define AFB_BINDING_VERSION 4
#define AFB_BINDING_X4R1_ITF_REVISION 7
#include <afb/afb-binding.h>
#include <json-c/json.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static inline double sl_now(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return 0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
static inline double sl_clamp(double x, double lo, double hi) {
    return fmax(lo, fmin(hi, x));
}
static inline void sl_json_free(void *p) { json_object_put(p); }
/* Takes ownership of j on both success and failure. */
static inline int sl_data(afb_data_t *out, json_object *j) {
    int rc = afb_create_data_raw(out, AFB_PREDEFINED_TYPE_JSON_C,
                               j, 0, sl_json_free, j);
    if (rc < 0) json_object_put(j);
    return rc;
}
static inline void sl_reply(afb_req_t req, int status, json_object *j) {
    afb_data_t d;
    if (sl_data(&d, j) < 0) { afb_req_reply(req, -ENOMEM, 0, NULL); return; }
    afb_req_reply(req, status, 1, &d);
}
static inline void sl_error(afb_req_t req, int status, const char *message) {
    json_object *j = json_object_new_object();
    json_object_object_add(j, "error", json_object_new_string(message));
    sl_reply(req, status, j);
}
/* Returns an owned JSON reference, independent from AFB parameter lifetime. */
static inline json_object *sl_args(unsigned n, afb_data_t const p[]) {
    afb_data_t d;
    if (n != 1 || afb_data_convert(p[0], AFB_PREDEFINED_TYPE_JSON_C, &d) < 0)
        return NULL;
    json_object *j = json_object_get((json_object *)afb_data_ro_pointer(d));
    afb_data_unref(d);
    if (!json_object_is_type(j, json_type_object)) { json_object_put(j); return NULL; }
    return j;
}
static inline bool sl_empty(unsigned n, afb_data_t const p[]) {
    if (!n) return true;
    json_object *j = sl_args(n, p);
    bool ok = j && json_object_object_length(j) == 0;
    json_object_put(j);
    return ok;
}
static inline bool sl_number(json_object *j, const char *key, double lo, double hi, double *v) {
    json_object *x;
    if (!json_object_object_get_ex(j, key, &x) ||
        !(json_object_is_type(x, json_type_double) || json_object_is_type(x, json_type_int)))
        return false;
    double value = json_object_get_double(x);
    if (!isfinite(value) || value < lo || value > hi) return false;
    *v = value; return true;
}
static inline json_object *sl_get(json_object *j, const char *k) {
    json_object *v = NULL;
    if (j) json_object_object_get_ex(j, k, &v);
    return v;
}
static inline void sl_double(json_object *j, const char *k, double v) {
    json_object_object_add(j, k, isfinite(v) ? json_object_new_double(v) : NULL);
}
static inline void sl_bool(json_object *j, const char *k, bool v) {
    json_object_object_add(j, k, json_object_new_boolean(v));
}
static inline void sl_string(json_object *j, const char *k, const char *v) {
    json_object_object_add(j, k, json_object_new_string(v));
}
/* Local acyclic calls only; callers must not hold their state mutex here. */
static inline json_object *sl_call(afb_api_t api, const char *target, const char *verb, json_object *arg) {
    afb_data_t in, out[1], converted;
    unsigned count = 1;
    int status = -EIO;
    if (sl_data(&in, arg) < 0) return NULL;
    int rc = afb_api_call_sync(api, target, verb, 1, &in, &status, &count, out);
    json_object *j = NULL;
    if (rc >= 0 && status >= 0 && count == 1 &&
        afb_data_convert(out[0], AFB_PREDEFINED_TYPE_JSON_C, &converted) >= 0) {
        j = json_object_get((json_object *)afb_data_ro_pointer(converted));
        afb_data_unref(converted);
    }
    if (rc >= 0) for (unsigned i = 0; i < count; i++) afb_data_unref(out[i]);
    return j;
}
static inline void sl_push(afb_event_t event, json_object *j) {
    afb_data_t d;
    if (sl_data(&d, j) >= 0) afb_event_push(event, 1, &d);
}
#endif
