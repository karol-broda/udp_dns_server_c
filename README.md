# Simple DNS Server in C
![C](https://img.shields.io/badge/C-%23A8B9CC.svg?style=for-the-badge&logo=c&logoColor=white)

### about
a (hopefully) lightweight, custom dns server implemented in c, designed to handle basic dns queries using predefined mappings from a json file. this server supports common dns record types such as a, aaaa, cname, mx, and ns records, and includes wildcard domain support.

## table of contents

- [features](#simple-features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Building the Project](#building-the-project)
- [Running the DNS Server](#running-the-dns-server)
- [DNS Mappings JSON Structure](#dns-mappings-json-structure)
- [Testing the DNS Server](#testing-the-dns-server)
- [Example Usage](#example-usage)
- [Contributing](#contributing)
- [License](#license)

## simple features

- **Customizable DNS Mappings:** Define DNS records in a JSON file with support for multiple record types.
- **Wildcard Domains:** Supports wildcard entries (e.g., `*.example.com`) to match subdomains.
- **Common DNS Record Types:** Handles A, AAAA, CNAME, MX, and NS records.
- **Lightweight Implementation:** Minimal dependencies and straightforward codebase for easy understanding and modification.

## prerequisites

- **C Compiler:** GCC or any compatible C compiler
- **Make:** for building the project using the provided `Makefile`.
- **libs:**
  - **cJSON:** included in the `lib/cJSON/` directory.
  - **uthash:** included in the `lib/uthash/` directory.

## installation

1. **clone the Repository:**

   ```bash
   git clone https://github.com/karol-broda/udp_dns_server_c.git
   cd udp_dns_server_c
   ```

2. **Directory Structure Overview:**

   ```
   .
   ├── Makefile
   ├── README.md
   ├── include
   │   ├── dns_parser.h
   │   ├── dns_records.h
   │   └── dns_server.h
   ├── lib
   │   ├── cJSON
   │   │   ├── cJSON.c
   │   │   └── cJSON.h
   │   └── uthash
   │       └── uthash.h
   └── src
       ├── dns_mappings.json
       ├── dns_parser.c
       ├── dns_server.c
       └── main.c
   ```

## building the project

1. **clean prev builds:**
   ```bash
   make clean
   ```

2. **compile the DNS server:**

   ```bash
   make
   ```

   This will compile the source files and produce an executable named `dns_server`

## running the dns server

1. **start the DNS server:**

   ```bash
   ./dns_server
   ```

   - the server listens on port `2053` by default for now.
   - ensure you have the necessary permissions or change the port to a non-privileged one (>1024).

2. **modify DNS port (optional):**

   if you wish to change the port number, edit `DNS_PORT` in `include/dns_server.h`:

   ```c
   #define DNS_PORT 2053  // change to your desired port number
   ```

   recompile the project after making changes:

   ```bash
   make clean
   make
   ```

## DNS Mappings JSON Structure

the DNS mappings are defined in a JSON file located at `src/dns_mappings.json`. this file specifies the DNS records for various domains.

### **Structure Overview**

the JSON file is structured as an object where each key is a domain name, and its value is another object containing DNS record types and their corresponding values.

```json
{
  "example.com": {
    "A": ["93.184.216.34"],
    "AAAA": ["2606:2800:220:1:248:1893:25c8:1946"],
    "CNAME": ["alias.example.com"],
    "MX": [
      {
        "priority": 10,
        "value": "mail.example.com"
      }
    ],
    "NS": ["ns1.example.com", "ns2.example.com"]
  },
  "*.example.com": {
    "A": ["93.184.216.35"]
  }
}
```

### **supported record types**

- **A:** ipv4 addresses
- **AAAA:** ipv6 addresses
- **CNAME:** canonical name records
- **MX:** mail exchange records, with `priority` and `value`
- **NS:** name server records

### **defining records**

- **A and AAAA Records:**

  ```json
  "example.com": {
    "A": ["93.184.216.34"],
    "AAAA": ["2606:2800:220:1:248:1893:25c8:1946"]
  }
  ```

- **CNAME Record:**

  ```json
  "example.com": {
    "CNAME": ["alias.example.com"]
  }
  ```

- **MX Records:**

  ```json
  "example.com": {
    "MX": [
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

- **NS Records:**

  ```json
  "example.com": {
    "NS": ["ns1.example.com", "ns2.example.com"]
  }
  ```

- **Wildcard Domains:**

  use `*` to define wildcard domains

  ```json
  "*.example.com": {
    "A": ["93.184.216.35"]
  }
  ```

### **notes:**

- **record values:**
  - for **A** and **AAAA** records, provide an array of IP addresses as strings
  - for **CNAME** and **NS** records, provide an array of domain names as strings
  - for **MX** records, provide an array of objects, each with a `priority` (integer) and a `value` (domain name)

- **wildcard domains:**
  - wildcard domains match any subdomain not explicitly defined
  - place the wildcard entry at the appropriate level in the JSON hierarchy

## testing the DNS server

use the `dig` command-line tool to test your DNS server

1. **query A Record:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com A
   ```

2. **query AAAA Record:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com AAAA
   ```

3. **query CNAME Record:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com CNAME
   ```

4. **query MX Record:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com MX
   ```

5. **query NS Record:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com NS
   ```

6. **query Wildcard Domain:**

   ```bash
   dig @127.0.0.1 -p 2053 sub.example.com A
   ```

## example usage

here is an example of how the server handles queries:

1. **start the server:**

   ```bash
   ./dns_server
   ```

2. **example server output:**

   ```plaintext
   Loading DNS mappings...
   Adding records for domain: example.com, type: A
   Added value: 93.184.216.34
   Added A record for example.com with 1 values
   DNS server started on port 2053...
   ```

3. **client query:**

   ```bash
   dig @127.0.0.1 -p 2053 example.com A
   ```

4. **server response:**

   ```plaintext
   Received query for domain: example.com, type: A
   Exact match found for domain example.com and type A
   Resolved for: example.com, type: A
   Response sent to: 127.0.0.1
   ```

5. **client output:**

   ```plaintext
   ; <<>> DiG 9.10.6 <<>> @127.0.0.1 -p 2053 example.com A
   ;; global options: +cmd
   ;; Got answer:
   ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345
   ;; flags: qr aa rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

   ;; QUESTION SECTION:
   ;example.com.                   IN      A

   ;; ANSWER SECTION:
   example.com.            3600    IN      A       93.184.216.34

   ;; Query time: 1 msec
   ;; SERVER: 127.0.0.1#2053(127.0.0.1)
   ;; WHEN: Mon Jan 01 12:00:00 UTC 2024
   ;; MSG SIZE  rcvd: 60
   ```

## contributing

contributions are welcome! please follow these steps:

1. **fork the repository:**

   click the "fork" button at the top right of the repository page

2. **clone your fork:**

   ```bash
   git clone https://github.com/karol-broda/udp_dns_server_c.git
   cd udp_dns_server_c
   ```

3. **create a new Branch:**

   ```bash
   git checkout -b feature/your-fancy-feature-name
   ```

4. **make changes and commit:**

   ```bash
   git add .
   git commit -m "add fancy feature"
   ```

5. **push to your fork:**

   ```bash
   git push origin feature/your-fancy-feature-name
   ```

6. **create a pull request:**

   go to the original repository and open a pull request from your forked branch

## License

This project is licensed under the MIT License

---
![Disclaimer](https://img.shields.io/badge/Disclaimer-Important-red?style=for-the-badge)
**Disclaimer:** this DNS server is intended for educational and testing purposes. it is not recommended to use this server in a production environment without proper security reviews and testing. (Alltough if you use it in a production environment, please contact me and tell me how it went/if you need spiritual welfare)
