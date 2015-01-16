### Distributed decentralized persistent data storage system.

#### Build status:
[![build status](https://travis-ci.org/irqlevel/ds.svg?branch=master)](https://travis-ci.org/irqlevel/ds)

#### Features:
1. Distributed - system consists of unlimited number of computer nodes connected in
"small world" network.
2. Decentralized - no one computer node is a center and no one computer node
holds meta information about all system. Each computer node holds only a parts
of meta and data of storage system.
3. Data storage API - CREATE/PUT/GET/DELETE.
4. Flexible replication of meta data and data by so called algorithm "(n, k) schema":
each data can be converted to an N different parts, and only K (where K < N) different parts is need
to restore origin data.
5. Flexible network topology - computer nodes can be added/removed to/from
storage.
6. Self-healing system. System should resurects in many cases of faults.
7. Fast data searching by assigned unique id.
8. Persistent - data is stored at persistent pool of linux devices like HDD/SSDs.

#### Implementation:
Main core written as linux kernel module for perfomance reasons - fastest path
from network stack to block devices stack bypassing user-space switches.
For each computer node of system core linux kernel module should be built and launched.
Core linux kernel module can:
- lock/control block devices to store parts of meta/data of objects.
- connect to other computers nodes
and receive clients/other computer nodes requests.

#### How to build:
- cd ~
- git clone https://github.com/irqlevel/ds.git
- cd ds
- make

Note, that you can build ds for particular kernel by
setting up "export DS_KERNEL_PATH=PATH_TO_YOUR_KERNEL_SOURCES" before make.

#### How to make basic tests:
- sudo ./test.sh

#### How to run:
- cd ~/ds
- sudo insmod bin/ds_crt.ko #load runtime helper module
- sudo insmod bin/ds.ko #load core kernel module
- sudo bin/ds_ctl --dev_add /dev/sdb --format #add block device /dev/sdb to 
object storage and format(!!!) it.
- sudo bin/ds_ctl --server_start IP PORT_NUMBER #run network server on IP:PORT_NUMBER
- bin/ds_client put -f myfile.txt #put file 'myfile.txt' inside storage
d963a52161d67bf9d1e7c09ce313b050
- bin/ds_client query -i d963a52161d67bf9d1e7c09ce313b050 #query stored file(object)
size : 11
block : 3
bsize : 4096
device : /dev/loop0
- bin/ds_client get -i d963a52161d67bf9d1e7c09ce313b050 -f output.txt #read file back from storage
- md5sum myfile.txt output.txt #check files are equal
40dca55eb18baafa452e43cb4a3cc5b5  myfile.txt
40dca55eb18baafa452e43cb4a3cc5b5  output.txt
- bin/ds_client delete -i d963a52161d67bf9d1e7c09ce313b050 #delete file from storage

#### How to stop:
- sudo rmmod ds_crt
- sudo rmmod ds

#### How to see execution/debug log:
/var/log/ds.log
