# note to self:
- do error checking for queues when they return -1 as index
- RR tasks are assumed to use all of time_slice.
- FIFO tasks run to completion; skip calculating etc, moment its selected run to completion
- do NOT forget to go over the usleep() calls -> multiply by 1000? its milliseconds...
- it says adds 5-7 processes and distributes amongst consumers evenly, what u mean gang

# Questions to ask:
- what to do if the CPU affinity is -1; do i randomize between the 4 cores? randomly pick a core for that process?
