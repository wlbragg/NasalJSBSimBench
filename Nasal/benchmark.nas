print("BENCHMARK NASAL LOADED");

# ============================================================
# CONFIGURATION
# ============================================================

var N_MATH_ITERS = 1000;
var N_PROP_ITERS = 200;
var HISTORY_SIZE = 512;

var bench_ns = "/sim/benchmark/nasal/";

var on=0;
var bench_timer = maketimer(0.0, func{
    var running = getprop("/addons/by-id/org.flightgear.addons.NasalJSBSimBench/addon-devel/run");

    if (running) {
        bench_frame(0);
        if (on == 0) enableOSD();
        on = 2;
    } else on = 0;
});

# ============================================================
# PROPERTY NODES
# ============================================================

var nodes = {
    time:        props.globals.getNode(bench_ns ~ "time", 1),
    nasal_time:  props.globals.getNode(bench_ns ~ "nasal-time", 1),
    jsb_time:    props.globals.getNode(bench_ns ~ "jsb-time", 1),

    time_max:    props.globals.getNode(bench_ns ~ "time-max", 1),
    time_min:    props.globals.getNode(bench_ns ~ "time-min", 1),
    jitter:      props.globals.getNode(bench_ns ~ "jitter", 1),
    gc_spikes:   props.globals.getNode(bench_ns ~ "gc-spikes", 1),

    math_out:    props.globals.getNode(bench_ns ~ "math-output", 1),
    pid_out:     props.globals.getNode(bench_ns ~ "pid-output", 1),
    filter_out:  props.globals.getNode(bench_ns ~ "filter-output", 1),
};

# JSBSim inputs

var jsb_input = props.globals.getNode("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/input", 1);
var jsb_target = props.globals.getNode("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/target", 1);

if (jsb_input.getValue() == nil)  jsb_input.setValue(0.0);
if (jsb_target.getValue() == nil) jsb_target.setValue(1.0);

# Initialize timing nodes
nodes.time_min.setValue(999999.0);
nodes.time_max.setValue(0.0);
nodes.jitter.setValue(0.0);
nodes.gc_spikes.setValue(0);

# ============================================================
# FIXED-SIZE HISTORY ARRAY (USING GLOBAL append())
# ============================================================

var time_history = [];
var time_history_idx = 0;

for (var i = 0; i < HISTORY_SIZE; i += 1)
    append(time_history, nil);

# ============================================================
# PID + FILTER IMPLEMENTATION
# ============================================================

var pid_state    = { integ: 0.0, prev_err: 0.0 };
var filter_state = { y: 0.0 };

var KP  = 2.0;
var KI  = 0.5;
var KD  = 0.1;
var DT  = 0.01;
var TAU = 0.2;

var clamp = func(v, lo, hi) {
    return v < lo ? lo : (v > hi ? hi : v);
};

var pid_step = func(target, input) {
    var err = target - input;
    pid_state.integ += err * DT;
    var deriv = (err - pid_state.prev_err) / DT;
    pid_state.prev_err = err;
    return clamp(KP * err + KI * pid_state.integ + KD * deriv, -10, 10);
};

var filter_step = func(u) {
    var alpha = DT / (TAU + DT);
    filter_state.y += alpha * (u - filter_state.y);
    return filter_state.y;
};

# ============================================================
# MEDIAN FUNCTION (NO %)
# ============================================================

var median = func(arr) {

    var tmp = [];

    # Copy only valid numeric values
    foreach (v; arr) {
        if (v != nil and typeof(v) == "number")
            append(tmp, v);
    }

    var n = size(tmp);
    if (n == 0) return nil;

    # Now tmp is guaranteed to be a pure Nasal array of numbers
    tmp.sort();

    # Odd length
    if (n == int(n/2)*2 + 1)
        return tmp[(n-1)/2];

    # Even length
    var a = tmp[n/2 - 1];
    var b = tmp[n/2];
    return 0.5 * (a + b);
};

# ============================================================
# MAIN BENCHMARK FRAME
# ============================================================

