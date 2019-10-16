# vlu

TEST_PROGS = build/bin/vlu_bench

CXXFLAGS =  -std=c++17 -march=haswell -g -O3

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

build/bin/vlu_%: build/obj/vlu_%.o
	$(call cmd,LD $@,$(@D),$(CXX) $(CXXFLAGS) $< -o $@)

build/obj/%.o: src/%.cc
	$(call cmd,CC $@,$(@D),$(CXX) $(CXXFLAGS) -c $^ -o $@)
