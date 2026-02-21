# High-level Nasal wrapper for JSBSim engine

var jsbsim = {
    create: func(model, name=nil) {
        return cppcall("jsbsim", "create", model, name);
    },

    destroy: func(id) {
        cppcall("jsbsim", "destroy", id);
    },

    set: func(id, path, value) {
        cppcall("jsbsim", "set", id, path, value);
    },

    get: func(id, path) {
        return cppcall("jsbsim", "get", id, path);
    },

    exists: func(id) {
        return cppcall("jsbsim", "exists", id);
    }
};
