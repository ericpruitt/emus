#!/usr/bin/env python
try:
    import readline
    import rlcompleter
    readline.parse_and_bind("tab: complete")
except ImportError:
    pass
finally:
    try:
        del readline
    except NameError:
        pass
    try:
        del readline
    except NameError:
        pass
