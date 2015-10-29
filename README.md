Cedos
============

About Cedos project
---------------------
Cedos is a "Cellular Data Offloading System" for delay-tolerant
mobile apps.

Despite the great potential of delay-tolerant Wi-Fi offloading,
existing mobile apps rarely support delayed offloading. This is
mainly because the burden of handling network disruption or delay
is placed solely on app developers.

Existing network stacks are not suitable or clumsy at addressing
network disruptions/delays, and popular servers are unfriendly to
delay-tolerant apps that intermittently download the content in
multiple networks. We find that many popular mobile apps do not
properly address network switchings and few apps correctly handle
a few minutes of delay between network connections.

Cedos is a practical delay-tolerant mobile network access architecture,
which enables easy development of delay-tolerant mobile apps by hiding
the complexity of handling mobility events in a new transport layer.
At the same time, Cedos allows developers to express their QoE
requirements (e.g., maximum user-specified delays), and exploit the
delays in mobile apps in order to maximize opportunistic Wi-Fi usage.

Please refer to http://cedos.kaist.edu/ for more details.



What's here?
------------
The main components of this distribution are:

* D2TP (libdtp), a transport layer protocol for mobile apps, providing
  TCP-like, reliable data transfer in stationary environments.
  D2TP hides network disruptions and allows delays when a mobile device
  is on the move. See [./libdtp/README.md] for more details.

* D2Prox (dprox-nginx), a network-embedded Web caching proxy, which
  works as a protocol translator between D2TP and legacy TCP.
  See [./dprox-nginx/README.md] for more details.

* ReadyCast, a delay-tolerant Android podcast downloader.
  ReadyCast supports subscription to any podcast feed published in the
  Internet. It allows the user to set the deadline of a podcast content
  download, benefiting from the D2TP layer for transparent delay management.
  See [./ReadyCast/README.md] for more details.


Contact
---------
* Homepage: http://cedos.kaist.edu/