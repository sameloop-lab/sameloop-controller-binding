# SameLoop AFB API contract — native schema sameloop/1

Endpoint: `http://127.0.0.1:1234/api` or `ws://127.0.0.1:1234/api`.
WebSocket subprotocol: `x-afb-ws-json1`.
AFB replies wrap the following application payload in their `response` field.

## Public API: sameloop-api

| Verb | JSON argument | Result |
|---|---|---|
| get | `{}` | State snapshot |
| settings | `{"mode":"AUTO","setpoint":25}` | Accepted settings and current snapshot |
| subscribe | `{}` | Snapshot, then events `sameloop-controller/state` |
| unsubscribe | `{}` | Empty object; stop events for this client |
| fault | see below | Sensor snapshot; only when test_controls=true |

`settings` accepts either or both fields. OFF/AUTO are case-sensitive. Setpoint
must be numeric, finite and in [15,40]. Empty objects, unknown fields, wrong types
and out-of-range values are rejected atomically, with no partial setting update.
There is no direct fan command in the public interface.

All failures use a negative AFB status with a JSON `{ "error": "reason" }`
payload. Successful statuses are zero. AFB's transport-level error description
may vary by binder version; callers must use the failure status, not its wording.

Snapshot example (illustrative, not a measured sample):

```json
{
  "schema": "sameloop/1",
  "stack": "redpesk-afb",
  "sequence": 100,
  "monotonic_s": 1234.5,
  "temperature": {"value": 28.2, "available": true, "monotonic_s": 1234.45, "sequence": 100},
  "fan": {"requested": 64, "applied": 62.7, "rpm": 2490, "available": true},
  "controller": {"mode": "AUTO", "setpoint": 25},
  "alarms": {"sensor_unavailable": false, "fan_unavailable": false,
             "overtemperature": false, "overtemperature_quality": "available"}
}
```

Unknown numeric values are `null`. A stale temperature can retain its last
numeric value but has `available:false`; always inspect availability/quality.
Sequences count updates; timestamps are host-local monotonic seconds, not UTC.
The controller's availability is updated each controller iteration; a blocked
controller must additionally be detected by clients through event freshness.

## Private APIs

- `sameloop-sensor/read {}` returns `value`, `valid`, `paused`, `monotonic_s`,
  `sequence`, `heat`. Until its first sample the sensor is invalid.
- `sameloop-sensor/fault` accepts any nonempty combination of `valid` (bool),
  `paused` (bool), `temperature` (-40..150 °C), `heat` (0..2 °C/s).
  Values are validated before any mutation. Injected temperature appears on the
  next nonpaused sample; a fault reply is not evidence of a new measurement.
- `sameloop-fan/read {}` returns `requested`, `applied`, `rpm`, `monotonic_s`,
  `watchdog_expired`.
- `sameloop-fan/apply {"duty":50}` accepts finite numeric duty [0,100], refreshes
  the command watchdog, and returns current feedback.
- `sameloop-controller` exposes get/settings/subscribe/unsubscribe as above.

Fault examples through the opt-in public facade:

```json
{"valid": false}
{"valid": true, "paused": false, "heat": 0.18}
{"paused": true}
{"temperature": 55, "heat": 1.2}
```

## VSS relationship

`temperature.value` -> Vehicle.SameLoop.Temperature.Value;
`temperature.available` includes validity AND freshness, whereas
Temperature.IsValid alone represents validity.
`fan.applied` -> Fan.DutyCycle (round to uint8 in a VSS adapter);
`fan.rpm` -> Fan.Speed (round to uint16);
`controller.setpoint/mode` -> Controller.Setpoint/Mode;
`alarms.overtemperature/sensor_unavailable` -> corresponding Alarm signals.
Requested duty, quality and freshness are native metadata/extensions, not extra
standard VSS claims. This delivery does not implement VSS, CAN/DBC or KUKSA.
