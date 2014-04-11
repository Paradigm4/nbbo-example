#!/bin/bash
# nbbo is a custom SciDB aggregation function. It consumes a special string, and it
# produces a comma-separated string formatted like:
# bid price, bid vol, ask price, ask vol.
#
# We normally run nbbo as variable_window aggregate, running over data points instead
# of the coordinate system.

# We need to look up symbols in the lookup table. Let's look up GE.
sym=$(iquery -o csv+ -aq "filter(symbols, symbol='MSFT')" | tail -1 | cut -d, -f1)

# Let's compute nbbo for one symbol, one day of arca book data:
q="variable_window(between(arca,$sym,NULL,$sym,NULL), ms, 1, 0, nbbo(order_record))"
iquery -aq "$q"

# Now for all of them
qall="variable_window(arca, ms, 1, 0, nbbo(order_record))"
time iquery -aq "aggregate($qall, count(*))"
