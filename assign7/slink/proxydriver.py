#!/usr/bin/python

import threading
import subprocess
import getopt
import shutil
import sys
import os
import pycurl
import time
import StringIO
import hashlib
import tempfile
import socket

class RequestResult(object):
    """Represents the result of a request"""
    def __init__(self, curl_obj, body):
        self.body = body
        self.status = curl_obj.getinfo(pycurl.HTTP_CODE)

    def success(self):
        return self.status == 200

    def print_result(self, hash_body=False):
        """Prints the result. If hash_body is True, a hash of the body is printed instead of the body itself,
        or if hash_body is None, the body won't be printed at all"""
        print("Status: %d" % self.status)
        if hash_body:
            print("Body hash: %s" % hashlib.md5(self.body).hexdigest())
            print('')
        elif hash_body is not None:
            print("Body:")
            print(self.body)
            print('')

class RequestFailure(RequestResult):
    """Represents a failed request"""
    def __init__(self, errstr):
        self.errstr = errstr
        self.status = None

    def success(self):
        return False

    def print_result(self, hash_body=False):
        print("Request failed: %s" % self.errstr)

class ConcurrentRequest(threading.Thread):
    """Thread subclass to handle concurrent requests"""
    def __init__(self, test, url):
        super(ConcurrentRequest, self).__init__()
        self.url = url
        self.test = test
        self.result = None
    
    def run(self):
        self.result = self.test.do_request(self.url)

