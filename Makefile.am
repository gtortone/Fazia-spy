bin_PROGRAMS = fazia_spy
fazia_spy_SOURCES = fazia-spy.cpp FzEventSet.pb.cpp
fazia_spy_CXXFLAGS = -std=c++0x -I/usr/include/root
fazia_spy_LDFLAGS = `root-config --libs` -L/usr/local/lib -lusb-1.0 -lprotobuf -lpthread -lz -lboost_thread -lboost_system -llog4cpp -lboost_program_options -lzmq 

include Makefile.pb
