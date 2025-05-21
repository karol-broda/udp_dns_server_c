#!/bin/bash

SERVER="localhost"
PORT="8053"
AUTH_TOKEN="123456"  # match the token in dns_server.h

function usage {
    echo "usage: $0 [OPTIONS] COMMAND"
    echo ""
    echo "Options:"
    echo "  -s, --server SERVER    Server address (default: localhost)"
    echo "  -p, --port PORT        Server port (default: 8053)"
    echo "  -t, --token TOKEN      Authentication token"
    echo "  -h, --help             Show this help message"
    echo ""
    echo "Commands:"
    echo "  add DOMAIN TYPE SCOPE VALUE"
    echo "    Add a new DNS record"
    echo "    Example: $0 add example.com A base 192.168.1.1"
    echo "    Example: $0 add example.com MX base \"10 mail.example.com\""
    echo ""
    echo "  delete DOMAIN TYPE SCOPE"
    echo "    Delete a DNS record"
    echo "    Example: $0 delete example.com A base"
    echo ""
    echo "  list"
    echo "    List all DNS records"
    echo ""
    echo "  reload"
    echo "    Reload DNS records from configuration file"
    echo ""
    echo "Scopes:"
    echo "  base      - Regular domain records"
    echo "  wildcard  - Wildcard domain records (*.domain)"
    echo "  subdomain - Specific subdomain records"
    echo ""
    exit 1
}

function send_command {
    echo -e "$AUTH_TOKEN $1" | nc $SERVER $PORT
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--server)
            SERVER="$2"
            shift 2
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -t|--token)
            AUTH_TOKEN="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            break
            ;;
    esac
done

if [ $# -lt 1 ]; then
    usage
fi

case "$1" in
    add)
        if [ $# -lt 5 ]; then
            echo "Error: 'add' command requires DOMAIN TYPE SCOPE VALUE"
            usage
        fi
        send_command "ADD $2 $3 $4 $5"
        ;;
    delete)
        if [ $# -lt 4 ]; then
            echo "Error: 'delete' command requires DOMAIN TYPE SCOPE"
            usage
        fi
        send_command "DELETE $2 $3 $4"
        ;;
    list)
        send_command "LIST"
        ;;
    reload)
        send_command "RELOAD"
        ;;
    *)
        echo "Error: Unknown command '$1'"
        usage
        ;;
esac