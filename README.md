# NOTES:

   * cannot use with web browser. the spec says that header fields are less than 128 bytes but when using a web browser is acutomatically adds a user agent field that is more than 128 bytes. must use curl instead
   * try to remove all the .txt files or random stuff added from testing before git commiting
   * i restructured a little bit - the request_handler.c uses request.c and response.c to handle a request fully. there is a handle_request() function that calls many other functions. thread_pool.c just calls the handle_request. eventually there will be threads that each individually call handle request with a client fd.
   * make sure you submit to autograder with the command down below (you need to include the unittest.c or else make all fails)
   * make sure to add your own ucsc config to the same file and then comment out mine when you autograder submit so we have more autograder attempts


# HELPFUL COMMANDS:

  # installs
   * apt update
   * apt install clang
   * apt install clang-format
   * pip3 install -r requirements.txt --break-system-packages

  # submitting
   * clang-format -i *.c *.h
   * python3 -m autograder.run.submit Makefile httpserver.c request_handler.c request_handler.h request.c request.h response.c response.h thread_pool.c thread_pool.h unit_test.c

  # testing
   * ./httpserver 5001
   * get: curl --http1.1 http://localhost:5001/foo.txt
   * put: curl -X PUT -d "hello please work i am creating a new file" http://localhost:5001/testing.txt
   * ( printf $'GET /foo.txt HTTP/9.1\r\n\r\n'; sleep 1 ) | nc localhost 5001
   * ( printf "GET /foo.txt HTTP/banana\r\n\r\n"; sleep 1 ) | nc localhost 5001

  # cleanup
   * make clean
   * find . -maxdepth 1 -type f -name "*.txt" ! -name "requirements.txt" -delete

# CURRENT SCORE: 60/170

   * for some reason I can't get any of the put tests to work even though it works on my machine
   * havent tested memory with valgrind yet but shouldnt be hard
   * hard to test invalid http version because you can't with curl with an invalid http version. I'm not sure why its failing either because it specifically checks for 1.1 in the request handler
   * audit log needs to be added should be easy
   * after all previous four bullet points are fixed, concurrency is need to be added. shouldn't be extremely hard but will take some time

Clang formatted files: 4 / 10
   [Fail] [0/1] httpserver.c not clang-formatted
   [Fail] [0/1] request.c not clang-formatted
   [Fail] [0/1] request.h not clang-formatted
   [Fail] [0/1] request_handler.c not clang-formatted
   [Pass] [1/1] request_handler.h clang-formatted
   [Fail] [0/1] response.c not clang-formatted
   [Pass] [1/1] response.h clang-formatted
   [Fail] [0/1] thread_pool.c not clang-formatted
   [Pass] [1/1] thread_pool.h clang-formatted
   [Pass] [1/1] unit_test.c clang-formatted
Make all builds program: 20 / 20
   [Pass] [4/4] make httpserver successful
   [Pass] [4/4] make successful
   [Pass] [4/4] make all successful
   [Pass] [4/4] make clean successful.
   [Pass] [4/4] make uses clang and correct flags
Tests for the third checkin: 67 / 70
   [Pass] [3/3] Perform get command for a small ascii file.
   [Pass] [3/3] Perform get command for a large ascii file.
   [Pass] [3/3] Perform get command for small binary file.
   [Pass] [3/3] Perform get command for large binary file.
   [Pass] [3/3] Perform put of a small ascii file.
   [Pass] [3/3] Perform put of a large ascii file.
   [Pass] [3/3] Perform put of a small binary file.
   [Pass] [3/3] Perform put of a large binary file.
   [Pass] [4/4] Perform many get commands.
   [Pass] [4/4] Perform many put commands.
   [Pass] [4/4] Test memory safety of gets with valgrind.
   [Pass] [4/4] Test memory safety of puts with valgrind.
   [Pass] [3/3] Perform put on an existing file.
   [Pass] [3/3] Try to get a file that does not exist.
   [Pass] [3/3] Use an invalid header on get.
   [Pass] [3/3] Try to get a directory.
   [Pass] [3/3] Put without a content length.
x   [Fail] [0/3] Use an invalid http version.
   [Pass] [3/3] Use an invalid http method.
   [Pass] [3/3] Use a request that does not end in CLRFCLRF.
   [Pass] [3/3] Use an valid, but unsupported, http version.
   [Pass] [3/3] Perform a few commands and check the audit log.
Tets for the fourth checkin: 40 / 70
   [Pass] [10/10] Execute httpserver with many requests to non-conflicting files.
   [Pass] [10/10] Execute httpserver with non-confliciting puts that pause.
   [Pass] [10/10] Execute httpserver with conflicting gets that pause.
   [Pass] [10/10] Execute httpserver with conflicting puts that pause.
x   [Fail] [0/30] Execute httpserver with many conflicting gets and puts.

Total: 131 / 170

