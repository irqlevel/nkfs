# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import os
import inspect
import copy
import logging

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)
TESTS_DIR = os.path.dirname(CURR_DIR)
TESTS_LOGS_DIR = os.path.join(TESTS_DIR, "logs")

PROJ_DIR = os.path.dirname(TESTS_DIR)

BIN_DIR = os.path.join(PROJ_DIR, "bin")
NKFS_CLIENT = os.path.join(BIN_DIR, "nkfs_client")
NKFS_CTL = os.path.join(BIN_DIR, "nkfs_ctl")

if not os.path.exists(TESTS_LOGS_DIR):
    os.mkdir(TESTS_LOGS_DIR)

LOGGING = {
    'version' : 1,
    'disable_existing_loggers' : False,
    'formatters': {
        'verbose' : {
            'format' : '%(levelname)s %(asctime)s %(module)s %(process)d %(message)s'
        },
        'simple' : {
            'format' : '%(levelname)s %(asctime)s %(module)s %(message)s'
        },
    },
    'handlers' : {
        'file' : {
            'level' : 'DEBUG',
            'class' : 'logging.handlers.RotatingFileHandler',
            'formatter' : 'verbose',
            'filename' : os.path.join(TESTS_LOGS_DIR, 'tests.log'),
            'maxBytes' : 10000000,
            'backupCount' : 10,
        },
 	'stdout' : {
            'level' : 'DEBUG',
	    'class' : 'logging.StreamHandler',
            'formatter' : 'verbose',
	},
    },
    'loggers' : {
        'main' : {
            'handlers' : ['file', 'stdout'],
            'level' : 'DEBUG',
            'propagate' : True,
        },
    },
}

def init_logging(log_dir = None, log_name = None):
        if log_dir == None:
            log_dir = os.getcwd()
        if log_name == None:
            log_name = 'test.log'
	d = copy.deepcopy(LOGGING)
	d['handlers']['file']['filename'] = os.path.join(log_dir, log_name)
	logging.config.dictConfig(d)
