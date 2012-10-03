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

- request_id
- dump request & response
- handler slave_ok



Reference:

http://www.mongodb.org/display/DOCS/Mongo+Wire+Protocol#MongoWireProtocol-OPQUERY
https://github.com/interactive-matter/bson-c  (MongoDB C Driver)
https://github.com/mongodb/mongo-c-driver/tree/master/src (another MongoDB c driver, offical version)

