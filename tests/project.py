# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import os
import sys
import time
import inspect
import logging
import argparse

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
        ssh.cmd("rmmod " + settings.DS_KO_NAME, throw = False)
    def undeploy(self):
        self.unload_mod()

def main(ip, user, passwd, pdir, cmd):
    p = Project(settings.PROJ_DIR)
    p.set_target(Target(ip, user, passwd))
    if cmd == 'build':
        p.build()
    elif cmd == 'rebuild':
        p.rebuild()
    elif cmd == 'clean':
        p.clean()
    elif cmd == 'deploy':
        p.rebuild()
        p.undeploy()
        p.deploy()
    elif cmd == 'deploy-run':
        p.rebuild()
        p.undeploy()
        p.deploy()
        p.load_mod()
    elif cmd == 'undeploy':
        p.undeploy()
    else:
        raise Exception("Unknown cmd=" + cmd)

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--ip', help='ip of node')
    ap.add_argument('--user', help='user of node')
    ap.add_argument('--passwd', help='user password')
    ap.add_argument('--pdir', help='project dir')
    ap.add_argument('cmd', help='command')

    args = ap.parse_args()
 
    ip = args.ip
    user = args.user
    passwd = args.passwd
    pdir = args.pdir
    cmd = args.cmd
    if not ip:
        ip = '10.30.18.211'
    if not user:
        user = 'root'
    if not passwd:
        passwd = '1q2w3es5'
    if not pdir:
        pdir = settings.PROJ_DIR
    main(ip, user, passwd, pdir, cmd)

