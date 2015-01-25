# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import paramiko
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
from cmd import StdFp, StdThread

dictconfig.dictConfig(settings.LOGGING)

log = logging.getLogger('main')

def ssh_cmd(ssh, cmd, throw = False, log = None):
    log.info("SSH:" + cmd)
    chan = None
    out_lines = []
    err_lines = []
    rc = 777
    try:
        chan = ssh.get_transport().open_session()
        chan.exec_command(cmd)
    
        stdout = StdFp(chan.makefile(), "SSH:stdout", elog = log)
        stderr = StdFp(chan.makefile_stderr(), "SSH:stderr", elog = log)

        stderr_t = StdThread(stderr)
        stdout_t = StdThread(stdout)

        stdout_t.start()
        stderr_t.start()
        
        rc = chan.recv_exit_status()
        stdout_t.join()
        stderr_t.join()
        out_lines = stdout.lines
        err_lines = stderr.lines

        if rc == 0:
            log.info("SSH:" + cmd + ":rc:" + str(rc))
        else:
            log.error("SSH:" + cmd + ":rc:" + str(rc))
    except Exception as e:
        log.exception(str(e))
    finally:
        if chan != None:
            try:
                chan.shutdown(2)
            except Exception as e:
                log.exception(str(e))

    if rc != 0 and throw:
        raise Exception("SSH:" + cmd + ":rc:" + str(rc))

    return rc, out_lines, err_lines

class SshExec:
    def __init__(self, log, host, user, passwd = None, key_file = None, timeout = None):
        self.host = host
        self.user = user
        self.passwd = passwd
        self.key_file = key_file
        self.log = log
        self.timeout = timeout

        if self.key_file != None:
            self.pkey = paramiko.RSAKey.from_private_key_file(self.key_file)
        else:
            self.pkey = None
  
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.ssh.connect(self.host, username=self.user, password = self.passwd, pkey=self.pkey, timeout = self.timeout)
        if self.user == 'root':
            self.rdir = os.path.join('/root', 'sshexec')
        else:
            self.rdir = os.path.join(os.path.join('/home', self.user), 'sshexec')

        self.ssh.exec_command('mkdir ' + self.rdir)
        self.ftp = self.ssh.open_sftp()
        self.log.info('opened ssh to host ' + self.host + ' rdir=' + self.rdir)
  
    def cmd(self, cmd, throw = True):
        return ssh_cmd(self.ssh, cmd, throw = throw, log = self.log)
 
    def file_get(self, remote_file, local_file):
        self.log.info("SSH:GETFILE:" + str(self.host) + " remote:" + remote_file + " local:" + local_file)
        self.ftp.get(remote_file, local_file)
        self.log.info("SSH:GETFILE:completed")
    def file_put(self, local_file, remote_file):
        self.log.info("SSH:PUTFILE:" + str(self.host) + " local:" + local_file + " remote:" + remote_file)
        self.ftp.put(local_file, remote_file)
        self.log.info("SSH:PUTFILE:completed")
    def file_getcwd(self):
        return self.ftp.getcwd()
    def file_chdir(self, path):
        return self.ftp.chdir(path)

if __name__ == '__main__':
    ssh = SshExec(log, "10.30.18.211", "root", "1q2w3es5")
    ssh.cmd("ps aux")

