from tests_lib import cmd
from tests_lib import settings
from tests_lib.ssh import SshExec
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

log = logging.getLogger('main')

class AmznNodeKeyPath():
	def __init__(self):
		pass
	def get(self):
		path = os.path.join(os.path.expanduser('~'), 'nkfs_test')
		path = os.path.join(path, 'nkfs_test_kp.pem')
		return path

class AmznNode():
	def __init__(self, ip, key_path, rootdir):
		self.ip = ip
		self.key_path = key_path
		self.wdir = os.path.join(rootdir, 'node_' + ip)
		exec_cmd2('mkdir -p ' + self.wdir, throw = True)

	def get_ssh(self):
		return SshExec(log, self.ip, "ec2-user", key_file=self.key_path)
	def prepare_nkfs(self):
		s = self.get_ssh()
		s.cmd('sudo rm -rf /var/log/nkfs.log')
		s.cmd('rm -rf nkfs')
		s.cmd('git clone https://github.com/irqlevel/nkfs.git')
		s.cmd('cd nkfs && make')
	def start_nkfs(self):
		s = self.get_ssh()
		s.cmd('cd nkfs && sudo scripts/start.sh')
	def neigh_add(self, ip):
		s = self.get_ssh()
		s.cmd('cd nkfs && sudo bin/nkfs_ctl neigh_add -r ' + ip + ' -t 9111 -s ' + self.ip + ' -p 9111')
	def stop_nkfs(self):
		s = self.get_ssh()
		s.cmd('cd nkfs && sudo scripts/stop.sh')
	def get_nkfs_log(self):
		s = self.get_ssh()
		lpath = os.path.join(s.rdir, 'nkfs.log')
		s.cmd('cd nkfs && sudo bin/nkfs_ctl klog_sync', throw = False)
		s.cmd('sudo cp /var/log/nkfs.log ' + lpath)
		s.cmd('sudo chown -R ec2-user:ec2-user ' + lpath)
		s.file_get(lpath, os.path.join(self.wdir, 'nkfs.log'))

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


def nodes_neighs_connect(nodes):
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
	exec_cmd2('mkdir -p ' + rootdir, throw = True)
	nodes = []	
	for ip in ips:
		n = AmznNode(ip, AmznNodeKeyPath().get(), rootdir)
		nodes.append(n)

	multi_process([n.prepare_nkfs for n in nodes])
	multi_process([n.start_nkfs for n in nodes])
	nodes_neighs_connect(nodes)
	log.info('will sleep 120 secs to simulate run')
	time.sleep(120)	#catch hbt
	multi_process([n.get_nkfs_log for n in nodes])
	multi_process([n.stop_nkfs for n in nodes])
