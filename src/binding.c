// SPDX-License-Identifier: MIT
#include "sl-common.h"
#include "model.h"
static afb_api_t self;
static afb_event_t changed;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool stopping;
static atomic_int pending;
static bool automatic, available, overtemperature, fan_ok;
static double setpoint=25, temperature=NAN, sample_time, requested;
static double applied=NAN, rpm=NAN;
static uint64_t generation, sample_sequence;
static json_object *snapshot_locked(void) {
    json_object *j=json_object_new_object(), *t=json_object_new_object();
    json_object *f=json_object_new_object(), *c=json_object_new_object(), *a=json_object_new_object();
    sl_string(j,"schema","sameloop/1"); sl_string(j,"stack","redpesk-afb");
    json_object_object_add(j,"sequence",json_object_new_uint64(generation));
    sl_double(j,"monotonic_s",sl_now());
    sl_double(t,"value",temperature); sl_bool(t,"available",available);
    sl_double(t,"monotonic_s",sample_time);
    json_object_object_add(t,"sequence",json_object_new_uint64(sample_sequence));
    sl_double(f,"requested",requested); sl_double(f,"applied",applied);
    sl_double(f,"rpm",rpm); sl_bool(f,"available",fan_ok);
    sl_string(c,"mode",automatic ? "AUTO" : "OFF"); sl_double(c,"setpoint",setpoint);
    sl_bool(a,"sensor_unavailable",!available); sl_bool(a,"fan_unavailable",!fan_ok);
    sl_bool(a,"overtemperature",overtemperature);
    sl_string(a,"overtemperature_quality",available ? "available" : "unavailable");
    json_object_object_add(j,"temperature",t); json_object_object_add(j,"fan",f);
    json_object_object_add(j,"controller",c); json_object_object_add(j,"alarms",a);
    return j;
}
static void tick(int signal, void *arg) {
    (void)arg;
    if(signal || atomic_load(&stopping)) return;
    json_object *s=sl_call(self,"sameloop-sensor","read",json_object_new_object());
    double value=NAN, stamp=0;
    bool parsed=s && sl_number(s,"value",-40,150,&value) &&
                sl_number(s,"monotonic_s",0,1e15,&stamp);
    bool fresh=parsed && sample_available(json_object_get_boolean(sl_get(s,"valid")),value,stamp,sl_now());
    uint64_t seq=s ? json_object_get_uint64(sl_get(s,"sequence")) : 0;
    json_object_put(s);
    pthread_mutex_lock(&lock);
    bool old_available=available, old_overtemperature=overtemperature;
    available=fresh; temperature=value; sample_time=stamp; sample_sequence=seq;
    if(fresh) overtemperature=value > 50;
    requested=control_duty(automatic,fresh,value,setpoint);
    double duty=requested;
    pthread_mutex_unlock(&lock);
    if(old_available != fresh) AFB_API_NOTICE(self,"Sensor available=%d",fresh);
    json_object *command=json_object_new_object(); sl_double(command,"duty",duty);
    json_object *f=sl_call(self,"sameloop-fan","apply",command);
    double actual=NAN, speed=NAN;
    bool ok=f && sl_number(f,"applied",0,100,&actual) && sl_number(f,"rpm",0,10000,&speed);
    json_object_put(f);
    pthread_mutex_lock(&lock);
    bool old_fan=fan_ok, new_overtemperature=overtemperature;
    fan_ok=ok; applied=ok ? actual : NAN; rpm=ok ? speed : NAN; generation++;
    json_object *out=snapshot_locked();
    pthread_mutex_unlock(&lock);
    if(old_fan != ok) AFB_API_NOTICE(self,"Fan available=%d",ok);
    if(old_overtemperature != new_overtemperature)
        AFB_API_NOTICE(self,"Overtemperature=%d",new_overtemperature);
    sl_push(changed,out);
    if(!atomic_load(&stopping)) {
        int job=afb_job_post(100,0,tick,NULL,&self); atomic_store(&pending,job);
        if(job < 0) AFB_API_ERROR(self,"Cannot reschedule controller: %d",job);
    }
}
static void get_state(afb_req_t req, unsigned n, afb_data_t const p[]) {
    if(!sl_empty(n,p)) { sl_error(req,-EINVAL,"get expects an empty object"); return; }
    pthread_mutex_lock(&lock); json_object *j=snapshot_locked(); pthread_mutex_unlock(&lock);
    sl_reply(req,0,j);
}
static void settings(afb_req_t req, unsigned n, afb_data_t const p[]) {
    json_object *j=sl_args(n,p); double next=0; bool have_setpoint=false, have_mode=false, mode=false;
    if(!j || json_object_object_length(j)==0) goto invalid;
    json_object_object_foreach(j,key,value) {
        if(!strcmp(key,"setpoint")) {
            if(!sl_number(j,key,15,40,&next)) goto invalid;
            have_setpoint=true;
        } else if(!strcmp(key,"mode") && json_object_is_type(value,json_type_string)) {
            const char *m=json_object_get_string(value);
            if(strcmp(m,"AUTO") && strcmp(m,"OFF")) goto invalid;
            mode=!strcmp(m,"AUTO"); have_mode=true;
        } else goto invalid;
    }
    pthread_mutex_lock(&lock);
    if(have_setpoint) setpoint=next;
    if(have_mode) automatic=mode;
    json_object *out=snapshot_locked();
    pthread_mutex_unlock(&lock);
    AFB_API_NOTICE(self,"Settings accepted: %s",json_object_to_json_string(j));
    json_object_put(j); sl_reply(req,0,out); return;
invalid:
    json_object_put(j); sl_error(req,-EINVAL,"expected mode OFF/AUTO and/or finite setpoint 15..40; no other fields");
}
static void subscribe(afb_req_t req, unsigned n, afb_data_t const p[]) {
    if(!sl_empty(n,p)) { sl_error(req,-EINVAL,"subscribe expects an empty object"); return; }
    if(afb_req_subscribe(req,changed)<0) { sl_error(req,-EIO,"subscription failed"); return; }
    get_state(req,n,p);
}
static void unsubscribe(afb_req_t req, unsigned n, afb_data_t const p[]) {
    if(!sl_empty(n,p)) { sl_error(req,-EINVAL,"unsubscribe expects an empty object"); return; }
    if(afb_req_unsubscribe(req,changed)<0) { sl_error(req,-EINVAL,"not subscribed"); return; }
    sl_reply(req,0,json_object_new_object());
}
static int ctl(afb_api_t api, afb_ctlid_t id, afb_ctlarg_t arg, void *data) {
    (void)arg; (void)data;
    if(id==afb_ctlid_Init) {
        self=api;
        int rc=afb_api_new_event(api,"state",&changed); if(rc<0) return rc;
        int job=afb_job_post(100,0,tick,NULL,&self); atomic_store(&pending,job);
        return job<0 ? job : 0;
    }
    if(id==afb_ctlid_Exiting) {
        atomic_store(&stopping,true);
        int job=atomic_load(&pending); if(job>0) afb_job_abort(job);
        /* Events are process-lifetime objects; no dlclose/hot unload supported. */
    }
    return 0;
}
static const afb_verb_t verbs[]={
    {.verb="get", .callback=get_state}, {.verb="settings", .callback=settings},
    {.verb="subscribe", .callback=subscribe}, {.verb="unsubscribe", .callback=unsubscribe}, {0}
};
const afb_binding_t afbBindingV4={
    .api="sameloop-controller", .verbs=verbs, .mainctl=ctl,
    .require_api="sameloop-sensor sameloop-fan", .noconcurrency=1,
    .info="SameLoop control loop and state events"
};
