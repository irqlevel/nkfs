### NKFS - distributed decentralized fault-tolerant file system.

#### Build status:
[![build status](https://travis-ci.org/irqlevel/nkfs.svg?branch=master)](https://travis-ci.org/irqlevel/nkfs)

#### Features:
1. Distributed - system consists of unlimited number of computer nodes connected in
"small world" network.
2. Decentralized - no one computer node is a center and no one computer node
holds meta information about all system. Each computer node holds only a parts
of meta and data of file system.
3. Files API - CREATE/PUT/GET/DELETE.
4. Flexible replication of meta data and data by so called algorithm "N-K schema":
each data can be converted to an N different parts, and only K (where K < N) different parts is need
to restore origin data.
5. Flexible network topology - computer nodes can be added/removed to/from
file system.
6. Self-healing system. System should resurects in many cases of faults.
7. Fast data searching by assigned unique id.
8. Data is stored at pool of linux devices like HDD/SSDs.
9. Integrity checks to detect data corruptions by XXHASH64 everywhere.

#### Implementation:
Main core written as linux kernel module for perfomance reasons - fastest path
from network stack to block devices stack bypassing user-space switches.
For each computer node of system core linux kernel module should be built and launched.
Core linux kernel module can:
- lock/control block devices to store parts of meta/data of files.
- connect to other computers nodes
and receive clients/other computer nodes requests.

#### Build:
```sh
$ cd ~
$ git clone https://github.com/irqlevel/nkfs.git
$ cd nkfs
$ make
```
Note, that you can build nkfs for particular kernel by
setting up "export NKFS_KERNEL_PATH=PATH_TO_YOUR_KERNEL_SOURCES" before make.

#### Tests:
```sh
$ sudo yum install python-pip
$ sudo pip install ConcurrentLogHandler
$ cd nkfs && sudo python tests/tests.py
```

#### Usage:
```sh
$ sudo insmod bin/nkfs_crt.ko #load runtime helper module

$ sudo insmod bin/nkfs.ko #load core kernel module

$ sudo bin/nkfs_ctl dev_add -d /dev/sdb -f #attach block device /dev/sdb to 
file system and format(!!!) it.

$ sudo bin/nkfs_ctl srv_start -s 127.0.0.1 -p 8000 #run network server at 127.0.0.1:8000

$ bin/nkfs_client put -s 127.0.0.1 -p 8000 -f myfile.txt #put already created file 'myfile.txt' inside storage
d963a52161d67bf9d1e7c09ce313b050

$ bin/nkfs_client query -s 127.0.0.1 -p 8000 -i d963a52161d67bf9d1e7c09ce313b050 #query stored file
obj_id : d963a52161d67bf9d1e7c09ce313b050 
size : 11
block : 3
bsize : 4096
device : /dev/loop0
sb_id : c7a236270cfb5accb45edeeb64f18e88

$ bin/nkfs_ctl dev_query -d /dev/loop0 #query info about attached device
dev_name : /dev/loop0
major : 7
minor : 0
sb_id : c7a236270cfb5accb45edeeb64f18e88
size : 104824832
used_size : 32768
free_size : 104845312
bsize : 4096
blocks : 25600
used_blocks : 8
inodes_tree_block : 2
bm_block : 1
bm_blocks : 1

$ bin/nkfs_client get -s 127.0.0.1 -p 8000 -i d963a52161d67bf9d1e7c09ce313b050 -f output.txt #read file back from storage

$ md5sum myfile.txt output.txt #check files are equal
40dca55eb18baafa452e43cb4a3cc5b5  myfile.txt
40dca55eb18baafa452e43cb4a3cc5b5  output.txt

$ bin/nkfs_client delete -s 127.0.0.1 -p 8000 -i d963a52161d67bf9d1e7c09ce313b050 #delete file from storage

$ sudo bin/nkfs_ctl dev_rem -d /dev/sdb #detach device from storage
$ sudo bin/nkfs_ctl srv_stop -s 127.0.0.1 -p 8000 #stop server
```
#### Shutdown:
```sh
$ sudo rmmod nkfs_crt
$ sudo rmmod nkfs
```
#### Logs:
```sh
$ tail -n 20 /var/log/nkfs.log
```
