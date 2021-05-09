## Unit-tests
The unit-tests use *Google Test* to test the functionality of the allocation and snapshot sub-systems.

## Dependencies
To satisfy the build dependencies for the unit-tests, 
you need to run `dependencies.sh` from the main directory.

## Environment
Make sure you have reserved huge-pages for the allocator and have an NVM file-system mounted at `/mnt/ram`.
Check the main documentation for details on how to prepare the environment.

## Building and running tests
Once you have all the dependencies installed and the environment ready, 
run the following commands to build and run the tests.

```bash
make
./test
```