class ProxyTest(object):
    def __init__(self, port):
        self.server_host = 'http://cs110-proxy.jerrycainjr.com'
        self.blocked_host = 'http://blacklisted.cs110-proxy.jerrycainjr.com'

        self.proxy = 'localhost:%d' % port
        self.curlshare = pycurl.CurlShare() # Needed to share cookies across threads
        self.curlshare.setopt(pycurl.SH_SHARE, pycurl.LOCK_DATA_COOKIE)

    def do_request(self, url, headers=None):
        """Makes a request through the proxy. Shares cookies across the session"""
        body = StringIO.StringIO()

        curl = pycurl.Curl()
        curl.setopt(pycurl.PROXY, self.proxy)
        curl.setopt(pycurl.URL, url)
        if headers is not None:
            curl.setopt(pycurl.HTTPHEADER, headers)
        curl.setopt(pycurl.SHARE, self.curlshare)
        curl.setopt(pycurl.WRITEFUNCTION, body.write)
        try:
            curl.perform()
        except pycurl.error, error:
            result = RequestFailure(error[1])
            curl.close()
            return result

        result = RequestResult(curl, body.getvalue())
        curl.close()
        return result

    def get_cookie(self):
        """Makes a request, which will be different from all later requests, go get a session cookie"""
        return self.do_request(self.server_host + '/basic-cacheable?printrequests&foo')

    def basic_test(self, urls, get_cookie=True, hash_body=False, start_request=0):
        """Runs a basic test with requests to multiple URLs"""
        # Make sure we have a cookie first
        if get_cookie:
            cookie_result = self.get_cookie()
            if not cookie_result.success():
                print("Failed to get session cookie")
                cookie_result.print_result()
                return

        results = []
        for url in urls:
            results.append(self.do_request(url))

        for (i, result) in enumerate(results):
            print("********** Request %d **********" % (i + start_request))
            result.print_result(hash_body)

    def blocked_test(self):
        """Runs a test to ensure requests are blocked correctly"""
        urls = [self.blocked_host + '/forbidden-cacheable', self.blocked_host + '/forbidden-uncacheable']
        # Make sure we have a cookie first
        cookie_result = self.get_cookie()
        if not cookie_result.success():
            print("Failed to get session cookie")
            cookie_result.print_result()
            return

        results = []
        for url in urls:
            results.append(self.do_request(url))

        for (i, result) in enumerate(results):
            print("********** Request %d **********" % i)
            if result.status is None: # Request totally failed (couldn't connect)
                print("Request errored out (not good)")
                result.print_result()
            elif result.status == 0: # Only happens when the response code can't be found
                print("Blocked") # This is a HACK to give points to students who forgot to set the protocol
                # print("Returned status 0 (malformed http response)")
            elif "You shouldn't be able to see this!" in result.body: # Definitely not blocked
                print("Not blocked")
            else: # String wasn't found
                print("Blocked")

        # Get info about how many requests reached the server
        self.basic_test([self.server_host + '/basic-uncacheable?printrequests'], get_cookie=False, start_request=len(results))

    def header_test(self, hash_body=False):
        """Tests the handling of the proxy headers"""
        cookie_result = self.get_cookie()
        if not cookie_result.success():
            print("Failed to get session cookie")
            cookie_result.print_result()
            return

        results = []
        results.append(self.do_request(self.server_host + '/basic-cacheable?printheaders'))
        results.append(self.do_request(self.server_host + '/basic-cacheable?printheaders', headers=['x-forwarded-for: 10.11.12.13',]))
        results.append(self.do_request(self.server_host + '/basic-uncacheable?printheaders'))

        for (i, result) in enumerate(results):
            print("********** Request %d **********" % i)
            result.print_result(hash_body)

    def proxy_cycle_test(self, hash_body=False):
        """Tests the handling of the cycles in proxy chaining. Ensures that 
        cycles are detected and that each request receives a response with error code 504.
        If no errors found, this function prints out nothing. """
                
        results = []
        results.append(self.do_request(self.server_host + '/static/plaintext.txt'))

        gateway_error_received = False
        for (i, result) in enumerate(results):
            # Check to see if a 504 is present in the result, either as the error code or at
            # the beginning of the body if they forgot to set the protocol to HTTP
            if result.status == 504 or (result.status == 0 and result.body.split()[0] == '504'): # This is good! 504 found!
                 gateway_error_received = True
            else:
                print("Found result: ")
                result.print_result(False)

        if not gateway_error_received: # This is bad!
            print("ERROR: A cycle was found in the chain of proxies, so we should have received a 504 error.")
            print("No such error received!");
        else:
            print("Looks like you detected a cycle.")

    def concurrent_test(self, blocking_url, num_blocking_calls, delay, other_urls, hash_body=False):
        """Runs a bunch of threads to blocking_url, and then sequentially runs other_urls before joining"""
        # Make sure we have a cookie first
        cookie_result = self.get_cookie()
        if not cookie_result.success():
            print("Failed to get session cookie")
            cookie_result.print_result()
            return

        threads = []
        for i in xrange(num_blocking_calls):
            if callable(blocking_url):
                url = blocking_url(i)
            else:
                url = blocking_url

            t = ConcurrentRequest(self, url)
            threads.append(t)
            t.start()

        time.sleep(delay)
        good = True
        if delay > 0:
            for t in threads:
                if not t.is_alive():
                    print("Thread died early! Result:\n")
                    t.result.print_result()
                    good = False

        if good: # Assuming no theads died early, run the rest of the tests and get the results from the threads
            self.basic_test(other_urls, get_cookie=False)

            results = []
            for t in threads:
                t.join()
                results.append((t.url, t.result))

            for (url, result) in sorted(results):
                print("********** CONCURRENT Request to url %s **********" % url)
                result.print_result(hash_body)

    def get_poll_url(self, i):
        """Converts a request index into a long-poll url"""
        return self.server_host + '/long-poll?id=%d' % i

    def get_static_url(self, i):
        """Converts a request index into a static resource url"""
        return self.server_host + '/static/%d.html' % (int(i / 3) + 1)

    def run_test(self, test):
        """Dispatches to the appropriate test"""
        if test == 'basic-caching': # Makes two requests, and ensures the second is from the cache
            self.basic_test([self.server_host + '/basic-cacheable?printrequests', self.server_host + '/basic-cacheable?printrequests'])
        elif test == 'no-extra-requests': # Makes two cacheable requests, and ensures that the second doesn't reach the server
            self.basic_test([self.server_host + '/basic-cacheable', self.server_host + '/basic-cacheable', self.server_host + '/basic-uncacheable?printrequests'])
        elif test == 'no-invalid-caching': # Makes two requests where caching should not be allowed
            self.basic_test([self.server_host + '/basic-uncacheable?printrequests', self.server_host + '/basic-uncacheable?printrequests'])
        elif test == 'headers': # Verifies the correct headers are sent to the server
            self.header_test()
        elif test == 'blocked': # Makes sure no requests for items in the blocklist get through
            self.blocked_test()
        elif test == 'concurrency': # Makes sure the thread pool works and is (at least) the correct size
            self.concurrent_test(self.get_poll_url, 15, 8.0, [self.server_host + '/long-poll-release?printrequests',])
        elif test == 'load': # Serves a bunch of static files and makes sure they all load
            self.concurrent_test(self.get_static_url, 21, 0, [], hash_body=True)
        elif test == 'simultaneous': # Makes sure that a second request to the same resource doesn't go through
            self.concurrent_test(self.server_host + '/delayed-request?printrequests', 1, 5.0, [self.server_host + '/delayed-request?printrequests',])
        elif test == 'basic-static-html':
            self.basic_test([self.server_host + '/static/1.html',], get_cookie=False, hash_body=True)
        elif test == 'basic-static-img':
            self.basic_test([self.server_host + '/static/logo4w.png',], get_cookie=False, hash_body=True)
        elif test == 'basic-static-text':
            self.basic_test([self.server_host + '/static/plaintext.txt',], get_cookie=False, hash_body=True)
        elif test == 'proxy-cycle':
            self.proxy_cycle_test()
        else:
            print("Unknown test: %s" % test)

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "", ["port=", "test=", "proxy=", "cwd=", "secondary-proxy-port="])
    except getopt.GetoptError as err:
        print str(err)
        sys.exit(2)
    test = None
    proxy_port = 12345
    proxy_executable = None
    proxy_cwd = None
    secondary_proxy_port = None
    for o, a in opts:
        if o == "--port":
            proxy_port = int(a)
        elif o == "--test":
            test = a
        elif o ==  "--proxy":
            proxy_executable = a
        elif o == "--cwd":
            proxy_cwd = a
        elif o == "--secondary-proxy-port":
            secondary_proxy_port = int(a)
        else:
            assert False, "unhandled option"
    assert test is not None, "test must be specified"
    assert proxy_executable is not None, "proxy executable must be specified"
    
    using_proxy_chaining = (secondary_proxy_port is not None)
    if test == "proxy-cycle":
        assert using_proxy_chaining, "The secondary-proxy-port flag is required for the proxy-cycle test"

    cache_dir = os.path.expanduser('~/.proxy-cache-' + socket.gethostname())
    shutil.rmtree(cache_dir, ignore_errors=True)
    
    # Get a place to store the stdout/stderr
    log_file = tempfile.TemporaryFile()
    
    try:
        if not using_proxy_chaining:
            proxy = subprocess.Popen([proxy_executable, "--port", str(proxy_port)], cwd=proxy_cwd, stdout=log_file, stderr=subprocess.STDOUT)
        else:
            proxy = subprocess.Popen([proxy_executable, "--port", str(proxy_port), "--proxy-server", "localhost", "--proxy-port", str(secondary_proxy_port)], cwd=proxy_cwd, stdout=log_file, stderr=subprocess.STDOUT)
    except:
        print("Couldn't start proxy")
        return

    if using_proxy_chaining:
        try:
            secondary_proxy = subprocess.Popen([proxy_executable, "--port", str(secondary_proxy_port), "--proxy-server", "localhost", "--proxy-port", str(proxy_port)], cwd=proxy_cwd, stdout=log_file, stderr=subprocess.STDOUT)
            #secondary_proxy = subprocess.Popen([proxy_executable, "--port", str(secondary_proxy_port)], cwd=proxy_cwd, stdout=log_file, stderr=subprocess.STDOUT)
        except:
            print("Couldn't start secondary proxy")
            return
    #elif test == 'proxy-chain-basic':
    #    try:
    #        exec_command = "ssh " + os.environ["USER"] + "@myth12.stanford.edu 'cd /usr/class/cs110/staff/bin/master-repos/assign6-soln/; " + proxy_executable + " --port 56565'" 
    #        print exec_command
    #        secondary_proxy = subprocess.Popen(exec_command, cwd=proxy_cwd, stdout=log_file, stderr=subprocess.STDOUT)
    #    except:
    #        print("Couldn't start secondary proxy")
    #        return
   

    # try:
    time.sleep(5) # Make sure the proxy is running
    pycurl.global_init(pycurl.GLOBAL_DEFAULT)
    test_runner = ProxyTest(proxy_port)
    test_runner.run_test(test)
    pycurl.global_cleanup()

    return_code = proxy.poll() # Get the return status if it already exited (crashed)

    if return_code is None: # Hopefully the proxy was still running
        proxy.terminate()
        proxy.wait()
    else:
        log_file.seek(0)
        print("Proxy crashed; return code: %d, output:" % return_code)
        print(log_file.read())

    if using_proxy_chaining:
        secondary_return_code = secondary_proxy.poll();
        if secondary_return_code is None:
            secondary_proxy.terminate()
            secondary_proxy.wait()

    # except: # Make sure the proxy is dead
    #     print("Something went wrong. Killing the proxy.")
    #     proxy.kill()
    #     proxy.wait()
    #     raise
    # finally:
    shutil.rmtree(cache_dir, ignore_errors=True) # Clean up

if __name__ == '__main__':
    main()
