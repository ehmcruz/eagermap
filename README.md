# EagerMap
EagerMap is a task mapping algorithm for communication-aware mapping.

## License
EagerMap is licensed under the GPLv2, as shown in the ```LICENSE```file.

## Compilation
EagerMap can be compiled by just running ```make```. No special libraries are required.

## Running Eagermap

After compilation, you can execute EagerMap in this way:
```
./eagermap <csv file> <arities>
```
where ```<csv file>``` is the filename of comma-separated file containing the communication matrix (an example file is locate in the matrices-example/ folder)
and ```<aritities>```is a comma-separated list of the arities of the hardware topology, including the root node (as an example, the string ```1,4,16``` represents a topology with 3 levels and 64 processing units (PUs) in total).

The communication matrix contains 1 line per task, in reverse order of the tasks (that is, the first task is on the last line of the matrix).
Each line contains the amount of communication to all the other tasks.
The matrix needs to be square and symmetric.


A full command line example is:
```
./eagermap matrices-example/SP-OMP.csv 1,4,16
```

## Output
After execution, EagerMap outputs statistics about the mapping (such as execution time of the algorithm and mapping quality), as well as the mapping itself.
The mapping is a list of PUs to which the tasks should be mapped.
For example, a mapping of ```7,1,0``` indicates that the first task should be mapped to PU 7, the second to PU 1, and so on.

## Publications
A full description of EagerMap and a comparison to other algorithms is presented in:

- Eduardo H. M. Cruz, Matthias Diener, Laércio L. Pilla, Philippe O. A. Navaux. **“An Efficient Algorithm for Communication-Based Task Mapping.”** International Conference on Parallel, Distributed, and Network-Based Processing (PDP), 2015. https://doi.org/10.1109/PDP.2015.25
- Eduardo H. M. Cruz, Matthias Diener, Laércio L. Pilla, Philippe O. A. Navaux. **“EagerMap: A Task Mapping Algorithm to Improve Communication and Load Balancing in Clusters of Multicore Systems.”** ACM Transactions on Parallel Computing (TOPC), 2019. https://doi.org/10.1145/3309711
