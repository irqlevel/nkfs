from tests_lib import cmd
from tests_lib.settings import NKFS_CLIENT, NKFS_CTL
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

class NkfsClient:
	def __init__(self, ip, port):
		self.ip = ip
		self.port = port
	def ctl_cmd(self, cmd):
		return NKFS_CTL + " " + cmd
	def add_dev(self, dev, fmt = False):
		task = self.ctl_cmd("dev_add") + " -d " + dev
		if fmt:
			task+= " -f"
		cmd.exec_cmd2(task, throw = True)
	def rem_dev(self, dev):
		cmd.exec_cmd2(self.ctl_cmd("dev_rem") + " -d " + dev, throw = True)
	def query_dev(self, dev):
		rc, std_out, std_err, c = cmd.exec_cmd2(self.ctl_cmd("dev_query") + " -d " + dev, throw = True)
		return std_out
	def start_srv(self, bind_ip, ext_ip, port):
		cmd.exec_cmd2(self.ctl_cmd("srv_start") + " -b " + bind_ip + " -e " + ext_ip + " -p " + str(port), throw = True)
	def stop_srv(self, bind_ip, port):
		cmd.exec_cmd2(self.ctl_cmd("srv_stop") + " -b " + bind_ip + " -p " + str(port), throw = True)
	def srv_args(self):
		return " -s " + self.ip + " -p " + str(self.port)
	def cli_cmd(self, cmd):
		return NKFS_CLIENT + " " + cmd + " " + self.srv_args()
	def put_file(self, fpath):
		rc, std_out, std_err, c = cmd.exec_cmd2(self.cli_cmd("put") + " -f " + fpath, throw = True)
		return std_out[0]
	def get_file(self, obj_id, out_fpath):
		cmd.exec_cmd2(self.cli_cmd("get") + " -f " + out_fpath + " -i " + obj_id, throw = True)
	def del_file(self, obj_id):
		cmd.exec_cmd2(self.cli_cmd("delete") + " -i " + obj_id, throw = True)

if __name__=="__main__":
	c = NkfsClient("0.0.0.0", 9111)
	devs = ["/dev/loop0", "/dev/loop1", "/dev/loop2"]
	srvs = [("0.0.0.0", 9111), ("0.0.0.0", 9112)]

	for ip, port in srvs:
		c.start_srv(ip, port)

	for d in devs:
		c.add_dev(d, True)

	oid = c.put_file("bin/nkfs.ko")
	for d in devs:
		c.query_dev(d)

	c.get_file(oid, "nkfs_copy.ko")

	c.del_file(oid)

	for d in devs:
		c.query_dev(d)
		
	for ip, port in srvs:
		c.stop_srv(ip, port)

	for d in devs:
		c.rem_dev(d)
