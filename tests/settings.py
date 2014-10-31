# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
import os
import inspect

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)
PROJ_DIR = os.path.dirname(CURR_DIR)

BIN_DIR = os.path.join(PROJ_DIR, "bin")

DS_KO_NAME = "ds.ko"
DS_CTL_NAME = "ds_ctl"

DS_KO = os.path.join(BIN_DIR, DS_KO_NAME)
DS_CTL = os.path.join(BIN_DIR, DS_CTL_NAME)

LOGGING = {
    'version' : 1,
    'disable_existing_loggers' : False,
    'formatters': {
        'verbose' : {
            'format' : '%(levelname)s %(asctime)s %(module)s %(process)d %(thread)d %(message)s'
        },
        'simple' : {
            'format' : '%(levelname)s %(asctime)s %(module)s %(message)s'
        },
    },
    'handlers' : {
	'file' : {
            'level' : 'DEBUG',
	    'class' : 'logging.FileHandler',
            'formatter' : 'simple',
            'filename' : os.path.join(CURR_DIR, 'ds_tests.log'),
	},
	'stdout' : {
            'level' : 'DEBUG',
	    'class' : 'logging.StreamHandler',
            'formatter' : 'simple',
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

