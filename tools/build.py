#!/usr/bin/env python2
import xml.etree.cElementTree as ET
import argparse
import os
import ntpath

parser = argparse.ArgumentParser(description='Bulding SDK directories')
parser.add_argument("file", help="Arctor source file")
parser.add_argument("xml", help="Configuration file")
parser.add_argument("port", help="TCP port") #deprecated
parser.add_argument("sys", help="System actors source file")

args = parser.parse_args()

if not os.path.exists(args.file):
    print "source file '"+args.file+"' does not exist"
    exit(1)

if not os.path.exists(args.xml):
    print "config file '"+args.xml+"'does not exist"
    exit(1)

if not os.path.exists(args.sys):
    print "System actors source file '"+args.sys+"'does not exist, use default : 'parts/App/systhreads_factory.cxx'"
    exit(1)

### create directories

base = os.path.basename(args.file)
p_name = os.path.splitext(base)[0]

os.makedirs("build/"+p_name)

data = ET.parse(args.xml)
root = data.getroot()

adict = {}
anames = []

for actors in root.findall("actors"):
    for actor in actors.findall('actor'):
	iid = actor.find('id').text
	name = actor.find('name').text
	adict[name]=iid
	anames.append(name)

for servers in root.findall("servers"):
    for server in servers.findall('server'):
	gactors = []
	genclaves = []
	gworkers= []

	g_e_mask = [0 for x in range(64)]

	iip = server.find('ip').text

	os.makedirs("build/"+p_name+"/"+iip+"/App")

	iworkers_cxx = open("build/"+p_name+"/"+iip+"/App/iworkers.cxx", 'w')
	iworkers_cxx.write("//GENERATED\n")
	iworkers_cxx.write("#ifndef IWORKERS_CXX\n")
	iworkers_cxx.write("#define IWORKERS_CXX\n")
	iworkers_cxx.write("\n#include \"worker.h\"\n\n")

	iworkers_cxx.write("void assign_workers(void) {\n")
	iworkers_cxx.write("\tmemset(workers, 0, sizeof(workers));\n\n")

	for workers in server.findall("workers"):
	    for worker in workers.findall('worker'):
		iid = worker.find('id').text
		gworkers.append(iid)
		icpus = []
		cpu_mask = 0
		cpus = worker.find('cpus')
		for cpu in cpus.findall('cpu'):
		    cpu_mask=cpu_mask+(1<<int(cpu.text))
		    icpus.append(cpu.text)

		print "workers[",iid,"].cpu_mask =",cpu_mask,";"
		iworkers_cxx.write("\tworkers["+iid+"].cpus_mask = "+str(cpu_mask)+";\n")

		enclave_mask = 0
		for enclaves in worker.findall('enclave'):
		    ienclave = enclaves.find('id').text
		    if ienclave != "0" and enclave_mask == 0:
			genclaves.append("0")
		    genclaves.append(ienclave)
		    enclave_mask +=(1<<int(ienclave))
		    actors = enclaves.find('actors')
		    iactors = []
		    actor_mask = 0
		    for actor in actors.findall('actor'):
			iactors.append(actor.text)
			gactors.append(actor.text)
			actor_mask +=(1<<int(adict[actor.text]))

		    g_e_mask[int(ienclave)] = g_e_mask[int(ienclave)] | actor_mask

		    print "workers[",iid,"].actors_mask[",ienclave,"] =",actor_mask,";"
		    iworkers_cxx.write("\tworkers["+iid+"].actors_mask["+ienclave+"] = "+str(actor_mask)+";\n")

		print "workers[",iid,"].enclaves_mask =",enclave_mask,";"
		iworkers_cxx.write("\tworkers["+iid+"].enclaves_mask = "+str(enclave_mask)+";\n\n")
	iworkers_cxx.write("}\n")
	iworkers_cxx.write("\n#endif\n")

	config_h = open("build/"+p_name+"/"+iip+"/config.h", 'w')
	config_h.write("//GENERATED\n")
	config_h.write("#ifndef CONFIG_H\n")
	config_h.write("#define CONFIG_H\n\n")
	config_h.write("#define MAX_NODES	50000\n")
	config_h.write("#define MAX_SOCKETS	20\n")
	config_h.write("#define MAX_GBOXES	(50+200)\n")
	config_h.write("#define MAX_DYN_ENCLAVES	10\n")
	config_h.write("#define MAX_DYN_WORKERS		20\n")

	config_h.write("#define TCP_PORT\t"+args.port+"\n")
	config_h.write("#define PRIVATE_STORE_SIZE	(BLOCK_SIZE*4)\n") #should be even divisable by 128

	config_h.write("#define MAX_ENCLAVES\t"+str(len(set(genclaves)))+"\n")

	config_h.write("#define MAX_WORKERS\t"+str(len(set(gworkers)))+"\n")
	config_h.write("#define MAX_ACTORS\t"+str(len(set(adict)))+"\n")
	config_h.write("\n#endif\n")
	config_h.close()

