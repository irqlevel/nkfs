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

log = logging.getLogger('main')

DEVS = ["/dev/loop0", "/dev/loop1", "/dev/loop2"]
PORT = 9111

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
			cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/load_mods.sh", throw = True)

		cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/loop_dev_create.sh", throw = True)

		c = self.get_client()
		self.srvs = [(self.bind_ip, self.ext_ip, PORT)]
		for bind_ip, ext_ip, port in self.srvs:
			c.start_srv(bind_ip, ext_ip, port)

		for d in DEVS:
			c.add_dev(d, True)
			self.devs.append(d)

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
				log.error("EXCEPTION %s" % e)

		for d in self.devs:
			try:
				c.rem_dev(d)
			except Exception as e:
				log.error("EXCEPTION %s" % e)

		try:
			if self.load_mods:
				cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/unload_mods.sh", throw = True)
		except Exception as e:
			log.error("EXCEPTION %s" % e)

		try:
			cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && scripts/loop_dev_del.sh", throw = True)
		except Exception as e:
			log.error("EXCEPTION %s" % e)


