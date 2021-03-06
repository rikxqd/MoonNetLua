require("functions")
require("Log")
require("SerializeUtil")

local protobuf 			= require "protobuf"
local MsgID 			= require("MsgID")
local Component 		= require("Component")
local Stream 			= require("Stream")
local BinaryReader 		= require("BinaryReader")

local GateLoginHandler = class("GateLoginHandler",Component)

function GateLoginHandler:ctor()
	GateLoginHandler.super.ctor(self)

	thisModule:RegisterMessageCB(MsgID.MSG_C2S_REQ_LOGIN, handler(self,self.OnRequestLogin))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SET_ACCOUNT_PLAYERID, handler(self,self.OnSetPlayerID))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SET_PLAYER_SCENEID, handler(self,self.OnSetSceneID))
	
	self.loginDatas = nil
	self.connects = nil
	self.loginModule = 0

	Log.Trace("ctor GateLoginHandler")
end

function GateLoginHandler:Start()
	GateLoginHandler.super.Start(self)

	self.loginDatas = thisModule:GetComponent("LoginDatas")
	assert(nil ~= self.loginDatas,"GateLoginHandler can not get LoginDatas")

	self.connects = thisModule:GetComponent("Connects")
	assert(nil ~= self.connects,"GateLoginHandler can not get Connects")
end

function GateLoginHandler:OnSessionClose( sessionID )
	Log.Trace("OnSessionClose sessionID[%u]",sessionID)
	if self.loginDatas:RemoveBySession(sessionID) then
		Log.Trace("SessionClose remove login serialNum success")
	end
end

function GateLoginHandler:OnClientClose(account, player)
	GateLoginHandler.super.OnClientClose(self,account,player)

	Log.ConsoleTrace("Client close accountID[%u] playerID[%u]",account,player)

	local sm = Stream.new()
	sm:WriteUInt16(MsgID.MSG_S2S_CLIENT_CLOSE)
	sm:WriteUInt64(account)
	sm:WriteUInt64(player)


	thisModule:Broadcast(sm:Bytes())

	self.connects:RemoveByAccount(account)
end

function GateLoginHandler:OnRequestLogin(uctx,data,rpc)
	Log.Trace("Message Size: %d",string.len(data))
	local s = Deserialize("NetMessage.C2SReqLogin",data)

	Log.Trace("1.Login username[%s] password[%s]", s.username,s.password)

	local  serialNum  =  self.loginDatas:CreateSerialNum()
	local  accountID  =  Util.HashString(s.username)
	local  sessionID  =  uctx;
	self.loginDatas:Add(serialNum,accountID,sessionID)

	Log.Trace("2.Login username[%s] password[%s] accountID[%u] sessionID[%d] add to login data ", s.username,s.password ,accountID,sessionID)

	-- send to login module
	local sm = Stream.new()
	sm:WriteUInt16(MsgID.MSG_C2S_REQ_LOGIN)
	sm:WriteUInt64(serialNum)
	sm:WriteUInt64(accountID)
	sm:WriteString(s.password)


	if self.loginModule == 0 then
		self.loginModule = thisModule:GetOtherModule("login")
	end

	assert(thisModule:GetLoginModule() ~= 0, "can not find login module")

	thisModule:SendRPC(thisModule:GetLoginModule(),sm:Bytes(),"",  function (data,userdata)
		Log.Trace("3.login receive echo accountID[%u] ", accountID);
		local ld = self.loginDatas:Find(serialNum)
		if nil == ld then
			Log.Warn("3.login receive echo accountID[%u], can not find logindatas", accountID);
			return
		end
	
		local br = BinaryReader.new(data)
		local loginret = br:ReadString()
		local sn = br:ReadUInt64()
		local act = br:ReadUInt64()


		assert(serialNum == sn and  accountID == act,"login check")

		if loginret == "Ok" then

			Log.Trace("4.login find login data accountID[%u] ", act);

			self.connects:SetAccount(ld.sessionID, act)

			Log.Trace("5.login add Connection data sessionID:%u accountID%u]  ", ld.sessionID, act);
			Log.Trace("6.login success [accountID %u]  ", act);
		end

		local s2clogin = { ret = loginret,accountID = act }
		local s2cmsg = Serialize(MsgID.MSG_S2C_LOGIN_RESULT,"NetMessage.S2CLogin",s2clogin)

		thisModule:SendNetMessage(sessionID,s2cmsg)

		if self.loginDatas:Remove(serialNum) then
			Log.Trace("7.login remove login data [%u] success", serialNum);
		end
	end )
end

function GateLoginHandler:OnSetPlayerID(uctx,data,rpc)
	local br = BinaryReader.new(data)
	local accountID = br:ReadUInt64()
	local playerID  = br:ReadUInt64()

	local conn = self.connects:FindByAccount(accountID)
	if nil == conn then
		Log.ConsoleError("OnSetPlayerID:can not find Connection.");
	end

	self.connects:SetPlayer(accountID,playerID)
	Log.Trace("set accountid[%u] playerid:%u",accountID,playerID)
end

function GateLoginHandler:OnSetSceneID(uctx,data,rpc)
	local br = BinaryReader.new(data)
	local playerID = br:ReadUInt64()
	local sceneID = br:ReadUInt64()

	local conn = self.connects:FindByPlayer(playerID)
	if nil == conn then
		Log.ConsoleError("OnSetSceneID:can not find Connection.");
	end

	conn.sceneID = sceneID
end

return GateLoginHandler

