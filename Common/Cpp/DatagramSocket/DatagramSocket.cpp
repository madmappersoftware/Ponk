#include "DatagramSocket.h"
#include <cstring>
#include "errno.h"
#include <iostream>
#include <memory>

/*********************************************************************************
  UNIX version
*********************************************************************************/

#if defined(__linux__) || defined (__APPLE__)

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

std::vector<NetworkInterface> listIPv4Interfaces()
{
    std::vector<NetworkInterface> result;

    ifaddrs* rawList = nullptr;
    if (getifaddrs(&rawList) != 0 || rawList == nullptr)
        return result;

    const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> list(rawList, &freeifaddrs);

    for (const ifaddrs* ifa = list.get(); ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if ((ifa->ifa_flags & IFF_UP) == 0)
            continue;
        // keep loopback, skip interfaces that can't multicast or broadcast
        if ((ifa->ifa_flags & (IFF_MULTICAST | IFF_LOOPBACK | IFF_BROADCAST)) == 0)
            continue;

        const auto* sin = reinterpret_cast<const sockaddr_in*>(ifa->ifa_addr);
        const unsigned int hostOrder = ntohl(sin->sin_addr.s_addr);

        result.push_back(NetworkInterface(ipIntToStr(hostOrder), hostOrder));
    }

    return result;
}

DatagramSocket::DatagramSocket(unsigned int interfaceIP, unsigned int port):
    m_port(port)
{
    m_socket = socket(AF_INET,SOCK_DGRAM,0);
    if (m_socket==INVALID_SOCKET) {
        std::cout << "Error creating datagram socket on port " << m_port << std::endl;
        assert(false);
        return;
    }

    // reuse addr
    int yes=1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
        std::cout << "Error setting SO_REUSEADDR option" << std::endl;
        assert(false);
    }

    #ifdef SO_REUSEPORT
        if (setsockopt (m_socket, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) != 0) {
            std::cout << "Error setting SO_REUSEPORT option" << std::endl;
            assert(false);
        }
    #endif

    // If port is 0, bind anyway so when sending a packet the OS know on which network to send it
    // (in case both networks have the same IP mask (ie 192.168.1.xxx / 255.255.255.0)
    struct SOCKADDR_IN addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(interfaceIP);
    bzero(&(addr.sin_zero), 8);     /* zero the rest of the struct */
    if(bind(m_socket, (struct sockaddr*)&addr, sizeof(sockaddr)) != 0) {
        std::cout << "Failed to bind datagram socket on interface " << ipIntToStr(interfaceIP) << " / port " << port << std::endl;
        assert(false);
        if (port != 0) {
            closeSocket();
            return;
        }
    }

    // set non blocking
    if (fcntl(m_socket,F_SETFL,O_NONBLOCK) < 0) {
        std::cout << "Error setting O_NONBLOCK option" << std::endl;
        assert(false);
        closeSocket();
        return;
    }

    // enable broadcast
    if (setsockopt(m_socket,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(int)) != 0)
    {
        std::cout << "Error setting SO_BROADCAST option" << std::endl;
        assert(false);
        closeSocket();
        return;
    }
}

DatagramSocket::~DatagramSocket()
{
    closeSocket();
}

void DatagramSocket::closeSocket()
{
    if (m_socket != INVALID_SOCKET) {
        close(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool DatagramSocket::isInitialized()
{
    return (m_socket != INVALID_SOCKET);
}

bool DatagramSocket::joinMulticastGroup(unsigned int ip, unsigned int interfaceIP) {
    int res = 0;

    // Set TTL (Time To Live)
    int ttl = 127;
    res = setsockopt(m_socket,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl));
    if (res < 0) {
        std::cout << "Error in DatagramSocket: could not set TTL, error: " << strerror(errno) << std::endl;
        assert(false);
    }

    struct ip_mreq mreq;
    mreq.imr_interface.s_addr = htonl(interfaceIP);
    mreq.imr_multiaddr.s_addr = htonl(ip);
    res = setsockopt(m_socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));
    if (res < 0) {
        if (errno == EADDRINUSE) {
            // Ignore, it means we already joined this group
        } else {
            std::cout << "Error in DatagramSocket: IP_ADD_MEMBERSHIP error: " << strerror(errno) << std::endl;
            assert(false);
            return false;
        }
    }

    return true;
}

bool DatagramSocket::leaveMulticastGroup(unsigned int ip, unsigned int interfaceIP) {
    struct ip_mreq mreq;
    mreq.imr_interface.s_addr = htonl(interfaceIP);
    mreq.imr_multiaddr.s_addr = htonl(ip);
    auto res = setsockopt(m_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));
    if (res < 0) {
        std::cout << "Error in DatagramSocket: IP_DROP_MEMBERSHIP error: " << strerror(errno) << std::endl;
        assert(false);
        return false;
    }

    return true;
}

