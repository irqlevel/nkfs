from tests_lib import cmd
from tests_lib import settings
from nkfs_client import NkfsClient
import tempfile
import os
import inspect
import hashlib
import random
import shutil
import logging
from multiprocessing import Process

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)

settings.init_logging()
log = logging.getLogger('main')

LOOP_DEVS = {"/dev/loop10" : "loop10_file", "/dev/loop11" : "loop11_file", "/dev/loop12" : "loop12_file"}
PORT = 9111

def drop_caches():
	for i in xrange(2):
		cmd.exec_cmd2("sync", throw = True, elog = log)
		cmd.exec_cmd2("echo 3 | tee /proc/sys/vm/drop_caches", throw = True, elog = log)

def bdev_flush_bufs(dev_name):
	cmd.exec_cmd2("blockdev --flushbufs " + dev_name, throw = True, elog = log)

def create_loop(loop_dev, loop_file, sizeMBs = 100):
	cmd.exec_cmd2("losetup -d " + loop_dev, throw = False, elog = log)
	cmd.exec_cmd2("rm -rf " + loop_file, throw = False, elog = log)
	cmd.exec_cmd2("dd if=/dev/zero of=" + loop_file + " bs=1M count="
			+ str(sizeMBs), throw = True, elog = log)
	drop_caches()
	cmd.exec_cmd2("losetup " + loop_dev + " " + loop_file, throw = True, elog = log)
	drop_caches()
	bdev_flush_bufs(loop_dev)

def delete_loop(loop_dev, loop_file):
	drop_caches()
	bdev_flush_bufs(loop_dev)
	cmd.exec_cmd2("losetup -d " + loop_dev, throw = True, elog = log)
	cmd.exec_cmd2("rm -rf " + loop_file, throw = True, elog = log)

class DsEnv:
	def __init__(self):
		pass

	def get_client(self):
		pass

	def prepare(self):
		pass
	def cleanup(self):
		pass

class NkfsLocalLoopEnv(DsEnv):
	def __init__(self, bind_ip, ext_ip, load_mods = True):
		DsEnv.__init__(self)
		self.devs = []
		self.srvs = []
		self.bind_ip = bind_ip
		self.ext_ip = ext_ip
		self.load_mods = load_mods

	def get_client(self):
		return NkfsClient(self.ext_ip, PORT)

	def prepare(self):
		if self.load_mods:
			cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/load_mods.sh", throw = True, elog = log)

		for loop_dev, loop_file in LOOP_DEVS.items():
			create_loop(loop_dev, loop_file)

		c = self.get_client()
		self.srvs = [(self.bind_ip, self.ext_ip, PORT)]
		for bind_ip, ext_ip, port in self.srvs:
			c.start_srv(bind_ip, ext_ip, port)

		for k, v in LOOP_DEVS.items():
			c.add_dev(k, True)
			self.devs.append(k)

	def query_devs(self):
		c = self.get_client()
		for d in self.devs:
			c.query_dev(d)

	def cleanup(self):
		c = self.get_client()
		for bind_ip, _, port in self.srvs:
			try:
				c.stop_srv(bind_ip, port)
			except Exception as e:
				log.exception("cant stop srv")

		for d in self.devs:
			try:
				c.rem_dev(d)
			except Exception as e:
				log.exception("cant rem dev")

		try:
			if self.load_mods:
				cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/unload_mods.sh", throw = True, elog = log)
		except Exception as e:
			log.exception("cant load mods")
		for loop_dev, loop_file in LOOP_DEVS.items():
			delete_loop(loop_dev, loop_file)
