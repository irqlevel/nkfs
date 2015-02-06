from tests_lib import cmd
from tests_lib import settings
from ds_env import DsLocalLoopEnv
from ds_test import DsTest

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

class FilePutGetTest(DsTest):
	def __init__(self, env, proc_num, file_count, file_min_size, file_max_size):
		DsTest.__init__(self, env)
		self.proc_num = proc_num
		self.file_count = file_count
		self.file_min_size = file_min_size
		self.file_max_size = file_max_size
                log.info("%s parameters: file_count %d, proc_num %d, file_min_size %d, file_max_size %d" %
                                (self.get_uid_name(), self.file_count, self.proc_num, self.file_min_size, self.file_max_size))
	def prepare(self):
		self.in_dir = tempfile.mkdtemp(dir=CURR_DIR)
		self.out_dir = tempfile.mkdtemp(dir=CURR_DIR)

	def proc_routine(self):
		i = 0
		obj_ids = []
		file_names = self.gen_tmp_files()
		for f in file_names:
			obj_ids.append(self.get_client().put_file(os.path.join(self.in_dir, f)))
		
		for obj_id in obj_ids:
			self.get_client().get_file(obj_id, os.path.join(self.out_dir, file_names[i]))
			i+= 1

	def test(self):
		self.env.query_devs()

		proc_list = [Process(target=self.proc_routine,args=()) for p in xrange(self.proc_num)]
		for p in proc_list:
			p.start()
	
		for p in proc_list:
			p.join()
	
		for p in proc_list:
			log.info("process %s exit with code: %d" % (p.name, p.exitcode))

		self.env.query_devs()

		res = self.check_file_hash()

		if len(res) != 0:
			log.error("FAILED: broken files: %s" % res)
		else:
			self.set_passed()

	def gen_tmp_files(self):
		tmp_filenames = []
		for f in xrange(0, self.file_count):
			tmp_file = tempfile.NamedTemporaryFile(prefix = "", dir = self.in_dir, delete = False)
			random_data = os.urandom(random.randint(self.file_min_size, self.file_max_size))
			tmp_file.write(random_data)
			tmp_filenames.append(os.path.basename(tmp_file.name))
			tmp_file.close()
		return tmp_filenames

	def put_file(self, file_name):
		c = self.get_client()	
		return c.put_file(file_name)

	def get_file(self, obj_id, out_file_name):
		c = self.get_client()	
		c.get_file(obj_id, out_file_name)

	def hash_large_file(self, path):
		hasher = hashlib.sha256()
		block_size = 128*hashlib.sha256().block_size #appropraite size for hashing large objects
		with open(path, "rb") as afile:
			buf = afile.read(block_size)
			while len(buf) > 0:
				hasher.update(buf)
				buf = afile.read(block_size)
		return buf

	def check_file_hash(self):	
		broken_files = []
	
		for root,dirs,files in os.walk(self.in_dir):
			in_files = files
		
		for filename in in_files:
			path_to_in_file = os.path.join(self.in_dir, filename)
			path_to_out_file = os.path.join(self.out_dir, filename)

			hash_in = self.hash_large_file(path_to_in_file)
			hash_out = self.hash_large_file(path_to_out_file)
			if hash_in != hash_out:
				broken_files.append(filename)	

		return broken_files

	def cleanup(self):
		shutil.rmtree(self.in_dir)
		shutil.rmtree(self.out_dir)

if __name__ == "__main__":
	env = None
	try:
		env = DsLocalLoopEnv()
		env.prepare()
		t = FilePutGetTest(env, 3, 10, 10, 10000000)
		t.run()
	except Exception as e:
		log.error("EXCEPTION: %s" % e)
	finally:
		try:
			if env:
				env.cleanup()
		except:
			pass
