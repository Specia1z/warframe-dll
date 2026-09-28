#pragma once

#include "owf_console.hpp"
#include "owf_web.hpp"

#include <vector>

struct owfUdpProxy
{
	struct Route
	{
		soup::SocketAddr downstream_addr;
		soup::SharedPtr<soup::Socket> upstream;
	};

	inline static soup::SharedPtr<soup::Worker> downstream;
	inline static soup::SocketAddr upstream_addr;
	inline static std::vector<Route> routes;

	static Route* findRouteByDownstream(const soup::SocketAddr& addr) noexcept
	{
		for (auto& route : routes)
		{
			if (route.downstream_addr == addr)
			{
				return &route;
			}
		}
		return nullptr;
	}

	static Route* findRouteByUpstream(const soup::Socket& socket) noexcept
	{
		for (auto& route : routes)
		{
			if (route.upstream.get() == &socket)
			{
				return &route;
			}
		}
		return nullptr;
	}

	static void setUpstreamAddr(const soup::SocketAddr& newAddr)
	{
		if (newAddr != owfUdpProxy::upstream_addr)
		{
#if LOGGING
			conout << "owfUdpProxy: New upstream: " << newAddr.toString() << std::endl;
#endif
			const bool bind = owfUdpProxy::upstream_addr.ip.isZero();
			for (auto& route : owfUdpProxy::routes)
			{
				route.upstream->close();
			}
			owfUdpProxy::routes.clear();
			owfUdpProxy::upstream_addr = newAddr;
			if (bind)
			{
				SOUP_IF_UNLIKELY (!owfUdpProxy::bind())
				{
					conout << soup::ObfusString("Failed to bind UDP/6951.").str();
				}
			}
		}
	}

	static bool bind()
	{
		return g_serv.bindUdp(6951, [](soup::Socket& s, soup::SocketAddr&& addr, std::string&& data) SOUP_EXCAL
		{
			downstream = g_serv.getShared(s);

			auto route = findRouteByDownstream(addr);
			if (!route)
			{
				routes.emplace_back();
				route = &routes.back();
				route->downstream_addr = addr;
				route->upstream = g_serv.addSocket();
#if LOGGING
				conout << "owfUdpProxy: New route: " << route->downstream_addr.toString() << " -> " << upstream_addr.toString() << std::endl;
#endif
			}

#if LOGGING
			conout << "owfUdpProxy: " << route->downstream_addr.toString() << " -> " << upstream_addr.toString() << ": " << soup::string::bin2hex(data) << std::endl;
#endif
			if (route->upstream->udpClientSend(upstream_addr, data))
			{
				upstreamRecv(*route->upstream);
			}
		});
	}

	static void upstreamRecv(soup::Socket& upstream_socket)
	{
		upstream_socket.udpRecv([](soup::Socket& s, soup::SocketAddr&& addr, std::string&& data, soup::Capture&&)
		{
			auto route = findRouteByUpstream(s);
			if (route && upstream_addr == addr)
			{
#if LOGGING
				conout << "owfUdpProxy: " << upstream_addr.toString() << " -> " << route->downstream_addr.toString() << ": " << soup::string::bin2hex(data) << std::endl;
#endif
				static_cast<soup::Socket*>(downstream.get())->udpServerSend(route->downstream_addr, data);
			}
			else
			{
#if LOGGING
				conout << "owfUdpProxy: Discarding packet from " << addr.toString() << ": " << soup::string::bin2hex(data) << std::endl;
#endif
			}
			if (route)
			{
				upstreamRecv(s);
			}
		});
	}
};
