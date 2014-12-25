Distributed decentralized persistent data storage system.

### build status
[![build status](https://travis-ci.org/irqlevel/ds.svg?branch=master)](https://travis-ci.org/irqlevel/ds)

```
Features:
1. Distributed - system consists of unlimited computer nodes connected in
"small world" network.
2. Decentralized - no one computer node is a center and no one computer node
holds meta information about all system. Each computer node holds only a parts
of meta and data of storage system.
3. Data storage API - CREATE/PUT/GET/DELETE.
4. Flexible replication of meta/data of storage system by so called algorithm
 - "(n, k) schema".
Each data can be converted to a N parts, and only K (K < N) parts is need
to restore origin data.
5. Flexible network topology - computer nodes can be added/removed to/from
storage.
6. Self-healing system. system should resurects in many cases of faults.
7. Fast data searching by assigned unique id.
8. Persistent - we store data at persistent pool of linux devices like HDDs.

Implementaion:
we do that in linux kernel mode primary for perfomance reasons.
In each computer node of system we build and run linux kernel module
that:
1. can lock/control block devices to store parts of meta/data of objects.
2. is a server that can connects to other computers nodes
and receive client requests.

How to build:
cd project_dir
export DS_KERNEL_PATH=PATH_TO_YOUR_KERNEL_SOURCES.
make

How to run:
sudo insmod bin/ds.ko #load kernel module
sudo bin/ds_ctl --dev_add /dev/sdb #add block device to object storage
sudo bin/ds_ctl --server_start PORT_NUMBER #run network server on 0.0.0.0:PORT_NUMBER

How to check storage log:
See dmesg or /var/log/ds.log
```
