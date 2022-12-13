--[[
Copyright 2008 Steven Barth <steven@midlink.org>
Copyright 2011 Jo-Philipp Wich <xm@subsignal.org>
Copyright 2013 Nils Schneider <nils@nilsschneider.net>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0
]]--

local util = require 'gluon.util'
local site = require 'gluon.site'
local sp = util.subprocess

local unistd = require 'posix.unistd'
local wait = require 'posix.sys.wait'

local f_keys = Form(translate("SSH keys"), translate("You can provide your SSH keys here (one per line):"), 'keys')
local s = f_keys:section(Section)
local keys = s:option(TextValue, "keys")
keys.wrap = "off"
keys.rows = 5
keys.default = util.readfile("/etc/dropbear/authorized_keys") or ""

function keys:write(value)
	value = util.trim(value:gsub("\r", ""))
	if value ~= "" then
		local f = io.open("/etc/dropbear/authorized_keys", "w")
		f:write(value, "\n")
		f:close()
	else
		unistd.unlink("/etc/dropbear/authorized_keys")
	end
end

local file = '/etc/dropbear/authorized_keys'
local uci = require('simple-uci').cursor()
local keys = {}

local site_keys = s:option(Flag, "disable_site_keys", translate("Disable and remove site keys"))
site_keys.default = uci:get_bool('gluon', 'core', 'disable_site_keys')

local function load_keys()
	for line in io.lines(file) do
		keys[line] = true
	end
end

function site_keys:write(value)
	-- NOTE: re-adding keys happens automatically if this option is off, so we don't need to take care of that
	if value and not site_keys.default then
		pcall(load_keys)

		local f = io.open(file, 'w')
		local all_site_keys = {}
		for _, key in ipairs(site.authorized_keys()) do
			all_site_keys[key] = true
		end

		for key, _ in pairs(keys) do
			if not all_site_keys[key] then
				f:write(key .. '\n')
			end
		end

		f:close()
		keys.default = ""
	end

	uci:set('gluon', 'core', 'disable_site_keys', value)
	uci:save('gluon')
end

local config = site.config_mode.remote_login
if not config.show_password_form(false) then
	-- password login is disabled in site.conf
	return f_keys
end

local min_password_length = config.min_password_length(12)
local mintype = 'minlength(' .. min_password_length .. ')'
local length_hint

if min_password_length > 1 then
	length_hint = translatef("%u characters min.", min_password_length)
end

local f_password = Form(translate("Password"), translate(
	"Alternatively, you can set a password to access your node. Please choose a "
	.. "secure password you don't use anywhere else.<br><br>If you set an empty "
	.. "password, login via password will be disabled. This is the default."
	), 'password'
)
f_password.reset = false

s = f_password:section(Section)

local pw1 = s:option(Value, "pw1", translate("Password"))
pw1.password = true
pw1.optional = true
pw1.datatype = mintype
function pw1.cfgvalue()
	return ''
end

local pw2 = s:option(Value, "pw2", translate("Confirmation"), length_hint)
pw2.password = true
pw2.optional = true
pw2.datatype = mintype
function pw2.cfgvalue()
	return ''
end

local function set_password(password)
	local options = {
		stdin = sp.PIPE,
		stdout = sp.DEVNULL,
		stderr = sp.DEVNULL,
	}
	local pid, pipe = sp.popen('passwd', {}, options)
	if not pid then
		return false
	end
	local inw = pipe.stdin

	unistd.write(inw, string.format('%s\n%s\n', password, password))
	unistd.close(inw)

	local wpid, status, code = wait.wait(pid)
	return wpid and status == 'exited' and code == 0
end

function f_password:write()
	if pw1.data ~= pw2.data then
		f_password.errmessage = translate("The password and the confirmation differ.")
		return
	end

	local pw = pw1.data

	if pw ~= nil and #pw > 0 then
		if set_password(pw) then
			f_password.message = translate("Password changed.")
		else
			f_password.errmessage = translate("Unable to change the password.")
		end
	else
		-- We don't check the return code here as the error 'password for root is already locked' is normal...
		os.execute('passwd -l root >/dev/null')
		f_password.message = translate("Password removed.")
	end
end

return f_keys, f_password
