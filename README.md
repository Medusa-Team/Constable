Constable
=========
This is partially ported version of Constable 64-bit from 32-bit version of Constable.
It is still under developement.

Constable is the authorization server for security system Medusa Voyager that runs in user space.
It is a process that decides which actions will Medusa permit or not. It is the only process, 
that is excluded from Medusa. Constable configuration consists of two parts:

0.Constable configuration
0.Configuration of rules for security system Medusa

Constable is completely independent from kernel, which is ensured by kernel sending all supported 
entities to Constable at the start.

Usage
-----
run constable with parameter minimal/constable.conf that blocks all syscalls
```
constable minimal/constable.conf
```
At this time, the only supported syscall in Medusa Voyager is symlink
