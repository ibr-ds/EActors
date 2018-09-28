rm -rf ./build

# developers
#fifo_test
#tools/build.py examples/fifo_test/fifo_test.c examples/fifo_test/fifo_test1.xml 0 parts/App/systhreads_none.cxx


#basic hello world, 1
tools/build.py examples/template/template.c examples/template/template-1.xml 0 parts/App/systhreads_factory.cxx
#basic hello world, U
#tools/build.py examples/template/template.c examples/template/template-2.xml 0 parts/App/systhreads_factory.cxx


#mbox-based ping-pong, U-U
#tools/build.py examples/pingpong/pingpong.c examples/pingpong/pingpong-1.xml 0 parts/App/systhreads_factory.cxx
#mbox-based ping-pong, U-1
#tools/build.py examples/pingpong/pingpong.c examples/pingpong/pingpong-2.xml 0 parts/App/systhreads_factory.cxx
#mbox-based ping-pong, 1-2
#tools/build.py examples/pingpong/pingpong.c examples/pingpong/pingpong-3.xml 0 parts/App/systhreads_factory.cxx
#mbox-based ping-pong, U-1, single worker
#tools/build.py examples/pingpong/pingpong.c examples/pingpong/pingpong-4.xml 0 parts/App/systhreads_factory.cxx

#cargo-based ping-pong, U-U //outdated
#tools/build.py examples/pingpong2/pingpong2.c examples/pingpong2/pingpong2-1.xml 0 parts/App/systhreads_factory.cxx
#cargo-based ping-pong, U-1 //outdated
#tools/build.py examples/pingpong2/pingpong2.c examples/pingpong2/pingpong2-2.xml 0 parts/App/systhreads_factory.cxx
#cargo-based ping-pong, 1-2
#tools/build.py examples/pingpong2/pingpong2.c examples/pingpong2/pingpong2-3.xml 0 parts/App/systhreads_factory.cxx
#cargo-based ping-pong, 1-2, single worker
#tools/build.py examples/pingpong2/pingpong2.c examples/pingpong2/pingpong2-4.xml 0 parts/App/systhreads_factory.cxx
#cargo-based ping-pong, 1-1, single worker
#tools/build.py examples/pingpong2/pingpong2.c examples/pingpong2/pingpong2-5.xml 0 parts/App/systhreads_factory.cxx

#cargo-based tripong, U-U-U //outdated
#tools/build.py examples/tripong/tripong.c examples/tripong/tripong1.xml 0 parts/App/systhreads_factory.cxx
#cargo-based tripong, U-1-2 //outdated
#tools/build.py examples/tripong/tripong.c examples/tripong/tripong2.xml 0 parts/App/systhreads_factory.cxx
#cargo-based tripong, 1-2-3
#tools/build.py examples/tripong/tripong.c examples/tripong/tripong3.xml 0 parts/App/systhreads_factory.cxx
#cargo-based tripong, U-1-1 //outdated
#tools/build.py examples/tripong/tripong.c examples/tripong/tripong4.xml 0 parts/App/systhreads_factory.cxx

#network+cargo- based pingpong, 1--1
#tools/build.py examples/pingpongN/pingpongN.c examples/pingpongN/pingpongN-1.xml 0 parts/App/systhreads_factory.cxx

#LA ping-pong 1-1 //outdated
#tools/build.py examples/pingpongLA/pingpongLA.c examples/pingpongLA/pingpongLA-1.xml 0 parts/App/systhreads_factory.cxx
#LA ping-pong 1-2
#tools/build.py examples/pingpongLA/pingpongLA.c examples/pingpongLA/pingpongLA-2.xml 0 parts/App/systhreads_factory.cxx

#DH ping-pong 1-1
#tools/build.py examples/pingpongDH/pingpongDH.c examples/pingpongDH/pingpongDH-1.xml 0 parts/App/systhreads_factory.cxx

#secure mulilti-party computation
#1+2
#tools/build.py examples/smc/smc.c examples/smc/smc3.xml 0 parts/App/systhreads_factory.cxx
#1+3
#tools/build.py examples/smc/smc.c examples/smc/smc4.xml 0 parts/App/systhreads_factory.cxx
#1+4
#tools/build.py examples/smc/smc.c examples/smc/smc5.xml 0 parts/App/systhreads_factory.cxx
#1+5
#tools/build.py examples/smc/smc.c examples/smc/smc6.xml 0 parts/App/systhreads_factory.cxx
#1+6
#tools/build.py examples/smc/smc.c examples/smc/smc7.xml 0 parts/App/systhreads_factory.cxx
#1+7
#tools/build.py examples/smc/smc.c examples/smc/smc8.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+2
#tools/build.py examples/smc/smc.c examples/smc/smc0-3.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+3
#tools/build.py examples/smc/smc.c examples/smc/smc0-4.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+4
#tools/build.py examples/smc/smc.c examples/smc/smc0-5.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+5
#tools/build.py examples/smc/smc.c examples/smc/smc0-6.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+6
#tools/build.py examples/smc/smc.c examples/smc/smc0-7.xml 0 parts/App/systhreads_factory.cxx
#single worker 1+7
#tools/build.py examples/smc/smc.c examples/smc/smc0-8.xml 0 parts/App/systhreads_factory.cxx