var bench_frame = func(dt) {

    var t_total0 = systime();
    var t_nasal0 = systime();

    var x = jsb_input.getValue();
    if (x == nil) x = 0.0;

    var acc = 0.0;

    for (var i = 0; i < N_MATH_ITERS; i += 1) {
        var v = math.sin(x * 0.123 + x * x * 0.987)
              + 0.5 * math.sin(x * 1.234)
              + 0.25 * (x * 0.777 + 0.333)
              + (x * x * 0.111 - 0.222 * x);
        acc += v;
        x += 0.0001;
    }
    nodes.math_out.setValue(acc);

    for (var j = 0; j < N_PROP_ITERS; j += 1) {
        var tmp = jsb_input.getValue();
        if (tmp == nil) tmp = 0.0;
        jsb_input.setValue(tmp + 0.00001);
    }

    var u_pid = pid_step(jsb_target.getValue(), jsb_input.getValue());
    var u_flt = filter_step(u_pid);

    nodes.pid_out.setValue(u_pid);
    nodes.filter_out.setValue(u_flt);

    var t_nasal1 = systime();
    nodes.nasal_time.setValue((t_nasal1 - t_nasal0) * 1000.0);

    var t_jsb0 = systime();

    var jsb_math = props.globals.getNode("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/math-output", 1).getValue() or 0;
    var jsb_pid  = props.globals.getNode("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/pid-output", 1).getValue() or 0;
    var jsb_filt = props.globals.getNode("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/filter-output", 1).getValue() or 0;

    var t_jsb1 = systime();
    nodes.jsb_time.setValue((t_jsb1 - t_jsb0) * 1000.0);

    var t_total1 = systime();
    var dt_ms = (t_total1 - t_total0) * 1000.0;
    nodes.time.setValue(dt_ms);

    # --------------------------------------------------------
    # JITTER + GC DETECTION (CIRCULAR BUFFER, NO .append())
    # --------------------------------------------------------

    time_history[time_history_idx] = dt_ms;

    time_history_idx += 1;
    if (time_history_idx >= HISTORY_SIZE)
        time_history_idx = 0;

    var tmin = nodes.time_min.getValue();
    var tmax = nodes.time_max.getValue();

    if (tmin == nil or tmin == 0 or dt_ms < tmin)
        nodes.time_min.setValue(dt_ms);

    if (tmax == nil or dt_ms > tmax)
        nodes.time_max.setValue(dt_ms);

    var median_time = median(time_history);
    if (median_time != nil) {

        nodes.jitter.setValue(nodes.time_max.getValue() - nodes.time_min.getValue());

        var spikes = nodes.gc_spikes.getValue();
        if (spikes == nil) spikes = 0;

        if (dt_ms > 2.0 * median_time)
            nodes.gc_spikes.setValue(spikes + 1);
    }
};

# On-screen displays
var enableOSD = func {
	var left  = screen.display.new(20, 10);
	var right = screen.display.new(-300, 10);

    
    left.add(bench_ns ~ "nasal-time");
    left.add(bench_ns ~ "math-output");
    left.add(bench_ns ~ "pid-output");
    left.add(bench_ns ~ "filter-output");
    left.add(bench_ns ~ "time");
    left.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/input");
    left.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/target");
    left.add(bench_ns ~ "jitter");
    left.add(bench_ns ~ "gc-spikes");
    left.add(bench_ns ~ "time-max");
    left.add(bench_ns ~ "time-min");

    right.add(bench_ns ~ "jsb-time");
    right.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/math-output");
    right.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/pid-output");
    right.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/fcs/benchmark/filter-output");
 	right.add(bench_ns ~ "time");
    right.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/input");
    right.add("/addons/org.flightgear.addons.NasalJSBSimBench/fdm/jsbsim/benchmark/target");
    right.add(bench_ns ~ "jitter");
    right.add(bench_ns ~ "gc-spikes");
    right.add(bench_ns ~ "time-max");
    right.add(bench_ns ~ "time-min");
}

setlistener("/sim/signals/fdm-initialized", func {
    bench_timer.start();
});
