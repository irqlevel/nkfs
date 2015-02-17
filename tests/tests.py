from tests_lib import cmd
from tests_lib import settings
from nkfs_test import NkfsTest, NkfsTestList
from nkfs_env import NkfsLocalLoopEnv
from file_put_get_test import FilePutGetTest
from file_put_del_test import FilePutDelTest
import tempfile
import os
import inspect
import hashlib
import random
import shutil
import logging
import multiprocessing

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)

log = logging.getLogger('main')

if __name__=="__main__":
	env = None
	try:
		env = NkfsLocalLoopEnv()
		env.prepare()
		ts = NkfsTestList()
		ts.addTests([FilePutDelTest(env, 3, 10, 10, 10000000), FilePutGetTest(env, 3, 10, 10, 10000000)])
		ts.run()
	except Exception as e:
		log.error("EXCEPTION: %s" % e)
	finally:
		try:
			if env:
				env.cleanup()
		except:
			pass
