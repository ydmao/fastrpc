# fastrpc #

fastrpc is a RPC library for C++.  It is derived from the
RPC library of an old version of pequod (https://github.com/bryankate/pequod).
The initial version was created collaberatively with Eddie Kohler, Bryan Ford,
Neha Narula, Robert Morris and Michael Kester.

## Features ##
Programmer use Protocol Buffer to specify rpc, services, and messages. fastrpc
uses its own plugin to generate serialization/deserialization code, stub for
rpc server and rpc clients.

fastrpc supports asynchronous and batching clients/servers. It uses libev to
handle asynchronous RPC.

fastrpc supports both blocking and non-blocking RPC. With non-blocking RPC, the
string fields of RPC messages share the string buffer with the transport layer.

fastrpc also supports synchronous RPC.

fastrpc supports both TCP network stack and Infiniband stack. On Infiniband,
fastrpc achieves ~10us latency.

## Performance and Usage ##

See https://github.com/ydmao/fastrpctest about how to incooperate fastrpc
into your own project.