#eos file
	with open("build/"+p_name+"/"+iip+"/eos", "wb") as out:
	    out.truncate(50 * 1024 * 1024)
#generated and cfg-based
	os.symlink("../../../../"+args.sys, "build/"+p_name+"/"+iip+"/App/systhreads.cxx")
	os.symlink("../../../../examples/"+p_name+"/"+base, "build/"+p_name+"/"+iip+"/App/payload.c")
	inc_mk = open("build/"+p_name+"/"+iip+"/App/inc.mk", 'w')
	inc_mk.write("#GENERATED\n")
	if os.path.isfile("examples/"+p_name+"/extras.cfg"):
	    extras = open("examples/"+p_name+"/extras.cfg", 'r')
	    content = extras.readlines()
	    content = [x.strip() for x in content] 
#todo -- something better than just indexes
	    c_files = content[0].split(' = ')
	    if len(c_files) > 1:
		c_files = c_files[1]
		for fname in c_files.split(" "):
		    os.symlink("../../../../examples/"+p_name+"/"+fname, "build/"+p_name+"/"+iip+"/App/"+fname)
	    else:
		c_files = ""

	    h_files = content[1].split(' = ')
	    if len(h_files) > 1:
		h_files = h_files[1]
		for fname in h_files.split(" "):
		    os.symlink("../../../../examples/"+p_name+"/"+fname, "build/"+p_name+"/"+iip+"/App/"+fname)

	    a_files = content[2].split(' = ')
	    if len (a_files) > 1:
		a_files = a_files[1]
	    else:
		a_files = ""

	    inc_mk.write("Inc_libs +="+a_files+"\n")
	    inc_mk.write("Inc_objs +="+c_files.replace(".c", ".o")+"\n")
	inc_mk.close()

	make_h = open("build/"+p_name+"/"+iip+"/run.sh", 'w')
	make_h.write("if ! make -C App; then echo build error, abort; exit; fi\n\n")

	enclave_cfg_h = open("build/"+p_name+"/"+iip+"/App/enclave_cfg.h", 'w')
	enclave_cfg_h.write("//GENERATED\n")
	enclave_cfg_h.write("#ifndef ENCLAVE_CFG_H\n")
	enclave_cfg_h.write("#define ENCLAVE_CFG_H\n\n")
	enclave_cfg_h.write("#define ENCLAVE_ID		"+str(0)+"\n")
	enclave_cfg_h.write("#define ACTORS_MASK	("+str(g_e_mask[0])+"ULL)\n\n")
	enclave_cfg_h.write("\n#endif\n")
	enclave_cfg_h.close()


	payload_h = open("build/"+p_name+"/"+iip+"/App/payload.h", 'w')
	payload_h.write("//GENERATED\n")
	payload_h.write("#ifndef PAYLOAD_H\n")
	payload_h.write("#define PAYLOAD_H\n\n")

	for j in range(0, len(set(adict))):
		if g_e_mask[0] & (1 << j):
		    payload_h.write("extern int "+str(anames[j])+"_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes); \n")
		    payload_h.write("extern int "+str(anames[j])+"(struct actor_s *self);\n")
	payload_h.write("\n")

	payload_h.write("int (*pf[])(struct actor_s *) = {")
	for j in range(0, len(set(adict))):
		if g_e_mask[0] & (1 << j):
		    payload_h.write(str(anames[j]))
		else:
		    payload_h.write("NULL")
		payload_h.write(",")
	payload_h.seek(-1, 1)
	payload_h.write("};\n")

	payload_h.write("int (*cf[])(struct actor_s *, queue *, queue *, queue *, queue *) = {")
	for j in range(0, len(set(adict))):
		if g_e_mask[0] & (1 << j):
		    payload_h.write(str(anames[j])+"_ctr")
		else:
		    payload_h.write("NULL")
		payload_h.write(",")
	payload_h.seek(-1, 1)
	payload_h.write("};\n")

	payload_h.write("\n#endif\n")
	payload_h.close()
