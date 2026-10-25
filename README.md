# sameloop-controller-binding

**Bind & Seek — Same Control Loop, Different Embedded Stacks**

Version 0.1.0 — Control loop, settings, alarms and subscriptions. C11, AFB binding API v4, MIT.

## Build

Install a C compiler, Meson, Ninja, pkg-config, JSON-C development headers and
AFB binding development headers (>= 4.2.0). With redpesk repositories configured,
the usual RPM development packages are `afb-binding-devel` and `json-c-devel`.
The host also needs `afb-binder` to run the demo. Fedora alone does not provide
all redpesk components; install them from your configured redpesk build/repository.

```sh
meson setup build --prefix=/usr
meson compile -C build
meson test -C build --print-errorlogs
# Optional installation:
meson install -C build --destdir "$PWD/stage"
```

Build output: `build/sameloop-controller-binding.so`.
Installed module: `<libdir>/sameloop/sameloop-controller-binding.so`.
AFB modules resolve AFB calls through the binder interface table; they do not
link directly to libafb. JSON-C, pthread and libm are linked normally.

## API

API name: `sameloop-controller`. Verbs: **get, settings, subscribe, unsubscribe**.
Runtime dependencies: sameloop-sensor, sameloop-fan.

[docs/API.md](docs/API.md) specifies request and response schemas. For the
full demonstration keep all four repositories as siblings and run
`./scripts/build-all.sh` then `./scripts/run-demo.py` from their parent directory.
The API repository also includes these orchestration files in `demo/` so it can
coordinate independent sibling Git checkouts.

Only `sameloop-api` is public. Load the other three modules with the binder
`private:` prefix, as done by the launcher. Default exposure is localhost only.
This version does not distinguish viewer and operator credentials; production
role-based policies are a subsequent integration task.

## Design and limits

State is protected by a mutex. No state mutex is held during synchronous
cross-API calls. Background sensor/controller jobs are serialized per job group;
AFB callbacks retain or consume data references explicitly. Jobs run approximately
at 10 Hz; this is a desktop demo, not a hard real-time loop. Modules live until
process exit; hot unloading is not supported.

`src/sl-common.h` is an intentionally duplicated, MIT-licensed utility header so
each repository builds independently. It must remain identical across projects.
External AFB dependencies retain their upstream licenses and are not bundled.

No hardware driver, RPM spec or Yocto recipe is included in this first version.
