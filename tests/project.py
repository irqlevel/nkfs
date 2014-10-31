# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import os
import sys
import time
import inspect
import logging

#currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
#parentdir = os.path.dirname(currentdir)
#sys.path.insert(0,parentdir) 

import settings
import dictconfig
from cmd import exec_cmd2
from ssh import SshExec

dictconfig.dictConfig(settings.LOGGING)

log = logging.getLogger('main')

class Target():
    def __init__(self, node, user, passwd):
        self.node = node
        self.user = user
        self.passwd = passwd

class Project():
    def __init__(self, path):
        self.path = os.path.abspath(path)
    def build(self):
        exec_cmd2("cd " + self.path + " && make ", throw = True)
    def clean(self):
        exec_cmd2("cd " + self.path + " && make clean ", throw = True)
    def rebuild(self):
        self.clean()
        self.build()
    def set_target(self, target):
        self.target = target
    def open_ssh_target(self):
        return SshExec(log, self.target.node, self.target.user, self.target.passwd)
    def deploy(self):
        ssh = self.open_ssh_target()
        ds_ko_p_r = os.path.join(ssh.rdir, settings.DS_KO_NAME)
        ds_ctl_p_r = os.path.join(ssh.rdir, settings.DS_CTL_NAME)
        ssh.file_put(settings.DS_KO, ds_ko_p_r)
        ssh.file_put(settings.DS_CTL, ds_ctl_p_r)
    def load_mod(self):
        ssh = self.open_ssh_target()
        ds_ko_p_r = os.path.join(ssh.rdir, settings.DS_KO_NAME)
        ssh.cmd("insmod " + ds_ko_p_r, throw = True)
    def unload_mod(self):
        ssh = self.open_ssh_target()
        ssh.cmd("rmmod " + settings.DS_KO_NAME, throw = True)

if __name__ == '__main__':
    p = Project("/mnt/sources/ds")
    p.rebuild()
    p.set_target(Target('10.30.18.211', 'root', '1q2w3es5'))
    p.deploy()
    p.load_mod()
    p.unload_mod()

