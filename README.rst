.. contents:: Table of Contents

why:

- mongo-php-drive is too heavy to create a connection
- too many php workers, too many connections

what's better: 

- single process, less ping to backend mongod
- one client => one server conn


design:

- one backend is a mongd
- backends became a replset