#App default
	os.symlink("../../../../parts/App/App.cpp", "build/"+p_name+"/"+iip+"/App/App.cpp")
	os.symlink("../../../../parts/App/Makefile", "build/"+p_name+"/"+iip+"/App/Makefile")
	os.symlink("../../../../parts/App/uactors.cxx", "build/"+p_name+"/"+iip+"/App/uactors.cxx")
	os.symlink("../../../parts/Enclave.edl", "build/"+p_name+"/"+iip+"/Enclave.edl")
	os.makedirs("build/"+p_name+"/"+iip+"/App/EActors")
	for item in os.listdir("parts/EActors/"):
	    os.symlink("../../../../../parts/EActors/" + item, "build/"+p_name+"/"+iip+"/App/EActors/" + item)

	os.makedirs("build/"+p_name+"/"+iip+"/App/sgx_utils")
	for item in os.listdir("parts/App/sgx_utils/"):
	    os.symlink("../../../../../parts/App/sgx_utils/" + item, "build/"+p_name+"/"+iip+"/App/sgx_utils/" + item)


#hinweise: This code generation process was developed especially for the SMC sum. it stores MEASURE of a next enclave in [0], and the first actor receives MEASURE of the last enclave in [1]
	for i in reversed(range(1, len(set(genclaves)))):
	    make_h.write("if ! make clean -C Enclave"+str(i)+"; then echo build error, abort; exit; fi\n")
	    if i == len(set(genclaves))-1:
		make_h.write("echo \"unsigned char csum[32 * 2]={0};\"  > Enclave"+str(i)+"/csum.c\n")
	    else:
		make_h.write("sgx_sign dump  -enclave Enclave"+str(i+1)+"/enclave.signed.so -dumpfile  Enclave"+str(i+1)+"/dump\n")
		make_h.write("echo \"unsigned char csum[32 * 2]={\" > Enclave"+str(i)+"/csum.c\n")
		make_h.write("grep -A2 hash  Enclave"+str(i+1)+"/dump |grep \"0x\" | sed -e 's/\s/,/g' | head -n 2 >> Enclave"+str(i)+"/csum.c\n")
		if( i == 1):
		    make_h.write("grep -A2 hash  Enclave"+str(len(set(genclaves))-1)+"/dump |grep \"0x\" | sed -e 's/\s/,/g'  | head -n 2 >> Enclave"+str(i)+"/csum.c\n")
		make_h.write("echo \"};\" >> Enclave"+str(i)+"/csum.c\n")

	    make_h.write("if ! make -C Enclave"+str(i)+"; then echo build error, abort; exit; fi\n\n")

	    os.makedirs("build/"+p_name+"/"+iip+"/Enclave"+str(i))
	    os.symlink("../../../../examples/"+p_name+"/"+base, "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/payload.c")
	    os.symlink("../../../../parts/Enclave/Enclave.cpp", "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/Enclave.cpp")
	    os.symlink("../../../../examples/"+p_name+"/Enclave.config.xml", "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/Enclave.config.xml")
