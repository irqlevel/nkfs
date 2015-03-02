from tests_lib import cmd
from tests_lib import settings
from nkfs_env import NkfsLocalLoopEnv
from nkfs_test import NkfsTest
from file_put_get_test import FilePutGetTest
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


class FilePutDelTest(FilePutGetTest):
	def __init__(self, env, proc_num, file_count, file_min_size, file_max_size):
		FilePutGetTest.__init__(self, env, proc_num, file_count, file_min_size, file_max_size)

	def proc_routine(self):
		i = 0
		obj_ids = []
		file_names = self.gen_tmp_files()
		for f in file_names:
			obj_ids.append(self.get_client().put_file(os.path.join(self.in_dir, f)))

		for obj_id in obj_ids:
			self.get_client().del_file(obj_id)
			i+= 1

	def test(self):
		self.env.query_devs()
		proc_list = [Process(target=self.proc_routine,args=()) for p in xrange(self.proc_num)]
		for p in proc_list:
			p.start()
		
		for p in proc_list:
			p.join()

		self.env.query_devs()
		passed = 0
		for p in proc_list:
			log.info("process %s exit with code: %d" % (p.name, p.exitcode))
			if 0 == p.exitcode:
				passed+= 1

		if passed == len(proc_list):
			self.set_passed()

if __name__ == "__main__":
	env = None
	try:
		env = DsLocalLoopEnv()
		env.prepare()
		t = FilePutDelTest(env, 3, 10, 10, 100000)
		t.run()
	except Exception as e:
		log.error("EXCEPTION: %s" % e)
	finally:
		try:
			if env:
				env.cleanup()
		except:
			pass
