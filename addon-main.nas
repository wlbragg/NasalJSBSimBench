#Nasal JSBSim Benchmark.
#Written and developed by Wayne Bragg (wlbragg(at)cox.net)
#Copyright (C) 2021 Wayne Bragg (wlbragg(at)cox.net)
#addon-main.nas
#Version 1.0.0 02/06/2026
#This file is licensed under the GPL license version 2 or later.

var main = func(addon) {

	print("Nasal JSBSim Benchmark addon initialized from path ", addon.basePath);
    logprint(LOG_INFO, "Nasal JSBSim Benchmark ", addon.basePath);

    # initialization
    setlistener("/sim/signals/fdm-initialized", func {

        foreach(var script; ['Nasal/benchmark.nas']) {
            var fname = addon.basePath ~ "/" ~ script;

			print("Nasal file loaded " ~ fname);
            io.load_nasal(fname, "nasaljsbsimbench");

        }

    }, 0, 1);

}