#
	    inc_mk = open("build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/inc.mk", 'w')
	    inc_mk.write("#GENERATED\n")
	    if os.path.isfile("examples/"+p_name+"/extras.cfg"):
		extras = open("examples/"+p_name+"/extras.cfg", 'r')
		content = extras.readlines()
		content = [x.strip() for x in content] 
		c_files = content[0].split(' = ')
		if( len(c_files) > 1):
		    c_files = c_files[1]
		    for fname in c_files.split(" "):
			os.symlink("../../../../examples/"+p_name+"/"+fname, "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/"+fname)
		else:
		    c_files = ""

		h_files = content[1].split(' = ')
		if( len(h_files) > 1):
		    h_files = h_files[1]
		    for fname in h_files.split(" "):
			os.symlink("../../../../examples/"+p_name+"/"+fname, "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/"+fname)
		else:
		    h_files = ""

		a_files = content[2].split(' = ')
		if( len(a_files) > 1):
		    a_files = a_files[1]
		else:
		    a_files = ""

		inc_mk.write("Inc_libs +="+a_files+"\n")
		inc_mk.write("Inc_objs +="+c_files.replace(".c", ".o")+"\n")
	    inc_mk.close()
#
	    os.symlink("../../../../parts/Enclave/Enclave_private.pem", "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/Enclave_private.pem")
	    os.symlink("../../../../parts/Enclave/Makefile", "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/Makefile")
	    os.makedirs("build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/EActors")
	    for item in os.listdir("parts/EActors/"):
		os.symlink("../../../../../parts/EActors/" + item, "build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/EActors/" + item)

	    enclave_cfg_h = open("build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/enclave_cfg.h", 'w')
	    enclave_cfg_h.write("//GENERATED\n")
	    enclave_cfg_h.write("#ifndef ENCLAVE_CFG_H\n")
	    enclave_cfg_h.write("#define ENCLAVE_CFG_H\n\n")
	    enclave_cfg_h.write("#define ENCLAVE_ID		"+str(i)+"\n")
	    enclave_cfg_h.write("#define ACTORS_MASK	("+str(g_e_mask[i])+"ULL)\n\n")
	    enclave_cfg_h.write("\n#endif\n")
	    enclave_cfg_h.close()


	    payload_h = open("build/"+p_name+"/"+iip+"/Enclave"+str(i)+"/payload.h", 'w')
	    payload_h.write("//GENERATED\n")
	    payload_h.write("#ifndef PAYLOAD_H\n")
	    payload_h.write("#define PAYLOAD_H\n\n")

	    for j in range(0, len(set(adict))):
		    if g_e_mask[i] & (1 << j):
			payload_h.write("extern int "+str(anames[j])+"_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes); \n")
			payload_h.write("extern int "+str(anames[j])+"(struct actor_s *self);\n")
	    payload_h.write("\n")

	    payload_h.write("int (*pf[])(struct actor_s *) = {")
	    for j in range(0, len(set(adict))):
		    if g_e_mask[i] & (1 << j):
			payload_h.write(str(anames[j]))
		    else:
			payload_h.write("NULL")
		    payload_h.write(",")
	    payload_h.seek(-1, 1)
	    payload_h.write("};\n")

	    payload_h.write("int (*cf[])(struct actor_s *, queue *, queue *, queue *, queue *) = {")
	    for j in range(0, len(set(adict))):
		    if g_e_mask[i] & (1 << j):
			payload_h.write(str(anames[j])+"_ctr")
		    else:
			payload_h.write("NULL")
		    payload_h.write(",")
	    payload_h.seek(-1, 1)
	    payload_h.write("};\n")

	    payload_h.write("\n#endif\n")
	    payload_h.close()

	make_h.close()
	os.chmod("build/"+p_name+"/"+iip+"/run.sh", 509)
