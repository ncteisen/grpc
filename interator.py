import subprocess
import json

print "Starting the process"

# subprocess.check_call(["make","clean"])

mapping = {}

for i in range(0, 2, 2):
  for j in range(0, 2, 2):
    print "subbing in %d, %d" % (i,j)
    subprocess.check_call(["sed", "-i", "", 's/.gain_p = .*/.gain_p = %d,/' % i, "src/core/ext/transport/chttp2/transport/chttp2_transport.c"])
    subprocess.check_call(["sed", "-i", "", 's/.gain_i = .*/.gain_i = %d,/' % j, "src/core/ext/transport/chttp2/transport/chttp2_transport.c"])
    subprocess.check_call(["make","bm_fullstack_unary_ping_pong"])
    subprocess.check_call(["bins/opt/bm_fullstack_unary_ping_pong", "--benchmark_filter=BM_UnaryPingPong<TCP, NoOpMutator, NoOpMutator>/256k/256k", "--benchmark_out=out.%d.%d.json" % (i,j), "--benchmark_out_format=json", "--benchmark_min_time=10"])
    with open("out.%d.json" % i, "r") as bm_file:
      loaded_json = json.loads(bm_file.read())
      mapping[(i,j)] = loaded_json['benchmarks'][0]['cpu_time']

print sorted(mapping.iteritems(), key=lambda (k,v):(v,k))
