from tests_lib import cmd
from tests_lib import settings
from nkfs_client import NkfsClient
import tempfile
import os
import inspect
import hashlib
import random
import shutil
import uuid
import logging
from multiprocessing import Process

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))

CURR_DIR = os.path.abspath(currentdir)

settings.init_logging()
log = logging.getLogger('main')

class NkfsTest:
	def __init__(self, env):
		self.env = env
		self.passed = False
		self.uid = str(uuid.uuid4())
	def prepare(self):
		pass
	def cleanup(self):
		pass
	def get_client(self):
		return self.env.get_client()
	def test(self):
		pass
	def run(self):
		try:
			log.info("PREPARING %s" % (self.get_uid_name(),))
			self.prepare()
			log.info("STARTING %s" % (self.get_uid_name(),))
			self.test()
			if self.passed:
				log.info("PASSED %s" % (self.get_uid_name(),))
			else:
				log.error("FAILED %s" % (self.get_uid_name(),))
		except Exception as e:
			log.exception("FAILED %s" % (self.get_uid_name(),))
			raise
		finally:
			try:
				self.cleanup()
			except Exception as e:
				log.exception("cleanup failed")
	def get_name(self):
		return self.__class__.__name__
	def get_uid_name(self):
		return self.get_name() + " : " + self.get_uid()
	def set_passed(self):
		self.passed = True
	def get_uid(self):
		return self.uid

class NkfsTestList:
	def __init__(self):
		self.tests = []
	def addTests(self, tests):
		for t in tests:
			self.tests.append(t)

	def run(self):
		for t in self.tests:
			try:
				t.run()
			except:
				pass
		self.report()

	def count_passed(self):
		passed = 0
		for t in self.tests:
			if t.passed:
				passed+= 1
		return passed

	def report(self):
		total = len(self.tests)
		log.info("TESTS PASSED %d among %d:" % (self.count_passed(), total))
		for t in self.tests:
			if t.passed:
				log.info("PASSED %s" % (t.get_uid_name(),))
			else:
				log.error("FAILED %s" % (t.get_uid_name(),))	
