/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "NetworkFrame.h"
#include "Session.h"
#include "NetworkServicePool.h"

#include "Detail/Log/Log.h"
#include "Common/LoopThread.hpp"
#include "Common/TupleUtils.hpp"
#include "Common/SyncQueue.hpp"
#include "Common/StringUtils.hpp"

namespace moon
{
	struct NetWorkFrame::Imp
	{
		Imp(uint8_t n)
			:servicepool(n),
			acceptor(servicepool.PollAService().GetIoService()),
			signals(servicepool.PollAService().GetIoService()),
			threadNum(n),
			bOpen(false)
		{

		}

		NetworkServicePool													servicepool;
		asio::ip::tcp::acceptor													acceptor;
		asio::signal_set																signals;
		asio::error_code															errorCode;
		//������ַ
		std::string																		listenAddress;
		//�����˿�
		std::string																		listenPort;
		//�����߳���
		uint8_t																			threadNum;

		bool																				bOpen;
	};

	NetWorkFrame::NetWorkFrame(const NetMessageDelegate& netMessageDelegate, uint8_t threadNum)
		:m_Imp(std::make_shared<Imp>(threadNum)), m_Delegate(netMessageDelegate)
	{
	}

	NetWorkFrame::~NetWorkFrame()
	{
		if (m_Imp->bOpen)
		{
			Stop();
		}
	}

	void NetWorkFrame::Listen(const std::string& ip, const std::string& port)
	{
		m_Imp->listenAddress = ip;
		m_Imp->listenPort = port;

		// Register to handle the signals that indicate when the AsioNetFrame should exit.
		// It is safe to register for the same signal multiple times in a program,
		// provided all registration for the specified signal is made through Asio.
		//	m_Imp->signals.add(SIGINT);
		//	m_Imp->signals.add(SIGTERM);
		//#if defined(SIGQUIT)
		//	m_Imp->signals.add(SIGQUIT);
		//#endif // defined(SIGQUIT)
		//	m_Imp->signals.async_wait(std::bind(&NetWorkFrame::stop, this));

	
		asio::ip::tcp::resolver resolver(m_Imp->servicepool.PollAService().GetIoService());
		asio::ip::tcp::resolver::query query(m_Imp->listenAddress, m_Imp->listenPort);
		asio::ip::tcp::endpoint endpoint = *resolver.resolve(query,m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			throw std::runtime_error(string_utils::format("resolve endpoint failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str()).data());
		}

		m_Imp->acceptor.open(endpoint.protocol(), m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			throw std::runtime_error(string_utils::format("acceptor.open failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str()).data());
		}

		// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
		m_Imp->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("acceptor.set_option SO_REUSEADDR failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str());
			return;
		}

		m_Imp->acceptor.bind(endpoint, m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("acceptor.bind failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str());
			return;
		}

		m_Imp->acceptor.listen();
	}

	void NetWorkFrame::PostAccept()
	{
		if (!m_Imp->acceptor.is_open())
		{
			return;
		}

		auto& ser = m_Imp->servicepool.PollAService();

		SessionPtr session =  ObjectCreateHelper<Session>::Create(m_Delegate, ser);

		m_Imp->acceptor.async_accept(session->GetSocket(), [session, this, &ser](const asio::error_code& e) {
			if (!e)
			{
				ser.AddSession(session);
				PostAccept();
				return;
			}
			m_Imp->errorCode = e;
		});
	}

	void NetWorkFrame::AsyncConnect(const std::string & ip, const std::string & port)
	{
		asio::ip::tcp::resolver resolver(m_Imp->servicepool.PollAService().GetIoService());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("resolve endpoint failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str());
			return;
		}

		auto& ser = m_Imp->servicepool.PollAService();

		SessionPtr session = ObjectCreateHelper<Session>::Create(m_Delegate, ser);

		asio::async_connect(session->GetSocket(), endpoint_iterator,
			[this, session, &ser, ip, port](const asio::error_code& e, asio::ip::tcp::resolver::iterator)
		{
			if (!e)
			{
				ser.AddSession(session);
			}
			else
			{
				CONSOLE_TRACE("connect failed:%s . address:%s  port%s.", e.message().c_str(), ip.c_str(), port.c_str());
			}
		});
	}

	SessionID moon::NetWorkFrame::SyncConnect(const std::string& ip, const std::string& port)
	{
		asio::ip::tcp::resolver resolver(m_Imp->servicepool.PollAService().GetIoService());
		asio::ip::tcp::resolver::query query(ip, port);
		asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("resolve endpoint failed:%s . address:%s  port:%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str());
			return 0;
		}

		auto& ser = m_Imp->servicepool.PollAService();

		SessionPtr session = ObjectCreateHelper<Session>::Create(m_Delegate, ser);

		asio::connect(session->GetSocket(), endpoint_iterator, m_Imp->errorCode);
		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("connect failed:%s . address:%s  port%s.", m_Imp->errorCode.message().c_str(), ip.c_str(), port.c_str());
			return 0;
		}

		ser.AddSession(session);

		return session->GetID();
	}

	void NetWorkFrame::Send(SessionID sessionID, const MemoryStreamPtr& msg)
	{
		m_Imp->servicepool.Send(sessionID, msg);
	}

	void moon::NetWorkFrame::CloseSession(SessionID sessionID, ESocketState state)
	{
		m_Imp->servicepool.CloseSession(sessionID, state);
	}

	void NetWorkFrame::Run()
	{
		if (m_Imp->bOpen)
			return;

		if (m_Imp->errorCode)
		{
			CONSOLE_TRACE("NetWorkFrame start failed:%s", m_Imp->errorCode.message().c_str());
			return;
		}

		LOG_TRACE("NetWorkFrame start succeed");

		m_Imp->servicepool.Run();

		PostAccept();

		m_Imp->bOpen = true;
	}

	void NetWorkFrame::Stop()
	{
		if (!m_Imp->bOpen)
		{
			return;
		}

		if (m_Imp->acceptor.is_open())
		{
			m_Imp->acceptor.close(m_Imp->errorCode);
		}

		m_Imp->servicepool.Stop();
		m_Imp->bOpen = false;

		LOG_TRACE("NetWorkFrame stop:%s", m_Imp->errorCode?m_Imp->errorCode.message().c_str():"OK");
	}

	int NetWorkFrame::GetErrorCode()
	{
		return m_Imp->errorCode.value();
	}

	std::string NetWorkFrame::GetErrorMessage()
	{
		return m_Imp->errorCode.message();
	}

	void NetWorkFrame::SetTimeout(uint32_t timeout)
	{
		auto& servs = m_Imp->servicepool.GetServices();
		for (auto iter = servs.begin(); iter != servs.end(); iter++)
		{
			iter->second->SetTimeout(timeout);
		}
	}
}





