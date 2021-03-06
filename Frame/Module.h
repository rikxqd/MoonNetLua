/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/
#pragma once
#include "MacroDefine.h"
#include "Common/noncopyable.hpp"

namespace moon
{
	class ModuleManager;
	DECLARE_SHARED_PTR(Message);

	class  Module :public noncopyable
	{
	public:
		friend class Worker;
		friend class ModuleManager;

		Module() noexcept;
		virtual ~Module() {}

		ModuleID 				GetID() const;
		const std::string		GetName() const;

		void							SetEnableUpdate(bool);
		bool							IsEnableUpdate();

		void							Send(ModuleID receiver, const std::string& data, const std::string& userdata, uint64_t rpcID, uint8_t type);

		void							SendByCache(ModuleID receiver, uint32_t cacheID, const std::string& userdata, uint64_t rpcID, uint8_t type);

		void							Broadcast(const std::string& data, const std::string& userdata, uint8_t type);

		uint32_t					CreateCache(const std::string& data);

		void							Exit();
	protected:
		virtual bool					Init(const std::string& config) { return true; };

		virtual void					Start() {}

		virtual void					Update(uint32_t interval);

		virtual void					Destory() {}

		virtual void					OnMessage(ModuleID sender, const std::string&data, const std::string& userdata, uint64_t rpcID, uint8_t type) {}

		size_t							GetMQSize();
		void								SetID(ModuleID moduleID);
		void								SetName(const std::string& name);
		void								SetManager(ModuleManager* mgr);

		void								SetOK(bool v);
		bool								IsOk();

		void								PushMessage(const MessagePtr& msg);
		/**
		*	check Message queue,handle a Message,if Message queue size >0,return true else return false
		*	@return
		*/
		bool								PeekMessage();
	private:
		struct  ModuleImp;
		std::shared_ptr<ModuleImp>		m_ModuleImp;
	};
};

