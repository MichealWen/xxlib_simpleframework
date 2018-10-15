﻿
WEB_PkgGenMd5_Value = 'af917fae71ebd6f269da6026e8cf9da5'

--[[
指令基类. 路由用户上下文. 校验身份权限.
]]
WEB_WEB_testcpp3_AuthInfo = {
    typeName = "WEB_WEB_testcpp3_AuthInfo",
    typeId = 24,
    Create = function()
        local o = {}
        o.__proto = WEB_WEB_testcpp3_AuthInfo
        o.__index = o
        o.__newindex = o
		o.__isReleased = false
		o.Release = function()
			o.__isReleased = true
		end


        o.id = 0 -- Int32
        return o
    end,
    FromBBuffer = function( bb, o )
        o.id = bb:ReadInt32()
    end,
    ToBBuffer = function( bb, o )
        bb:WriteInt32( o.id )
    end
}
BBuffer.Register( WEB_WEB_testcpp3_AuthInfo )
WEB_WEB_testcpp3_Cmd1 = {
    typeName = "WEB_WEB_testcpp3_Cmd1",
    typeId = 25,
    Create = function()
        local o = {}
        o.__proto = WEB_WEB_testcpp3_Cmd1
        o.__index = o
        o.__newindex = o
		o.__isReleased = false
		o.Release = function()
			o.__isReleased = true
		end


        setmetatable( o, WEB_WEB_testcpp3_AuthInfo.Create() )
        return o
    end,
    FromBBuffer = function( bb, o )
        local p = getmetatable( o )
        p.__proto.FromBBuffer( bb, p )
    end,
    ToBBuffer = function( bb, o )
        local p = getmetatable( o )
        p.__proto.ToBBuffer( bb, p )
    end
}
BBuffer.Register( WEB_WEB_testcpp3_Cmd1 )
WEB_WEB_testcpp3_Cmd2 = {
    typeName = "WEB_WEB_testcpp3_Cmd2",
    typeId = 26,
    Create = function()
        local o = {}
        o.__proto = WEB_WEB_testcpp3_Cmd2
        o.__index = o
        o.__newindex = o
		o.__isReleased = false
		o.Release = function()
			o.__isReleased = true
		end


        setmetatable( o, WEB_WEB_testcpp3_AuthInfo.Create() )
        return o
    end,
    FromBBuffer = function( bb, o )
        local p = getmetatable( o )
        p.__proto.FromBBuffer( bb, p )
    end,
    ToBBuffer = function( bb, o )
        local p = getmetatable( o )
        p.__proto.ToBBuffer( bb, p )
    end
}
BBuffer.Register( WEB_WEB_testcpp3_Cmd2 )