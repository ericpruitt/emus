#!/usr/bin/env python
def _install_eager_iterable_shims():
    """
    Modify "filter", "map", "range" and "zip" so they return lists in Python 3.

    Since the Python REPL is often used as a scratchpad or calculator, several
    functions have been modified so the user need not explicitly cast the
    returned values as lists to see the results.
    """
    import sys

    if sys.version_info < (3, ):
        return

    for function in [filter, map, range, zip]:
        # A closure is used so each shim function sees a different "name".
        def closure(name):
            def shim(*args, **kwargs):
                return list(getattr(builtins, name)(*args, **kwargs))

            shim.__doc__ = function.__doc__
            shim.__name__ = function.__name__
            setattr(__import__(__name__), name, shim)

        closure(function.__name__)


try:
    _install_eager_iterable_shims()
finally:
    del _install_eager_iterable_shims
