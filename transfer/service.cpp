// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os>
#include <net/interfaces>

/**
 * An example to show incoming and outgoing TCP Connections.
 * In this example, IncludeOS is listening on port 80.
 *
 * Data received on port 80 will be redirected to a
 * outgoing connection to a (in this case) python server (server.py)
 *
 * Data received from the python server connection
 * will be redirected back to the client.
 *
 * To try it out, use netcat to connect to this IncludeOS instance.
**/

using Connection_ptr = net::tcp::Connection_ptr;
using Disconnect = net::tcp::Connection::Disconnect;

// Address to our python server: 10.0.2.2:1337
// @note: This may have to be modified depending on network and server settings.
net::Socket python_server{ {10,0,0,1} , 1337};

void Service::start()
{
#ifdef USERSPACE_LINUX
  extern void create_network_device(int N, const char* route, const char* ip);
  create_network_device(0, "10.0.0.0/24", "10.0.0.1");
#endif
  auto& inet = net::Interfaces::get(0);
  inet.network_config(
      {  10, 0,  0, 42 },  // IP
      { 255,255,255, 0 },  // Netmask
      {  10, 0,  0,  1 },  // Gateway
      {  10, 0,  0,  1 }); // DNS

  // Set up a TCP server on port 81
  auto& server = inet.tcp().listen(81);
  printf("Server listening: %s \n", server.local().to_string().c_str());

  // When someone connects to our server
  server.on_connect(
  [&inet] (Connection_ptr client) {
    printf("Connected [Client]: %s\n", client->to_string().c_str());
    // Make an outgoing connection to our python server
    auto outgoing = inet.tcp().connect(python_server);
    // When outgoing connection to python sever is established
    outgoing->on_connect(
    [client] (Connection_ptr python) {
        if (!python) {
          printf("Connection failed!\n");
          return;
        }
        printf("Connected [Python]: %s\n", python->to_string().c_str());

        // Setup handlers for when data is received on client and python connection
        // When client reads data
        client->on_read(1024, [python](auto buf) {
            python->write(buf);
          });

        // When client is disconnecting
        client->on_disconnect([python](Connection_ptr, Disconnect reason) {
            printf("Disconnected [Client]: %s\n", reason.to_string().c_str());
            python->close();
          });

        // When python is disconnecting
        python->on_disconnect([client](Connection_ptr, Disconnect reason) {
            printf("Disconnected [Python]: %s\n", reason.to_string().c_str());
            client->close();
          });
      }); // << onConnect (outgoing (python))
  }); // << onConnect (client)
}