bool DatagramSocket::setMulticastInterface(unsigned int interfaceIP) {
    struct in_addr addr;
    addr.s_addr = htonl(interfaceIP); // 0 = default
    if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) != 0) {
        std::cout << "Error in DatagramSocket: could not set IP_MULTICAST_IF, error: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool DatagramSocket::sendBroadcast(unsigned int port,void * buf,unsigned int buflen)
{
    SOCKADDR_IN to;
    memset(&to,0,sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr= htonl(0xFFFFFFFF);
    to.sin_port = htons(port);
    bzero(&(to.sin_zero), 8);     /* zero the rest of the struct */
    auto ret = sendto(m_socket,(char*)buf,buflen,0,(sockaddr *) &to, sizeof ( SOCKADDR_IN ));
    if (ret!=buflen) {
        std::cout << "Error in DatagramSocket: sendto error: " << strerror(errno) << std::endl;
    }
    return ((unsigned int)ret == buflen);
}

bool DatagramSocket::sendTo(const GenericAddr & addr,const void *buf,unsigned int buflen)
{
    if (buflen == 0) {
        assert(false);
        return false;
    }

    SOCKADDR_IN to;
    memset(&to,0,sizeof(to));
    to.sin_family = addr.family;
    to.sin_addr.s_addr = htonl(addr.ip);
    to.sin_port = htons(addr.port);
    bzero(&(to.sin_zero), 8);     /* zero the rest of the struct */
    auto ret = sendto(m_socket,(void *)buf,buflen,0,(sockaddr *)&to,sizeof(sockaddr));
    if (ret != buflen) {
        std::cout << "Error in DatagramSocket: sendto error: " << strerror(errno) << " on interface " << ipIntToStr(addr.ip) << std::endl;
    }
    return ((unsigned int)ret == buflen);
}

bool DatagramSocket::recvFrom(GenericAddr & addr,void * buf,unsigned int & buflen)
{
    SOCKADDR_IN from;
    memset((void*)&from,0,sizeof(from));
    int from_addr_len = sizeof(from);
    auto res = recvfrom(m_socket,(void*)buf,buflen,0,(sockaddr*)&from,(socklen_t*)&from_addr_len);

    // we received datas
    if (res > 0) {
        assert(from_addr_len == sizeof(sockaddr));
        buflen = static_cast<unsigned int>(res);

        addr.family = AF_INET;
        addr.ip = ntohl(from.sin_addr.s_addr);
        addr.port = ntohs(from.sin_port);
        
        return true;
    } else {
        if (errno == EWOULDBLOCK) {
            // that's normal for non blocking sockets
            // when there is no data
            buflen = 0;
            return true;
        }
        else
        {
            // no datas, no error
            buflen = 0;
            return true;
        }
    }
}

#endif

/*********************************************************************************
  WIN32 version
*********************************************************************************/

#if defined(_WIN32)

#include <cassert>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

std::vector<NetworkInterface> listIPv4Interfaces()
{
    std::vector<NetworkInterface> result;

    const ULONG family = AF_INET;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER;

    ULONG bufLen = 15000;
    std::vector<unsigned char> buffer;
    PIP_ADAPTER_ADDRESSES adapters = nullptr;
    DWORD ret = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && ret == ERROR_BUFFER_OVERFLOW; ++attempt)
    {
        buffer.resize(bufLen);
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(family, flags, nullptr, adapters, &bufLen);
    }
    if (ret != NO_ERROR)
        return result;

    for (const IP_ADAPTER_ADDRESSES* ad = adapters; ad != nullptr; ad = ad->Next)
    {
        if (ad->OperStatus != IfOperStatusUp)
            continue;
        // keep loopback, skip non-multicast adapters
        if (ad->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
            (ad->Flags & IP_ADAPTER_NO_MULTICAST) != 0)
            continue;

        for (const IP_ADAPTER_UNICAST_ADDRESS* ua = ad->FirstUnicastAddress; ua != nullptr; ua = ua->Next)
        {
            if (ua->Address.lpSockaddr == nullptr ||
                ua->Address.lpSockaddr->sa_family != AF_INET)
                continue;

            const auto* sin = reinterpret_cast<const sockaddr_in*>(ua->Address.lpSockaddr);
            const unsigned int hostOrder = ntohl(sin->sin_addr.s_addr);

            result.push_back(NetworkInterface{ipIntToStr(hostOrder), hostOrder});
        }
    }

    return result;
}

DatagramSocket::DatagramSocket(unsigned int interfaceIP, unsigned int port)
{
    m_port = port;

    static bool wsaStartedUp = false;
    if (wsaStartedUp == false) {
        wsaStartedUp = true;
        WSADATA wsaData = {0};
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    // create socket
    m_socket=socket ( AF_INET, SOCK_DGRAM, 0 );
    if (INVALID_SOCKET ==  m_socket) {
        std::cout << "Error: Could not create DatagramSocket" << std::endl;
        closeSocket();
        return;
    }

    // enable bradcast
    BOOL fBroadcast=1;
    int err = setsockopt(m_socket,SOL_SOCKET,
            SO_BROADCAST,(CHAR *)&fBroadcast,
            sizeof(BOOL));
    if ( SOCKET_ERROR == err ) {
        std::cout << "Error: Could not set DatagramSocket SO_BROADCAST option" << std::endl;
        closeSocket();
        return;
    }

    // reuseaddr
    BOOL fReUseAddr=1;
    err = setsockopt(m_socket,SOL_SOCKET,
            SO_REUSEADDR,(CHAR *)&fReUseAddr,
            sizeof(BOOL));
    if ( SOCKET_ERROR == err ) {
        std::cout << "Error: Could not set DatagramSocket SO_REUSEADDR option" << std::endl;
    }

    // non blocking
    unsigned long value = 1;
    err = ioctlsocket(m_socket, FIONBIO, &value);
    if ( SOCKET_ERROR == err ) {
        std::cout << "Error: Could not set DatagramSocket FIONBIO value" << std::endl;
        closeSocket();
        return;
    }

    // increase buffer size
    int bufLen = 200000;
    err = setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (CHAR *)&bufLen, sizeof(bufLen));
    if ( SOCKET_ERROR == err ) {
        std::cout << "Error: Could not set DatagramSocket SO_RCVBUF option" << std::endl;
    }

    // If port is 0, bind anyway so when sending a packet the OS know on which network to send it
    // (in case both networks have the same IP mask (ie 192.168.1.xxx / 255.255.255.0)
    SOCKADDR_IN own;
    own.sin_family = AF_INET;
    own.sin_addr.s_addr = htonl ( interfaceIP );
    own.sin_port = htons ( m_port );
    err = bind (m_socket, (SOCKADDR *) &own, sizeof (SOCKADDR_IN) );
    if (err != 0) {
        if (port != 0) {
            std::cout << "Failed to bind datagram socket" << std::endl;
            assert(false);
            closeSocket();
            return;
        }
    }
}

DatagramSocket::~DatagramSocket()
{
    closeSocket();
}

void DatagramSocket::closeSocket()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

bool DatagramSocket::isInitialized()
{
    return m_socket != INVALID_SOCKET;
}

bool DatagramSocket::joinMulticastGroup(unsigned int ip, unsigned int interfaceIP) {
    struct ip_mreq mreq;

    int ttl = 64; // Limits to same region
    if (setsockopt(
        m_socket,
        IPPROTO_IP,
        IP_MULTICAST_TTL,
        (char *)&ttl,
        sizeof(ttl)) == SOCKET_ERROR)
    {
        int osErr = WSAGetLastError();
        std::cout << "Error in DatagramSocket: could not set IP_MULTICAST_TTL (error " << std::to_string(osErr) << ")" << std::endl;
        assert(false);
    }

    #ifndef IP_ADD_SOURCE_MEMBERSHIP
        #define IP_ADD_SOURCE_MEMBERSHIP  15 /* join IP group/source */
    #endif

    // Join the multicast group from which to receive datagrams.
    mreq.imr_multiaddr.s_addr = htonl(ip);
    mreq.imr_interface.s_addr = htonl(interfaceIP);
    if (setsockopt (m_socket,
                    IPPROTO_IP,
                    IP_ADD_MEMBERSHIP,
                    (char FAR *)&mreq,
                    sizeof (mreq)) == SOCKET_ERROR)
    {
        int osErr = WSAGetLastError();

        // WSAEADDRNOTAVAIL error means that we already joined this group
        if (osErr != WSAEADDRNOTAVAIL)
        {
            std::cout << "Error in DatagramSocket: could not set IP_ADD_MEMBERSHIP (error " << std::to_string(osErr) << ")" << std::endl;
            assert(false);
            return false;
        }
    }

    return true;
}

bool DatagramSocket::leaveMulticastGroup(unsigned int ip, unsigned int interfaceIP) {
    #ifndef IP_DROP_SOURCE_MEMBERSHIP
        #define IP_DROP_SOURCE_MEMBERSHIP  16 /* leave IP group/source */
    #endif

    // Join the multicast group from which to receive datagrams.
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl(ip);
    mreq.imr_interface.s_addr = htonl(interfaceIP);
    if (setsockopt (m_socket,
                    IPPROTO_IP,
                    IP_DROP_SOURCE_MEMBERSHIP,
                    (char FAR *)&mreq,
                    sizeof (mreq)) == SOCKET_ERROR)
    {
        int osErr = WSAGetLastError();

        // WSAEADDRNOTAVAIL error means that we already joined this group
        if (osErr != WSAEADDRNOTAVAIL)
        {
            std::cout << "Error in DatagramSocket: could not set IP_DROP_SOURCE_MEMBERSHIP (error " << std::to_string(osErr) << ")" << std::endl;
            assert(false);
            return false;
        }
    }

    return true;
}

bool DatagramSocket::setMulticastInterface(unsigned int interfaceIP) {
    struct in_addr addr;
    addr.s_addr = htonl(interfaceIP); // 0 = default
    if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int osErr = WSAGetLastError();
        std::cout << "Error in DatagramSocket: could not set IP_MULTICAST_IF (error " << std::to_string(osErr) << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatagramSocket::sendBroadcast(unsigned int port, void * buf, unsigned int buflen)
{
    SOCKADDR_IN target;
    target.sin_family = AF_INET;
    target.sin_addr.s_addr=htonl ( INADDR_BROADCAST );
    target.sin_port = htons ( port );
    int res = sendto(m_socket,
        (char*)buf,buflen,0,
        (SOCKADDR *) &target, sizeof ( SOCKADDR_IN ));
    if (res != buflen) {
        assert(false);
        return false;
    }
    return true;
}

bool DatagramSocket::sendTo(const GenericAddr & addr, const void *buf, unsigned int buflen)
{
    SOCKADDR_IN target;
    target.sin_family = addr.family;
    target.sin_addr.s_addr= htonl(addr.ip);
    target.sin_port = htons(addr.port);
    int res = sendto(m_socket,(char*) buf,buflen,0,(SOCKADDR *) &target, sizeof ( SOCKADDR_IN ));
    if (res != buflen) {
        int osErr = WSAGetLastError();
        if (osErr == WSAEWOULDBLOCK) {
            // there is nothing on the input
            return true;
        } else if (osErr == WSAECONNRESET) {
            // nothing on the other hand, ignore
            return true;
        } else {
            std::cout << "Error in DatagramSocket: writing failed (error " << std::to_string(osErr) << ")" << std::endl;
        }

        return false;
    }
    return true;
}

bool DatagramSocket::recvFrom(GenericAddr& addr, void * buf, unsigned int & buflen)
{
    SOCKADDR_IN source;
    source.sin_family = AF_INET;
    source.sin_addr.s_addr = htonl(INADDR_ANY);
    source.sin_port = htons(m_port);
    int nSize = sizeof ( SOCKADDR_IN );
    buflen = recvfrom (m_socket,(char*)buf,buflen,0,(SOCKADDR FAR *) &source,&nSize);

    if (buflen == SOCKET_ERROR) {
        buflen = 0;

        int osErr = WSAGetLastError();
        if (osErr == WSAEWOULDBLOCK) {
            // there is nothing on the input
            return true;
        } else if (osErr == WSAECONNRESET) {
            // recfrom documentation froim Windows: WSAECONNRESET - On a UDP-datagram socket this error indicates a previous send operation resulted in an ICMP Port Unreachable message
            // Ignore this error or we'll get a connection reset after sending a packet to a non existing target
            return true;
        } else {
            std::cout << "Error in DatagramSocket: reading failed (error " << std::to_string(osErr) << ")" << std::endl;
        }

        return false;
    }

    addr.family = AF_INET;
    addr.ip = ntohl(source.sin_addr.s_addr);
    addr.port = ntohs(source.sin_port);

    return true;
}

#endif
