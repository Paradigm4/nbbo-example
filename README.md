nbbo-example
============

Example custom aggregation function that computes national best bid and offer

The nbbo aggregate expects as input a single internally delimited string with the following format:
       'entry_type,ref_unique_id,price,size,symbol,ordertype|'
This string is created in step (3) below.

### Compile & copy the custom aggregation operator

The nbbo custom aggregate function is defined in nbbo/src/nbbo.cpp. You'll need to have installed
a copy of SciDB and its development packages. You can install SciDB's development packages with

#### Installation of SciDB devel packages on RHEL 6 or CentOS
```
yum install scidb-14.3-dev.x86_64 scidb-14.3-dev-tools.x86_64 scidb-14.3-dev-tools-dbg.x86_64 scidb-14.3-plugins-dbg.x86_64 scidb-14.3-libboost-devel.x86_64 scidb-14.3-libboost-static.x86_64
```
#### Installation of SciDB devel packages on Ubuntu
```
apt-get install scidb-14.3-dev scidb-14.3-dev-tools scidb-14.3-libboost1.54-dev scidb-14.3-libmpich2-dev scidb-14.3-libboost1.54-all-dev
```

Build and install the NBBO aggregate with:
```
cd nbbo
make
cp libnbbo.so /opt/scidb/14.3/lib/scidb/plugins
```
Adjusting your target directory accordingly for your installed version of SciDB.
Be sure to copy libnbbo.so to the plugin directory on all your SciDB cluster nodes.


## Synopsis

The nbbo aggregate function normally runs with SciDB's `variable_window` aggregate operator along a time dimension:

```
variable_window(arca, ms, 1, 0, nbbo(order_record))
```

where,

* arca  is a time by symbol data matrix,
* ms    is the name of the time dimension,
* 1, 0  indicaes that nbbo variable window size of 1 trailing (required)
* order_record is a specially formatted string input attribute for nbbo.

The nbbo function expects the input attribute to have the following form:
```
order_type, ref_id, price, size, symbol, ask_bid_type|
```
(the string must have a trailing vertical bar.)


## Example Use

### Download raw example ARCA book data into SciDB
```
wget -O - -q ftp://ftp.nyxdata.com/Historical%20Data%20Samples/TAQ%20NYSE%20ArcaBook/EQY_US_ALL_ARCA_BOOK_20130404.csv.gz
./load_arca.sh EQY_US_ALL_ARCA_BOOK_20130404.csv.gz
```
The raw ARCA data contains three different record formats corresponding to the
three different order types: A)dd, M)odify, & D)elete.  The load makes
three passes through the raw data file, one per order type, and standardizes
all three record types into a common format. The `load_arch.sh` script standardizes records, storing
them into a single array named arca_flat.


### Redimension into a 2D array organized by time and symbol
```
./redim_arca.sh
```
Since SciDB requires integer dimensions, the symbol dimension is first mapped
to an integer. This script then appends a new attribute named order_record to
each row in the arca_flat array. This attribute is the input value required by
the nbbo aggregate, described in step (1) above. 


### Run arca_example.sh

This produces the NBBO, formatted as the following comma separated output: 
       {symbol, timestamp} 'bid price, bid volume, ask price, ask volume'

For example, inserting 'MSFT' after symbol= in the first line of code in this
file produces the NBBO for MSFT. The value 5148 is the integer index value
associated with MSFT.  Subsequent lines in the file produce the NBBO for all
8000+ exchange traded equities.
