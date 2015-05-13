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
import argparse
import multiprocessing

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)

class NkfsNode():
	def __init__(self, log, ip, root_dir, dev=None, passwd=None, key_file=None, user=None):
		self.ip = ip
		self.log = log
		self.key_file = key_file
		self.passwd = passwd
		self.root_dir = root_dir
		self.wdir = os.path.join(self.root_dir, 'node_' + ip)
		self.user = user
		self.dev = dev
		exec_cmd2('mkdir -p ' + self.wdir, throw = True, elog = log)
		if self.user == 'root':
			self.rdir = '/root/sshexec'
		else:
			self.rdir = '/home/' + self.user + '/sshexec'
		self.ssh_exec('mkdir -p ' + self.rdir)
		self.cpus = int(self.ssh_exec('nproc')[1][0])

	def ssh_exec(self, cmd, throw = True):
		u = SshUser(self.log, self.ip, self.user, password=self.passwd, key_file=self.key_file, ftp = False)
		return ssh_exec(u, cmd, throw = throw)

	def ssh_file_get(self, remote_file, local_file):
		u = SshUser(self.log, self.ip, self.user, password=self.passwd, key_file=self.key_file, ftp = True)
		ssh_file_get(u, remote_file, local_file)
	def ssh_file_put(self, local_file, remote_file):
		u = SshUser(self.log, self.ip, self.user, password=self.passwd, key_file=self.key_file, ftp = True)
		ssh_file_get(u, local_file, remote_file)
	def prepare_nkfs(self):
		self.ssh_exec('sudo rm -rf /var/log/nkfs.log')
		self.ssh_exec('rm -rf nkfs')
		self.ssh_exec('git clone https://github.com/irqlevel/nkfs.git')
		self.ssh_exec('cd nkfs && git checkout -b develb origin/devel')
		self.ssh_exec('cd nkfs && make -j ' + str(self.cpus))
	def start_nkfs(self):
		self.ssh_exec('sudo iptables -F')
		self.ssh_exec('cd nkfs && sudo insmod bin/nkfs_crt.ko')
		self.ssh_exec('cd nkfs && sudo insmod bin/nkfs.ko')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl dev_add -d ' + self.dev + ' -f')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl srv_start -b 0.0.0.0 -e ' + self.ip + ' -p 9111')

	def neigh_add(self, ip):
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl neigh_add -e ' + ip + ' -p 9111')

	def query_nkfs(self):
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl dev_query -d ' + self.dev)
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl neigh_info')

	def stop_nkfs(self):
		self.ssh_exec('sudo rmmod nkfs')
		self.ssh_exec('sudo rmmod nkfs_crt')

	def get_nkfs_log(self):
		self.ssh_exec('mkdir -p ' + self.rdir)
		lpath = os.path.join(self.rdir, 'nkfs.log')
		dpath = os.path.join(self.rdir, 'dmesg.out')
		self.ssh_exec('cd nkfs && sudo bin/nkfs_ctl klog_sync', throw = False)
		self.ssh_exec('sudo cp /var/log/nkfs.log ' + lpath)
		self.ssh_exec('sudo dmesg > ' + dpath)
		self.ssh_exec('sudo chown -R ' + self.user + ':' + self.user + ' ' + self.rdir)
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


def run_cluster_test(ips, user, passwd, dev):
	root_dir = os.path.abspath('cluster_test')
	exec_cmd2('rm -rf ' + root_dir, throw = True)
	exec_cmd2('mkdir ' + root_dir, throw = True)
	settings.init_logging(log_dir = root_dir, log_name = 'tests.log')
	log = logging.getLogger('main')
	log.info('starting')
	nodes = []	
	for ip in ips:
		n = NkfsNode(log, ip, root_dir, dev=dev, passwd=passwd, user=user)
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

if __name__=="__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("-d", "--device", type=str, default='/dev/sdx', help="node storage device")
	parser.add_argument("-p", "--password", type=str, default='1q2w3e', help="node user password")
	parser.add_argument("-u", "--user", type=str, default='root', help="node user")
	parser.add_argument("ips", type=str, help="nodes ips")
	args = parser.parse_args()
	ips = args.ips.split(':')
	run_cluster_test(ips, args.user, args.password, args.device)
