D2Prox server
=================
Upgrading the existing infrastructure (e.g., servers, middleboxes) to
support D2TP is costly and time-consuming. Instead, we propose placing a
network-embedded Web caching proxy, D2Prox, which works as a protocol
translator, allowing client-side mobility while it supports backward
compatibility to TCP-based servers. Although D2Prox can sit anywhere
in the Internet, we envision that it is located near a cellular ISP core
network to curb the latency stretch between an origin server and a client.

Note that D2Prox is built based on nginx (http://nginx.org) code.
We use web proxy module of nginx, and slightly modified it to
translate between D2TP and TCP.


Build/compile and run D2Prox server
-------------------------------------

0. Install prerequisite library
  ```
  sudo apt-get install libpcre3-dev (ubuntu)
  ```

1. Configure and build dprox-nginx
  ```
  cd dprox-nginx
  ./configure --with-select_module
  make
  sudo make install 
  ```

2. Go to nginx configuration folder and set up
  ```
  cd /usr/local/nginx
  ... (Refer to http://nginx.org/en/docs/)
  ```

3. Run dprox-nginx server
  ```
  sudo ./nginx
  ```


 