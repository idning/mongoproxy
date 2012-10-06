.. contents:: Table of Contents

why:

- mongo-php-drive is too heavy to create a connection
- too many php workers, too many connections
- client do not to know it's a replset(no slaveok for client)

what's better: 

- single process, less ping to backend mongod
- avoid blocking ping before querys.
- one client => one server conn

design:

- one backend is a mongd
- backends became a replset

TODO: 

- print request_id into log
- dump request & response
- handler slave_ok *done*
- handler primary change
- client idle
- server side timeout.. *done*
- close serverside conn when too many idle conns(easy to do).

config: conf/mongoproxy.cfg ::

    MONGOPROXY_BIND = 0.0.0.0
    MONGOPROXY_PORT = 9527

    #replica set
    MONGOPROXY_USE_REPLSET = 1

    MONGOPROXY_REPLSET_NAME = cluster0
    #select will go to slaves
    MONGOPROXY_REPLSET_ENABLE_SLAVE = 1

    #MONGOPROXY_BACKEND = 127.0.0.1:27017
    MONGOPROXY_BACKEND = 127.0.0.1:30001,127.0.0.1:30003

usage::

    ning@ning-laptop ~/idning/blog_and_notes$ mongo --port 9527 
    MongoDB shell version: 2.0.6
    connecting to: 127.0.0.1:9527/test
    SECONDARY> db.foo.find()
    { "_id" : ObjectId("506afa5300ee31bfddede23d"), "x" : 3 }
    { "_id" : ObjectId("506afa7a5428e4e2f65ce8fb"), "x" : 3 }
    { "_id" : ObjectId("506b30adf6f0dc3792367efe"), "3" : 5 }
    { "_id" : ObjectId("506b9f4cc50ba0162da625bf"), "x" : 3 }
    SECONDARY> db.foo.insert({x:9})
    PRIMARY> db.foo.find()
    { "_id" : ObjectId("506afa5300ee31bfddede23d"), "x" : 3 }
    { "_id" : ObjectId("506afa7a5428e4e2f65ce8fb"), "x" : 3 }
    { "_id" : ObjectId("506b30adf6f0dc3792367efe"), "3" : 5 }
    { "_id" : ObjectId("506b9f4cc50ba0162da625bf"), "x" : 3 }
    { "_id" : ObjectId("506ba7e74f1b2720690ce3c0"), "x" : 9 }
    PRIMARY> 


Reference:

- http://www.mongodb.org/display/DOCS/Mongo+Wire+Protocol#MongoWireProtocol-OPQUERY
- https://github.com/interactive-matter/bson-c  (MongoDB C Driver)
- https://github.com/mongodb/mongo-c-driver/tree/master/src (another MongoDB c driver, offical version)

