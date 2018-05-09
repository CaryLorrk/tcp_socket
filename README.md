# tcp_socket test
A recipe for reproducing the problem.  

Build
-----
make -j [jobs]

Run
---
main this_host_id [hosts...]  
example:  
for host0: ./main 0 host0 host1
for host1: ./main 1 host0 host1  

When the CNT(in main.cc) become bigger, the performance drops significantly.
