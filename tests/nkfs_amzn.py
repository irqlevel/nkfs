from tests_lib import cmd
from tests_lib import settings
from tests_lib.ssh import SshUser, ssh_exec, ssh_file_put, ssh_file_get
from tests_lib.cmd import exec_cmd2

import tempfile
import os
import inspect
import hashlib
import random
import shutil
import uuid
import logging
import sys
import time

import multiprocessing

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)

class AmznNodeKeyPath():
	def __init__(self):
		pass
	def get(self):
		path = os.path.join(os.path.expanduser('~'), 'nkfs_test')
		path = os.path.join(path, 'nkfs_kp.pem')
		return path

class AmznNode():
	def __init__(self, log, ip, key_path, rootdir, user='ec2-user'):
		self.ip = ip
		self.log = log
		self.key_path = key_path
		self.wdir = os.path.join(rootdir, 'node_' + ip)
		exec_cmd2('mkdir -p ' + self.wdir, throw = True, elog = log)
		self.user = user
	def ssh_exec(self, cmd, throw = True):
		u = SshUser(self.log, self.ip, self.user, key_file=self.key_path, ftp = False)
		ssh_exec(u, cmd, throw = throw)
	def ssh_file_get(self, remote_file, local_file):
		u = SshUser(self.log, self.ip, self.user, key_file=self.key_path, ftp = True)
		ssh_file_get(u, remote_file, local_file)
	def ssh_file_put(self, local_file, remote_file):
		u = SshUser(self.log, self.ip, self.user, key_file=self.key_path, ftp = True)
		ssh_file_get(u, local_file, remote_file)

	def prepare_nkfs(self):
		self.ssh_exec('sudo rm -rf /var/log/nkfs.log')
		self.ssh_exec('rm -rf nkfs')
		self.ssh_exec('git clone https://github.com/irqlevel/nkfs.git')
		self.ssh_exec('cd nkfs && make')
	def start_nkfs(self):
		self.ssh_exec('sudo iptables -F')
		self.ssh_exec('cd nkfs && sudo insmod bin/nkfs_crt.ko')
		self.ssh_exec('cd nkfs && sudo insmod bin/nkfs.ko')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl dev_add -d /dev/sdb -f')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl srv_start -b 0.0.0.0 -e ' + self.ip + ' -p 9111')

	def neigh_add(self, ip):
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl neigh_add -e ' + ip + ' -p 9111')

	def query_nkfs(self):
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl dev_query -d /dev/sdb')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl neigh_info')

	def stop_nkfs(self):
		self.ssh_exec('sudo rmmod nkfs')
		self.ssh_exec('sudo rmmod nkfs_crt')

	def get_nkfs_log(self):
		if self.user == 'root':
			rdir = '/root/sshexec'
		else:
			rdir = '/home/' + self.user + '/sshexec'
		self.ssh_exec('mkdir -p ' + rdir)
		lpath = os.path.join(rdir, 'nkfs.log')
		dpath = os.path.join(rdir, 'dmesg.out')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl klog_sync', throw = False)
		self.ssh_exec('sudo cp /var/log/nkfs.log ' + lpath)
		self.ssh_exec('sudo dmesg > ' + dpath)
		self.ssh_exec('sudo chown -R ec2-user:ec2-user ' + rdir)
		self.ssh_file_get(lpath, os.path.join(self.wdir, 'nkfs.log'))
		self.ssh_file_get(dpath, os.path.join(self.wdir, 'dmesg.out'))

def multi_process(fl):
	ps = []
	for f in fl:
		p = multiprocessing.Process(target=f, args=())
		ps.append(p)
	for p in ps:
		p.start()
	for p in ps:
		p.join()
	for p in ps:
		if p.exitcode:
			raise Exception("process exitcode %d" % p.exitcode)


def nodes_connect(nodes):
	pairs = {}
	for n in nodes:
		for m in nodes:
			if n.ip != m.ip:
				if not pairs.has_key((n.ip, m.ip)) and not pairs.has_key((m.ip, n.ip)):
					pairs[(n.ip, m.ip)] = (n, m)
	for n, m in pairs.values():
		n.neigh_add(m.ip)


if __name__=="__main__":
	i = 0
	ips = []
	for arg in sys.argv:
		if i > 0:
			ips.append(sys.argv[i])
		i+= 1

	if len(ips) == 0:
		raise Exception("No ips specified")

	rootdir = os.path.abspath('amzn_tests')
	exec_cmd2('rm -rf ' + rootdir, throw = True)
	exec_cmd2('mkdir ' + rootdir, throw = True)
	settings.init_logging(log_dir = rootdir, log_name = 'test.log')
	log = logging.getLogger('main')
	log.info('starting')
	nodes = []	
	for ip in ips:
		n = AmznNode(log, ip, AmznNodeKeyPath().get(), rootdir)
		nodes.append(n)

	try:
		multi_process([n.prepare_nkfs for n in nodes])
		multi_process([n.start_nkfs for n in nodes])
		nodes_connect(nodes)
		log.info('will sleep 20 secs to simulate run')
		time.sleep(20)	#catch hbt
		multi_process([n.query_nkfs for n in nodes])
	finally:
		try:
			multi_process([n.get_nkfs_log for n in nodes])
		except:
			pass
		try:
			multi_process([n.stop_nkfs for n in nodes])
		except:
			pass
	log.info('stopping')
