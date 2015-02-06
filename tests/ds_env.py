from tests_lib import cmd
from tests_lib import settings
from ds_client import DsClient
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
SRVS = [("0.0.0.0", 9111), ("0.0.0.0", 9112)]

class DsEnv:
	def __init__(self):
		pass

	def get_client(self):
		pass

	def prepare(self):
		pass
	def cleanup(self):
		pass

class DsLocalLoopEnv(DsEnv):
	def __init__(self):
		DsEnv.__init__(self)
		self.devs = []
		self.srvs = []

	def get_client(self):
		return DsClient("0.0.0.0", 9111)

	def prepare(self):
		cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./load_mods.sh", throw = True)
		cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./loop_dev_create.sh", throw = True)

		c = self.get_client()
		for ip, port in SRVS:
			c.start_srv(ip, port)
			self.srvs.append((ip, port))
		for d in DEVS:
			c.add_dev(d, True)
			self.devs.append(d)

	def query_devs(self):
		c = self.get_client()
		for d in self.devs:
			c.query_dev(d)

	def cleanup(self):
		c = self.get_client()
		for ip, port in self.srvs:
			try:
				c.stop_srv(ip, port)
			except Exception as e:
				log.error("EXCEPTION %s" % e)

		for d in self.devs:
			try:
				c.rem_dev(d)
			except Exception as e:
				log.error("EXCEPTION %s" % e)

		try:
			cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./unload_mods.sh", throw = True)
		except Exception as e:
			log.error("EXCEPTION %s" % e)

		try:
			cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./loop_dev_del.sh", throw = True)
		except Exception as e:
			log.error("EXCEPTION %s" % e)


