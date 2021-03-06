/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#include "ModuleManager.h"
#include "Worker.h"
#include "Message.h"
#include "Module.h"
#include "Detail/Log/Log.h"
#include "Common/StringUtils.hpp"
#include "ObjectCreateHelper.h"

namespace moon
{
	ModuleManager::ModuleManager()
		:m_nextWorker(0)
		,m_IncreaseModuleID(1)
		,m_MachineID(1)
	{

	}

	ModuleManager::~ModuleManager()
	{

	}

	void ModuleManager::Init(const std::string& config)
	{
		auto kv_config = string_utils::parse_key_value_string(config);

		if (!contains_key(kv_config, "worker_num"))
		{
			CONSOLE_WARN("config does not have [worker_num],will initialize worker_num = 1");
		}

		if (!contains_key(kv_config, "machine_id"))
		{
			CONSOLE_WARN("config does not have [machine_id],will initialize machine_id = 1");
		}

		int workerNum;

		if (contains_key(kv_config, "worker_num"))
		{
			workerNum = string_utils::string_convert<int>(kv_config["worker_num"]);
		}
		else
		{
			workerNum = 1;
		}

		if (contains_key(kv_config, "machine_id"))
		{
			m_MachineID = string_utils::string_convert<int>(kv_config["machine_id"]);
		}
		else
		{
			m_MachineID = 0;
		}

		for (uint8_t i = 0; i != workerNum; i++)
		{
			auto wk = std::make_shared<Worker>();
			m_Workers.push_back(wk);
			wk->SetID(i);
		}

		CONSOLE_TRACE("ModuleManager initialized with %d Worker thread.", workerNum);
	}

	uint8_t ModuleManager::GetMachineID()
	{
		return m_MachineID;
	}

	void ModuleManager::RemoveModule(ModuleID id)
	{
		uint8_t workerID = GetWorkerID(id);
		if (workerID < m_Workers.size())
		{
			m_Workers[workerID]->RemoveModule(id);
		}
	}

	void ModuleManager::Send(ModuleID sender, ModuleID receiver, const std::string & data, const std::string & userdata, uint64_t rpcID, uint8_t type)
	{
		Assert((type != (uint8_t)EMessageType::Unknown), "send unknown type message!");

		auto msg = ObjectCreateHelper<Message>::Create(data.size());
		msg->SetSender(sender);
		msg->SetReceiver(receiver);
		msg->WriteData(data);
		msg->SetUserData(userdata);
		msg->SetRPCID(rpcID);
		msg->SetType(EMessageType(type));
	
		uint8_t workerID = GetWorkerID(receiver);
		if (workerID < m_Workers.size())
		{
			m_Workers[workerID]->DispatchMessage(msg);
		}
	}

	void ModuleManager::SendEx(ModuleID sender, ModuleID receiver, const MemoryStreamPtr & data, const std::string & userdata, uint64_t rpcID, uint8_t type)
	{
		Assert((type != (uint8_t)EMessageType::Unknown), "send unknown type message!");

		auto msg = ObjectCreateHelper<Message>::Create(data);
		msg->SetSender(sender);
		msg->SetReceiver(receiver);
		msg->SetUserData(userdata);
		msg->SetRPCID(rpcID);
		msg->SetType(EMessageType(type));

		uint8_t workerID = GetWorkerID(receiver);
		if (workerID < m_Workers.size())
		{
			m_Workers[workerID]->DispatchMessage(msg);
		}
	}

	void ModuleManager::Broadcast(ModuleID sender, const std::string & data, const std::string & userdata, uint8_t type)
	{
		Assert((type != (uint8_t)EMessageType::Unknown), "send unknown type message!");

		auto msg = ObjectCreateHelper<Message>::Create(data.size());
		msg->SetSender(sender);
		msg->SetReceiver(0);
		msg->WriteData(data);
		msg->SetUserData(userdata);
		msg->SetType(EMessageType(type));

		for (auto& w : m_Workers)
		{
			w->Broadcast(msg);
		}
	}

	void ModuleManager::Run()
	{
		CONSOLE_TRACE("ModuleManager start");
		for (auto& w : m_Workers)
		{
			w->Run();
		}
	}

	void ModuleManager::Stop()
	{
		for (auto& w : m_Workers)
		{
			w->Stop();
		}
		CONSOLE_TRACE("ModuleManager stop");
	}

	uint8_t ModuleManager::GetNextWorkerID()
	{
		if (m_nextWorker < m_Workers.size())
		{
			uint8_t tmp = m_nextWorker.load();
			m_nextWorker++;
			if (m_nextWorker == m_Workers.size())
			{
				m_nextWorker = 0;
			}
			return tmp;
		}
		return 0;
	}

	void ModuleManager::AddModuleToWorker(uint8_t workerid, const ModulePtr& module)
	{
		for (auto& wk : m_Workers)
		{
			if (wk->GetID() == workerid)
			{
				wk->AddModule(module);
				return;
			}
		}
	}

	uint8_t ModuleManager::GetWorkerID(ModuleID actorID)
	{
		return ((actorID >> 16) & 0xFF);
	}

};

