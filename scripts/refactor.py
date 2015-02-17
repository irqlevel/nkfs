import os

def refactor_file(fpath, old, new):
	fp = open(fpath, 'r')
	lines = []
	while True:
		l = fp.readline()
		if l == "":
			break
		l = l.replace(old, new)
		lines.append(l)
	fp.close()
	fp = open(fpath, 'w')
	for l in lines:
		fp.write(l)
	fp.close()

def refactor_dir(path, exts, old, new):
	for dirpath, dirnames, filenames in os.walk(path):
		for f in filenames:
			fname = os.path.join(dirpath, f)
			_, ext = os.path.splitext(fname)
			if ext in exts:
				refactor_file(fname, old, new)

if __name__=="__main__":
	refactor_dir(os.path.abspath("."), ['.c', '.h'], 'DS_', 'NKFS_')
	refactor_dir(os.path.abspath("."), ['.c', '.h'], 'ds_', 'nkfs_')
