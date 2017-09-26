#!/usr/bin/env python
from __future__ import print_function


def _configure_lazy_loader_proxies():
    """
    Populate the REPL scope with lazy-loading proxies for all known modules.
    """
    import json
    import os
    import pydoc
    import sys

    class LazyLoadedModuleProxy(object):
        """
        Module proxy that delays the actual import until an attribute that does
        not start with "_" is retrieved or set or the "__dir__" method is
        invoked.
        """
        def __init__(self, name, parent=None):
            self._module = None
            self._name = name
            self._parent = parent or __import__(__name__)
            self._attribute_name = name.split(".")[-1]

        def __str__(self):
            return "<module '%s' from '???' (not yet loaded)>" % self._name

        def __repr__(self):
            return self.__str__()

        @property
        def _real_module(self):
            """
            If a module has not yet been loaded, import it and set itself as
            the attribute of its parent module or the REPL scope.

            Returns: The real module.
            """
            if not self._module:
                fromlist = "." in self._name
                self._module = __import__(self._name, fromlist=fromlist)
                setattr(self._parent, self._attribute_name, self._real_module)

            return self._module

        def __dir__(self):
            return dir(self._real_module)

        def __setattr__(self, name, value):
            if not name.startswith("_"):
                object.__setattr__(self._real_module, name, value)

            object.__setattr__(self, name, value)

        def __getattr__(self, name):
            if name.startswith("_"):
                return object.__getattribute__(self, name)

            module = self._name + "." + name

            if module in modules:
                descendent = LazyLoadedModuleProxy(module, self)
                setattr(self, name, descendent)
                return descendent

            return getattr(self._real_module, name)

    def help(*args, **kwargs):
        """
        Wrapper around the built-in help command that supports instances of
        LazyLoadedModuleProxy.

        Calling help() at the Python prompt starts an interactive help session.
        Calling help(thing) prints help for the python object 'thing'.
        """
        if args and isinstance(args[0], LazyLoadedModuleProxy):
            args = (args[0]._real_module, ) + args[1:]

        return pydoc.help(*args, **kwargs)

    repl_scope = __import__(__name__)
    setattr(repl_scope, "help", help)

    # Each $MAJOR.$MINOR version has its own cache file.
    major, minor = sys.version_info[:2]
    cache_file_basename = ".python%d.%d-repl-modules.cache" % (major, minor)
    cache_file = os.path.expanduser(os.path.join("~", cache_file_basename))

    try:
        cache_status = os.stat(cache_file)
    except Exception:
        out_of_date = True
    else:
        most_recent_modification = 0
        for path in sys.path:
            try:
                status = os.stat(path)
            except Exception:
                continue
            if status.st_mtime > most_recent_modification:
                most_recent_modification = status.st_mtime

        out_of_date = most_recent_modification > cache_status.st_mtime

    if out_of_date:
        print("Module name cache is out of date; updating...", end=" ")
        sys.stdout.flush()

        import pkgutil
        modules = set()

        blacklisted_module_names = set((
            "ptyprocess",                   # Breaks ^C
            "pyatspi",                      # Breaks ^C
            "pycurl",                       # Breaks ^C
            "xpra.x11.bindings",            # Causes segmentation fault
        ))

        for name in blacklisted_module_names:
            sys.modules[name] = None

        for _, name, _ in pkgutil.walk_packages(onerror=lambda *a, **k: None):
            if name not in blacklisted_module_names:
                modules.add(name)

        for name in sys.builtin_module_names:
            if name != "__main__":
                modules.add(name)

        # Clear entries for blacklisted modules to make it possible to
        # explcitily import them.
        for name in blacklisted_module_names:
            del sys.modules[name]

        with open(cache_file, "w") as iostream:
            json.dump(list(modules), iostream)

        print("Done updating cache.")

    else:
        with open(cache_file) as iostream:
            modules = set(json.load(iostream))

    # Create lazy-loader proxies for all top-level module identifiers.
    for name in modules:
        if "." not in name and name != "repr":
            setattr(repl_scope, name, LazyLoadedModuleProxy(name))


try:
    _configure_lazy_loader_proxies()
except Exception as e:
    print("Failed to configure lazy-loaded modules:", e)
    del e
else:
    try:
        import readline
        import rlcompleter
        readline.parse_and_bind("tab: complete")
    except ImportError:
        pass
finally:
    del _configure_lazy_loader_proxies
