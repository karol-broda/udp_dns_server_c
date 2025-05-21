# simple dns server in c

![c](https://img.shields.io/badge/C-%23A8B9CC.svg?style=for-the-badge&logo=c&logoColor=white)

this monorepo contains a lightweight dns server implemented in c that handles basic dns queries using predefined mappings from a json file.

## table of contents

- [simple dns server in c](#simple-dns-server-in-c)
  - [table of contents](#table-of-contents)
  - [about](#about)
  - [getting started](#getting-started)
    - [prerequisites](#prerequisites)
    - [installation](#installation)
    - [building the project](#building-the-project)
  - [usage](#usage)
    - [running the dns server](#running-the-dns-server)
    - [dns management interface](#dns-management-interface)
      - [using the management script](#using-the-management-script)
      - [management interface protocol](#management-interface-protocol)
      - [scopes explained](#scopes-explained)
    - [dns mappings structure](#dns-mappings-structure)
      - [structure overview](#structure-overview)
      - [supported record types](#supported-record-types)
      - [defining records](#defining-records)
    - [testing the server](#testing-the-server)
  - [example usage](#example-usage)
  - [troubleshooting](#troubleshooting)
    - [server won't start](#server-wont-start)
    - [management interface issues](#management-interface-issues)
  - [security considerations](#security-considerations)
  - [contributing](#contributing)
  - [license](#license)

## about

a lightweight, custom dns server implemented in c, designed to handle basic dns queries using predefined mappings from a json file. supports common dns record types such as a, aaaa, cname, mx, and ns records, and includes wildcard domain support. it also features a management interface for dynamically adding and removing dns records without restarting the server.

## getting started

### prerequisites

- **c compiler:** gcc or any compatible c compiler
- **make:** for building the project using the provided `makefile`
- **libraries:**
  - **cjson:** included in the `lib/cjson/` directory
  - **uthash:** included in the `lib/uthash/` directory
- **system dependencies:**
  - posix threads (pthread)
  - standard c libraries

### installation

1. clone the repository:
   ```bash
   git clone https://github.com/karol-broda/udp_dns_server_c.git
   cd udp_dns_server_c
   ```

### building the project

1. clean previous builds:
   ```bash
   make clean
   ```

2. compile the dns server:
   ```bash
   make
   ```
   this will compile the source files and produce an executable named `dns_server`.

3. install the dns server (optional):
   ```bash
   sudo make install
   ```
   this will install the dns server and management script to `/usr/local/bin/`.

## usage

### running the dns server

start the dns server:
```bash
./dns_server
```

the server listens on port `2053` by default for dns queries and the management interface listens on port `8053` by default.

you can modify the following configuration options by editing [include/dns_server.h](./include/dns_server.h):
```c
#define DEFAULT_DNS_PORT 2053      // dns service port
#define DEFAULT_MGMT_PORT 8053     // management interface port
#define DEFAULT_MAPPINGS_FILE "dns_mappings.json"  // path to dns mappings
#define DEFAULT_TTL 3600           // default ttl for dns records
#define DEFAULT_AUTH_TOKEN "change_this_token"     // auth token
```

**important:** be sure to change the default authentication token before deploying to production!

### dns management interface

the dns server includes a management interface that allows you to dynamically add, delete, and modify dns records without restarting the server.

#### using the management script

the easiest way to manage the dns server is using the included `dns_mgmt.sh` script:

```bash
# make the script executable (if not already)
chmod +x dns_mgmt.sh

# list all current dns records
./dns_mgmt.sh list

# add a new a record
./dns_mgmt.sh add example.com a base 192.168.1.1

# add a new mx record
./dns_mgmt.sh add example.com mx base "10 mail.example.com"

# delete a record
./dns_mgmt.sh delete example.com a base

# reload dns mappings from the configuration file
./dns_mgmt.sh reload
```

#### management interface protocol

the management interface uses a simple text-based protocol over tcp. each command follows this format:

```
<auth_token> <command> <parameters...>
```

available commands:
- `add <domain> <type> <scope> <value>` - add a new dns record
- `delete <domain> <type> <scope>` - delete a dns record
- `list` - list all dns records
- `reload` - reload dns mappings from the configuration file

for example, to add a new a record manually:

```bash
echo "change_this_token add example.com a base 192.168.1.1" | nc localhost 8053
```

#### scopes explained

when adding or deleting records, you need to specify a scope:

- `base` - regular domain records
- `wildcard` - wildcard domain records (*.domain)
- `subdomain` - specific subdomain records

### dns mappings structure

the dns mappings are defined in a json file located at `src/dns_mappings.json`. this file specifies the dns records for various domains.

#### structure overview

the json file is structured with a top-level `"domains"` object. each key within `"domains"` is a domain name, and its value is another object containing `"records"`, and optionally `"wildcards"` and `"subdomains"`.

```json
{
  "domains": {
    "example.com": {
      "records": {
        "a": ["93.184.216.34"],
        "aaaa": ["2606:2800:220:1:248:1893:25c8:1946"],
        "cname": ["alias.example.com"],
        "mx": [
          {
            "priority": 10,
            "value": "mail.example.com"
          }
        ],
        "ns": ["ns1.example.com", "ns2.example.com"]
      },
      "wildcards": {
        "records": {
          "a": ["93.184.216.35"]
        }
      },
      "subdomains": {
        "subdomain1": {
          "records": {
            "a": ["93.184.216.36"]
          }
        }
      }
    }
  }
}
```

#### supported record types

- **a:** ipv4 addresses
- **aaaa:** ipv6 addresses
- **cname:** canonical name records
- **mx:** mail exchange records, with `priority` and `value`
- **ns:** name server records
- **txt:** text records
- **srv:** service records

#### defining records

- **a and aaaa records:**

  ```json
  "records": {
    "a": ["93.184.216.34"],
    "aaaa": ["2606:2800:220:1:248:1893:25c8:1946"]
  }
  ```

- **cname record:**

  ```json
  "records": {
    "cname": ["alias.example.com"]
  }
  ```

- **mx records:**

  ```json
  "records": {
    "mx": [
      {
        "priority": 10,
        "value": "mail.example.com"
      },
      {
        "priority": 20,
        "value": "mail2.example.com"
      }
    ]
  }
  ```

- **ns records:**

  ```json
  "records": {
    "ns": ["ns1.example.com", "ns2.example.com"]
  }
  ```

- **wildcard domains:**

  use the `"wildcards"` key within a domain to define wildcard records.

  ```json
  "wildcards": {
    "records": {
      "a": ["93.184.216.35"]
    }
  }
  ```

### testing the server

use the `dig` command-line tool to test your dns server:

```bash
# query a record
dig @127.0.0.1 -p 2053 example.com a

# query aaaa record
dig @127.0.0.1 -p 2053 example.com aaaa

# query cname record
dig @127.0.0.1 -p 2053 example.com cname

# query mx record
dig @127.0.0.1 -p 2053 example.com mx

# query ns record
dig @127.0.0.1 -p 2053 example.com ns

# query wildcard domain
dig @127.0.0.1 -p 2053 sub.example.com a
```

## example usage

here is an example of how the server handles queries:

1. start the server:

   ```bash
   ./dns_server
   ```

2. example server output:

   ```plaintext
   [2025-05-21 10:15:32] [info] starting dns server...
   [2025-05-21 10:15:32] [info] loading dns mappings from dns_mappings.json
   [2025-05-21 10:15:32] [info] adding records for domain: example.com, type: a
   [2025-05-21 10:15:32] [info] added a record for example.com with 1 values
   [2025-05-21 10:15:32] [info] dns server started on port 2053
   [2025-05-21 10:15:32] [info] dns management interface listening on port 8053
   ```

3. add a new record using the management interface:

   ```bash
   ./dns_mgmt.sh add test.com a base 192.168.1.10
   ```

4. result:

   ```plaintext
   success: record added
   ```

5. server log output:

   ```plaintext
   [2025-05-21 10:16:45] [info] adding records for domain: test.com, type: a
   [2025-05-21 10:16:45] [info] added a record for test.com with 1 values
   ```

6. client query:

   ```bash
   dig @127.0.0.1 -p 2053 test.com a
   ```

7. server log output:

   ```plaintext
   [2025-05-21 10:17:10] [info] received query for domain: test.com, type: a
   [2025-05-21 10:17:10] [info] exact match found for domain test.com and type a
   [2025-05-21 10:17:10] [info] resolved for: test.com, type: a
   [2025-05-21 10:17:10] [info] response sent to: 127.0.0.1
   ```

8. client output:

   ```plaintext
   ; <<>> dig 9.10.6 <<>> @127.0.0.1 -p 2053 test.com a
   ;; global options: +cmd
   ;; got answer:
   ;; ->>header<<- opcode: query, status: noerror, id: 12345
   ;; flags: qr aa rd; query: 1, answer: 1, authority: 0, additional: 0
   
   ;; question section:
   ;test.com.                      in      a
   
   ;; answer section:
   test.com.               3600    in      a       192.168.1.10
   
   ;; query time: 1 msec
   ;; server: 127.0.0.1#2053(127.0.0.1)
   ;; when: wed may 21 10:17:10 utc 2025
   ;; msg size  rcvd: 45
   ```

## troubleshooting

### server won't start

if the server fails to start:

1. check that the specified ports (2053 and 8053 by default) are available:
   ```bash
   netstat -tuln | grep 2053
   netstat -tuln | grep 8053
   ```

2. ensure you have the necessary permissions to bind to the ports:
   ```bash
   # if running on ports below 1024
   sudo ./dns_server
   ```

3. verify the dns_mappings.json file exists and is valid:
   ```bash
   jq . src/dns_mappings.json
   ```

### management interface issues

if you're having trouble with the management interface:

1. verify the authentication token is correct:
   ```bash
   # check the token in the source code
   grep -r "DEFAULT_AUTH_TOKEN" include/
   ```

2. ensure tcp connectivity to the management port:
   ```bash
   nc -zv localhost 8053
   ```

3. check server logs for any error messages related to the management interface.

## security considerations

1. **authentication token**: the management interface requires an authentication token. change the default token (`default_auth_token` in `dns_server.h`) before deploying.

2. **firewall rules**: configure firewall rules to restrict access to the management port (8053 by default).

3. **run as non-root**: for production use, consider running the dns server as a non-privileged user after binding to the ports.

4. **tls encryption**: for added security, consider implementing tls encryption for the management interface.

## contributing

contributions are welcome! please follow these steps:

1. fork the repository:
   click the "fork" button at the top right of the repository page.

2. clone your fork:
   ```bash
   git clone https://github.com/karol-broda/udp_dns_server_c.git
   cd udp_dns_server_c
   ```

3. create a new branch:
   ```bash
   git checkout -b feature/your-fancy-feature-name
   ```

4. make changes and commit:
   ```bash
   git add .
   git commit -m "add fancy feature"
   ```

5. push to your fork:
   ```bash
   git push origin feature/your-fancy-feature-name
   ```

6. create a pull request:
   go to the original repository and open a pull request from your forked branch.

## license

this project is licensed under the mit license.

---
![disclaimer](https://img.shields.io/badge/Disclaimer-Important-red?style=for-the-badge)
<br />
**disclaimer:** this dns server is intended for educational and testing purposes. it is not recommended to use this server in a production environment without proper security reviews and testing. (although if you use it in a production environment, please contact me and tell me how it went/if you need spiritual welfare)