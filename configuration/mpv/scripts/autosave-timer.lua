-- Periodically saves "watch later" data during playback, rather than only on
-- quit. The autosave period can be changed from the default of 5 seconds by
-- creating "$MPV_CONF_DIR/script-opts/autosave-timer.conf" then populating
-- with the desired save period:
--
--     save_period=60
--
-- Adapted from
-- <https://gist.github.com/TheDrHax/fa2dba9023aace168de029f51799948d>.
local mp = require 'mp'
local mp_options = require 'mp.options'

local options = {
    save_period = 5
}

local previous_path = nil
local timer = nil

local function save()
    mp.command("write-watch-later-config")
end

local function pause_handler(_, paused)
    if timer then
        if paused then
            timer:kill()
        else
            timer:resume()
        end
    end

    save()
end

local function file_loaded()
    if previous_path then
        -- We have to delete the entry for the previous file so playlist
        -- resumption works as expected.
        mp.commandv("delete-watch-later-config", previous_path)
    end

    previous_path = mp.get_property("path")

    if timer then
        timer:kill()
    end

    if mp.get_property_bool("seekable", true) then
        if timer then
            timer:resume()
        else
            timer = mp.add_periodic_timer(options.save_period, save)
        end
    end
end

mp_options.read_options(options, "autosave-timer")
mp.register_event("file-loaded", file_loaded)
mp.observe_property("pause", "bool", pause_handler)
