-- Copyright 2008 Steven Barth <steven@midlink.org>
-- Copyright 2017-2018 Matthias Schiffer <mschiffer@universe-factory.net>
-- Licensed to the public under the Apache License 2.0.

local unistd = require 'posix.unistd'
local classes = require 'gluon.web.model.classes'

local util = require 'gluon.web.util'
local instanceof = util.instanceof

-- Loads a model from given file, creating an environment and returns it
local function load(filename, i18n)
	local func = assert(loadfile(filename))

	setfenv(func, setmetatable({}, {__index =
		function(_, key)
			return classes[key] or i18n[key] or _G[key]
		end
	}))

	local models = { func() }

	for k, model in ipairs(models) do
		if not instanceof(model, classes.Node) then
			error("model definition returned an invalid model object")
		end
		model.index = k
	end

	return models
end

return function(config, http, renderer, name, pkg)
	local hidenav = false

	local modeldir = config.base_path .. '/model/'
	local filename = modeldir..name..'.lua'

	if not unistd.access(filename) then
		error("Model '" .. name .. "' not found!")
	end

	local i18n = setmetatable({
		i18n = renderer.i18n
	}, {
		__index = renderer.i18n(pkg)
	})

	local maps = load(filename, i18n)

	for _, map in ipairs(maps) do
		map:parse(http)
	end

	local reload = false

	for _, map in ipairs(maps) do
		map:handle()
		hidenav = hidenav or map.hidenav

		if map.reload and map.state == classes.FORM_VALID then
			reload = true
		end
	end

	--[[
		A model is built from the configuration as it was before the form was
		submitted, so a section that a write() created or removed is missing
		from what is rendered afterwards, and only appears once the page is
		loaded a second time.

		A model whose write() changes which sections exist can set `reload` to
		have them built again from the configuration it just wrote. It is not
		done for every model, because a write() may also have set up what is to
		be rendered - the setup wizard swaps in its reboot page there - and
		building the model again would throw that away.

		An invalid submission is never reloaded either: those models carry the
		error markers and what the user typed.
	]]
	if reload then
		maps = load(filename, i18n)

		hidenav = false
		for _, map in ipairs(maps) do
			hidenav = hidenav or map.hidenav
		end
	end

	renderer.render_layout('model/wrapper', {
		maps = maps,
	}, nil, {
		hidenav = hidenav,
	})
end
