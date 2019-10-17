# vlu

TEST_PROGS = build/vlu_bench build/vlu_demo build/vlu_test

CXXFLAGS =  -std=c++11 -march=haswell -g -O3

all: $(TEST_PROGS)

clean:
	rm -fr build

backup: clean
	tar czf ../$(shell basename $(shell pwd)).tar.gz .

ifdef V
cmd = @mkdir -p $2 ; echo "$3"; $3
else
cmd = @echo "$1"; mkdir -p $2 ; $3
endif

build/vlu_%: build/vlu_%.o
	$(call cmd,LD $@,$(@D),$(CXX) $(CXXFLAGS) $< -o $@)

build/%.o: src/%.cc
	$(call cmd,CC $@,$(@D),$(CXX) $(CXXFLAGS) -c $^ -o $@)
